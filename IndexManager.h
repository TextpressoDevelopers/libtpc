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

namespace tpc {

    namespace index {

        static const std::string index_root_location("/usr/local/textpresso/luceneindex/");
        static const std::string document_indexname("fulltext");
        static const std::string sentence_indexname("sentence");
        static const std::string document_indexname_cs("fulltext_cs");
        static const std::string sentence_indexname_cs("sentence_cs");

        static const int maxHits(1000000);
        static const int field_cache_min_hits(10000);

        static const int max_num_sentenceids_in_query(200);
        static const int max_num_docids_in_query(200);

        static const std::set<std::string> document_fields_detailed{"accession_compressed", "title_compressed",
                                                                    "author_compressed", "journal_compressed", "year",
                                                                    "abstract_compressed", "filepath",
                                                                    "literature_compressed", "identifier",
                                                                    "fulltext_compressed", "fulltext_cat_compressed"};
        static const std::set<std::string> sentence_fields_detailed{"sentence_id", "begin", "end",
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

        /*!
         * @enum QueryType
         * @brief the type of query to perform on the index
         */
        enum class QueryType {
            document = 1, sentence_without_ids = 2, sentence_with_ids = 3
        };

        /*!
         * @struct SentenceSummary
         * @brief data structure that contains summary information related to a sentence as the result of a search
         *
         * @var <b>sentence_id</b> the identifier of the sentence
         * stored in the Lucene index
         * @var <b>score</b> the score of the sentence returned by the search
         */
        struct SentenceSummary {
            int sentence_id{-1};
            double score{0};
        };

        /*!
         * @struct SentenceDetails
         * @brief data structure that contains detailed information related to a sentence
         *
         * @var <b>sentence_id</b> the identifier of the sentence
         * @var <b>doc_position_begin</b> position in the document where the sentence begins (-1 if not set)
         * @var <b>doc_position_end</b> position in the document where the sentence ends (-1 if not set)
         * @var <b>sentence_text</b> the text of the sentence
         * @var <b>categories_string</b> a string with the list of categories associated with each word in the sentence
         * @var <b>score</b> the score of the sentence
         */
        struct SentenceDetails : public SentenceSummary {
            int doc_position_begin{-1};
            int doc_position_end{-1};
            std::string sentence_text;
            std::string categories_string;
        };

        /*!
         * @struct Document
         * @brief generic information of a document
         *
         * @var <b>identifier</b> the identifier of the document
         * @var <b>score</b> the score of the document returned by a search
         * @var <b>year</b> the publication date, used to sort documents
         */
        struct Document {
            std::string identifier;
            double score{0};
            std::string year;
        };

        /*!
         * @struct DocumentSummary
         * @brief data structure that contains summary information related to a document as the result of a search
         *
         * @var <b>matching_sentences</b> the list of sentences contained in the document that match a sentence search
         */
        struct DocumentSummary : public Document {
            std::vector<SentenceSummary> matching_sentences;
        };

        /*!
         * @struct DocumentDetails
         * @brief data structure that contains detailed information related to a document as the result of a search
         *
         * @var <b>filepath</b> the filepath of the document
         * @var <b>fulltext</b> the fulltext of the document
         * @var <b>categories_string</b> the list of categories associated with the words in the fulltext
         * @var <b>abstract</b> the abstract of the document
         * @var <b>literature</b> the literature of the document
         * @var <b>accession</b> the accession of the document
         * @var <b>title</b> the title of the document
         * @var <b>author</b> the author(s) of the document
         * @var <b>journal</b> the journal of the document
         */
        struct DocumentDetails : public Document {
            std::string filepath;
            std::string fulltext;
            std::string categories_string;
            std::string abstract;
            std::string literature;
            std::string accession;
            std::string title;
            std::string author;
            std::string journal;
            std::vector<SentenceDetails> sentences_details;
        };

        /*!
         * query object
         */
        struct Query {
            QueryType type{QueryType::document};
            std::string query_text{""};
            bool case_sensitive{false};
            bool sort_by_year{false};
            std::vector<std::string> literatures{};
        };

        /*!
         * @struct SearchResults
         * @brief results generated by a search
         *
         * @var <b>query</b> the query that generated the result
         * @var <b>hit_socuments</b> the documents that match the search query
         * @var <b>total_num_sentences</b> number of sentences in the document
         * @var <b>max_score</b> documents highest score
         * @var <b>min_score</b> documents lowest score
         */
        struct SearchResults {
            Query query;
            std::vector<DocumentSummary> hit_documents{};
            size_t total_num_sentences{0};
            double max_score{0};
            double min_score{DBL_MAX};
        };

        class index_exception : public std::exception {
            const char *what() const throw() override {
                return "index not found";
            }
        };

        /*!
         * add, retrieve or remove documents from Textpresso index
         */
        class IndexManager {
        public:

            IndexManager() = default;
            explicit IndexManager(const std::string& index_path, bool read_only = true);
            ~IndexManager() {
                for (auto &it : readers_vec_map) {
                    for (const auto& reader : it.second) {
                        reader->close();
                    }
                }
            };
            IndexManager(const IndexManager& other) {
                readers_vec_map = other.readers_vec_map;
                readers_map = other.readers_map;
                readersname_map = other.readersname_map;
                index_dir = other.index_dir;
                available_literatures = other.available_literatures;
                available_subindices = other.available_subindices;
                readonly = other.readonly;
            };
            IndexManager& operator=(const IndexManager& other) {
                readers_vec_map = other.readers_vec_map;
                readers_map = other.readers_map;
                readersname_map = other.readersname_map;
                index_dir = other.index_dir;
                available_literatures = other.available_literatures;
                available_subindices = other.available_subindices;
                readonly = other.readonly;
            };
            IndexManager(IndexManager&& other) noexcept :
                    readers_vec_map(std::move(other.readers_vec_map)),
                    readers_map(std::move(other.readers_map)),
                    readersname_map(std::move(other.readersname_map)),
                    readonly(other.readonly) {  };
            IndexManager& operator=(IndexManager&& other) noexcept {
                readers_vec_map = std::move(other.readers_vec_map);
                readers_map = std::move(other.readers_map);
                readersname_map = std::move(other.readersname_map);
                index_dir = std::move(other.index_dir);
                available_literatures = std::move(other.available_literatures);
                available_subindices = std::move(other.available_subindices);
                readonly = other.readonly;
            };

            void close() {
                for (auto &it : readers_vec_map) {
                    for (const auto& reader : it.second) {
                        reader->close();
                    }
                }
            }

            /*!
             * @brief search the Textpresso index for documents matching the provided Lucene query and return summary
             * information with a list of results sorted by their match score
             *
             * The results returned by this method contain basic information regarding the documents matching the searches
             *
             * Note that while the documents are sorted by score, their matched sentences, in case of sentence searches,
             * are not sorted in order to obtain better performances
             * @param query a query object
             * @param doc_ids limit the search to a set of document ids. This is useful for sentence queries to retrieve
             * the sentence ids for a set of documents obtained by a previous search without ids
             * @return the list of the documents matching the query sorted by their scores and encapsulated in a
             * SearchResutl object
             */
            SearchResults search_documents(const Query &query, const std::set<std::string> &doc_ids = {});

            /*!
             * @brief get detailed information about a document specified by a DocumentSummary object
             *
             * @param doc_summary the DocumentSummary object that identifies the document
             * @param literatures the list of subindices to search
             * @param include_sentences_details whether to retrieve the details of the matching sentences specified in the
             * DocumentSummary object
             * @param include_doc_fields the list of fields to retrieve for the document. Retrieve all fields if not
             * specified
             * @param include_match_sentences_fields the list of fields to retrieve for the matching sentences specified in
             * the DocumentSummary object
             * @param exclude_doc_fields the list of fields to exclude for the document
             * @param exclude_match_sentences_fields the list of fields to exclude for the matching sentences specified in
             * the DocumentSummary object
             * @return the detailed information of the document
             */
            DocumentDetails get_document_details(const DocumentSummary &doc_summary,
                                                 const std::vector<std::string> &literatures,
                                                 bool include_sentences_details = true,
                                                 std::set<std::string> include_doc_fields = document_fields_detailed,
                                                 std::set<std::string> include_match_sentences_fields = sentence_fields_detailed,
                                                 const std::set<std::string> &exclude_doc_fields = {},
                                                 const std::set<std::string> &exclude_match_sentences_fields = {});

            /*!
             * @brief get detailed information for a set of documents specified by a list of DocumentSummary objects
             *
             * @param doc_summaries a list of DocumentSummary object that identifies the documents to be searched and,
             * optionally, the list of sentences in the matching_sentences field of the document for which to retrieve
             * detailed information
             * @param literatures the list of subindices to search
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
             * @return the detailed information of the documents
             */
            std::vector<DocumentDetails> get_documents_details(const std::vector<DocumentSummary> &doc_summaries,
                                                               const std::vector<std::string> &literatures,
                                                               bool sort_by_year,
                                                               bool include_sentences_details = true,
                                                               std::set<std::string> include_doc_fields = document_fields_detailed,
                                                               std::set<std::string> include_match_sentences_fields = sentence_fields_detailed,
                                                               const std::set<std::string> &exclude_doc_fields = {},
                                                               const std::set<std::string> &exclude_match_sentences_fields = {});

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
                                                    int max_num_papers_per_subindex = 50000);

            /*!
             * add a file to a textpresso index
             * @param file_path the path to a compressed cas file
             * @param literature the literature to which the file have to be added
             * @param max_num_papers_per_subindex max number of papers per subindex
             */
            void add_file_to_index(const std::string& file_path, const std::string& literature,
                                   int max_num_papers_per_subindex = 50000);

            void remove_file_from_index(const std::string& identifier, const std::string& literature);

        private:

            /*!
             * create a collection of sub-readers with multiple Lucene indexes
             * @param literatures the list of directory names for the indexed literatures
             * @param type the type of query to be performed by the subreaders
             * @param case_sensitive whether to get case sensitive subreaders
             * @return a collection of readers created from the Lucene indexes
             */
            Lucene::Collection<Lucene::IndexReaderPtr> get_subreaders(const std::vector<std::string> &literatures,
                                                                      QueryType type,
                                                                      bool case_sensitive = false);

            /*!
             * collect and return document basic information for a collection of matches obtained from a document search
             * @param matches_collection the collection of documents matching the search query
             * @param subreaders the readers used during the search
             * @param searcher the searcher used during the search
             * @return the list of Document objects with information related to the matching documents, encapsulated in a
             * SearchResult object
             */
            SearchResults read_documents_summaries(const Lucene::Collection<Lucene::ScoreDocPtr> &matches_collection,
                                                   const Lucene::Collection<Lucene::IndexReaderPtr> &subreaders,
                                                   Lucene::SearcherPtr searcher, bool sort_by_year = false);

            /*!
             * collect and return document information for a collection of matches obtained from a sentence search
             * @param matches_collection the collection of sentences matching the search query
             * @param subreaders the readers used during the search
             * @param searcher the searcher used during the search
             * @return the list of Document objects with information related to the matching sentences and their respective
             * documents, encapsulated in a SearchResult object
             */
            SearchResults read_sentences_summaries(const Lucene::Collection<Lucene::ScoreDocPtr> &matches_collection,
                                                   const Lucene::Collection<Lucene::IndexReaderPtr> &subreaders,
                                                   Lucene::SearcherPtr searcher, bool sort_by_year = false, bool return_match_sentences_ids = false);

            /*!
             * get detailed information for a document specified by a DocumentSummary object
             * @param doc_summary a DocumentSummary object that identifies a document
             * @param doc_parser a Lucene query parser
             * @param searcher a Lucene searcher
             * @param fsel a Lucene field selector
             * @param fields the set of fields to be retrieved for the document
             * @return the details of the document
             */

            DocumentDetails read_document_details(const DocumentSummary &doc_summary,
                                                  Lucene::QueryParserPtr doc_parser,
                                                  Lucene::SearcherPtr searcher,
                                                  Lucene::FieldSelectorPtr fsel,
                                                  const std::set<Lucene::String> &fields);

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
            void update_sentences_details_for_document(const DocumentSummary &doc_summary,
                                                       DocumentDetails &doc_details,
                                                       Lucene::QueryParserPtr sent_parser,
                                                       Lucene::SearcherPtr searcher,
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
                                                                const std::set<Lucene::String> &fields);

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
             */
            void add_cas_file_to_index(const char *file_path, std::string index_descriptor, std::string tempDir);

            /*!
             * process a single file to be added to the index, calling the appropriate UIMA annotator
             * @param filepath the path of the file
             * @param first_paper whether the file is the first one to add to the subindex
             * @param tmp_conf the temporary configuration file names
             * @return true if the file was valid and it has been processed correctly, false otherwise
             */
            bool process_single_file(const std::string &filepath, bool &first_paper, const TmpConf &tmp_conf);

            void remove_document_from_specific_subindex(const std::string& identifier, const std::string& literature,
                                                        QueryType type, bool case_sensitive);

            std::map<std::string, std::vector<Lucene::IndexReaderPtr>> readers_vec_map;
            std::map<std::string, Lucene::IndexReaderPtr> readers_map;
            std::string index_dir;
            std::set<std::string> available_literatures;
            std::set<std::string> available_subindices;
            bool readonly;
            std::map<std::string, std::vector<std::string>> readersname_map;
        };
    }
}


#endif //LIBTPC_TPCINDEXREADER_H
