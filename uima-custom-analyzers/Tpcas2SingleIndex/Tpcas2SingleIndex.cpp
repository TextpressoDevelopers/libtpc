/* 
 * File:   Tpcas2Lpp.cpp
 * Author: mueller
 * 
 * Created on February 5, 2013, 1:27 PM
 * modified Nov, 2013, liyuling
 */
#include "Tpcas2SingleIndex.h"
#include <lucene++/FileUtils.h>
#include "CASUtils.h"
#include "TpTrie.h"
#include <lucene++/LuceneHeaders.h>
#include <lucene++/CompressionTools.h>
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
#include <codecvt>
#include <chrono>
#include <regex>
#include <boost/archive/text_iarchive.hpp>
#include "../../lucene-custom/CaseSensitiveAnalyzer.h"
#include "../../CASManager.h"
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

using namespace std;
using namespace boost;
using namespace std::chrono;
using namespace tpc::cas;

Tpcas2SingleIndex::Tpcas2SingleIndex() {
    root_dir = "/usr/local/textpresso/tpcas";
}

Tpcas2SingleIndex::Tpcas2SingleIndex(const Tpcas2SingleIndex & orig) {
}

Tpcas2SingleIndex::~Tpcas2SingleIndex() {
}

/* magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/ */
static const uint64_t InitialFNV = 14695981039346656037U;
static const uint64_t FNVMultiple = 1099511628211;

static const int sen_length_max = 100;

static const string celegans_bib = "/usr/local/textpresso/celegans_bib/";

static const String fieldStartMark = L"BEGIN ";
static const String fieldEndMark = L" END";

/* Fowler / Noll / Vo (FNV) Hash */
string tpfnv(const UnicodeStringRef s) {
    uint64_t hash = InitialFNV;
    for (uint64_t i = 0; i < s.length(); i++) {
        hash = hash ^ (s[i]); /* xor  the low 8 bits */
        hash = hash * FNVMultiple; /* multiply by the magic number */
    }
    stringstream shex;
    shex << std::setw(16) << std::setfill('0') << std::hex << hash;
    return shex.str();
}


//const char* node_types[] = {
//    "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration"
//};

vector< pair<int32_t, int32_t> > RemoveDelimiters(const UnicodeStringRef usdocref,
        vector<string> & regex,
        vector< pair<int32_t, int32_t> > & p,
        int32_t mfcl, int32_t mbcl) {
    vector< pair<int32_t, int32_t> > result;
    vector< pair<int32_t, int32_t> >::iterator it;
    for (it = p.begin(); it != p.end(); it++) {
        int32_t b = (*it).first;
        int32_t e = (*it).second + 1;
        string strip;
        usdocref.extract(b - mfcl, e - b + mfcl + mbcl, strip);
        bool matches = false;
        vector<string>::iterator is;
        for (is = regex.begin(); is != regex.end(); is++) {
            boost::regex expr(*is);
            matches = (matches || boost::regex_search(strip, expr));
        }
        if (!matches) {
            result.push_back(*it);
        } else {
            b = ((b - 100 > 0)) ? b - 100 : 0;
            usdocref.extract(b, 200, strip);
        }
    }
    return result;
}

pair<int32_t, int32_t> MaximizeBoundaries(pair<int32_t, int32_t> i,
        pair<int32_t, int32_t> j) {
    int32_t ifi = i.first;
    int32_t ise = i.second + 1; // need to make this modification because
    // trie class has different boundary convention.
    int32_t jfi = j.first;
    int32_t jse = j.second + 1;
    if (ifi != jfi) {
        if (ise != jse) {
            // at least one boundaries needs to be the same;
            // return error code -1.
            return make_pair(-1, -1);
        } else {
            // upper boundary is the same;
            // adjust lower boundary.
            return make_pair((ifi < jfi) ? ifi : jfi, ise);
        }
    } else {
        // lower boundary is the same;
        // adjust upper boundary (trivial if upper boundary are the same).
        return make_pair(ifi, (ise > jse) ? ise : jse);
    }
}

