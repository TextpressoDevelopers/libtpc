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

using namespace tpc::cas;
using namespace std;

void CASManager::add_file(FileType type, const string& cas_repo_location, const string& sub_location,
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
    string foutname = cas_repo_location + "/" + sub_location + "/" + boost::filesystem::path(file_path).parent_path()
                                                                           .filename().string();
    string fimageoutname = foutname + "/images/" + boost::filesystem::path(file_path).parent_path().filename().string();
    boost::filesystem::create_directories(foutname + "/images");
    foutname.append("/" + file_name_tpcas);
    stringstream sout;
    string file_time = to_string(boost::filesystem::last_write_time(file_path));

    switch (type) {
        case FileType::pdf:
            try {
                PdfInfo myInfo(file_path, fimageoutname);
                myInfo.StreamAll(sout);
                const char *descriptor = pdf2tpcasdescriptor.c_str();
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
            const char *descriptor = xml2tpcasdescriptor.c_str();
            Stream2Tpcas stp(sout, foutname, descriptor);
            stp.processInputStream();
            break;
    }
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
    return matching_categories;
}
