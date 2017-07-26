/**
    Project: libtpc
    File name: TpcIndexWriter.h
    
    @author valerio
    @version 1.0 7/25/17.
*/

#ifndef LIBTPC_TPCINDEXWRITER_H
#define LIBTPC_TPCINDEXWRITER_H

#include <string>

namespace tpc {

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

    class TpcIndexWriter {
    public:

        /*!
         * create a textpresso index from a set of cas files
         * @param input_cas_dir the directory containing the cas files to be added to the index
         * @param output_index_dir the path of the directory where to save the index
         * @param max_num_papers_per_subindex max number of papers per subindex
         */
        static void create_new_index(const std::string& input_cas_dir, const std::string& output_index_dir,
                                     int max_num_papers_per_subindex = 50000);
        static void update_existing_index();
    private:
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
        static void add_cas_file_to_index(const char* file_path, std::string index_descriptor, std::string tempDir);

        /*!
         * process a single file to be added to the index, calling the appropriate UIMA annotator
         * @param filepath the path of the file
         * @param first_paper whether the file is the first one to add to the subindex
         * @param tmp_conf the temporary configuration file names
         * @return true if the file was valid and it has been processed correctly, false otherwise
         */
        static bool process_single_file(const std::string &filepath, bool& first_paper, const TmpConf& tmp_conf);
    };
}


#endif //LIBTPC_TPCINDEXWRITER_H
