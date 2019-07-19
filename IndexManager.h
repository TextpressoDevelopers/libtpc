/**
    Project: libtpc
    File name: TpcIndexReader.h
    
    @author valerio
    @version 1.0 7/25/17.
*/

#ifndef LIBTPC_TPCINDEXREADER_H
#define LIBTPC_TPCINDEXREADER_H

#include <vector>
#include <string>
#include <lucene++/LuceneHeaders.h>
#include <cfloat>
#include "CASManager.h"
#include "DataStructures.h"

namespace tpc {

    namespace index {

        static const std::string INDEX_ROOT_LOCATION("/usr/local/textpresso/luceneindex/");
        static const std::string CAS_ROOT_LOCATION("/usr/local/textpresso/tpcas/");
        static const std::string CORPUS_COUNTER_FILENAME("cc.cfg");
        static const std::string DOCUMENT_INDEXNAME("fulltext");
        static const std::string SENTENCE_INDEXNAME("sentence");
        static const std::string DOCUMENT_INDEXNAME_CS("fulltext_cs");
        static const std::string SENTENCE_INDEXNAME_CS("sentence_cs");

        static const int MAX_HITS(1000000);
        static const int FIELD_CACHE_MIN_HITS(30000);

        static const int MAX_NUM_SENTENCES_IN_QUERY(200);
        static const int MAX_NUM_DOCIDS_IN_QUERY(200);

        static const std::set<std::string> INDEX_TYPES{DOCUMENT_INDEXNAME, SENTENCE_INDEXNAME, DOCUMENT_INDEXNAME_CS,
                                                       SENTENCE_INDEXNAME_CS};
        static const std::string SUBINDEX_NAME = "subindex";
        static const std::set<std::string> DOCUMENTS_FIELDS_DETAILED{"accession_compressed", "title_compressed",
                                                                    "author_compressed", "journal_compressed", "year",
                                                                    "abstract_compressed", "filepath",
                                                                    "corpus", "doc_id",
                                                                    "fulltext_compressed", "type_compressed",
                                                                     "fulltext_cat_compressed"};
        static const std::set<std::string> SENTENCE_FIELDS_DETAILED{"sentence_id", "begin", "end",
                                                                    "sentence_compressed", "sentence_cat_compressed"};

        /*!
         * @struct TmpConf
         * @brief data structure that represents information about temporary configuration files of an index
         *
         * @var <b>new_index_flag</b> location of file which, if exists, indicates that a new index must be created
         * @var <b>index_descriptor</b> the location of the descriptor file for the index
         * @var <b>tmp_dir</b> the temporary directory where the configuration is stored
         */
        struct TmpConf {
            std::string new_index_flag;
            std::string index_descriptor;
            std::string tmp_dir;
        };

        class tpc_exception : public std::runtime_error {
        public:
            explicit tpc_exception(char const* const message) throw(): std::runtime_error(message) { }
            virtual char const* what() const throw() { return std::exception::what(); }
        };

        /*!
         * add, retrieve or remove documents from Textpresso index
         */
        class IndexManager {
        public:

            IndexManager() = default;

            /*!
             * create a new index manager object
             * @param index_path the path to the index
             * @param cas_path the path to cas (tpcas-2)
             * @param read_only whether the index should be opened in read-only mode
             * @param external whether the index is external or standalone
             */
            explicit IndexManager(const std::string& index_path, const std::string& cas_path="", bool read_only = true, bool external = false):
                    index_dir(index_path),
                    cas_dir(cas_path),
                    readonly(read_only),
                    external(external),
                    readers_map(),
                    corpus_doc_counter(),
                    externalIndexManager() { };
            ~IndexManager() {
                close();
            };
            IndexManager(const IndexManager& other) {
                readers_map = other.readers_map;
                index_dir = other.index_dir;
                cas_dir = other.cas_dir;
                readonly = other.readonly;
                external = other.external;
                corpus_doc_counter = other.corpus_doc_counter;
                externalIndexManager = other.externalIndexManager;
            };
            IndexManager& operator=(const IndexManager& other) {
                readers_map = other.readers_map;
                index_dir = other.index_dir;
                cas_dir = other.cas_dir;
                readonly = other.readonly;
                external = other.external;
                corpus_doc_counter = other.corpus_doc_counter;
                externalIndexManager = other.externalIndexManager;
            };
            IndexManager(IndexManager&& other) noexcept :
                    readers_map(std::move(other.readers_map)),
                    readonly(other.readonly),
                    external(other.external),
                    index_dir(std::move(other.index_dir)),
                    cas_dir(std::move(other.cas_dir)),
                    corpus_doc_counter(std::move(other.corpus_doc_counter)),
                    externalIndexManager(std::move(other.externalIndexManager)) {}
            IndexManager& operator=(IndexManager&& other) noexcept {
                readers_map = std::move(other.readers_map);
                index_dir = std::move(other.index_dir);
                cas_dir = std::move(other.cas_dir);
                readonly = other.readonly;
                external = other.external;
                corpus_doc_counter = std::move(other.corpus_doc_counter);
                externalIndexManager = std::move(other.externalIndexManager);
            };

