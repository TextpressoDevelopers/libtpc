/* 
 * File:   Tpcas2Lpp.cpp
 * Author: mueller
 * 
 * Created on February 5, 2013, 1:27 PM
 * modified Nov, 2013, liyuling
 */

#include "Tpcas2Lpp.h"
#include <lucene++/FileUtils.h>
#include "CASUtils.h"
#include <lucene++/LuceneHeaders.h>
#include <iomanip>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

#include <unicode/regex.h>
#include <locale>
#include <string>
#include <boost/algorithm/string/find_iterator.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "../../lucene-custom/CaseSensitiveAnalyzer.h"

//#include <unordered_map>

//#include <codecvt>

Tpcas2Lpp::Tpcas2Lpp() {
}

Tpcas2Lpp::Tpcas2Lpp(const Tpcas2Lpp & orig) {
}

Tpcas2Lpp::~Tpcas2Lpp() {
}

/* magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/ */
static const uint64_t InitialFNV = 14695981039346656037U;
static const uint64_t FNVMultiple = 1099511628211;

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

void IndexBib(string xml_text, IndexWriterPtr bibwriter, String l_filenamehash) {
    boost::regex nline("\\n");
    xml_text = boost::regex_replace(xml_text, nline, "");
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
    DocumentPtr bibdoc = newLucene<Document > ();
    String l_author = StringUtils::toString(author.c_str());
    String l_accession = StringUtils::toString(accession.c_str());
    String l_type = StringUtils::toString(type.c_str());
    String l_title = StringUtils::toString(title.c_str());
    String l_journal = StringUtils::toString(journal.c_str());
    String l_citation = StringUtils::toString(citation.c_str());
    String l_year = StringUtils::toString(year.c_str());

    bibdoc->add(newLucene<Field > (L"author", l_author, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"accession", l_accession, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"type", l_type, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"title", l_title, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"journal", l_journal, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"citation", l_citation, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"year", l_year, Field::STORE_YES, Field::INDEX_ANALYZED));
    bibdoc->add(newLucene<Field > (L"identifier", l_filenamehash, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));

    bibwriter->addDocument(bibdoc);
}

