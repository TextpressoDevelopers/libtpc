/* 
 * File:   TpTokenizer.cpp
 * Author: mueller
 * 
 * Created on February 4, 2013, 3:15 PM
 */

#include "TpTokenizer.h"
#include "../uimaglobaldefinitions.h"
#include "AnnotationCounter.h"
#include <boost/regex.hpp>
#include <sstream>
#include <iomanip>

using namespace std;

/* magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/ */
static const uint64_t InitialFNV = 14695981039346656037U;
static const uint64_t FNVMultiple = 1099511628211;

/* Fowler / Noll / Vo (FNV) Hash */
string tpfnv(const UnicodeStringRef &s) {
    uint64_t hash = InitialFNV;
    for (uint64_t i = 0; i < s.length(); i++) {
        hash = hash ^ (s[i]); /* xor  the low 8 bits */
        hash = hash * FNVMultiple; /* multiply by the magic number */
    }
    stringstream shex;
    shex << std::setw(16) << std::setfill('0') << std::hex << hash;
    return shex.str();
}

pair<int32_t, int32_t> MaximizeBoundaries(pair<int32_t, int32_t> i,
        pair<int32_t, int32_t> j) {
    // need to make these modifications (offsets) because
    // trie class has different boundary convention than UIMA
    int32_t ifi = i.first;
    int32_t ise = i.second + 1;
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

void WriteOutAnnotations(CAS & tcas, const UnicodeStringRef usdocref,
        vector< pair<int32_t, int32_t> > & p, Type t1, Feature f1,
        Type t2, Feature f2, AnnotationCounter & ac, bool writeoutdelimiters) {
    FSIndexRepository & indexRep = tcas.getIndexRepository();
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
        //int32_t b = res.first;
        //int32_t e = res.second;
        it++;
    }
    // use unique beginnings and endings of tokendelimiters to annotate tokens.
    int32_t laste = 0;
    for (it = merged.begin(); it != merged.end(); it++) {
        int32_t b = (*it).first;
        int32_t e = (*it).second;
        if (b != laste) {
            AnnotationFS fsNewTok = tcas.createAnnotation(t2, laste, b);
            UnicodeString wd;
            usdocref.extract(laste, b - laste, wd);
            fsNewTok.setStringValue(f2, wd);
            Feature faid = t2.getFeatureByBaseName("aid");
            if (faid.isValid())
                fsNewTok.setIntValue(faid, ac.GetNextId());
            indexRep.addFS(fsNewTok);
        }
        laste = e;
        if (writeoutdelimiters) {
            AnnotationFS fsNewTok = tcas.createAnnotation(t2, b, e);
            UnicodeString wd;
            usdocref.extract(b, e - b, wd);
            fsNewTok.setStringValue(f2, wd);
            Feature faid = t2.getFeatureByBaseName("aid");
            if (faid.isValid())
                fsNewTok.setIntValue(faid, ac.GetNextId());
            indexRep.addFS(fsNewTok);
        }
    }
}

// PdfTag end position seem to be off by one (the end position should be 1 after the tag ends).
// Leave if for now as other classes depend on this erroneous definition.
void WriteOutPdfTags(CAS & tcas, const UnicodeStringRef usdocref,
        vector< pair<int32_t, int32_t> > & p, Type t1, Feature f1, Feature f2) {
    FSIndexRepository & indexRep = tcas.getIndexRepository();
    vector< pair<int32_t, int32_t> >::iterator it;
    for (it = p.begin(); it != p.end(); it++) {
        int32_t b = (*it).first;
        int32_t e = (*it).second + 1;
        int32_t endoftag = usdocref.indexOf('>', b);
        if (b != endoftag) {
            AnnotationFS fsNewTok = tcas.createAnnotation(t1, b, endoftag);
            UnicodeString strip;
            usdocref.extract(b, e - b, strip);
            UnicodeString tt;
            strip.extract(6, strip.length() - 1, tt);
            fsNewTok.setStringValue(f1, tt);
            UnicodeString fulltag;
            usdocref.extract(b, endoftag - b + 1, fulltag);
            UnicodeString value;
            int32_t equalsign = fulltag.indexOf('=', 0);
            if (equalsign > -1) {
                fulltag.extract(equalsign + 1, fulltag.length() - equalsign - 3, value);
                fsNewTok.setStringValue(f2, value);
            }
            indexRep.addFS(fsNewTok);
        }
    }
}

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

TpTokenizer::TpTokenizer() {
    dlsetToken = set<UnicodeString > (G_initT, G_initT + G_initT_No);
    dlsetSentence = set<UnicodeString > (G_initS, G_initS + G_initS_No);
    disqSentence.push_back("[\\(>\\s][A-Z]\\.[<\\s]$");
    const string part1 = "[\\(>\\s](Prof|Ph\\.D|Dr|[Ff]igs?|[Vv]ol|i\\.e|e\\.g";
    const string part2 = "|[Nn]o|[Vv]s|[Ee]x|al|ca)\\.[<\\s]$";
    disqSentence.push_back(part1 + part2);
    maxfrontdisqcharlength = 4; // sniplet length in front of sentence delimiter
    maxbackdisqcharlength = 0; // sniplet length  following sentence delimiter.
    PdfTags = set<UnicodeString > (G_initP, G_initP + G_initP_No);
}

TpTokenizer::TpTokenizer(const TpTokenizer & orig) {
}

TpTokenizer::~TpTokenizer() {
}