            void close() {
                for (auto &it : readers_map) {
                    it.second->close();
                }
            }

            /*!
             * return the list of indexed corpora
             * @param cas_path location of cas files used to generate indices
             * @return a vector of strings, representing the list of available corpora in the index
             */
            static std::vector<std::string> get_available_corpora(const std::string& cas_path);

            /*!
             * return the list of additional corpora
             * @return a vector of strings, representing the list of additional corpora in the index
             */
            std::vector<std::string> get_additional_corpora();

            /*!
             * return the number of articles indexed under a specific corpus
             * @param corpus the value of the corpus
             * @param external whether to retrieve the number of articles per corpus from the external index
             * @return the numbe of articles indexed under the specified corpus
             */
            int get_num_articles_in_corpus(const std::string& corpus, bool external = false);

            /*!
             * @brief search the Textpresso index for documents matching the provided Lucene query and return summary
             * information with a list of results sorted by their match score
             *
             * The results returned by this method contain only the internal identifier of the indexed documents and
             * other summary information regarding the documents matching the provided query. To get detailed
             * information for the returned internal ids, use the function IndexManager::get_documents_details passing
             * the hit_documents vector in the DataStructures::SearchResults object returned by this function, or
             * IndexManager::get_document_details to retrieve details for a single document
             *
             * Note that while the documents are sorted by score, their matched sentences, in case of sentence searches,
             * are not sorted in order to obtain better performances
             * @param query a query object
             * @param matches_only perform a partial search that returns a Lucene internal object representing the
             * collection of matches. This object can be passed to a subsequent call to this method to continue the
             * search and get the complete results. This is useful to get an initial estimate of the size of the
             * complete search
             * @param doc_ids limit the search to a set of document ids. This is useful for sentence queries to retrieve
             * the sentence ids for a set of documents obtained by a previous search without ids
             * @param partialResults the results of a previous partial search. The search will be completed with the
             * sentence or document scores starting from the provided matching documents
             * @return the list of the documents matching the query sorted by their scores and encapsulated in a
             * SearchResutl object
             */
            SearchResults search_documents(const Query &query, bool matches_only = false,
                                           const std::set<std::string> &doc_ids = {},
                                           const SearchResults& partialResults = SearchResults());

            /*!
             * @brief get detailed information about a document specified by a DocumentSummary object
             *
             * @param doc_summary the DocumentSummary object that identifies the document
             * @param include_sentences_details whether to retrieve the details of the matching sentences specified in the
             * DocumentSummary object
             * @param include_doc_fields the list of fields to retrieve for the document. Retrieve all fields if not
             * specified
             * @param include_match_sentences_fields the list of fields to retrieve for the matching sentences specified in
             * the DocumentSummary object
             * @param exclude_doc_fields the list of fields to exclude for the document
             * @param exclude_match_sentences_fields the list of fields to exclude for the matching sentences specified in
             * the DocumentSummary object
             * @param include_all_sentences whether to retrieve the details of all sentences in the document
             * @param include_all_sentences_fields fields to be included for all sentences
             * @param exclude_all_sentences_fields fields to be excluded for all sentences
             * @param remove_tags whether to remove any tags (e.g., pdf tags) from the text, including the fulltext,
             * matching sentences and all sentences
             * @param remove_newlines whether to remove newlines and extra whitespaces from the text, including the
             * fulltext, matching sentences and all sentences
             * @return the detailed information of the document
             */
            DocumentDetails get_document_details(const DocumentSummary &doc_summary,
                                                 bool include_sentences_details = true,
                                                 std::set<std::string> include_doc_fields = DOCUMENTS_FIELDS_DETAILED,
                                                 std::set<std::string> include_match_sentences_fields = SENTENCE_FIELDS_DETAILED,
                                                 const std::set<std::string> &exclude_doc_fields = {},
                                                 const std::set<std::string> &exclude_match_sentences_fields = {},
                                                 bool include_all_sentences = false,
                                                 std::set<std::string> include_all_sentences_fields = SENTENCE_FIELDS_DETAILED,
                                                 const std::set<std::string> &exclude_all_sentences_fields = {},
                                                 bool remove_tags = false, bool remove_newlines = false);

