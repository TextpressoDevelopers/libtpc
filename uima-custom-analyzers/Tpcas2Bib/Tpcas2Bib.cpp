/* 
 * File:   Tpcas2Lpp.cpp
 * Author: mueller
 * 
 * Created on February 5, 2013, 1:27 PM
 * modified Nov, 2013, liyuling
 */

#include "Tpcas2Bib.h"
#include <lucene++/FileUtils.h>
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

using namespace std;
using namespace boost;

Tpcas2Bib::Tpcas2Bib() {
}

Tpcas2Bib::Tpcas2Bib(const Tpcas2Bib & orig) {
}

Tpcas2Bib::~Tpcas2Bib() {
}

/* magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/ */
static const uint64_t InitialFNV = 14695981039346656037U;
static const uint64_t FNVMultiple = 1099511628211;
static const int sen_length_max = 100;
static const string celegans_bib = "/usr/local/textpresso/celegans_bib/";

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

const char* node_types[] = {
    "null", "document", "element", "pcdata", "cdata", "comment", "pi", "declaration"
};

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
                    output_line = output_line + " " + line;
                else output_line += line;
            }
        }
    }
    return output_line;
}

vector<String> GetBib(string fullfilename)
 {
    vector<String> bib_info;
    boost::filesystem::path source = fullfilename;
    string filename = source.filename().string();
    boost::replace_all(filename, ".tpcas", ".bib");
    string bib_file("/run/shm/bib/" + filename);
    std::ifstream f(bib_file.c_str());
    string str;
    while (std::getline(f, str)) {
        cout << str << endl;
        vector<string> items;
        boost::algorithm::split_regex(items, str, regex("\:\:"));
        string field = items[0];
        string content = items[2];
        cout << field << endl;
        // Process str
        if (field == "author") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "accession") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "type") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "title") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "journal") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "citation") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "year") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
        if (field == "abstract") {
            cout << content << endl;
            bib_info.push_back(Lucene::String((const wchar_t*)content.c_str()));
        }
    }
    return bib_info;
}

