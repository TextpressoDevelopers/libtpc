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
#include <boost/regex.hpp>
#include <regex>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <uima/xmideserializer.hpp>
#include "uima/xmiwriter.hpp"
#include "Utils.h"

using namespace tpc::cas;
using namespace std;

void CASManager::convert_raw_file_to_cas1(const string& file_path, FileType type, const string& out_dir,
                                          bool use_parent_dir_as_outname)
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
    string foutname;
    string fimageoutname;
    if (use_parent_dir_as_outname) {
        foutname = out_dir + "/" + boost::filesystem::path(file_path).parent_path().filename().string();
        fimageoutname = foutname + "/images/" + boost::filesystem::path(file_path).parent_path().filename().string();
    } else {
        foutname = out_dir + "/" + boost::filesystem::path(file_path).stem().string();
        fimageoutname = foutname + "/images/" + boost::filesystem::path(file_path).stem().string();
    }
    boost::filesystem::create_directories(foutname + "/images");
    foutname.append("/" + file_name_tpcas);
    stringstream sout;

    switch (type) {
        case FileType::pdf:
            try {
                PdfInfo myInfo(file_path, fimageoutname);
                myInfo.StreamAll(sout);
                const char *descriptor = PDF2TPCAS_DESCRIPTOR.c_str();
                Stream2Tpcas stp(sout, foutname, descriptor);
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
            const char *descriptor = XML2TPCAS_DESCRIPTOR.c_str();
            Stream2Tpcas stp(sout, foutname, descriptor);
            stp.processInputStream();
            break;
    }
}