            /*!
             * @brief get detailed information for a set of documents specified by a list of DocumentSummary objects
             *
             * @param doc_summaries a list of DocumentSummary object that identifies the documents to be searched and,
             * optionally, the list of sentences in the matching_sentences field of the document for which to retrieve
             * detailed information
             * @param sort_by_year whether to sort the results by year
             * @param include_sentences_details whether to retrieve the details of the matching sentences specified in the
             * DocumentSummary object
             * @param include_doc_fields the list of fields to retrieve for the document. Retrieve all fields if not
             * specified
             * @param include_match_sentences_fields the list of fields to retrieve for the matching sentences specified in
             * the DocumentSummary object
             * @param exclude_doc_fields the list of fields to exclude for the document
             * @param exclude_match_sentences_fields the list of fields to exclude for the matching sentences specified in
             * the DocumentSummary object
             * @param include_all_sentences whether to retrieve the details of all sentences in the document
             * @param include_all_sentences_fields fields to be included for all sentences
             * @param exclude_all_sentences_fields fields to be excluded for all sentences
             * @param remove_tags whether to remove any tags (e.g., pdf tags) from the text, including the fulltext,
             * matching sentences and all sentences
             * @param remove_newlines whether to remove newlines and extra whitespaces from the text, including the
             * fulltext, matching sentences and all sentences
             * @return the detailed information of the documents
             */
            std::vector<DocumentDetails> get_documents_details(const std::vector<DocumentSummary> &doc_summaries,
                                                               bool sort_by_year,
                                                               bool include_sentences_details = true,
                                                               std::set<std::string> include_doc_fields = DOCUMENTS_FIELDS_DETAILED,
                                                               std::set<std::string> include_match_sentences_fields = SENTENCE_FIELDS_DETAILED,
                                                               const std::set<std::string> &exclude_doc_fields = {},
                                                               const std::set<std::string> &exclude_match_sentences_fields = {},
                                                               bool include_all_sentences = false,
                                                               std::set<std::string> include_all_sentences_fields = SENTENCE_FIELDS_DETAILED,
                                                               const std::set<std::string> &exclude_all_sentences_fields = {},
                                                               bool remove_tags = false, bool remove_newlines = false);

            std::set<std::string> get_words_belonging_to_category_from_document_fulltext(const std::string& fulltext,
                                                                                         const std::string& fulltext_cat,
                                                                                         const std::string& category);

            // comparators for reverse sorting of documents and sentence objects
            static bool document_score_gt(const Document &a, const Document &b) { return a.score > b.score; }
            static bool document_year_score_gt(const Document &a, const Document &b) {
                if (a.year != b.year) return a.year > b.year;
                return a.score > b.score;
            }

            static bool sentence_greater_than(const SentenceSummary &a, const SentenceSummary &b) {
                return a.score > b.score;
            }

            /*!
             * create a textpresso index from a set of cas files
             * @param input_cas_dir the directory containing the cas files to be added to the index
             * @param max_num_papers_per_subindex max number of papers per subindex
             */
            void create_index_from_existing_cas_dir(const std::string &input_cas_dir,
                                                    const std::set<std::string>& file_list = {},
                                                    int max_num_papers_per_subindex = 50000);

