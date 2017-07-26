/**
    Project: libtpc
    File name: reader_tests.cpp
    
    @author valerio
    @version 1.0 7/19/17.
*/

#include <boost/filesystem/operations.hpp>
#include "../TpcCommons.h"
#include "gtest/gtest.h"
#include "../TpcIndexReader.h"

using namespace tpc;

namespace {

// The fixture for testing class Foo.
    class ReaderTest : public testing::Test {
    protected:

        ReaderTest() {
            index_root_dir = "/usr/local/textpresso/luceneindex/";
            literatures = {"C. elegans_0", "C. elegans Supplementals_0", "PMCOA Animal_0", "PMCOA C. elegans_0"};
        }

        // If the constructor and destructor are not enough for setting up
        // and cleaning up each test, you can define the following methods:

        void SetUp() override {
            query_sentence_woids.sort_by_year = false;
            query_sentence_woids.type = QueryType::sentence_without_ids;
            query_sentence_woids.query_text = "sentence:al";
            query_sentence_woids.case_sensitive = false;
            query_sentence_woids.literatures = literatures;

            query_sentence_wids.sort_by_year = false;
            query_sentence_wids.type = QueryType::sentence_with_ids;
            query_sentence_wids.query_text = "sentence:al";
            query_sentence_wids.case_sensitive = false;
            query_sentence_wids.literatures = literatures;

            query_sentence_year.sort_by_year = true;
            query_sentence_year.type = QueryType::sentence_without_ids;
            query_sentence_year.query_text = "sentence:test";
            query_sentence_year.case_sensitive = false;
            query_sentence_year.literatures = literatures;

            query_document.sort_by_year = true;
            query_document.type = QueryType::document;
            query_document.query_text = "fulltext:DYN-1";
            query_document.case_sensitive = false;
            query_document.literatures = literatures;
        }

        void TearDown() override{
        }

        std::string index_root_dir;
        std::vector<std::string> literatures;
        Query query_sentence_woids;
        Query query_sentence_wids;
        Query query_sentence_year;
        Query query_document;
    };

    TEST_F(ReaderTest, SearchReturnsCorrectTypeForDoc) {
        ASSERT_TRUE(
                TpcIndexReader::search_documents(index_root_dir, query_document).query.type == QueryType::document);
    }

    TEST_F(ReaderTest, SearchReturnsCorrectTypeForSentWoIds) {
        ASSERT_TRUE(
                TpcIndexReader::search_documents(index_root_dir, query_sentence_woids).query.type ==
                        QueryType::sentence_without_ids);
    }

    TEST_F(ReaderTest, SearchReturnsCorrectTypeForSentWIds) {
        ASSERT_TRUE(
                TpcIndexReader::search_documents(index_root_dir, query_sentence_wids).query.type ==
                QueryType::sentence_with_ids);
    }

    TEST_F(ReaderTest, SearchReturnsExpectedNumberOfHitsDocumentSearch) {
        ASSERT_GT(TpcIndexReader::search_documents(index_root_dir, query_document).hit_documents.size(), 0);
    }

    TEST_F(ReaderTest, SearchReturnsExpectedNumberOfHitsSentenceSearch) {
        ASSERT_GT(TpcIndexReader::search_documents(index_root_dir, query_sentence_woids).hit_documents.size(), 0);
    }

    TEST_F(ReaderTest, SearchReturnsNumSentencesGt0SentenceSearch) {
        ASSERT_GT(TpcIndexReader::search_documents(index_root_dir, query_sentence_woids).total_num_sentences, 0);
    }

    TEST_F(ReaderTest, SearchReturnsResultsOrderedByYear) {
        SearchResults results = TpcIndexReader::search_documents(index_root_dir, query_sentence_year);
        DocumentSummary prev = DocumentSummary();
        for (const DocumentSummary& doc : results.hit_documents) {
            if (!prev.year.empty()) {
                ASSERT_LE(doc.year, prev.year);
            }
        }
    }

    TEST_F(ReaderTest, SearchSummaryAndDetailsHaveSameSize) {
        SearchResults results = TpcIndexReader::search_documents(index_root_dir, query_document);
        std::vector<DocumentDetails> docDetails = TpcIndexReader::get_documents_details(
                results.hit_documents, index_root_dir, query_document.literatures, false, false);
        ASSERT_EQ(results.hit_documents.size(), docDetails.size());
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
