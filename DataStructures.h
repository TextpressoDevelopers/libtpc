/**
    Project: libtpc
    File name: Query.h
    
    @author valerio
    @version 1.0 11/1/17.
*/

#ifndef LIBTPC_QUERY_H
#define LIBTPC_QUERY_H

#include <vector>
#include <string>
#include <lucene++/LuceneHeaders.h>
#include <cfloat>

namespace tpc {

    namespace index {

        static const std::vector<std::string> LUCENE_SPECIAL_CHARS = {"!", "(", ")", "{", "}", "[", "]", "^",
                                                                      "~", ":"};

        /*!
         * @enum QueryType
         * @brief the type of query to perform on the index
         */
        enum class QueryType {
            document = 1, sentence = 2
        };

        enum class DocumentType {
            main = 1, external = 2
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
            int lucene_internal_id{-1};
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
         * @var <b>documentType</b> whether the document comes from main or external index
         */
        struct Document {
            std::string identifier;
            double score{0};
            std::string year;
            int lucene_internal_id{-1};
            DocumentType documentType{DocumentType::main};
        };

        /*!
         * @struct DocumentSummary
         * @brief data structure that contains summary information related to a document as the result of a search
         *
         * @var <b>matching_sentences</b> the list of sentences contained in the document that match a sentence search
         */
        struct DocumentSummary : public Document {
            std::vector <SentenceSummary> matching_sentences;
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
            std::vector <std::string> corpora;
            std::string accession;
            std::string title;
            std::string author;
            std::string journal;
            std::string type;
            std::vector <SentenceDetails> sentences_details;
            std::vector <SentenceDetails> all_sentences_details;
        };

        /*!
         * query object
         */
        class Query {
        public:
            QueryType type{QueryType::document};
            std::string keyword{""};
            std::string exclude_keyword{""};
            std::string year{""};
            std::string author{""};
            std::string accession{""};
            std::string journal{""};
            std::string paper_type{""};
            bool case_sensitive{false};
            bool sort_by_year{false};
            bool exact_match_author{false};
            bool exact_match_journal{false};
            bool categories_and_ed{true};
            std::vector<std::string> literatures{};
            std::vector<std::string> categories{};

            /*!
             * combine the query fields and get the full query text
             * @return the text for the Lucene query
             */
            std::string get_query_text() const;
        private:
            void add_field_to_text_if_not_empty(const std::string& field_value, const std::string& lucene_field_name,
                                                bool exact_match_field, std::string& query_text,
                                                bool concat_with_or = false) const;
            void add_categories_to_text(std::string& query_text) const;
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
         * @var <b>indexMatches</b> results of partial search
         * @var <b>externalMatches</b> results of partial search on external index
         */
        struct SearchResults {
            Query query;
            std::vector <DocumentSummary> hit_documents{};
            size_t total_num_sentences{0};
            double max_score{0};
            double min_score{DBL_MAX};
            Lucene::Collection <Lucene::ScoreDocPtr> partialIndexMatches{};
            Lucene::Collection <Lucene::ScoreDocPtr> partialExternalMatches{};

            void update(const SearchResults &other);
        };
    }
}


#endif //LIBTPC_QUERY_H