            /*!
             * add a file to a textpresso index
             * @param file_path the path to a compressed cas file
             * @param literature the literature of the file
             * @param max_num_papers_per_subindex max number of papers per subindex
             */
            void add_file_to_index(const std::string& file_path, int max_num_papers_per_subindex = 50000);

            /*!
             * remove a specific file from the index
             * @param identifier the id of the file to remove, currently represented by the filepath field stored in
             * lucene
             */
            void remove_file_from_index(const std::string& identifier);

            /*!
             * update the document counters for the index and save them to file
             */
            void calculate_and_save_corpus_counter();

            /*!
             * create an external database for sentences containing their document ids
             */
            void save_all_doc_ids_for_sentences_to_db();

            /*!
             * create an external database for documents containing their year field
             */
            void save_all_years_for_documents_to_db();

            /*!
             * whether the index has an external index attached
             * @return true if the index has an external index attached, false otherwise
             */
            bool has_external_index();

            /*!
             * add an external index to the main one
             * @param external_idx_path the path to the external index
             */
            void set_external_index(std::string external_idx_path);

            /*!
             * remove the external index
             */
            void remove_external_index();

            /*!
             * get the list of additional corpora available from the external index
             * @return the list of additional corpora od the external index
             */
            std::vector<std::string> get_external_corpora();

        private:

            /*!
             * create a collection of sub-readers with multiple Lucene indexes
             * @param type the type of query to be performed by the subreaders
             * @param case_sensitive whether to get case sensitive subreaders
             * @return a collection of readers created from the Lucene indexes
             */
            Lucene::Collection<Lucene::IndexReaderPtr> get_subreaders(QueryType type, bool case_sensitive = false);

            /*!
             * collect and return document basic information for a collection of matches obtained from a document search
             * @param matches_collection the collection of documents matching the search query
             * @param subreaders the readers used during the search
             * @param searcher the searcher used during the search
             * @return the list of Document objects with information related to the matching documents, encapsulated in a
             * SearchResult object
             */
            SearchResults read_documents_summaries(const Lucene::Collection<Lucene::ScoreDocPtr> &matches_collection,
                                                   bool sort_by_year = false);

            /*!
             * collect and return document information for a collection of matches obtained from a sentence search
             * @param matches_collection the collection of sentences matching the search query
             * @param subreaders the readers used during the search
             * @param searcher the searcher used during the search
             * @return the list of Document objects with information related to the matching sentences and their respective
             * documents, encapsulated in a SearchResult object
             */
            SearchResults read_sentences_summaries(const Lucene::Collection<Lucene::ScoreDocPtr> &matches_collection,
                                                   bool sort_by_year = false);

            /*!
             * get detailed information for the sentences of a document specifed by a DocumentSummary object and update the
             * respective information in the provided DocumentDetails object
             * @param doc_summary a DocumentSummary object that identifies a document
             * @param doc_details the DocumentDetails object to be updated with the new detailed information about the
             * sentences
             * @param sent_parser a Lucene query parser
             * @param searcher a Lucene searcher
             * @param fsel a Lucene field selector
             * @param fields the set of fields to be retrieved for the sentences
             * @return the details of the document
             */
            void update_match_sentences_details_for_document(const DocumentSummary &doc_summary,
                                                             DocumentDetails &doc_details,
                                                             Lucene::QueryParserPtr sent_parser,
                                                             Lucene::SearcherPtr searcher,
                                                             Lucene::FieldSelectorPtr fsel,
                                                             const std::set<Lucene::String> &fields,
                                                             bool use_lucene_internal_ids,
                                                             Lucene::MultiReaderPtr sent_reader);

            /*!
             * get detailed information for the complete sentences list for a document specifed by a DocumentSummary
             * object and update the respective information in the provided DocumentDetails object
             * @param doc_details the DocumentDetails object that identifies the document related to the sentences to
             * be retrieved and that needs be updated with the new detailed information about the sentences. Note that
             * the object must contain a non null identifier
             * @param fsel a Lucene field selector
             * @param fields the set of fields to be retrieved for the sentences
             * @param internal_lucene_ids whether internal lucene ids are used for the search
             * @return the details of the document
             */
            void update_all_sentences_details_for_document(DocumentDetails &doc_details,
                                                           Lucene::FieldSelectorPtr fsel,
                                                           const std::set<Lucene::String> &fields);

