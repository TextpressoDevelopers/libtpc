/**
    Project: libtpc
    File name: TpcCASManager.cpp
    
    @author valerio
    @version 1.0 7/28/17.
*/

#include "cas-generators/pdf2tpcas/PdfInfo.h"
#include "cas-generators/Stream2Tpcas.h"
#include "cas-generators/xml2tpcas/ReadXml2Stream.h"
#include "CASManager.h"
#include <boost/filesystem/path.hpp>
#include <podofo/podofo.h>
#include <boost/filesystem/operations.hpp>

using namespace tpc::cas;
using namespace std;

void CASManager::add_file(FileType type, const string& cas_repo_location, const string& literature,
                             const string& file_path)
{
    // get rid of overwhelming messages from podofo library
    PoDoFo::PdfError::EnableDebug(false);
    PoDoFo::PdfError::EnableLogging(false);

    string file_name_no_ext = boost::filesystem::path(file_path).filename().string();
    string file_name_tpcas;
    size_t extPos = file_name_no_ext.rfind('.');
    if (extPos != std::string::npos) {
        file_name_no_ext.erase(extPos);
    }
    file_name_tpcas = file_name_no_ext + ".tpcas";
    string foutname = cas_repo_location + "/" + literature + "/" + file_name_no_ext;
    string fimageoutname = foutname + "/images";
    foutname.append("/" + file_name_tpcas);
    fimageoutname.append("/" + file_name_no_ext);
    boost::filesystem::create_directories(fimageoutname);
    stringstream sout;
    string file_time = to_string(boost::filesystem::last_write_time(file_path));
    switch (type) {
        case FileType::pdf:
            try {
                PdfInfo myInfo(file_path, fimageoutname);
                myInfo.StreamAll(sout);
                const char *descriptor = pdf2tpcasdescriptor.c_str();
                Stream2Tpcas stp(sout, foutname, descriptor, file_time);
                stp.processInputStream();
            } catch (PoDoFo::PdfError &e) {
                cerr << "Error: An error occurred during processing the pdf file." << endl << e.GetError() << endl
                     << file_path << endl;
                e.PrintErrorMsg();
            }
            break;
        case FileType::xml:
            ReadXml2Stream rs(file_path.c_str());
            std::stringstream sout;
            rs.GetStream(sout);
            const char *descriptor = xml2tpcasdescriptor.c_str();
            Stream2Tpcas stp(sout, foutname, descriptor, file_time);
            stp.processInputStream();
            break;
    }
}
