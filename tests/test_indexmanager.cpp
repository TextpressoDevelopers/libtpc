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
            int test = 0;
            index_root_dir = "/usr/local/share/textpresso/data/index";
            literatures = {"C. elegans", "C. elegans Supplementals"};
            cas_root_dir = "/usr/local/share/textpresso/data/tpcas";
            single_cas_files_dir = "/usr/local/share/textpresso/data/single_cas_files/C. elegans";
            output_index_dir = "/tmp/textpresso_test/index_writer_test";
            query_sentence.sort_by_year = false;
            query_sentence.type = QueryType::sentence;
            query_sentence.keyword = "al";
            query_sentence.case_sensitive = false;
            query_sentence.literatures = literatures;

            query_sentence_year.sort_by_year = true;
            query_sentence_year.type = QueryType::sentence;
            query_sentence_year.keyword = "al";
            query_sentence_year.case_sensitive = false;
            query_sentence_year.literatures = literatures;

            query_document.sort_by_year = true;
            query_document.type = QueryType::document;
            query_document.keyword = "al";
            query_document.case_sensitive = false;
            query_document.literatures = literatures;

            query_test_quoting.type = QueryType::document;
            query_test_quoting.keyword = "DNA AND Binding:2 OR DYN-1";
            query_test_quoting.case_sensitive = false;
            query_test_quoting.literatures = literatures;

            query_test_categories.type = QueryType::document;
            query_test_categories.accession = "WBPaper00046156 WBPaper00004263";
            query_test_categories.case_sensitive = false;
            query_test_categories.literatures = literatures;

            boost::filesystem::create_directories("/tmp/textpresso_test/index");
            indexManager = IndexManager("/tmp/textpresso_test/index", false);
            indexManager.create_index_from_existing_cas_dir(cas_root_dir + "/C. elegans");
            indexManager.save_all_years_for_documents_to_db();
            indexManager.save_all_doc_ids_for_sentences_to_db();
        }

        ~IndexManagerTest() {
            boost::filesystem::remove_all("/tmp/textpresso_test/index");
        }

        // If the constructor and destructor are not enough for setting up
        // and cleaning up each test, you can define the following methods:

        void SetUp() override {

        }

        void TearDown() override{

        }

        std::string index_root_dir;
        std::vector<std::string> literatures;
        Query query_sentence;
        Query query_sentence_year;
        Query query_document;
        Query query_test_quoting;
        Query query_test_categories;

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

    TEST_F(IndexManagerTest, AddSingleDocumentsToIndexTest) {
        indexManager.add_file_to_index(single_cas_files_dir + "/WBPaper00029298/WBPaper00029298.tpcas.gz");
    }

    TEST_F(IndexManagerTest, DeleteDocument) {
        indexManager.remove_file_from_index("C. elegans/WBPaper00046156/WBPaper00046156.tpcas.gz");
    }

    TEST_F(IndexManagerTest, QuotingQueryText) {
        indexManager.search_documents(query_test_quoting);
    }

    TEST_F(IndexManagerTest, GetWordsinCategories) {
        SearchResults results = indexManager.search_documents(query_test_categories);
        for (auto& docSummary : results.hit_documents) {
            DocumentDetails docDetails = indexManager.get_document_details(docSummary, false,
                                                                           {"doc_id", "fulltext_compressed",
                                                                            "fulltext_cat_compressed"}, {}, {}, {});
            std::set<std::string> words = indexManager.get_words_belonging_to_category_from_document_fulltext(
                    docDetails.fulltext, docDetails.categories_string, "Gene (C. elegans) (tpgce:0000001)");
        }
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
