/**
    Project: libtpc
    File name: TpcCASManager.h
    
    @author valerio
    @version 1.0 7/28/17.
*/

#ifndef LIBTPC_TPCCASMANAGER_H
#define LIBTPC_TPCCASMANAGER_H

#include <string>

namespace tpc {

    namespace cas {

        static const std::string pdf2tpcasdescriptor("/usr/local/uima_descriptors/TpTokenizer.xml");
        static const std::string xml2tpcasdescriptor("/usr/local/uima_descriptors/TxTokenizer.xml");

        enum class FileType {
            pdf = 1, xml = 2
        };

        class CASManager {
        public:
            /*!
             * add an article in pdf or xml format to the Textpresso repository, transforming it in cas1 format
             * @param type the type of file
             * @param cas_repo_location the location of the cas repository
             * @param literature the name of the literature where to add the file
             * @param file_path the path to the file
             */
            static void add_file(FileType type, const std::string &cas_repo_location,
                                 const std::string &literature, const std::string &file_path);
        };
    }
}

#endif //LIBTPC_TPCCASMANAGER_H
