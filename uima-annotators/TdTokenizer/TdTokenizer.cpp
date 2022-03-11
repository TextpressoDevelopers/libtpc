/* 
 * File:   TdTokenizer.cpp
 * Author: mueller
 * 
 * Created on December 13, 2021, 2:15 PM
 */

#include "TdTokenizer.h"
#include "../uimaglobaldefinitions.h"
#include "textAndImageManager.h"
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace boost::filesystem;

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
    shex << setw(16) << setfill('0') << hex << hash;
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
        vector< pair<int32_t, int32_t> > & p, Type t, Feature f,
        AnnotationCounter & ac, bool writeoutdelimiters) {
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
        it++;
    }
    // use unique beginnings and endings of delimiters to annotate items at hand.
    int32_t laste = 0;
    for (it = merged.begin(); it != merged.end(); it++) {
        int32_t b = (*it).first;
        int32_t e = (*it).second;
        if (b != laste) {
            AnnotationFS fsNewTok = tcas.createAnnotation(t, laste, b);
            UnicodeString wd;
            usdocref.extract(laste, b - laste, wd);
            fsNewTok.setStringValue(f, wd);
            Feature faid = t.getFeatureByBaseName("aid");
            if (faid.isValid())
                fsNewTok.setIntValue(faid, ac.GetNextId());
            indexRep.addFS(fsNewTok);
        }
        laste = e;
        if (writeoutdelimiters) {
            AnnotationFS fsNewTok = tcas.createAnnotation(t, b, e);
            UnicodeString wd;
            usdocref.extract(b, e - b, wd);
            fsNewTok.setStringValue(f, wd);
            Feature faid = t.getFeatureByBaseName("aid");
            if (faid.isValid())
                fsNewTok.setIntValue(faid, ac.GetNextId());
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

bool TdTokenizer::hasSection(const set<UnicodeString>& sectionNames,
        const vector<UnicodeString>& sections) {
    bool ret = false;
    for (auto x : sections) if (sectionNames.find(x) != sectionNames.end()) return true;
    return ret;
}

void TdTokenizer::combineSectionAnnotations(CAS & cas,
        const set<UnicodeString>& sectionNames,
        const vector<UnicodeString>& sections,
        const vector<size_t>& b, const vector<size_t>& e,
        const UnicodeString type,
        AnnotationCounter ac) {
    FSIndexRepository & indexRep = cas.getIndexRepository();
    for (size_t i = 0; i < sections.size() - 1; i++) {
        if (sectionNames.find(sections[i]) != sectionNames.end())
            if (e[i] == b[i + 1]) {
                AnnotationFS fsSection = cas.createAnnotation(sectiontype_, b[i], e[i + 1]);
                fsSection.setStringValue(sectiontype_type_, type);
                fsSection.setStringValue(sectiontype_content_, sections[i + 1]);
                Feature faid = sectiontype_.getFeatureByBaseName("aid");
                if (faid.isValid()) fsSection.setIntValue(faid, ac.GetNextId());
                indexRep.addFS(fsSection);
            }
    }
}

std::string utf8Printable(const string & string) {
    int c, i;
    std::string ret("");
    int ix(string.length());
    for (i = 0; i < ix; i++) {
        c = (unsigned char) string[i];
        if (c == 0x09 || c == 0x0a || c == 0x0d || (0x20 <= c && c <= 0x7e)) // is_printable
            ret += string[i];
        else if (c == 0xc2) {
            if (i + 1 < ix)
                if (((unsigned char) string[i + 1]) == 0xb0) {
                    ret += " degree ";
                    i++;
                } else if (((unsigned char) string[i + 1]) == 0xb5) {
                    ret += " micro ";
                    i++;
                } else if (((unsigned char) string[i + 1]) == 0xb1) {
                    ret += " plus/minus ";
                    i++;
                } else if (((unsigned char) string[i + 1]) == 0xa9) {
                    ret += " copyright ";
                    i++;
                }
        } else if (c == 0xe2) {
            if (i + 2 < ix)
                if (((unsigned char) string[i + 1]) == 0x80)
                    if (((unsigned char) string[i + 2]) == 0x94) {
                        ret += "-";
                        i += 2;
                    }
        } else if (c == 0xef) {
            if (i + 2 < ix) {
                if (((unsigned char) string[i + 1]) == 0xac) {
                    if (((unsigned char) string[i + 2]) == 0x82)
                        ret += "fl";
                    if (((unsigned char) string[i + 2]) == 0x81)
                        ret += "fi";
                    if (((unsigned char) string[i + 2]) == 0x80)
                        ret += "ff";
                    i += 2;

                }
            }
        }
    }
    return ret;
}

TdTokenizer::TdTokenizer() {
    dlsetToken_ = set<UnicodeString > (G_initT, G_initT + G_initT_No);
    dlsetSentence_ = set<UnicodeString > (G_initS, G_initS + G_initS_No);
    disqSentence_.push_back("[\\(>\\s][A-Z]\\.[<\\s]$");
    const string part1 = "[\\(>\\s](Prof|Ph\\.D|Dr|[Ff]igs?|[Vv]ol|i\\.e|e\\.g";
    const string part2 = "|[Nn]o|[Vv]s|[Ee]x|al|ca)\\.[<\\s]$";
    disqSentence_.push_back(part1 + part2);
    maxfrontdisqcharlength_ = 4; // sniplet length in front of sentence delimiter
    maxbackdisqcharlength_ = 0; // sniplet length  following sentence delimiter.
    dlsetSection_.clear();
    for (auto x : sectionArticleB) dlsetSection_.insert(x);
    for (auto x : sectionArticleE) dlsetSection_.insert(x);
    for (auto x : sectionAbstract) dlsetSection_.insert(x);
    for (auto x : sectionIntroduction) dlsetSection_.insert(x);
    for (auto x : sectionResult) dlsetSection_.insert(x);
    for (auto x : sectionDiscussion) dlsetSection_.insert(x);
    for (auto x : sectionConclusion) dlsetSection_.insert(x);
    for (auto x : sectionBackground) dlsetSection_.insert(x);
    for (auto x : sectionMaterialsMethods) dlsetSection_.insert(x);
    for (auto x : sectionDesign) dlsetSection_.insert(x);
    for (auto x : sectionAcknowledgments) dlsetSection_.insert(x);

    for (auto x : sectionReferences) dlsetSection_.insert(x);
}

TdTokenizer::TdTokenizer(const TdTokenizer & orig) {
}

TdTokenizer::~TdTokenizer() {
}

TyErrorId TdTokenizer::initialize(AnnotatorContext & rclAnnotatorContext) {
    set<UnicodeString>::iterator it;
    trieToken_ = new TpTrie();
    for (it = dlsetToken_.begin(); it != dlsetToken_.end(); it++) {
        trieToken_->addWord(*it);
    }
    trieSentence_ = new TpTrie();
    for (it = dlsetSentence_.begin(); it != dlsetSentence_.end(); it++) {
        trieSentence_->addWord(*it);
    }
    trieSection_ = new TpTrie();
    for (it = dlsetSection_.begin(); it != dlsetSection_.end(); it++) {

        trieSection_->addWord(*it);
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TdTokenizer::typeSystemInit(TypeSystem const & crTypeSystem) {
    filenametype_ = crTypeSystem.getType("org.apache.uima.textpresso.filename");
    if (!filenametype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.filename.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    filenametype_name_ = filenametype_.getFeatureByBaseName("value");
    rawsourcetype_ = crTypeSystem.getType("org.apache.uima.textpresso.rawsource");
    if (!rawsourcetype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.rawsource.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    rawsourcetype_type_ = rawsourcetype_.getFeatureByBaseName("value");
    tokentype_ = crTypeSystem.getType("org.apache.uima.textpresso.token");
    if (!tokentype_.isValid()) {
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.token.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tokentype_content_ = tokentype_.getFeatureByBaseName("content");
    sentencetype_ = crTypeSystem.getType("org.apache.uima.textpresso.sentence");
    if (!sentencetype_.isValid()) {
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.sentence.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    sentencetype_content_ = sentencetype_.getFeatureByBaseName("content");
    tpfnvhashtype_ =
            crTypeSystem.getType("org.apache.uima.textpresso.tpfnvhash");
    if (!tpfnvhashtype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.tpfnvhash.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tpfnvhashtype_content_ = tpfnvhashtype_.getFeatureByBaseName("content");
    pagetype_ = crTypeSystem.getType("org.apache.uima.textpresso.page");
    if (!pagetype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.page.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    pagetype_value_ = pagetype_.getFeatureByBaseName("value");
    dblbrktype_ = crTypeSystem.getType("org.apache.uima.textpresso.dblbrk");
    if (!dblbrktype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.dblbrk.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    rawsectiontype_ =
            crTypeSystem.getType("org.apache.uima.textpresso.rawsection");
    if (!rawsectiontype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.rawsection.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    rawsectiontype_content_ = rawsectiontype_.getFeatureByBaseName("content");

    sectiontype_ =
            crTypeSystem.getType("org.apache.uima.textpresso.section");
    if (!sectiontype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.section.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    sectiontype_content_ = sectiontype_.getFeatureByBaseName("content");
    sectiontype_type_ = sectiontype_.getFeatureByBaseName("type");
    imagetype_ =
            crTypeSystem.getType("org.apache.uima.textpresso.image");
    if (!imagetype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.image.");
        cerr << "TdTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    imagetype_filename_ = imagetype_.getFeatureByBaseName("filename");
    imagetype_page_ = imagetype_.getFeatureByBaseName("page");

    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TdTokenizer::destroy() {

    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TdTokenizer::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    AnnotationCounter ac(tcas);
    UnicodeStringRef usprelimref = tcas.getDocumentText();
    int32_t firsthash = usprelimref.indexOf('#', 0);
    UnicodeString numberstring;
    usprelimref.extract(0, firsthash, numberstring);
    stringstream auxstream;
    int32_t result;
    auxstream << numberstring;
    auxstream >> result;
    UnicodeString filename;
    usprelimref.extract(firsthash + 1, result - firsthash - 2, filename);
    UnicodeString usaux;
    usprelimref.extract(result, usprelimref.length() - result, usaux);

    string dummy;
    string f(usaux.toUTF8String<string >(dummy));

    textAndImageManager taim = textAndImageManager(path(f).parent_path());
    taim.loadImageFilenames();
    taim.loadTextFilenames();
    taim.loadTextfiles();
    string doc("\nBeginning of Article\n");
    vector<int> pageLengths;
    for (auto x : taim.textFile()) {
        std::string s(utf8Printable(x.second));
        doc += s;
        int len = 0;
        // for utf8 encoding, characters can be encoded with more than 1 byte.
        for (int i = 0; i != s.length(); i++)
            len += (x.second[i] & 0xc0) != 0x80;
        pageLengths.push_back(len);
    }
    doc += "\nEnd of Article\n";
    UnicodeString ustrInputText;
    ustrInputText.append(UnicodeString::fromUTF8(StringPiece(doc)));
    UnicodeStringRef usdocref(ustrInputText);
    tcas.setDocumentText(usdocref);
    getAnnotatorContext().getLogger().logMessage("process called");
    UnicodeString dst;
    usdocref.extract(0, usdocref.length(), dst);

    vector< pair<int32_t, int32_t> > p = trieToken_->searchAllWords(dst);
    sort(p.begin(), p.end()); // just to make sure it is sorted.
    WriteOutAnnotations(tcas, usdocref, p, tokentype_, tokentype_content_, ac, true);
    p.clear();

    p = trieSentence_->searchAllWords(dst);
    p = RemoveDelimiters(usdocref, disqSentence_, p,
            maxfrontdisqcharlength_, maxbackdisqcharlength_);
    sort(p.begin(), p.end());
    WriteOutAnnotations(tcas, usdocref, p, sentencetype_, sentencetype_content_, ac, false);
    p.clear();

    p = trieSection_->searchAllWords(dst);
    sort(p.begin(), p.end()); // just to make sure it is sorted.
    WriteOutAnnotations(tcas, usdocref, p, rawsectiontype_, rawsectiontype_content_, ac, true);
    p.clear();

    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    vector<UnicodeString> sections;
    vector<size_t> sectionsB;
    vector<size_t> sectionsE;
    while (aait.isValid()) {
        Type currentType = aait.get().getType();
        string annType = currentType.getName().asUTF8();
        if (annType == "org.apache.uima.textpresso.rawsection") {
            sectionsB.push_back(aait.get().getBeginPosition());
            sectionsE.push_back(aait.get().getEndPosition());
            Feature f = currentType.getFeatureByBaseName("content");
            sections.push_back(aait.get().getStringValue(f).getBuffer());
        }
        aait.moveToNext();
    }

    bool hasResult(hasSection(sectionResult, sections));
    bool hasIntroduction(hasSection(sectionIntroduction, sections));
    bool hasBackground(hasSection(sectionBackground, sections));
    bool hasDiscussion(hasSection(sectionDiscussion, sections));
    bool hasConclusion(hasSection(sectionConclusion, sections));
    bool hasMM(hasSection(sectionMaterialsMethods, sections));
    bool hasDesign(hasSection(sectionDesign, sections));
    bool hasReferences(hasSection(sectionReferences, sections));
    int score(0);
    if (hasIntroduction || hasBackground) score++;
    if (hasDiscussion || hasConclusion) score++;
    if (hasResult) score++;
    if (hasMM || hasDesign) score++;
    if (hasReferences) score++;
    if (score > 3) {
        combineSectionAnnotations(tcas, sectionArticleB, sections,
                sectionsB, sectionsE, "beginning of article", ac);
        combineSectionAnnotations(tcas, sectionArticleE, sections,
                sectionsB, sectionsE, "end of article", ac);
        combineSectionAnnotations(tcas, sectionAbstract, sections,
                sectionsB, sectionsE, "abstract", ac);
        combineSectionAnnotations(tcas, sectionIntroduction, sections,
                sectionsB, sectionsE, "introduction", ac);
        combineSectionAnnotations(tcas, sectionResult, sections,
                sectionsB, sectionsE, "result", ac);
        combineSectionAnnotations(tcas, sectionDiscussion, sections,
                sectionsB, sectionsE, "discussion", ac);
        combineSectionAnnotations(tcas, sectionConclusion, sections,
                sectionsB, sectionsE, "conclusion", ac);
        combineSectionAnnotations(tcas, sectionBackground, sections,
                sectionsB, sectionsE, "background", ac);
        combineSectionAnnotations(tcas, sectionMaterialsMethods, sections,
                sectionsB, sectionsE, "materials and methods", ac);
        combineSectionAnnotations(tcas, sectionDesign, sections,
                sectionsB, sectionsE, "design", ac);
        combineSectionAnnotations(tcas, sectionAcknowledgments, sections,
                sectionsB, sectionsE, "acknowledgments", ac);
        combineSectionAnnotations(tcas, sectionReferences, sections,
                sectionsB, sectionsE, "references", ac);
    }
    FSIndexRepository & indexRep = tcas.getIndexRepository();
    AnnotationFS fsNewTok = tcas.createAnnotation(tpfnvhashtype_, 0, usdocref.length());
    string shex = tpfnv(usdocref);
    UnicodeString ushex(shex.c_str());
    fsNewTok.setStringValue(tpfnvhashtype_content_, ushex);
    indexRep.addFS(fsNewTok);
    AnnotationFS fsNT2 = tcas.createAnnotation(rawsourcetype_, 0, usdocref.length());
    fsNT2.setStringValue(rawsourcetype_type_, "tai"); // stands for text and image
    // (the new pdftotext system)
    indexRep.addFS(fsNT2);
    AnnotationFS fsNT3 = tcas.createAnnotation(filenametype_, 0, usdocref.length());
    fsNT3.setStringValue(filenametype_name_, filename);
    indexRep.addFS(fsNT3);
    int b(0);
    std::map<int, std::pair<size_t, size_t>> pageBE;
    for (int i = 0; i != pageLengths.size(); i++) {
        AnnotationFS fsPage = tcas.createAnnotation(pagetype_, b, b + pageLengths[i]);
        // store page B and E for image annotation below
        pageBE.insert(std::make_pair<int, std::pair<size_t, size_t >>
                (i + 1, std::make_pair<size_t, size_t>(b, b + pageLengths[i])));
        fsPage.setIntValue(pagetype_value_, i + 1);
        b += pageLengths[i];
        Feature faid = pagetype_.getFeatureByBaseName("aid");
        if (faid.isValid())
            fsPage.setIntValue(faid, ac.GetNextId());
        indexRep.addFS(fsPage);
    }
    for (auto x : taim.imageFilenames()) {
        path p(x);
        string s(p.stem().stem().extension().string());
        int pagenumber(stoi(s.substr(1, s.size() - 1)));
        std::pair<size_t, size_t> auxpair(pageBE[pagenumber]);
        AnnotationFS fsImage = tcas.createAnnotation(imagetype_, auxpair.first, auxpair.second);
        fsImage.setIntValue(imagetype_page_, pagenumber);
        fsImage.setStringValue(imagetype_filename_, UnicodeString(x.c_str()));
        Feature faid = imagetype_.getFeatureByBaseName("aid");
        if (faid.isValid())
            fsImage.setIntValue(faid, ac.GetNextId());
        indexRep.addFS(fsImage);
    }
    size_t found(0);
    while (found < std::string::npos) {
        size_t pos = found + 1;
        found = doc.find("\n\n", pos);
        AnnotationFS fsPage = tcas.createAnnotation(dblbrktype_, found, found + 2);
        Feature faid = dblbrktype_.getFeatureByBaseName("aid");
        if (faid.isValid())
            fsPage.setIntValue(faid, ac.GetNextId());
        indexRep.addFS(fsPage);
    }
    Type rawsection = tcas.getTypeSystem().getType("org.apache.uima.textpresso.rawsection");
    ANIndex rawsectionindex = tcas.getAnnotationIndex(rawsection);
    ANIterator aait2 = rawsectionindex.iterator();
    aait2.moveToFirst();
    std::vector<AnnotationFS> removeList;
    while (aait2.isValid()) {
        removeList.push_back(aait2.get());
        aait2.moveToNext();
    }
    while (!removeList.empty()) {
        indexRep.removeFS(removeList.back());
        removeList.pop_back();
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(TdTokenizer);