TyErrorId TpTokenizer::initialize(AnnotatorContext & rclAnnotatorContext) {
    set<UnicodeString>::iterator it;
    trieToken = new TpTrie();
    for (it = dlsetToken.begin(); it != dlsetToken.end(); it++) {
        trieToken->addWord(*it);
    }
    trieSentence = new TpTrie();
    for (it = dlsetSentence.begin(); it != dlsetSentence.end(); it++) {
        trieSentence->addWord(*it);
    }
    triePdfTags = new TpTrie();
    for (it = PdfTags.begin(); it != PdfTags.end(); it++) {
        triePdfTags->addWord(*it);
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TpTokenizer::typeSystemInit(TypeSystem const & crTypeSystem) {
    filenametype = crTypeSystem.getType("org.apache.uima.textpresso.filename");
    if (!filenametype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.filename.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    filenametype_name = filenametype.getFeatureByBaseName("value");
    rawsourcetype = crTypeSystem.getType("org.apache.uima.textpresso.rawsource");
    if (!rawsourcetype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.rawsource.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    rawsourcetype_type = rawsourcetype.getFeatureByBaseName("value");
    tokentype = crTypeSystem.getType("org.apache.uima.textpresso.token");
    if (!tokentype.isValid()) {
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.token.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tokentype_content = tokentype.getFeatureByBaseName("content");
    tokendelimitertype =
            crTypeSystem.getType("org.apache.uima.textpresso.tokendelimiter");
    if (!tokendelimitertype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.tokendelimiter.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tokendelimitertype_content = tokendelimitertype.getFeatureByBaseName("content");

    sentencetype = crTypeSystem.getType("org.apache.uima.textpresso.sentence");
    if (!sentencetype.isValid()) {
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.sentence.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    sentencetype_content = sentencetype.getFeatureByBaseName("content");
    sentencedelimitertype =
            crTypeSystem.getType("org.apache.uima.textpresso.sentencedelimiter");
    if (!sentencedelimitertype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.sentencedelimiter.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    sentencedelimitertype_content
            = sentencedelimitertype.getFeatureByBaseName("content");
    tpfnvhashtype =
            crTypeSystem.getType("org.apache.uima.textpresso.tpfnvhash");
    if (!tpfnvhashtype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.tpfnvhash.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tpfnvhashtype_content = tpfnvhashtype.getFeatureByBaseName("content");
    pdftagtype =
            crTypeSystem.getType("org.apache.uima.textpresso.pdftag");
    if (!pdftagtype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.pdftag.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    pdftagtype_tagtype = pdftagtype.getFeatureByBaseName("tagtype");
    pdftagtype_value = pdftagtype.getFeatureByBaseName("value");
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TpTokenizer::destroy() {
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TpTokenizer::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    AnnotationCounter ac(tcas);
    UnicodeStringRef usprelimref = tcas.getDocumentText();
    int32_t firsthash = usprelimref.indexOf('#', 0);
    UnicodeString numberstring;
    usprelimref.extract(0, firsthash, numberstring);
    std::stringstream auxstream;
    int32_t result;
    auxstream << numberstring;
    auxstream >> result;
    UnicodeString filename;
    usprelimref.extract(firsthash + 1, result - firsthash - 2, filename);
    UnicodeString usaux;
    usprelimref.extract(result, usprelimref.length() - result, usaux);
    UnicodeStringRef usdocref(usaux);
    tcas.setDocumentText(usdocref);
    //    UnicodeStringRef usdocref = tcas.getDocumentText();
    getAnnotatorContext().getLogger().logMessage("process called");
    UnicodeString dst;
    usdocref.extract(0, usdocref.length(), dst);

    vector< pair<int32_t, int32_t> > p = trieToken->searchAllWords(dst);
    sort(p.begin(), p.end()); // just to make sure it is sorted.
    WriteOutAnnotations(tcas, usdocref, p,
            tokendelimitertype, tokendelimitertype_content,
            tokentype, tokentype_content, ac, true);

    p.clear();
    p = trieSentence->searchAllWords(dst);
    p = RemoveDelimiters(usdocref, disqSentence, p,
            maxfrontdisqcharlength, maxbackdisqcharlength);
    sort(p.begin(), p.end());
    WriteOutAnnotations(tcas, usdocref, p,
            sentencedelimitertype, sentencedelimitertype_content,
            sentencetype, sentencetype_content, ac, false);
    p.clear();
    p = triePdfTags->searchAllWords(dst);
    WriteOutPdfTags(tcas, usdocref, p, pdftagtype, pdftagtype_tagtype, pdftagtype_value);

    //    Consider introducing hyphenated word annotation;
    //    Here is the pattern from the old Tokenizer Perl script.
    //    joins words hyphenated by the end of line
    //    $line =~ s/([a-z]+)- *\n+([a-z]+)/$1$2/g;

    FSIndexRepository & indexRep = tcas.getIndexRepository();
    AnnotationFS fsNewTok = tcas.createAnnotation(tpfnvhashtype, 0, usdocref.length());
    string shex = tpfnv(usdocref);
    UnicodeString ushex(shex.c_str());
    fsNewTok.setStringValue(tpfnvhashtype_content, ushex);
    indexRep.addFS(fsNewTok);
    AnnotationFS fsNT2 = tcas.createAnnotation(rawsourcetype, 0, usdocref.length());
    fsNT2.setStringValue(rawsourcetype_type, "pdf");
    indexRep.addFS(fsNT2);
    AnnotationFS fsNT3 = tcas.createAnnotation(filenametype, 0, usdocref.length());
    fsNT3.setStringValue(filenametype_name, filename);
    indexRep.addFS(fsNT3);
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(TpTokenizer);