vector<wstring> GetSentences(const UnicodeStringRef usdocref,
        vector< pair<int32_t, int32_t> > & p) {

    vector<wstring> results;

    vector< pair<int32_t, int32_t> > merged;
    merged.clear();
    vector< pair<int32_t, int32_t> >::iterator it = p.begin();
    while (it < p.end()) {
        vector< pair<int32_t, int32_t> >::iterator it2 = it + 1;
        pair<int32_t, int32_t> res = MaximizeBoundaries(*it, *it2);
        if (res.first < 0) {
            res = make_pair((*it).first, (*it).second + 1);
        } else {
            it++;
        }
        merged.push_back(res);
        int32_t b = res.first;
        int32_t e = res.second;
        it++;
    }
    // use unique beginnings and endings of tokendelimiters to annotate tokens.
    int32_t laste = 0;
    for (it = merged.begin(); it != merged.end(); it++) {
        int32_t b = (*it).first;
        int32_t e = (*it).second;
        if (b != laste) {

            UnicodeString wd;
            usdocref.extract(laste, b - laste, wd);

            wstring ws_sentence;
            for (int i = 0; i < wd.length(); ++i)
                ws_sentence += static_cast<wchar_t> (wd[i]);

            results.push_back(ws_sentence);
        }
        laste = e;
    }
    return results;
}

