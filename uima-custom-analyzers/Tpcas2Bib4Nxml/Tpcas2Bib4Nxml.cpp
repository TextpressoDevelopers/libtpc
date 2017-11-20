/* 
 * File:   Tpcas2Bib4Nxml.cpp
 * Author: mueller
 * 
 * Created on October 26, 2016, 12:05 PM
 */

#include "Tpcas2Bib4Nxml.h"
#include "CASUtils.h"
#include "TpTrie.h"
#include <iomanip>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/regex.hpp>

#include <iostream>
#include <fstream>
#include <unicode/regex.h>
#include <locale>
#include <string>
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <bits/algorithmfwd.h>
#include <boost/algorithm/string.hpp>
#include <boost/regex.h>

Tpcas2Bib4Nxml::Tpcas2Bib4Nxml() {
}

Tpcas2Bib4Nxml::Tpcas2Bib4Nxml(const Tpcas2Bib4Nxml& orig) {
}

Tpcas2Bib4Nxml::~Tpcas2Bib4Nxml() {
}

namespace {
    /* magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/ */
    static const uint64_t InitialFNV = 14695981039346656037U;
    static const uint64_t FNVMultiple = 1099511628211;

    /* Fowler / Noll / Vo (FNV) Hash */
    string tpfnv(const uima::UnicodeStringRef s) {
        uint64_t hash = InitialFNV;
        for (uint64_t i = 0; i < s.length(); i++) {
            hash = hash ^ (s[i]); /* xor  the low 8 bits */
            hash = hash * FNVMultiple; /* multiply by the magic number */
        }
        stringstream shex;
        shex << std::setw(16) << std::setfill('0') << std::hex << hash;
        return shex.str();
    }
    const char* node_types[] = {
        "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration"
    };

    std::vector<std::string> GetBibFromXML(string xml_text) {
        boost::regex nline("\\n");
        xml_text = boost::regex_replace(xml_text, nline, "");
        //find author
        string t_xmltext = xml_text;
        boost::regex authorregex("<contrib-group>(.+?)</contrib-group>");
        boost::smatch author_matches;
        string author = "";
        while (boost::regex_search(t_xmltext, author_matches, authorregex)) {
            int size = author_matches.size();
            string hit_text = author_matches[1];
            boost::smatch name_matches;
            boost::regex nameregex("<surname>(.+?)</surname>\\s+<given-names>(.+?)</given-names>");
            while (boost::regex_search(hit_text, name_matches, nameregex)) {
                author = author + name_matches[1] + " " + name_matches[2] + ", ";
                hit_text = name_matches.suffix().str();
            }
            t_xmltext = author_matches.suffix().str();
        }
        boost::regex comma("\\, $");
        author = boost::regex_replace(author, comma, "");
        string clean_author = ""; //remove all special chars from title to get a clean title
        for (int i = 0; i < author.length(); i++) {
            char c = author[i];
            if (std::isspace(c) || std::isalnum(c) || std::ispunct(c)) {
                clean_author += c;
            } else {
                clean_author += " ";
            }
        }
        //find accession
        t_xmltext = xml_text;
        string accession = "";
        boost::regex pmidregex("<article-id pub-id-type=\"pmid\">(\\d+?)</article-id>");
        boost::regex pmcregex("<article-id pub-id-type=\"pmc\">(\\d+?)</article-id>");
        boost::smatch pmid_matches;
        boost::smatch pmc_matches;
        if (boost::regex_search(t_xmltext, pmid_matches, pmidregex)) {
            accession = "PMID       " + pmid_matches[1];
        } else if (boost::regex_search(t_xmltext, pmc_matches, pmcregex)) {
            accession = "PMC       " + pmc_matches[1];
        }
        t_xmltext = xml_text;
        string type = "";
        boost::regex typeregex("article-type=\"(.+?)\"");

        boost::smatch type_matches;
        if (boost::regex_search(t_xmltext, type_matches, typeregex)) {
            type = type_matches[1];
        }
        t_xmltext = xml_text;
        string journal = "";
        boost::regex journalregex("<journal-title>(.+?)</journal-title>");
        boost::smatch journal_matches;
        if (boost::regex_search(t_xmltext, journal_matches, journalregex)) {
            journal = journal_matches[1];
        }
        t_xmltext = xml_text;
        string title = "";
        boost::regex articleregex("<article-title>(.+?)</article-title>");
        boost::smatch article_matches;
        if (boost::regex_search(t_xmltext, article_matches, articleregex)) {
            title = article_matches[1];
        }
        string clean_title = ""; //remove all special chars from title to get a clean title
        for (int i = 0; i < title.length(); i++) {
            char c = title[i];
            if (std::isspace(c) || std::isalnum(c) || std::ispunct(c)) {
                clean_title += c;
            } else {
                clean_title += " ";
            }
        }
        // find citation
        t_xmltext = xml_text;
        string citation = "";
        boost::regex volumeregex("<volume>(\\d+)</volume>");
        boost::smatch volume_matches;
        if (boost::regex_search(t_xmltext, volume_matches, volumeregex)) {
            citation = citation + "V : " + volume_matches[1] + " ";
        }
        boost::regex issueregex("<issue>(\\d+)</issue>");
        boost::smatch issue_matches;
        if (boost::regex_search(t_xmltext, issue_matches, issueregex)) {
            citation = citation + "(" + issue_matches[1] + ") ";
        }
        boost::regex pageregex("<fpage>(\\d+)</fpage>\\s+<lpage>(\\d+)</lpage>");
        boost::smatch page_matches;
        if (boost::regex_search(t_xmltext, page_matches, pageregex)) {
            citation = citation + "pp. " + page_matches[1] + "-" + page_matches[2];
        }
        // find year
        t_xmltext = xml_text;
        string year = "";
        boost::regex yearregex("<pub-date pub-type=\".*?\">.*?<year>(\\d+)</year>\\s+</pub-date>");
        boost::smatch year_matches;
        if (boost::regex_search(t_xmltext, year_matches, yearregex)) {
            year = year_matches[1];
        }
        // find abstract
        t_xmltext = xml_text;
        string abstract = "";
        boost::regex abstractregex("<abstract.*?>(.+?)</abstract>");
        boost::smatch abstract_matches;
        if (boost::regex_search(t_xmltext, abstract_matches, abstractregex)) {
            abstract = abstract_matches[1];
        }
        string clean_abstract = ""; //remove all special chars from title to get a clean title
        for (int i = 0; i < abstract.length(); i++) {
            char c = abstract[i];
            if (std::isspace(c) || std::isalnum(c) || std::ispunct(c)) {
                clean_abstract += c;
            } else {
                clean_abstract += " ";
            }
        }
        std::vector<std::string> bibinfo;
        bibinfo.push_back(clean_author);
        bibinfo.push_back(accession);
        bibinfo.push_back(type);
        bibinfo.push_back(clean_title);
        bibinfo.push_back(journal);
        bibinfo.push_back(citation);
        bibinfo.push_back(year);
        bibinfo.push_back(clean_abstract);
        return bibinfo;
    }

