/**
    Project: libtpc
    File name: TpcIndexWriter.cpp
    
    @author valerio
    @version 1.0 7/25/17.
*/

#include "TpcIndexWriter.h"
#include "Utils.h"
#include "uima-custom-analyzers/Tpcas2SingleIndex/CASUtils.h"
#include <boost/filesystem.hpp>
#include <string>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <uima/exceptions.hpp>
#include <uima/resmgr.hpp>
#include <uima/engine.hpp>
#include <xercesc/util/XMLString.hpp>
#include <uima/xmideserializer.hpp>

using namespace boost::filesystem;
using namespace std;
using namespace tpc;
using namespace xercesc;

void TpcIndexWriter::create_new_index(const std::string &input_cas_dir, const std::string &output_index_dir,
                                      int max_num_papers_per_subindex)
{
    path input_cas_dir_path(input_cas_dir);
    int counter_cas_files(0);
    bool first_paper;
    TmpConf tmp_conf = TmpConf();
    string subindex_dir;
    for (directory_iterator dir_it(input_cas_dir_path); dir_it != directory_iterator(); ++dir_it) {
        if (counter_cas_files % max_num_papers_per_subindex == 0) {
            // create new subindex
            subindex_dir = output_index_dir + "_" + to_string(counter_cas_files / max_num_papers_per_subindex);
            tmp_conf = write_tmp_conf_files(subindex_dir);
            first_paper = true;
            if (!exists(tmp_conf.new_index_flag)) {
                std::ofstream f_newindexflag(tmp_conf.new_index_flag.c_str());
                f_newindexflag << "newindexflag";
                f_newindexflag.close();
            }
            create_subindex_dir_structure(subindex_dir);
        }
        if (is_regular_file(dir_it->status())) {
            std::string filepath(dir_it->path().string());
            if (!process_single_file(filepath, first_paper, tmp_conf)) {
                continue;
            }
            ++counter_cas_files;
            cout << "total number of cas files added for " << input_cas_dir_path.filename().string() << ": "
                 << to_string(counter_cas_files) << endl;
        } else if (is_directory(dir_it->status())) {
            path subdir(dir_it->path().string().c_str());
            for (directory_iterator dir_it2(subdir); dir_it2 != directory_iterator(); ++dir_it2) {
                if (is_regular_file(dir_it2->status())) {
                    std::string filepath(dir_it2->path().string().c_str());
                    if (!process_single_file(filepath, first_paper, tmp_conf)) {
                        continue;
                    }
                    ++counter_cas_files;
                    cout << "total number of cas files added for "
                         << input_cas_dir_path.filename().string() << ": " << to_string(counter_cas_files) << endl;
                }
            }
        }
    }
}

void TpcIndexWriter::create_subindex_dir_structure(const string &index_path) {
    if (!exists(index_path)) {
        create_directories(index_path);
        create_directories(index_path + "/fulltext");
        create_directories(index_path + "/fulltext_cs");
        create_directories(index_path + "/sentence");
        create_directories(index_path + "/sentence_cs");
    }
}

TmpConf TpcIndexWriter::write_tmp_conf_files(const string &index_path) {
    // temp conf files
    std::string temp_dir;
    bool dir_created = false;
    while (!dir_created) {
        temp_dir = Utils::get_temp_dir_path();
        dir_created = create_directories(temp_dir);
    }
    Utils::write_index_descriptor(index_path, temp_dir + "/Tpcas2SingleIndex.xml", temp_dir);
    TmpConf tmpConf = TmpConf();
    tmpConf.index_descriptor = temp_dir + "/Tpcas2SingleIndex.xml";
    tmpConf.new_index_flag = temp_dir + "/newindexflag";
    tmpConf.tmp_dir = temp_dir;
    return tmpConf;
}

