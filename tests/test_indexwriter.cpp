/**
    Project: libtpc
    File name: test_indexwriter.cpp
    
    @author valerio
    @version 1.0 7/26/17.
*/

#include <boost/filesystem/operations.hpp>
#include "../TpcCommons.h"
#include "gtest/gtest.h"
#include "../TpcIndexReader.h"
#include "../TpcIndexWriter.h"

using namespace tpc;

namespace {

    class WriterTest : public testing::Test {
    protected:

        WriterTest() {
            cas_root_dir = "/usr/local/share/textpresso/data/tpcas";
            output_index_dir = "/tmp/textpresso/index_writer_test";
        }

        void SetUp() override {

        }

        void TearDown() override {

        }

        std::string cas_root_dir;
        std::string output_index_dir;
    };
    TEST_F(WriterTest, CreateIndexTest) {
        TpcIndexWriter::create_new_index(cas_root_dir + "/celegans", output_index_dir + "/celegans");
        ASSERT_EQ(boost::filesystem::exists(output_index_dir), true);
        TpcIndexWriter::create_new_index(cas_root_dir + "/pmcoa_celegans", output_index_dir + "/pmcoa_celegans");
        ASSERT_EQ(boost::filesystem::exists(output_index_dir), true);
        boost::filesystem::remove_all(output_index_dir);
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}