vector<String> GetBibFromXML(string xml_text) {

    boost::regex nline("\\n");
    xml_text = boost::regex_replace(xml_text, nline, "");
    //find author
    string t_xmltext = xml_text;
    boost::regex authorregex("\<contrib-group\>(.+?)\<\/contrib-group\>");

    boost::smatch author_matches;
    string author = "";
    while (boost::regex_search(t_xmltext, author_matches, authorregex)) {
        int size = author_matches.size();
        string hit_text = author_matches[1];
        boost::smatch name_matches;
        boost::regex nameregex("\<surname\>(.+?)\<\/surname\>\\s+\<given-names\>(.+?)\<\/given-names\>");
        while (boost::regex_search(hit_text, name_matches, nameregex)) {
            //cout <<"surname " << name_matches[1] << "given name " << name_matches[2] << endl;
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
    t_xmltext = xml_text;
    string accession = "";
    boost::regex pmidregex("\<article-id pub-id-type=\"pmid\"\>(\\d+?)\<\/article-id\>");
    boost::regex pmcregex("\<article-id pub-id-type=\"pmc\"\>(\\d+?)\<\/article-id\>");

    boost::smatch pmid_matches;
    boost::smatch pmc_matches;
    if (boost::regex_search(t_xmltext, pmid_matches, pmidregex)) {
        accession = "PMID       " + pmid_matches[1];
    } else if (boost::regex_search(t_xmltext, pmc_matches, pmcregex)) {
        accession = "PMC       " + pmc_matches[1];
    }
    // find article type
    t_xmltext = xml_text;
    string type = "";
    boost::regex typeregex("article-type=\"(.+?)\"");

    boost::smatch type_matches;
    if (boost::regex_search(t_xmltext, type_matches, typeregex)) {
        type = type_matches[1];
    }
    // find journal
    t_xmltext = xml_text;
    string journal = "";
    boost::regex journalregex("\<journal-title\>(.+?)\<\/journal-title\>");

    boost::smatch journal_matches;
    if (boost::regex_search(t_xmltext, journal_matches, journalregex)) {
        journal = journal_matches[1];
    }
    // find article title
    t_xmltext = xml_text;
    string title = "";
    boost::regex articleregex("\<article-title\>(.+?)\<\/article-title\>");
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
    boost::regex volumeregex("\<volume\>(\\d+)\<\/volume\>");
    boost::smatch volume_matches;
    if (boost::regex_search(t_xmltext, volume_matches, volumeregex)) {
        citation = citation + "V : " + volume_matches[1] + " ";
    }
    boost::regex issueregex("\<issue\>(\\d+)\<\/issue\>");
    boost::smatch issue_matches;
    if (boost::regex_search(t_xmltext, issue_matches, issueregex)) {
        citation = citation + "(" + issue_matches[1] + ") ";
    }
    boost::regex pageregex("\<fpage\>(\\d+)\<\/fpage\>\\s+\<lpage\>(\\d+)\<\/lpage\>");
    boost::smatch page_matches;
    if (boost::regex_search(t_xmltext, page_matches, pageregex)) {
        citation = citation + "pp. " + page_matches[1] + "-" + page_matches[2];
    }
    // find year
    t_xmltext = xml_text;
    string year = "";
    boost::regex yearregex("\<pub-date pub-type=\".*?\"\>.*?\<year\>(\\d+)\<\/year\>\\s+\<\/pub-date\>");
    boost::smatch year_matches;
    if (boost::regex_search(t_xmltext, year_matches, yearregex)) {
        year = year_matches[1];
    }
    // find abstract
    t_xmltext = xml_text;
    string abstract = "";
    boost::regex abstractregex("\<abstract.*?\>(.+?)<\/abstract\>");
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
    String l_author = StringUtils::toString(clean_author.c_str());
    String l_accession = StringUtils::toString(accession.c_str());
    String l_type = StringUtils::toString(type.c_str());
    String l_title = StringUtils::toString(clean_title.c_str());
    String l_journal = StringUtils::toString(journal.c_str());
    String l_citation = StringUtils::toString(citation.c_str());
    String l_year = StringUtils::toString(year.c_str());
    String l_abstract = StringUtils::toString(clean_abstract.c_str());
    vector<String> bibinfo;
    bibinfo.push_back(l_author);
    bibinfo.push_back(l_accession);
    bibinfo.push_back(l_type);
    bibinfo.push_back(l_title);
    bibinfo.push_back(l_journal);
    bibinfo.push_back(l_citation);
    bibinfo.push_back(l_year);
    bibinfo.push_back(l_abstract);
    return bibinfo;
}

string readFromWBfile(const char* field, string paper) {
    string file = celegans_bib + "/" + field + "/" + paper;
    string output_line = "";
    if (boost::filesystem::exists(file)) {
        std::ifstream f(file.c_str());
        if (f) {
            while (f) {
                std::string line;
                getline(f, line);
                if (field == "accession")
                    output_line = output_line + " "+line;
                else output_line += line;
            }
        }
    }
    return output_line;
}

vector<String> Tpcas2SingleIndex::GetBib(string fullfilename)
{
    vector<String> bib_info;
    boost::filesystem::path source  = fullfilename;
    string filename = source.filename().string();
    boost::replace_all(filename, ".tpcas", ".bib");
    string bib_file(tempDir+"/"+filename);
    std::ifstream f(bib_file.c_str());
    string str;
    while (std::getline(f, str))
    {
        vector<string> items;
        boost::split(items, str, boost::is_any_of("|"));
        string field = items[0];
        string content  = items[1].c_str();
        boost::replace_all(content, "\377", "");
        boost::replace_all(content, "\\377", "");
        String content_conv = StringUtils::toString(content.c_str());
        boost::replace_all(content_conv, "\377", "");
        boost::replace_all(content_conv, "\\377", "");
        // Process str
        if(field == "author")
        {
            bib_info.push_back(content_conv);
        }
        if(field == "accession")
        {
            bib_info.push_back(content_conv);
        }
         if(field == "type")
        {
            bib_info.push_back(content_conv);
        }
         if(field == "title")
        {

            bib_info.push_back(content_conv);
        }
         if(field == "journal")
        {
            bib_info.push_back(content_conv);
        }
         if(field == "citation")
        {
            bib_info.push_back(content_conv);
        }
         if(field == "year")
        {
            bib_info.push_back(content_conv);
        }
         if(field == "abstract")
        {
            bib_info.push_back(content_conv);
        }
    }
    return bib_info;
}

vector<String> GetBibFromWB(string paper) {
    vector<String> bib;
    string author_line = readFromWBfile("author", paper);
    string accession_line = readFromWBfile("accession", paper);
    accession_line = accession_line +" "+ paper;
    string type_line = readFromWBfile("type", paper);
    string title_line = readFromWBfile("title", paper);
    string journal_line = readFromWBfile("journal", paper);
    string citation_line = readFromWBfile("citation", paper);
    string year_line = readFromWBfile("year", paper);
    boost::regex sup("\.sup\.d+");
    paper = boost::regex_replace(paper, sup, "");
    string abstract_line = readFromWBfile("abstract", paper);
    String l_author = StringUtils::toString(author_line.c_str());
    String l_accession = StringUtils::toString(accession_line.c_str());
    String l_type = StringUtils::toString(type_line.c_str());
    String l_title = StringUtils::toString(title_line.c_str());
    String l_journal = StringUtils::toString(journal_line.c_str());
    String l_citation = StringUtils::toString(citation_line.c_str());
    String l_year = StringUtils::toString(year_line.c_str());
    String l_abstract = StringUtils::toString(abstract_line.c_str());
    bib.push_back(l_author);
    bib.push_back(l_accession);
    bib.push_back(l_type);
    bib.push_back(l_title);
    bib.push_back(l_journal);
    bib.push_back(l_citation);
    bib.push_back(l_year);
    bib.push_back(l_abstract);
    return bib;
}

std::map<wstring, vector<wstring> > collectCategoryMapping(CAS& tcas) {
    string filenamehash = gettpfnvHash(tcas);
    FSIndexRepository & indices = tcas.getIndexRepository();
    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    int count = 0;
    map<wstring, vector<wstring> > cat_map;
    while (aait.isValid()) {
        count++;
        Type currentType = aait.get().getType();
        UnicodeStringRef tnameref = currentType.getName();
        string annType = tnameref.asUTF8();
        if (annType == "org.apache.uima.textpresso.lexicalannotation") {
            Feature fterm = currentType.getFeatureByBaseName("term");
            UnicodeStringRef uterm = aait.get().getStringValue(fterm);
            wstring ws_term;
            UnicodeString wd;
            uterm.extract(0, uterm.length(), wd);
            for (int i = 0; i < wd.length(); ++i)
                ws_term += static_cast<wchar_t> (wd[i]);
            Feature fcategory = currentType.getFeatureByBaseName("category");
            UnicodeStringRef ucategory = aait.get().getStringValue(fcategory);
            wstring ws_category;
            UnicodeString wd1;
            ucategory.extract(0, ucategory.length(), wd1);
            for (int i = 0; i < wd1.length(); ++i)
                ws_category += static_cast<wchar_t> (wd1[i]);
            long begin = aait.get().getBeginPosition(); //.getStringValue(fid);
            long end = aait.get().getEndPosition();
            int term_size = cat_map[ws_term].size();
            int j = 0;
            if (term_size > 0) {
                while (j < term_size) {
                    if (cat_map[ws_term].at(j) == ws_category) {
                        break;
                    }
                    j++;
                }
                if (j == term_size) {
                    cat_map[ws_term].push_back(ws_category);
                }
            } else {
                cat_map[ws_term].push_back(ws_category);
            }
            wstring ws_lexicalannotaitonid(filenamehash.begin(), filenamehash.end());
            ws_lexicalannotaitonid = ws_lexicalannotaitonid + L"|" + ws_category + L"|" + ws_term;
        }
        aait.moveToNext();
    }
    return cat_map;
}

void IndexSentences(CAS& tcas, map<wstring, vector<wstring> > cat_map, vector<String> bib_info, const string& corpora,
                    const string& doc_id, const IndexWriterPtr& sentencewriter) {
    std::hash<std::string> string_hash;
    String l_author = fieldStartMark + bib_info[0] + fieldEndMark;
    String l_accession = bib_info[1];
    String l_type = bib_info[2];
    String l_title = fieldStartMark + bib_info[3] + fieldEndMark;
    String l_journal = fieldStartMark + bib_info[4] + fieldEndMark;
    String l_citation = bib_info[5];
    String l_year = bib_info[6];
    String l_abstract = bib_info[7];
    Type sent_type = tcas.getTypeSystem().getType("org.apache.uima.textpresso.sentence");
    ANIndex sentenceindex = tcas.getAnnotationIndex(sent_type);
    ANIterator aait = sentenceindex.iterator();
    aait.moveToFirst();
    int count = 0;
    int sentence_id = 0;
    while (aait.isValid()) {
        count++;
        Type currentType = aait.get().getType();
        UnicodeStringRef tnameref = currentType.getName();
        string annType = tnameref.asUTF8();
        if (annType == "org.apache.uima.textpresso.sentence") {
            ++sentence_id;
            Feature fcontent = currentType.getFeatureByBaseName("content");
            UnicodeStringRef ucontent = aait.get().getStringValue(fcontent);
            // Need to preserve UnicodeString as much as possible by maybe
            // going wchar_t by wchar_t.
            wstring w_sentence;
            UnicodeString wd;
            ucontent.extract(0, ucontent.length(), wd);
            for (int i = 0; i < wd.length(); ++i)
                w_sentence += static_cast<wchar_t> (wd[i]);
            boost::wregex tagregex(L"\<_pdf.+?\/\>");
            w_sentence = boost::regex_replace(w_sentence, tagregex, "");
            Feature begincontent = currentType.getFeatureByBaseName("begin");
            int begin = aait.get().getIntValue(begincontent);
            Feature endcontent = currentType.getFeatureByBaseName("end");
            int end = aait.get().getIntValue(endcontent);
            wstring w_sentence_cat;
            wstring w_sentence_pos;
            w_sentence = Tpcas2SingleIndex::RemoveTags(w_sentence);
            vector<wstring> words;
            boost::split(words, w_sentence, boost::is_any_of(" \n\t'\\/()[]{}:.;,!?"));
            int position = 0;
            for (int j = 0; j < words.size(); j++) {
                int b = position;
                int e = position + words[j].length();
                position = e + 1;
                map<wstring, vector<wstring> >::iterator it = cat_map.find(words[j]);
                if (it != cat_map.end()) // found category term CAT_TERM
                {
                    w_sentence_cat += boost::algorithm::join(it->second, "|");
                    w_sentence_cat += L"\t";
                    w_sentence_pos += boost::lexical_cast<wstring>(b);
                    w_sentence_pos += L",";
                    w_sentence_pos += boost::lexical_cast<wstring>(e);
                    w_sentence_pos += L"\t";

                } else //NOT_CAT term
                {
                    w_sentence_cat += L"NA\t"; // to save space
                    w_sentence_pos += L"0,0\t"; //to save space
                }
            }
            milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
            string sentence_id_str = to_string(count);

            w_sentence_cat = w_sentence_cat.substr(0, w_sentence_cat.length() -
                                                      1); ///remove last \t to avoid empty string after split
            w_sentence_pos = w_sentence_pos.substr(0, w_sentence_pos.length() - 1);
            //testing w_cat_string and w_positions matches and retrieve words from w_cleanText
            DocumentPtr sentencedoc = newLucene<Document>();
            sentencedoc->add(newLucene<Field>(L"sentence_id", StringUtils::toString(sentence_id_str.c_str()),
                                              Field::STORE_YES, Field::INDEX_NOT_ANALYZED_NO_NORMS));
            sentencedoc->add(newLucene<Field>(L"doc_id", StringUtils::toString(doc_id.c_str()), Field::STORE_YES,
                                              Field::INDEX_NOT_ANALYZED_NO_NORMS));
            sentencedoc->add(newLucene<Field>(L"sentence",
                                              StringUtils::toString<wstring>(w_sentence),
                                              Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"sentence_compressed",
                                              CompressionTools::compressString(
                                                      StringUtils::toString<wstring>(w_sentence)),
                                              Field::STORE_YES));
            sentencedoc->add(newLucene<Field>(L"sentence_cat", StringUtils::toString<wstring>(w_sentence_cat),
                                              Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"sentence_cat_compressed",
                                              CompressionTools::compressString(
                                                      StringUtils::toString<wstring>(w_sentence_cat)),
                                              Field::STORE_YES));
            sentencedoc->add(newLucene<Field>(L"begin", StringUtils::toString<int>(begin), Field::STORE_YES,
                                              Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"end", StringUtils::toString<int>(end), Field::STORE_YES,
                                              Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"author", l_author, Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"accession", l_accession, Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"type", l_type, Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"title", l_title, Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"journal", l_journal, Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"citation", l_citation, Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"year", l_year, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field>(L"corpus", String(corpora.begin(), corpora.end()), Field::STORE_NO,
                                              Field::INDEX_ANALYZED));
            sentencewriter->addDocument(sentencedoc);
        }
        aait.moveToNext();
    }
}

TyErrorId Tpcas2SingleIndex::initialize(AnnotatorContext & rclAnnotatorContext) {
    if (!rclAnnotatorContext.isParameterDefined("TempDirectory") ||
            rclAnnotatorContext.extractValue("TempDirectory", tempDir) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"TempDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() TempDirectory - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    string newindexflag = tempDir + "/newindexflag";
    bool b_newindex = false; //create new index or adding to existing index.
    if (boost::filesystem::exists(newindexflag)) {
        //cout << "creating new indexing...." << endl;
        b_newindex = true;
    } else {
        //cout << "adding to index....." << endl;
        b_newindex = false;
    }
    // creating token index writer
    if (!rclAnnotatorContext.isParameterDefined("TokenLuceneIndexDirectory") ||
            rclAnnotatorContext.extractValue("TokenLuceneIndexDirectory", tokenindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"TokenLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Token - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String TokenIndexDir = StringUtils::toString(tokenindexdirectory.c_str());
    if (!rclAnnotatorContext.isParameterDefined("TokenCaseSensitiveLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("TokenCaseSensitiveLuceneIndexDirectory", tokenindexdirectory_casesens) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"TokenCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Token - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String TokenCaseSensitiveIndexDir = StringUtils::toString(tokenindexdirectory_casesens.c_str());
    //creating sentence index writer
    if (!rclAnnotatorContext.isParameterDefined("SentenceLuceneIndexDirectory") ||
            rclAnnotatorContext.extractValue("SentenceLuceneIndexDirectory", sentenceindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"SentenceLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Sentence - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String SentenceIndexDir = StringUtils::toString(sentenceindexdirectory.c_str());
    sentencewriter = newLucene<IndexWriter > (FSDirectory::open(SentenceIndexDir),
                                              newLucene<StandardAnalyzer > (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                              IndexWriter::MaxFieldLengthUNLIMITED);
    if (!rclAnnotatorContext.isParameterDefined("SentenceCaseSensitiveLuceneIndexDirectory") ||
            rclAnnotatorContext.extractValue("SentenceCaseSensitiveLuceneIndexDirectory", sentenceindexdirectory_casesens) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"SentenceCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Sentence - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String SentenceCaseSensitiveIndexDir = StringUtils::toString(sentenceindexdirectory_casesens.c_str());
    sentencewriter_casesens = newLucene<IndexWriter > (FSDirectory::open(SentenceCaseSensitiveIndexDir),
            newLucene<CaseSensitiveAnalyzer > (LuceneVersion::LUCENE_30), b_newindex, //create new index
            IndexWriter::MaxFieldLengthUNLIMITED);
    if (!rclAnnotatorContext.isParameterDefined("FulltextLuceneIndexDirectory") ||
            rclAnnotatorContext.extractValue("FulltextLuceneIndexDirectory", fulltextindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"FulltextLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String FulltextIndexDir = StringUtils::toString(fulltextindexdirectory.c_str());
    fulltextwriter = newLucene<IndexWriter > (FSDirectory::open(FulltextIndexDir),
            newLucene<StandardAnalyzer > (LuceneVersion::LUCENE_30), b_newindex, //create new index
            IndexWriter::MaxFieldLengthUNLIMITED);
    if (!rclAnnotatorContext.isParameterDefined("FulltextCaseSensitiveLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("FulltextCaseSensitiveLuceneIndexDirectory", fulltextindexdirectory_casesens) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"FulltextCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String FulltextCaseSensitiveIndexDir = StringUtils::toString(fulltextindexdirectory_casesens.c_str());
    fulltextwriter_casesens = newLucene<IndexWriter > (FSDirectory::open(FulltextCaseSensitiveIndexDir),
                                              newLucene<CaseSensitiveAnalyzer > (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                              IndexWriter::MaxFieldLengthUNLIMITED);
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2SingleIndex::typeSystemInit(TypeSystem const & crTypeSystem) {
    // input type and feature
    std::vector<Type> alltypes;
    crTypeSystem.getAllTypes(alltypes);
    std::vector<Type>::iterator atit;
    const UnicodeString textpresso("textpresso");
    for (atit = alltypes.begin(); atit != alltypes.end(); atit++) {
        UnicodeStringRef turef = (*atit).getName();
        // get types defined by Textpresso only
        if (!(*atit).isValid()) {
            UnicodeString wd;
            turef.extract(0, turef.length(), wd);
            getAnnotatorContext().getLogger().logError("Error getting Type object for");
            getAnnotatorContext().getLogger().logError(wd);
            cerr << "Tpcas2Lpp::typeSystemInit - Error. See logfile" << endl;
            return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
        } else {
            types_.push_back(*atit);
        }
    }
    for (atit = types_.begin(); atit != types_.end(); atit++) {
        vector<Feature> f;
        (*atit).getAppropriateFeatures(f);
        for (vector<Feature>::iterator fit = f.begin(); fit != f.end(); fit++) {
            features_[*atit].push_back(*fit);
        }
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

wstring getCatString(CAS& tcas, map<wstring, vector<wstring>> cat_map, wstring w_cleanText) {
    wstring w_cat_string_temp = w_cleanText;
    vector<wstring> words;
    boost::split(words, w_cleanText, boost::is_any_of(" \n\t'\\/()[]{}:.;,!?"));
    wstring w_cat_string;
    for (wstring word : words) {
        if (cat_map.find(word) != cat_map.end()) {
            w_cat_string += boost::algorithm::join(cat_map[word], "|") + L"\t";
        } else {
            w_cat_string += L"NA\t";
        }
    }
    w_cat_string = w_cat_string.substr(0, w_cat_string.length() - 1);
    return w_cat_string;
}

TyErrorId Tpcas2SingleIndex::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    FSIndexRepository & indices = tcas.getIndexRepository();
    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    UnicodeStringRef usdocref = tcas.getDocumentText();
    string pid = tpfnv(usdocref);
    wstring w_cleanText = getCleanText(tcas);

    int global_doc_counter(0);
    // read global doc counter from file
    std::ifstream ifs;
    if(const char* env_p = std::getenv("INDEX_PATH")) {
        ifs.open(string(env_p) + "/counter.dat", std::ios::binary);
    } else {
        ifs.open("/usr/local/textpresso/luceneindex/counter.dat", std::ios::binary);
    }
    if (ifs) {
        boost::archive::text_iarchive ia(ifs);
        // read class state from archive
        ia >> global_doc_counter;
    }
    // increment global doc counter to get the doc_id for the new document to add
    ++global_doc_counter;
    // save the new value back to file
    std::ofstream ofs;
    if(const char* env_p = std::getenv("INDEX_PATH")) {
        ofs.open(string(env_p) + "/counter.dat");
    } else {
        ofs.open("/usr/local/textpresso/luceneindex/counter.dat");
    }
    {
        boost::archive::text_oarchive oa(ofs);
        oa << global_doc_counter;
    }
    int victim = global_doc_counter;
    string base64_id;
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#@";
    do {
        base64_id.insert(0, 1, base64_chars[victim % 64]);
        victim /= 64;
    } while (victim > 0);

    // collecting and indexing categories
    auto cat_map = collectCategoryMapping(tcas);
    wstring w_cat_string = getCatString(tcas, cat_map, w_cleanText);

    w_cat_string = w_cat_string.substr(0, w_cat_string.length() - 1); ///remove last \t to avoid empty string after split
    vector<String> bib_info;
    //string regex_wb = "WBPaper";
    string filename = getFilename(tcas);
    // if filename contains C. elegans or C. elegans supplemental, then set corpus according to filename, otherwise
    // get subject and title from xml fulltext and classify according to regex
    string corpora("BG");
    String l_article_type;
    if (std::regex_match (filename, std::regex("useruploads\/(.*)"))) {
        std::regex rgx("useruploads\/([^\/]+)\/([^\/]+)\/.*");
        std::smatch matches;
        std::regex_search(filename, matches, rgx);
        string username = matches[1];
        string fn = matches[2];
        map<string, set<string>> papers_lit_map;
        std::ifstream ifs("/usr/local/textpresso/useruploads/" + username + "/uploadedfiles/lit.cfg", std::ios::binary);
        if (ifs) {
            boost::archive::text_iarchive ia(ifs);
            // read class state from archive
            ia >> papers_lit_map;
        }
        corpora.append(boost::join(papers_lit_map[fn], "ED BG"));
        corpora.append("ED");
    } else {
        if (getCASType(tcas) == "pdf") {
            if (std::regex_match(filename, std::regex("^C\. elegans Supplementals\/(.*)"))) {
                corpora.append("C. elegans SupplementalsED");
            } else {
                corpora.append("C. elegansED");
            }
        } else {
            string xml_text;
            usdocref.extractUTF8(xml_text);
            BibInfo bibInfo = CASManager::get_bib_info_from_xml_text(xml_text);
            vector<string> corpora_vec = CASManager::classify_article_into_corpora_from_bib_file(bibInfo);
            if (corpora_vec.empty()) {
                corpora_vec.push_back(PMCOA_UNCLASSIFIED);
            }
            corpora.append(boost::algorithm::join(corpora_vec, "ED BG"));
            corpora.append("ED");
        }
    }
    bib_info = GetBib(filename);
    String l_filepath = StringUtils::toString(filename.c_str());
    vector<string> filepathSplit;
    boost::split(filepathSplit, filename, boost::is_any_of("/"));
    bib_info.push_back(l_filepath);
    String l_author = fieldStartMark + bib_info[0] + fieldEndMark;
    String l_accession = bib_info[1];
    if (l_accession.size() == 0) {
        l_accession = L"Not available";
    }
    String l_type = bib_info[2];
    if (l_type.size() == 0) {
        l_type = L"Not available";
    }
    String l_title = fieldStartMark + bib_info[3] + fieldEndMark;
    String l_journal = fieldStartMark + bib_info[4] + fieldEndMark;
    String l_citation = bib_info[5];
    String l_year = bib_info[6];
    String l_abstract = bib_info[7];
    if (l_abstract.size() == 0) {
        l_abstract = L"Abstract is not available";
    }
    if (l_journal.size() == 0) {
        l_journal = L"Not available";
    }
    if (w_cleanText.size() == 0) {
        w_cleanText = L"Not available";
    }
    if (w_cat_string.size() == 0) {
        w_cat_string = L"Not available";
    }
    if (l_author.size() == 0) {
        l_author = L"Not available";
    }
    if (l_title.size() == 0) {
        l_title = L"Not available";
    }
    if (w_cleanText.empty()) {
        w_cleanText = L"not available";
    }
    DocumentPtr fulltextdoc = newLucene<Document > ();
    fulltextdoc->add(newLucene<Field > (L"doc_id", StringUtils::toString(base64_id.c_str()),
                                        Field::STORE_YES, Field::INDEX_NOT_ANALYZED_NO_NORMS));
    fulltextdoc->add(newLucene<Field > (L"filepath", l_filepath, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"fulltext", StringUtils::toString<wstring>(w_cleanText), Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"fulltext_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(w_cleanText)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"fulltext_cat",
                                        StringUtils::toString<wstring>(w_cat_string),
                                        Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"fulltext_cat_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(w_cat_string)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"author", l_author, Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"author_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(l_author)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"accession", l_accession, Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"accession_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(l_accession)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"type", l_type, Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"type_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(l_type)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"title", l_title, Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"title_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(l_title)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"journal", l_journal, Field::STORE_NO, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"journal_compressed",
                                        CompressionTools::compressString(StringUtils::toString<wstring>(l_journal)),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"citation", l_citation, Field::STORE_YES, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"year", l_year, Field::STORE_YES, Field::INDEX_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"abstract_compressed", CompressionTools::compressString(l_abstract),
                                        Field::STORE_YES));
    fulltextdoc->add(newLucene<Field > (L"corpus", String(corpora.begin(), corpora.end()), Field::STORE_YES,
                                        Field::INDEX_ANALYZED));
    fulltextwriter->addDocument(fulltextdoc);
    fulltextwriter_casesens->addDocument(fulltextdoc);
    IndexSentences(tcas, cat_map, bib_info, corpora, base64_id, sentencewriter);
    IndexSentences(tcas, cat_map, bib_info, corpora, base64_id, sentencewriter_casesens);
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2SingleIndex::destroy() {
    if (fulltextwriter) {
        fulltextwriter->commit();
        fulltextwriter->close();
    }
    if (fulltextwriter_casesens) {
        fulltextwriter_casesens->commit();
        fulltextwriter_casesens->close();
    }
    sentencewriter->close();
    sentencewriter_casesens->close();
    return (TyErrorId) UIMA_ERR_NONE;
}

wstring Tpcas2SingleIndex::RemoveTags(wstring w_cleantext) {
    boost::wregex tagregex(L"\<.+?\>");
    w_cleantext = boost::regex_replace(w_cleantext, tagregex, "");
    boost::wregex tagregex2(L"\<\/.+?\>");
    w_cleantext = boost::regex_replace(w_cleantext, tagregex2, "");
    return w_cleantext;
}

MAKE_AE(Tpcas2SingleIndex);