void TpcIndexWriter::add_cas_file_to_index(const char* file_path, string index_descriptor, string temp_dir_path) {
    std::string gzfile(file_path);
    string source_file = gzfile;
    boost::replace_all(source_file, ".tpcas.gz", ".bib");
    string bib_file = source_file;
    path source = source_file;
    string filename = source.filename().string();
    boost::replace_all(filename, ".tpcas.gz", ".bib");
    if(gzfile.find(".tpcas") == std::string::npos) {
        //std::cerr << "No .tpcas file found for file " << source.filename().string() << endl;
        return;
    }
    if(!exists(bib_file)) {
        //std::cerr << "No .bib file found for file " << source.filename().string() << endl;
        return;
    }
    string bib_file_temp = temp_dir_path + "/" + filename;
    path dst  = temp_dir_path + "/" + filename;
    boost::filesystem::copy_file(source,dst,copy_option::overwrite_if_exists);
    const char * descriptor = index_descriptor.c_str();
    string tpcasfile = Utils::decompress_gzip(gzfile, temp_dir_path);

    try {
        /* Create/link up to a UIMACPP resource manager instance (singleton) */
        (void) uima::ResourceManager::createInstance("TPCAS2LINDEXAE");
        uima::ErrorInfo errorInfo;
        uima::AnalysisEngine * pEngine
                = uima::Framework::createAnalysisEngine(descriptor, errorInfo);
        if (errorInfo.getErrorId() != UIMA_ERR_NONE) {
            std::cerr << std::endl
                      << "  Error string  : "
                      << uima::AnalysisEngine::getErrorIdAsCString(errorInfo.getErrorId())
                      << std::endl
                      << "  UIMACPP Error info:" << std::endl
                      << errorInfo << std::endl;
            exit((int) errorInfo.getErrorId());
        }
        uima::TyErrorId utErrorId; // Variable to store UIMACPP return codes
        /* Get a new CAS */
        uima::CAS* cas = pEngine->newCAS();
        if (cas == nullptr) {
            std::cerr << "pEngine->newCAS() failed." << std::endl;
            exit(1);
        }
        /* process input / cas */
        try {
            /* initialize from an xmicas */
            XMLCh* native = XMLString::transcode(tpcasfile.c_str());
            LocalFileInputSource fileIS(native);
            XMLString::release(&native);
            uima::XmiDeserializer::deserialize(fileIS, *cas, true);
            std::string filename(tpcasfile);
            /* process the CAS */
            auto text = getFulltext(*cas);
            if (text.length() > 0) {
                ((uima::AnalysisEngine *) pEngine)->process(*cas);
            } else {
                cout << "Empty file. Skip." << endl;
            }
        } catch (uima::Exception e) {
            uima::ErrorInfo errInfo = e.getErrorInfo();
            std::cerr << "Error " << errInfo.getErrorId() << " " << errInfo.getMessage() << std::endl;
            std::cerr << errInfo << std::endl;
        }
        /* call collectionProcessComplete */
        utErrorId = pEngine->collectionProcessComplete();
        /* Free annotator */
        utErrorId = pEngine->destroy();
        delete cas;
        delete pEngine;
        std::remove(tpcasfile.c_str()); //delete uncompressed temp casfile
        std::remove(bib_file_temp.c_str());
    } catch (uima::Exception e) {
        std::cerr << "Exception: " << e << std::endl;
    }
}

bool TpcIndexWriter::process_single_file(const string& filepath, bool& first_paper, const TmpConf& tmp_conf) {
    if (filepath.find(".tpcas") == std::string::npos)
        return false;
    cout << "processing cas file: " << filepath << endl;
    if (first_paper) {
        add_cas_file_to_index(filepath.c_str(), tmp_conf.index_descriptor, tmp_conf.tmp_dir);
        first_paper = false;
        boost::filesystem::remove(tmp_conf.new_index_flag);
    } else {
        add_cas_file_to_index(filepath.c_str(), tmp_conf.index_descriptor, tmp_conf.tmp_dir);
    }
    return true;
}

