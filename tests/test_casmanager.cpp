/**
    Project: libtpc
    File name: reader_tests.cpp
    
    @author valerio
    @version 1.0 7/19/17.
*/

#include <boost/filesystem/operations.hpp>
#include "gtest/gtest.h"
#include "../CASManager.h"

using namespace tpc::cas;
using namespace boost::filesystem;
using namespace std;

namespace {

    class CASManagerTest : public testing::Test {
    protected:

        CASManagerTest() {
            pdf_file_path = "/usr/local/share/textpresso/data/pdf/C. elegans";
            tmp_dir = "/tmp/textpresso_test/tpcas";
        }

        // If the constructor and destructor are not enough for setting up
        // and cleaning up each test, you can define the following methods:

        void SetUp() override {
        }

        void TearDown() override{
        }

        string pdf_file_path;
        string tmp_dir;
    };

    void convert_dir_recursively(const string& inputDir, const string& outputDir, const string& literature) {
        for (directory_iterator dit(inputDir); dit != directory_iterator(); ++dit) {
            if (is_regular_file(*dit)) {
                CASManager::convert_raw_file_to_cas1(dit->path().string(), FileType::pdf, outputDir);
            } else if (is_directory(*dit)){
                convert_dir_recursively(dit->path().string(), outputDir, literature);
            }
        }
    }

    TEST_F(CASManagerTest, AddPdfToCAS) {
        path p(pdf_file_path);
        string literature = p.filename().string();
        create_directories(tmp_dir);
        convert_dir_recursively(pdf_file_path, tmp_dir, literature);
        ASSERT_TRUE(exists(tmp_dir));
    }
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