TyErrorId Tpcas2Lpp::initialize(AnnotatorContext & rclAnnotatorContext) {
    string tempDir;
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
    //if (boost::filesystem::exists("/run/shm/newindexflag")) {
    if (boost::filesystem::exists(newindexflag)) {
        b_newindex = true;
    } else {
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
    tokenwriter = newLucene<IndexWriter > (FSDirectory::open(TokenIndexDir),
                                           newLucene<StandardAnalyzer > (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                           IndexWriter::MaxFieldLengthUNLIMITED);

    if (!rclAnnotatorContext.isParameterDefined("TokenCaseSensitiveLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("TokenCaseSensitiveLuceneIndexDirectory", tokenindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"TokenCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Token - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String TokenCaseSensitiveIndexDir = StringUtils::toString(tokenindexdirectory_casesens.c_str());
    tokenwriter_casesens = newLucene<IndexWriter > (FSDirectory::open(TokenCaseSensitiveIndexDir),
                                                    newLucene<CaseSensitiveAnalyzer> (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                                    IndexWriter::MaxFieldLengthUNLIMITED);
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
        rclAnnotatorContext.extractValue("SentenceCaseSensitiveLuceneIndexDirectory", sentenceindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"SentenceCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Sentence - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String SentenceCaseSensitiveIndexDir = StringUtils::toString(sentenceindexdirectory_casesens.c_str());
    sentencewriter_casesens = newLucene<IndexWriter > (FSDirectory::open(SentenceCaseSensitiveIndexDir),
                                                       newLucene<CaseSensitiveAnalyzer> (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                                       IndexWriter::MaxFieldLengthUNLIMITED);
    if (!rclAnnotatorContext.isParameterDefined("LexicalLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("LexicalLuceneIndexDirectory", lexicalindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"LexicalLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Lexical - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String LexicalIndexDir = StringUtils::toString(lexicalindexdirectory.c_str());
    lexicalwriter = newLucene<IndexWriter > (FSDirectory::open(LexicalIndexDir),
                                             newLucene<StandardAnalyzer> (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                             IndexWriter::MaxFieldLengthUNLIMITED);

    if (!rclAnnotatorContext.isParameterDefined("LexicalCaseSensitiveLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("LexicalCaseSensitiveLuceneIndexDirectory", lexicalindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"LexicalCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() Lexical - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String LexicalCaseSensitiveIndexDir = StringUtils::toString(lexicalindexdirectory_casesens.c_str());
    lexicalwriter_casesens = newLucene<IndexWriter > (FSDirectory::open(LexicalCaseSensitiveIndexDir),
                                                      newLucene<CaseSensitiveAnalyzer> (LuceneVersion::LUCENE_30), b_newindex, //create new index
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
        rclAnnotatorContext.extractValue("FulltextCaseSensitiveLuceneIndexDirectory", fulltextindexdirectory) != UIMA_ERR_NONE) {
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

    if (!rclAnnotatorContext.isParameterDefined("BibliographyLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("BibliographyLuceneIndexDirectory", bibindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition 
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"BibliographyLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String BibIndexDir = StringUtils::toString(bibindexdirectory.c_str());
    bibwriter = newLucene<IndexWriter > (FSDirectory::open(BibIndexDir),
                                         newLucene<StandardAnalyzer > (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                         IndexWriter::MaxFieldLengthUNLIMITED);

    if (!rclAnnotatorContext.isParameterDefined("BibliographyCaseSensitiveLuceneIndexDirectory") ||
        rclAnnotatorContext.extractValue("BibliographyCaseSensitiveLuceneIndexDirectory", bibindexdirectory) != UIMA_ERR_NONE) {
        // log the error condition
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"BibliographyCaseSensitiveLuceneIndexDirectory\" not found in component descriptor");
        cerr << "Tpcas2Lucene::initialize() - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    String BibCaseSensitiveIndexDir = StringUtils::toString(bibindexdirectory_casesens.c_str());
    bibwriter_casesens = newLucene<IndexWriter > (FSDirectory::open(BibCaseSensitiveIndexDir),
                                                  newLucene<CaseSensitiveAnalyzer> (LuceneVersion::LUCENE_30), b_newindex, //create new index
                                                  IndexWriter::MaxFieldLengthUNLIMITED);

    return (TyErrorId) UIMA_ERR_NONE;

}

TyErrorId Tpcas2Lpp::typeSystemInit(TypeSystem const & crTypeSystem) {
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
            if (fit != f.end() - 1) cout << ", ";
            features_[*atit].push_back(*fit);
        }
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2Lpp::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    FSIndexRepository & indices = tcas.getIndexRepository();
    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    UnicodeStringRef usdocref = tcas.getDocumentText();
    string pid = tpfnv(usdocref);
    String Spid = StringUtils::toString(pid.c_str());
    string filenamehash = gettpfnvHash(tcas);
    //indexing fulltext
    wstring w_cleanText = getCleanText(tcas);
    String l_filenamehash = StringUtils::toString(filenamehash.c_str());
    DocumentPtr fulltextdoc = newLucene<Document > ();
    fulltextdoc->add(newLucene<Field > (L"identifier", l_filenamehash, Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
    fulltextdoc->add(newLucene<Field > (L"text", StringUtils::toString<wstring > (w_cleanText), Field::STORE_YES, Field::INDEX_ANALYZED));
    fulltextwriter->addDocument(fulltextdoc);
    fulltextwriter_casesens->addDocument(fulltextdoc);
    //indexing bibliographic information
    string xml_text = getXMLstring(tcas);
    IndexBib(xml_text, bibwriter, l_filenamehash);
    IndexBib(xml_text, bibwriter_casesens, l_filenamehash);
    //indexing sentence and lexicalannotation
    aait.moveToFirst();
    int count = 0;
    map<wstring, vector< pair<long, long> > > lexicaldocs; // "identifier+category+term, <begin,end>s"
    while (aait.isValid()) {
        count++;
        Type currentType = aait.get().getType();
        UnicodeStringRef tnameref = currentType.getName();
        string annType = tnameref.asUTF8();
        if (annType == "org.apache.uima.textpresso.sentence") {
            vector<Feature> fList;
            currentType.getAppropriateFeatures(fList);
            long begin = aait.get().getBeginPosition(); //.getStringValue(fid);
            long end = aait.get().getEndPosition();
            wstring sentenceid(filenamehash.begin(), filenamehash.end());
            Feature fcontent = currentType.getFeatureByBaseName("content");
            UnicodeStringRef ucontent = aait.get().getStringValue(fcontent);
            wstring ws;
            UnicodeString wd;
            ucontent.extract(0, ucontent.length(), wd);
            for (int i = 0; i < wd.length(); ++i)
                ws += static_cast<wchar_t> (wd[i]);
            DocumentPtr sentencedoc = newLucene<Document > ();
            sentencedoc->add(newLucene<Field > (L"identifier", StringUtils::toString<wstring > (sentenceid), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"text", StringUtils::toString<wstring > (ws), Field::STORE_NO, Field::INDEX_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"begin", StringUtils::toString<long>(begin), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
            sentencedoc->add(newLucene<Field > (L"end", StringUtils::toString<long>(end), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
            sentencewriter->addDocument(sentencedoc);
            sentencewriter_casesens->addDocument(sentencedoc);
        }


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
            wstring ws_lexicalannotaitonid(filenamehash.begin(), filenamehash.end());
            ws_lexicalannotaitonid = ws_lexicalannotaitonid + L"|" + ws_category + L"|" + ws_term;
            //modify to  category+term as single document
            pair<long, long> p(begin, end);
            if (lexicaldocs.find(ws_lexicalannotaitonid) == lexicaldocs.end()) {
                vector<pair<long, long> > begin_ends;
                begin_ends.push_back(p);
                lexicaldocs.insert(pair<wstring, vector<pair<long, long> > > (ws_lexicalannotaitonid, begin_ends)); // [ws_lexicalannotaitonid] = begin_ends;
            } else {
                lexicaldocs[ws_lexicalannotaitonid].push_back(p);
            }
        }
        aait.moveToNext();
    }
    // concatenate begins/ends as a string  "positions". split "identifier/category/term" from hash(map) key.
    for (map<wstring, vector< pair<long, long> > >::iterator it = lexicaldocs.begin(); it != lexicaldocs.end(); it++) {
        wstring ws_lexicalannotaitonid = (*it).first;
        vector<wstring> subwstrings;
        boost::split(subwstrings, ws_lexicalannotaitonid, boost::is_any_of(L"|"));
        wstring ws_identifier = subwstrings[0];
        wstring ws_category = subwstrings[1];
        wstring ws_term = subwstrings[2];
        vector<pair<long, long> > begin_ends = (*it).second;
        wstring ws_beginend = L"";
        if(begin_ends.size() > 0)
        {
            long begin = begin_ends[0].first;
            long end = begin_ends[0].second;
            ws_beginend = boost::lexical_cast<wstring > (begin) + L"," + boost::lexical_cast<wstring > (end);
        }
        for (int j = 1; j < begin_ends.size(); j++) {
            long begin = begin_ends[j].first;
            long end = begin_ends[j].second;
            ws_beginend = ws_beginend + L"|" + boost::lexical_cast<wstring > (begin) + L"," + boost::lexical_cast<wstring > (end);
        }

        DocumentPtr lexicaldoc = newLucene<Document > ();
        lexicaldoc->add(newLucene<Field > (L"identifier", StringUtils::toString<wstring > (ws_identifier), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
        lexicaldoc->add(newLucene<Field > (L"term", StringUtils::toString<wstring > (ws_term), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
        lexicaldoc->add(newLucene<Field > (L"category", StringUtils::toString<wstring > (ws_category), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
        lexicaldoc->add(newLucene<Field > (L"positions", StringUtils::toString<wstring > (ws_beginend), Field::STORE_YES, Field::INDEX_NOT_ANALYZED));
        lexicaldoc->setBoost(begin_ends.size());
        lexicalwriter->addDocument(lexicaldoc);
        lexicalwriter_casesens->addDocument(lexicaldoc);
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2Lpp::destroy() {
    fulltextwriter->close();
    fulltextwriter_casesens->close();
    tokenwriter->close();
    tokenwriter_casesens->close();
    sentencewriter->close();
    sentencewriter_casesens->close();
    lexicalwriter->close();
    lexicalwriter_casesens->close();
    bibwriter->close();
    bibwriter_casesens->close();
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(Tpcas2Lpp);