int CASManager::convert_cas1_to_cas2(const string &file_path, const std::string &out_dir)
{
    string foutname = out_dir + "/" + boost::filesystem::path(file_path).parent_path().filename().string();
    string temp_dir_path = boost::filesystem::temp_directory_path().string();
    string tpcasfile = Utils::decompress_gzip(file_path, temp_dir_path);
    try {
        /* Create/link up to a UIMACPP resource manager instance (singleton) */
        (void) uima::ResourceManager::createInstance("TPCAS2LINDEXAE");
        uima::ErrorInfo errorInfo;
        const char* descriptor = TPCAS1_2_TPCAS2_DESCRIPTOR.c_str();
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
            string filehash = Utils::gettpfnvHash(*cas);
            /* process the CAS */
            auto text = Utils::getFulltext(*cas);
            if (text.length() > 0) {
                ((uima::AnalysisEngine *) pEngine)->process(*cas);
            } else {
                cout << "Skip file." << endl;
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
        return 1;
    } catch (uima::Exception e) {
        std::cerr << "Exception: " << e << std::endl;
        return 0;
    }
}

void CASManager::writeXmi(uima::CAS & outCas, int num, std::string outfn) {
    std::string ofn;
    ofn.append(outfn);
    ofn.append("_seg_");
    std::stringstream s;
    s << num;
    ofn.append(s.str());
    //open a file stream for output xmi
    std::ofstream file;
    file.open(ofn.c_str(), std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening output xmi: " << ofn.c_str() << std::endl;
        exit(99);
    }
    //serialize the cas
    uima::XmiWriter writer(outCas, true);
    writer.write(file);
    file.close();
}

BibInfo CASManager::get_bib_info_from_xml_text(const std::string &xml_text) {
    boost::regex nline("\\n");
    string xml_text_nn = boost::regex_replace(xml_text, nline, "");
    //find author
    boost::regex authorregex("\<contrib-group\>(.+?)\<\/contrib-group\>");
    boost::smatch author_matches;
    string author = "";
    string text_copy = xml_text_nn;
    while (boost::regex_search(text_copy, author_matches, authorregex)) {
        int size = author_matches.size();
        std::string hit_text = author_matches[1];
        boost::smatch name_matches;
        boost::regex nameregex("\<surname\>(.+?)\<\/surname\>\\s+\<given-names\>(.+?)\<\/given-names\>");
        while (boost::regex_search(hit_text, name_matches, nameregex)) {
            author = author + name_matches[1] + " " + name_matches[2] + ", ";
            hit_text = name_matches.suffix().str();
        }
        text_copy = author_matches.suffix().str();
    }
    boost::regex comma("\\, $");
    author = boost::regex_replace(author, comma, "");
    //find subject
    boost::regex subjectregex("\<subject\>(.+?)\<\/subject>");
    boost::smatch subject_matches;
    std::string subject = "";
    text_copy = xml_text_nn;
    while (boost::regex_search(text_copy, subject_matches, subjectregex)) {
        subject = subject + subject_matches[1] + ", ";
        text_copy = subject_matches.suffix().str();
    }
    subject = boost::regex_replace(subject, comma, "");
    //find accession
    std::string accession = "";
    boost::regex pmidregex("\<article-id pub-id-type=\"pmid\"\>(\\d+?)\<\/article-id\>");
    boost::regex pmcregex("\<article-id pub-id-type=\"pmc\"\>(\\d+?)\<\/article-id\>");
    boost::smatch pmid_matches;
    boost::smatch pmc_matches;
    if (boost::regex_search(xml_text_nn, pmid_matches, pmidregex)) {
        accession = "PMID       " + pmid_matches[1];
    } else if (boost::regex_search(xml_text_nn, pmc_matches, pmcregex)) {
        accession = "PMC       " + pmc_matches[1];
    }
    // find article type
    std::string type = "";
    boost::regex typeregex("article-type=\"(.+?)\"");
    boost::smatch type_matches;
    if (boost::regex_search(xml_text_nn, type_matches, typeregex)) {
        type = type_matches[1];
    }
    // find journal
    std::string journal = "";
    boost::regex journalregex("\<journal-title\>(.+?)\<\/journal-title\>");
    boost::smatch journal_matches;
    if (boost::regex_search(xml_text_nn, journal_matches, journalregex)) {
        journal = journal_matches[1];
    }
    // find article title
    std::string title = "";
    boost::regex articleregex("\<article-title\>(.+?)\<\/article-title\>");
    boost::smatch article_matches;
    if (boost::regex_search(xml_text_nn, article_matches, articleregex)) {
        title = article_matches[1];
    }
    // find abstract
    std::string abstract = "";
    boost::regex abstractregex("\<abstract\>(.+?)\<\/abstract\>");
    boost::smatch abstract_matches;
    if (boost::regex_search(xml_text_nn, abstract_matches, abstractregex)) {
        abstract = abstract_matches[1];
    }
    // find citation
    std::string citation = "";
    boost::regex volumeregex("\<volume\>(\\d+)\<\/volume\>");
    boost::smatch volume_matches;
    if (boost::regex_search(xml_text_nn, volume_matches, volumeregex)) {
        citation = citation + "V : " + volume_matches[1] + " ";
    }
    boost::regex issueregex("\<issue\>(\\d+)\<\/issue\>");
    boost::smatch issue_matches;
    if (boost::regex_search(xml_text_nn, issue_matches, issueregex)) {
        citation = citation + "(" + issue_matches[1] + ") ";
    }
    boost::regex pageregex("\<fpage\>(\\d+)\<\/fpage\>\\s+\<lpage\>(\\d+)\<\/lpage\>");
    boost::smatch page_matches;
    if (boost::regex_search(xml_text_nn, page_matches, pageregex)) {
        citation = citation + "pp. " + page_matches[1] + "-" + page_matches[2];
    }
    // find year
    std::string year = "";
    boost::regex yearregex("\<pub-date pub-type=\".*?\"\>.*?\<year\>(\\d+)\<\/year\>\\s+\<\/pub-date\>");
    boost::smatch year_matches;
    if (boost::regex_search(xml_text_nn, year_matches, yearregex)) {
        year = year_matches[1];
    }
    BibInfo bibInfo = BibInfo();
    bibInfo.author = author;
    bibInfo.accession = accession;
    bibInfo.type = type;
    bibInfo.title = title;
    bibInfo.journal = journal;
    bibInfo.citation = citation;
    bibInfo.year = year;
    bibInfo.abstract = abstract;
    bibInfo.subject = subject;
    return bibInfo;
}

std::vector<std::string> CASManager::classify_article_into_corpora_from_bib_file(const BibInfo &bib_info) {
    vector<string> matching_categories;
    for (const auto& cat : PMCOA_CAT_REGEX) {
        regex cat_regex(cat.second);
        if (regex_match(bib_info.subject, cat_regex) || regex_match(bib_info.title, cat_regex) ||
                regex_match(bib_info.journal, cat_regex)) {
            matching_categories.push_back(cat.first);
        }
    }
    if (matching_categories.empty()) {
        matching_categories.push_back(PMCOA_UNCLASSIFIED);
    }
    return matching_categories;
}