    void WriteBib(vector<std::string> bib_info) {

        std::string l_author = bib_info[0];
        std::string l_accession = bib_info[1];
        std::string l_type = bib_info[2];
        std::string l_title = bib_info[3];
        std::string l_journal = bib_info[4];
        std::string l_citation = bib_info[5];
        std::string l_year = bib_info[6];
        std::string l_abstract = bib_info[7];
        std::string l_filepath = bib_info[8];
        cout << "filepath " << l_filepath << endl;
        std::string bibpath = l_filepath;
        boost::replace_all(bibpath, L".tpcas", L".bib");
        cout << "bib file is " << bibpath << endl;
        ofstream output(bibpath.c_str());
        output << "author|" << l_author << endl;
        output << "accession|" << l_accession << endl;
        output << "type|" << l_type << endl;
        output << "title|" << l_title << endl;
        output << "journal|" << l_journal << endl;
        output << "citation|" << l_citation << endl;
        output << "year|" << l_year << endl;
        output << "abstract|" << l_abstract << endl;
        output.close();
    }
}

uima::TyErrorId Tpcas2Bib4Nxml::initialize(uima::AnnotatorContext & rclAnnotatorContext) {
    string tempDir;
    if (!rclAnnotatorContext.isParameterDefined("TempDirectory") ||
            rclAnnotatorContext.extractValue("TempDirectory", tempDir) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"TempDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() TempDirectory - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

uima::TyErrorId Tpcas2Bib4Nxml::typeSystemInit(uima::TypeSystem const & crTypeSystem) {
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

uima::TyErrorId Tpcas2Bib4Nxml::process(uima::CAS & tcas, uima::ResultSpecification const & crResultSpecification) {
    vector<std::string> bib_info;
    string filename = getFilename(tcas);
    if(const char* env_p = std::getenv("TPCAS_PATH")) {
        filename = string(env_p) + "/" + filename;
    } else {
        filename = "/usr/local/textpresso/tpcas/" + filename;
    }
    string xml_text = getXMLstring(tcas);
    if (xml_text == "") {
        throw uima::Exception(uima::ErrorInfo());
        //return (uima::TyErrorId) UIMA_ERR_ANNOTATOR_COULD_NOT_FIND;
    }
    bib_info = GetBibFromXML(xml_text);
    bib_info.push_back(filename);
    WriteBib(bib_info);
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

uima::TyErrorId Tpcas2Bib4Nxml::destroy() {
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

using namespace uima;
MAKE_AE(Tpcas2Bib4Nxml);