            static std::set<Lucene::String> compose_field_set(const std::set<std::string> &include_fields,
                                                              const std::set<std::string> &exclude_fields,
                                                              const std::set<std::string> &required_fields = {});

            void update_document_details(DocumentDetails &doc_details, Lucene::String field,
                                         Lucene::DocumentPtr doc_ptr);

            std::vector<DocumentDetails> read_documents_details(const std::vector<DocumentSummary> &doc_summaries,
                                                                Lucene::QueryParserPtr doc_parser,
                                                                Lucene::SearcherPtr searcher,
                                                                Lucene::FieldSelectorPtr fsel,
                                                                const std::set<Lucene::String> &fields,
                                                                bool use_lucene_internal_ids,
                                                                Lucene::MultiReaderPtr doc_reader);

            template <typename Function> void transform_document_text_fields(Function f,
                    std::vector<DocumentDetails> &documents)
            {
                for (auto &document : documents) {
                    if (!document.abstract.empty()) {
                        document.abstract = f(document.abstract);
                    }
                    if (!document.fulltext.empty()) {
                        document.fulltext = f(document.fulltext);
                    }
                    for (auto &sentence : document.sentences_details) {
                        if (!sentence.sentence_text.empty()) {
                            sentence.sentence_text = f(sentence.sentence_text);
                        }
                    }
                    document.sentences_details.erase(std::remove_if(document.sentences_details.begin(),
                            document.sentences_details.end(), [](const SentenceDetails &s) {return s.sentence_text.empty() || s.sentence_text == " ";}),
                                    document.sentences_details.end());
                    for (auto &sentence : document.all_sentences_details) {
                        if (!sentence.sentence_text.empty()) {
                            sentence.sentence_text = f(sentence.sentence_text);
                        }
                    }
                    document.all_sentences_details.erase(std::remove_if(document.all_sentences_details.begin(),
                            document.all_sentences_details.end(), [](const SentenceDetails &s)
                            {
                                return s.sentence_text.empty() || s.sentence_text == " ";
                            }), document.all_sentences_details.end());
                }
            }

            /*!
             * write the temporary conf files for a subindex with the UIMA files needed
             * @param index_path the output directory of the subindex
             * @return a TmpConf object representing the information about the newly created files
             */
            static TmpConf write_tmp_conf_files(const std::string &index_path);

            /*!
             * create the directory structure for a subindex
             * @param index_path the path of the subindex to create
             */
            static void create_subindex_dir_structure(const std::string &index_path);

            /*!
             * add a cas file to the index. The cas file is processed through UIMA engine to extract sentences and other
             * features to be added to the index
             * @param file_path the path of the cas file to be added to the index
             * @param index_descriptor the index descriptor location
             * @param temp_dir the temp dir location
             * @param update_db whether to update the db with the new entry
             */
            int add_cas_file_to_index(const char *file_path, std::string index_descriptor, std::string tempDir,
                                      bool update_db);

            /*!
             * process a single file to be added to the index, calling the appropriate UIMA annotator
             * @param filepath the path of the file
             * @param first_paper whether the file is the first one to add to the subindex
             * @param tmp_conf the temporary configuration file names
             * @param update_db whether to update the entries in the db
             * @return true if the file was valid and it has been processed correctly, false otherwise
             */
            bool process_single_file(const std::string &filepath, bool &first_paper, const TmpConf &tmp_conf,
                                     bool update_db = false);

            std::string remove_document_from_index(std::string identifier, bool case_sensitive);
            void remove_sentences_for_document(const std::string& doc_id, bool case_sensitive);

            void add_doc_and_sentences_to_bdb(std::string identifier);

            void save_corpus_counter();

            void update_corpus_counter();

            /*!
             * load information about the number of documents indexed per corpus from file
             */
            void load_corpus_counter();
            int get_num_docs_in_corpus_from_index(const std::string& corpus);

            std::map<std::string, Lucene::IndexReaderPtr> readers_map;
            std::string index_dir;
            std::string cas_dir;
            bool readonly;
            bool external;
            std::map<std::string, int> corpus_doc_counter;
            std::shared_ptr<IndexManager> externalIndexManager;
        };
    }
}


#endif //LIBTPC_TPCINDEXREADER_H
