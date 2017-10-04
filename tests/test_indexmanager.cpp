/**
    Project: libtpc
    File name: reader_tests.cpp
    
    @author valerio
    @version 1.0 7/19/17.
*/

#include <boost/filesystem/operations.hpp>
#include "gtest/gtest.h"
#include "../IndexManager.h"

using namespace tpc::index;

namespace {

    class IndexManagerTest : public testing::Test {
    protected:

        IndexManagerTest() {
            index_root_dir = "/usr/local/share/textpresso/data/index";
            literatures = {"celegans", "pmcoa_celegans"};
            cas_root_dir = "/usr/local/share/textpresso/data/tpcas";
            single_cas_files_dir = "/usr/local/share/textpresso/data/single_cas_files";
            output_index_dir = "/tmp/textpresso_test/index_writer_test";
            indexManager = IndexManager(index_root_dir);
        }

        // If the constructor and destructor are not enough for setting up
        // and cleaning up each test, you can define the following methods:

        void SetUp() override {
            query_sentence.sort_by_year = false;
            query_sentence.type = QueryType::sentence;
            query_sentence.query_text = "sentence:al";
            query_sentence.case_sensitive = false;
            query_sentence.literatures = literatures;

            query_sentence_year.sort_by_year = true;
            query_sentence_year.type = QueryType::sentence;
            query_sentence_year.query_text = "sentence:al";
            query_sentence_year.case_sensitive = false;
            query_sentence_year.literatures = literatures;

            query_document.sort_by_year = true;
            query_document.type = QueryType::document;
            query_document.query_text = "fulltext:al";
            query_document.case_sensitive = false;
            query_document.literatures = literatures;
        }

        void TearDown() override{
        }

        std::string index_root_dir;
        std::vector<std::string> literatures;
        Query query_sentence;
        Query query_sentence_year;
        Query query_document;

        std::string cas_root_dir;
        std::string single_cas_files_dir;
        std::string output_index_dir;
        IndexManager indexManager;
    };

    TEST_F(IndexManagerTest, SearchReturnsCorrectTypeForDoc) {
        ASSERT_TRUE(indexManager.search_documents(query_document).query.type == QueryType::document);
    }

    TEST_F(IndexManagerTest, SearchReturnsCorrectTypeForSent) {
        ASSERT_TRUE(indexManager.search_documents(query_sentence).query.type == QueryType::sentence);
    }

    TEST_F(IndexManagerTest, SearchReturnsExpectedNumberOfHitsDocumentSearch) {
        ASSERT_GT(indexManager.search_documents(query_document).hit_documents.size(), 0);
    }

    TEST_F(IndexManagerTest, SearchReturnsExpectedNumberOfHitsSentenceSearch) {
        ASSERT_GT(indexManager.search_documents(query_sentence).hit_documents.size(), 0);
    }

    TEST_F(IndexManagerTest, SearchReturnsNumSentencesGt0SentenceSearch) {
        ASSERT_GT(indexManager.search_documents(query_sentence).total_num_sentences, 0);
    }

    TEST_F(IndexManagerTest, SearchReturnsResultsOrderedByYear) {
        SearchResults results = indexManager.search_documents(query_sentence_year);
        DocumentSummary prev = DocumentSummary();
        for (const DocumentSummary& doc : results.hit_documents) {
            if (!prev.year.empty()) {
                ASSERT_LE(doc.year, prev.year);
            }
        }
    }

    TEST_F(IndexManagerTest, SearchSummaryAndDetailsHaveSameSize) {
        SearchResults results = indexManager.search_documents(query_document);
        std::vector<DocumentDetails> docDetails = indexManager.get_documents_details(
                results.hit_documents, false, false);
        ASSERT_EQ(results.hit_documents.size(), docDetails.size());
    }

    TEST_F(IndexManagerTest, CreateIndexTest) {
        IndexManager indexManager1("/tmp/textpresso_test/index");
        indexManager1.create_index_from_existing_cas_dir(cas_root_dir + "/celegans");
        ASSERT_EQ(boost::filesystem::exists("/tmp/textpresso_test/index/celegans_0"), true);
        indexManager1.create_index_from_existing_cas_dir(cas_root_dir + "/pmcoa_celegans");
        ASSERT_EQ(boost::filesystem::exists("/tmp/textpresso_test/index/pmcoa_celegans_0"), true);
        boost::filesystem::remove_all(output_index_dir);
    }

    TEST_F(IndexManagerTest, AddSingleDocumentsToIndexTest) {
        IndexManager indexManager1("/tmp/textpresso_test/index");
        indexManager1.add_file_to_index(single_cas_files_dir + "/WBPaper00029298.tpcas.gz");
    }

    TEST_F(IndexManagerTest, DeleteDocument) {
        IndexManager indexManager1("/tmp/textpresso_test/index", false);
        indexManager1.remove_file_from_index("076e42e3e7ee32a9");
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