vector<String> GetBibFromWB(string paper) {
    vector<String> bib;
    string author_line = readFromWBfile("author", paper);
    string accession_line = readFromWBfile("accession", paper);
    accession_line = accession_line + " " + paper;
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

wstring increaseString(wstring str) {
    wstring increased_string(L"");
    for (int i = 0; i < str.length(); i++) {
        increased_string += (str.at(i) + 1);
    }
    return increased_string;
}

wstring decreaseString(wstring str) {
    wstring decreased_string(L"");
    for (int i = 0; i < str.length(); i++) {
        decreased_string += (str.at(i) - 1);
    }
    return decreased_string;
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

void IndexSentences(CAS& tcas, map<wstring, vector<wstring> > cat_map, vector<String> bib_info, IndexWriterPtr sentencewriter) {
    string filenamehash = gettpfnvHash(tcas);
    String l_filenamehash = StringUtils::toString(filenamehash.c_str());
    String l_author = bib_info[0];
    String l_accession = bib_info[1];
    String l_type = bib_info[2];
    String l_title = bib_info[3];
    String l_journal = bib_info[4];
    String l_citation = bib_info[5];
    String l_year = bib_info[6];
    String l_abstract = bib_info[7];
    Type sent_type = tcas.getTypeSystem().getType("org.apache.uima.textpresso.sentence");
    ANIndex sentenceindex = tcas.getAnnotationIndex(sent_type);
    ANIterator aait = sentenceindex.iterator();
    aait.moveToFirst();
    int count = 0;
    while (aait.isValid()) {
        count++;
        Type currentType = aait.get().getType();
        UnicodeStringRef tnameref = currentType.getName();
        string annType = tnameref.asUTF8();
        if (annType == "org.apache.uima.textpresso.sentence") {
            Feature fcontent = currentType.getFeatureByBaseName("content");
            UnicodeStringRef ucontent = aait.get().getStringValue(fcontent);
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
            vector<wstring> words;
            boost::split(words, w_sentence, boost::is_any_of(" "));
            int position = 0;
            for (int j = 0; j < words.size(); j++) {
                int b = position;
                int e = position + words[j].length();
                position = e + 1;
                map<wstring, vector<wstring> >::iterator it = cat_map.find(words[j]);
                if (it != cat_map.end()) // found category term CAT_TERM
                {
                    vector<wstring> categories = it->second;
                    for (int j = 0; j < categories.size() - 1; j++) {
                        w_sentence_cat += categories[j];
                        w_sentence_cat += L"|";
                    }
                    w_sentence_cat += categories[categories.size() - 1];
                    w_sentence_cat += L"\t";
                    //adding to positions string
                    w_sentence_pos += boost::lexical_cast<wstring > (b);
                    w_sentence_pos += L",";
                    w_sentence_pos += boost::lexical_cast<wstring > (e);
                    w_sentence_pos += L"\t";
                } else //NOT_CAT term
                {
                    w_sentence_cat += L"NON_CAT\t"; // to save space
                    w_sentence_pos += L"0,0\t"; //to save space
                }
            }
            w_sentence_cat = w_sentence_cat.substr(0, w_sentence_cat.length() - 1); ///remove last \t to avoid empty string after split
            w_sentence_pos = w_sentence_pos.substr(0, w_sentence_pos.length() - 1);
            DocumentPtr sentencedoc = newLucene<Document > ();
            sentencedoc->add(newLucene<Field > (L"identifier", l_filenamehash, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
            FieldPtr sentence_fp = newLucene<Field > (L"sentence", StringUtils::toString<wstring > (w_sentence), Field::STORE_YES, Field::INDEX_ANALYZED_NO_NORMS);
            sentence_fp->setOmitNorms(true);
            sentencedoc->add(sentence_fp);

            sentencedoc->add(newLucene<Field > (L"sentence_cat", StringUtils::toString<wstring > (w_sentence_cat), Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"begin", StringUtils::toString<int > (begin), Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"end", StringUtils::toString<int > (end), Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"author", l_author, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"accession", l_accession, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"type", l_type, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"title", l_title, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"journal", l_journal, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"citation", l_citation, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"year", l_year, Field::STORE_YES, Field::INDEX_ANALYZED));
            sentencewriter->addDocument(sentencedoc);
        }
        aait.moveToNext();
    }
}

void WriteBib(vector<String> bib_info) {
    String l_author;
    String l_accession;
    String l_type;
    String l_title;
    String l_journal;
    String l_citation;
    String l_year;
    String l_abstract;
    String l_filepath;
    if (bib_info.size() > 1) {
    	l_author = bib_info[0];
        l_accession = bib_info[1];
        l_type = bib_info[2];
        l_title = bib_info[3];
    	l_journal = bib_info[4];
    	l_citation = bib_info[5];
    	l_year = bib_info[6];
    	l_abstract = bib_info[7];
    	l_filepath = bib_info[8];
    }
    wstring w_author = l_author.c_str();
    string s_author(w_author.begin(), w_author.end());
    wstring w_accession = l_accession.c_str();
    string s_accession(w_accession.begin(), w_accession.end());
    wstring w_type = l_type.c_str();
    string s_type(w_type.begin(), w_type.end());
    wstring w_title = l_title.c_str();
    string s_title(w_title.begin(), w_title.end());
    wstring w_journal = l_journal.c_str();
    string s_journal(w_journal.begin(), w_journal.end());
    wstring w_citation = l_citation.c_str();
    string s_citation(w_citation.begin(), w_citation.end());
    wstring w_year = l_year.c_str();
    string s_year(w_year.begin(), w_year.end());
    wstring w_abstract = l_abstract.c_str();
    string s_abstract(w_abstract.begin(), w_abstract.end());
    wstring w_filepath = l_filepath.c_str();
    wstring w_bibpath = w_filepath;
    boost::replace_all(w_bibpath, L".tpcas", L".bib");
    string s_bibpath(w_bibpath.begin(), w_bibpath.end());
    ofstream output(s_bibpath.c_str());
    output << "author|" << s_author << endl;
    output << "accession|" << s_accession << endl;
    output << "type|" << s_type << endl;
    output << "title|" << s_title << endl;
    output << "journal|" << s_journal << endl;
    output << "citation|" << s_citation << endl;
    output << "year|" << s_year << endl;
    output << "abstract|" << s_abstract << endl;
    output.close();
}

void IndexBib(vector<String> bib_info, IndexWriterPtr bibwriter, String l_filenamehash) {
    String l_author = bib_info[0];
    String l_accession = bib_info[1];
    String l_type = bib_info[2];
    String l_title = bib_info[3];
    String l_journal = bib_info[4];
    String l_citation = bib_info[5];
    String l_year = bib_info[6];
    String l_abstract = bib_info[7];
    String l_filepath = bib_info[8];
    WriteBib(bib_info);
    DocumentPtr bibdoc = newLucene<Document > ();
    bibdoc->add(newLucene<Field > (L"author", l_author, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"accession", l_accession, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"type", l_type, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"title", l_title, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"journal", l_journal, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"citation", l_citation, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"year", l_year, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"identifier", l_filenamehash, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
    bibdoc->add(newLucene<Field > (L"abstract", l_abstract, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
    bibdoc->add(newLucene<Field > (L"filepath", l_filepath, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
    bibwriter->addDocument(bibdoc);
}

TyErrorId Tpcas2Bib::initialize(AnnotatorContext & rclAnnotatorContext) {
    string tempDir;
    if (!rclAnnotatorContext.isParameterDefined("TempDirectory") ||
            rclAnnotatorContext.extractValue("TempDirectory", tempDir) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"TempDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() TempDirectory - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2Bib::typeSystemInit(TypeSystem const & crTypeSystem) {
    // input type and feature
    std::vector<Type> alltypes;
    crTypeSystem.getAllTypes(alltypes);
    std::vector<Type>::iterator atit;
    const UnicodeString textpresso("textpresso");
    for (atit = alltypes.begin(); atit != alltypes.end(); atit++) {
        UnicodeStringRef turef = (*atit).getName();
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

TyErrorId Tpcas2Bib::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    vector<String> bib_info;
    string regex_wb = "WBPaper";
    string filename = getFilename(tcas);
    boost::regex regex_to_match(regex_wb.c_str(), boost::regex::icase);
    if (boost::regex_search(filename, regex_to_match)) {
        boost::regex suffix(".tpcas");
        string paper = boost::regex_replace(filename, suffix, "");
        boost::smatch wbpaper_matches;
        boost::regex wbregex("(WBPaper\\d{8})");
        if (boost::regex_search(paper, wbpaper_matches, wbregex)) {
            paper = wbpaper_matches[1];
            bib_info = GetBibFromWB(paper);
        }
    } else {
        string xml_text = getXMLstring(tcas);
        bib_info = GetBibFromXML(xml_text);
    }

    String l_filepath = StringUtils::toString(filename.c_str());
    if(const char* env_p = std::getenv("TPCAS_PATH")) {
        l_filepath = (StringUtils::toString(env_p) + L"/" + l_filepath);
    } else {
        l_filepath = L"/usr/local/textpresso/tpcas/" + l_filepath;
    }
    bib_info.push_back(l_filepath);
    WriteBib(bib_info);
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2Bib::destroy() {
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(Tpcas2Bib);




