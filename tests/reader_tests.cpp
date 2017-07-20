/**
    Project: libtpc
    File name: reader_tests.cpp
    
    @author valerio
    @version 1.0 7/19/17.
*/

#include "../reader.h"
#include "gtest/gtest.h"

using namespace tpc::reader;

namespace {

// The fixture for testing class Foo.
    class ReaderTest : public testing::Test {
    protected:

        ReaderTest() {
            index_root_dir = "/usr/local/textpresso/luceneindex";
            literatures = {"C. elegans_0", "C. elegans Supplementals_0"};
        }

        virtual ~ReaderTest() {
        }

        // If the constructor and destructor are not enough for setting up
        // and cleaning up each test, you can define the following methods:

        virtual void SetUp() {
        }

        virtual void TearDown() {
        }

        std::string index_root_dir;
        std::vector<std::string> literatures;
    };

    TEST_F(ReaderTest, SearchReturnsCorrectTypeForDoc) {
        ASSERT_TRUE(SearchIndex::get_search_hits(index_root_dir, QueryType::document, "fulltext:al", literatures)
                            .query_type == QueryType::document);
    }

    TEST_F(ReaderTest, SearchReturnsCorrectTypeForSent) {
        ASSERT_TRUE(SearchIndex::get_search_hits(index_root_dir, QueryType::sentence, "sentence:al", literatures)
                            .query_type == QueryType::sentence);
    }

    TEST_F(ReaderTest, SearchReturnsExpectedNumberOfHitsDocumentSearch) {
        EXPECT_EQ(15351,
                  SearchIndex::get_search_hits(index_root_dir, QueryType::document, "fulltext:al", literatures)
                          .hit_documents.size());
    }

    TEST_F(ReaderTest, SearchReturnsExpectedNumberOfHitsSentenceSearch) {
        EXPECT_EQ(15351,
                  SearchIndex::get_search_hits(index_root_dir, QueryType::sentence, "sentence:al", literatures)
                          .hit_documents.size());
    }

    TEST_F(ReaderTest, SearchReturnsResultsOrderedByYear) {
        SearchResult results = SearchIndex::get_search_hits(index_root_dir, QueryType::sentence, "sentence:al",
                                                            literatures);
        Document prev;
        for (const Document& doc : results.hit_documents) {
            if (prev.year.empty()) {
                ASSERT_LE(doc.year, prev.year);
            }
        }

    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
