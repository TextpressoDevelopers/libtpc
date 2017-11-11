/* 
 * File:   TxTokenizer.cpp
 * Author: mueller
 * 
 * Created on April 30, 2013, 1:28 PM
 */

#include "TxTokenizer.h"
#include "../uimaglobaldefinitions.h"
#include "AnnotationCounter.h"
#include <boost/regex.hpp>
#include <sstream>
#include <iomanip>

using namespace std;

/* magic numbers from http://www.isthe.com/chongo/tech/comp/fnv/ */
static const uint64_t InitialFNV = 14695981039346656037U;
static const uint64_t FNVMultiple = 1099511628211;

int32_t TxTokenizer::FindStartTag(const UnicodeString docstring, const UnicodeString name, int32_t pos) {
    int32_t annb(0);
    UnicodeString s("<" + name);
    pos = docstring.indexOf(s, pos + 1);
    if (pos > 0) {
        // need to make sure that tag name ends with space or ">"
        // long unsigned nameend = min(docstring.find(" ", pos), docstring.find(">", pos));
        int32_t nameend = min(docstring.indexOf(' ', pos), docstring.indexOf('>', pos));
        if (nameend == pos + 1 + name.length()) {
            annb = pos;
        }
    } else {
        return pos;
    }
    return annb;
}

int32_t TxTokenizer::FindEndTag(const UnicodeString docstring, const UnicodeString name, int32_t pos) {
    UnicodeString et1("</" + name + ">");
    int32_t pos1 = docstring.indexOf(et1, pos + 1);
    UnicodeString et2("<" + name + " ");
    int32_t pos2 = docstring.indexOf(et2, pos - 2);
    UnicodeString et3("/>");
    int32_t pos3 = docstring.indexOf(et3, pos2);
    int32_t pos4 = docstring.indexOf('<', pos2 + 1);
    if (pos4 < pos3)
        pos2 = -1;
    if (pos1 > 0)
        if (pos2 > 0) {
            int32_t aux = min(pos1, pos2);
            return aux;
        } else {
            return pos1;
        } else {
        int32_t aux = max(pos2, -1);
        return aux;
    }
}

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

void TxTokenizer::TraverseTree(pugi::xml_node pnode, UnicodeString docstring,
        int32_t pos, CAS & tcas, Type t1, Feature f1, Feature f2, Feature f3) {
    FSIndexRepository & indexRep = tcas.getIndexRepository();
    for (pugi::xml_node_iterator it = pnode.begin(); it != pnode.end(); ++it) {
        if (it->type() == pugi::node_element) {
            const char * ccname = it->name();
            UnicodeString name(ccname);
            int32_t annb = FindStartTag(docstring, name, pos);
            pos = annb + 1;
            TraverseTree(*it, docstring, pos, tcas, t1, f1, f2, f3);
            int tagchecksum = 1;
            // TODO find out why tagchecksum constantly increases for some tags. The check for tagchecksum <= 1000 is
            // not optimal
            while (tagchecksum > 0 && tagchecksum <= 1000) {
                int32_t poss = FindStartTag(docstring, name, pos);
                pos = FindEndTag(docstring, name, pos);
                tagchecksum -= 1;
                while ((poss > 0) && (poss <= pos)) {
                    tagchecksum += 1;
                    poss = FindStartTag(docstring, name, poss + 1);
                }
            }
            if (pos > 0) {
                int anne = docstring.indexOf('>', pos) + 1;
                UnicodeString annvalue;
                stringstream saux;
                saux.clear();
                if (it->attributes_begin() != it->attributes_end()) {
                    pugi::xml_attribute_iterator first = it->attributes_begin();
                    saux << first->name() << "=\'";
                    saux << first->value() << "\'";
                    for (pugi::xml_attribute_iterator ait = ++first;
                            ait != it->attributes_end(); ++ait) {
                        saux << " ";
                        saux << ait->name() << "=\'";
                        saux << ait->value() << "\'";
                    }
                }
                UnicodeString annattributes(saux.str().c_str());
                if (annb != anne) {
                    AnnotationFS fsNewTok = tcas.createAnnotation(t1, annb, anne);
                    fsNewTok.setStringValue(f1, name);
                    fsNewTok.setStringValue(f2, annvalue);
                    fsNewTok.setStringValue(f3, annattributes);
                    indexRep.addFS(fsNewTok);
                }
            }
        } else if (it->type() == pugi::node_pcdata) {
            // if it's pcdata pos, then pos is still at the beginning 
            // position  + 1 of the last xml bracket. Need to find 
            // character '>'. The following position is then the beginning
            // of the pcdata.
            UnicodeString br('>');
            pos = docstring.indexOf(br, pos);
            if (pos > 0) {
                pos++;
                UnicodeString name("pcdata");
                const char * ccvalue = it->value();
                UnicodeString annvalue(ccvalue);
                UnicodeString annattributes;
                AnnotationFS fsNewTok = tcas.createAnnotation(t1, pos, pos + annvalue.length());
                fsNewTok.setStringValue(f1, name);
                fsNewTok.setStringValue(f2, annvalue);
                fsNewTok.setStringValue(f3, annattributes);
                indexRep.addFS(fsNewTok);
            }
        }
    }
}

void TxTokenizer::FindAndWriteOutXMLTags(CAS & tcas, const UnicodeStringRef usdocref,
        Type t1, Feature f1, Feature f2, Feature f3) {
    FSIndexRepository & indexRep = tcas.getIndexRepository();
    UnicodeString docstring;
    usdocref.extract(0, usdocref.length(), docstring);
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load(usdocref.asUTF8().c_str());
    if (result) {
        pugi::xml_node root = doc.first_child();
        UnicodeString rootname(root.name());
        /// deal with root start tag
        int32_t annb = 0;
        int32_t pos = docstring.indexOf('<');
        while (pos > 0) {
            if (docstring.indexOf(rootname, pos) - pos == 1) break;
            pos = docstring.indexOf('<', pos + 1);
        }
        if (pos > 0) {
            // need to make sure that tag ends with space or ">"
            int32_t nameend = min(docstring.indexOf(' ', pos),
                    docstring.indexOf('>', pos));
            if (nameend == pos + 1 + rootname.length()) {
                annb = pos;
            }
        }
        pos++;
        TraverseTree(root, docstring, pos, tcas, t1, f1, f2, f3);
        // deal with root end tag
        UnicodeString et("</");
        pos = docstring.indexOf(et, pos + 1);
        while (pos > 0) {
            if (docstring.indexOf(rootname, pos) - pos == 2)
                // need to make sure that tag ends with ">"
                if (docstring.indexOf('>', pos) == pos + 2 + rootname.length())
                    break;
            pos = docstring.indexOf(et, pos + 1);
        }
        if (pos > 0) {
            int anne = pos + 4 + rootname.length();
            UnicodeString annvalue(root.text().get());
            stringstream saux;
            saux.clear();
            if (root.attributes_begin() != root.attributes_end()) {
                pugi::xml_attribute_iterator first = root.attributes_begin();
                saux << first->name() << "=\'";
                saux << first->value() << "\'";
                for (pugi::xml_attribute_iterator ait = ++first;
                        ait != root.attributes_end(); ++ait) {
                    saux << " ";
                    saux << ait->name() << "=\'";
                    saux << ait->value() << "\'";
                }
            }
            UnicodeString annattributes(saux.str().c_str());
            if (annb != anne) {
                AnnotationFS fsNewTok = tcas.createAnnotation(t1, annb, anne);
                fsNewTok.setStringValue(f1, rootname);
                fsNewTok.setStringValue(f2, annvalue);
                fsNewTok.setStringValue(f3, annattributes);
                indexRep.addFS(fsNewTok);
            }
        }
    } else {
        getAnnotatorContext().getLogger().logError(
                "Error parsing document through XML parser.");
        std::cerr << "TxTokenizer::FindAndWriteOutXMLTags - Error. See logfile" << endl;
        std::cerr << "XML parsed with errors.\n";
        std::cerr << "Error description: " << result.description() << "\n";
        std::cerr << "Error offset: " << result.offset << ".\n\n";
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

TxTokenizer::TxTokenizer() {
    dlsetToken = set<UnicodeString > (G_initT, G_initT + G_initT_No);
    dlsetSentence = set<UnicodeString > (G_initS, G_initS + G_initS_No);
    disqSentence.push_back("[\\(>\\s][A-Z]\\.[<\\s]$");
    const string part1 = "[\\(>\\s](Prof|Ph\\.D|Dr|[Ff]igs?|[Vv]ol|i\\.e|e\\.g";
    const string part2 = "|[Nn]o|[Vv]s|[Ee]x|al|ca)\\.[<\\s]$";
    disqSentence.push_back(part1 + part2);
    maxfrontdisqcharlength = 4; // sniplet length in front of sentence delimiter
    maxbackdisqcharlength = 0; // sniplet length  following sentence delimiter.
}

TxTokenizer::TxTokenizer(const TxTokenizer & orig) {
}

TxTokenizer::~TxTokenizer() {
}

TyErrorId TxTokenizer::initialize(AnnotatorContext & rclAnnotatorContext) {
    set<UnicodeString>::iterator it;
    trieToken = new TpTrie();
    for (it = dlsetToken.begin(); it != dlsetToken.end(); it++) {
        trieToken->addWord(*it);
    }
    trieSentence = new TpTrie();
    for (it = dlsetSentence.begin(); it != dlsetSentence.end(); it++) {
        trieSentence->addWord(*it);
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TxTokenizer::typeSystemInit(TypeSystem const & crTypeSystem) {
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
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.rawsource.");
        cerr << "TpTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    rawsourcetype_type = rawsourcetype.getFeatureByBaseName("value");
    tokentype = crTypeSystem.getType("org.apache.uima.textpresso.token");
    if (!tokentype.isValid()) {
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.token.");
        cerr << "TxTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tokentype_content = tokentype.getFeatureByBaseName("content");
    tokendelimitertype =
            crTypeSystem.getType("org.apache.uima.textpresso.tokendelimiter");
    if (!tokendelimitertype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.tokendelimiter.");
        cerr << "TxTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tokendelimitertype_content = tokendelimitertype.getFeatureByBaseName("content");
    sentencetype = crTypeSystem.getType("org.apache.uima.textpresso.sentence");
    if (!sentencetype.isValid()) {
        getAnnotatorContext().getLogger(). logError(
                "Error getting Type object for org.apache.uima.textpresso.sentence.");
        cerr << "TxTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    sentencetype_content = sentencetype.getFeatureByBaseName("content");
    sentencedelimitertype =
            crTypeSystem.getType("org.apache.uima.textpresso.sentencedelimiter");
    if (!sentencedelimitertype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.sentencedelimiter.");
        cerr << "TxTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    sentencedelimitertype_content
            = sentencedelimitertype.getFeatureByBaseName("content");
    tpfnvhashtype =
            crTypeSystem.getType("org.apache.uima.textpresso.tpfnvhash");
    if (!tpfnvhashtype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.tpfnvhash.");
        cerr << "TxTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    tpfnvhashtype_content = tpfnvhashtype.getFeatureByBaseName("content");
    xmltagtype =
            crTypeSystem.getType("org.apache.uima.textpresso.xmltag");
    if (!xmltagtype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.xmltag.");
        cerr << "TxTokenizer::typeSystemInit - Error. See logfile." << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    xmltagtype_value = xmltagtype.getFeatureByBaseName("value");
    xmltagtype_content = xmltagtype.getFeatureByBaseName("content");
    xmltagtype_term = xmltagtype.getFeatureByBaseName("term");
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TxTokenizer::destroy() {
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TxTokenizer::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
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
    //   UnicodeStringRef usdocref = tcas.getDocumentText();
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
    FindAndWriteOutXMLTags(tcas, usdocref, xmltagtype, xmltagtype_value,
            xmltagtype_term, xmltagtype_content);
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
    fsNT2.setStringValue(rawsourcetype_type, "nxml");
    indexRep.addFS(fsNT2);
    AnnotationFS fsNT3 = tcas.createAnnotation(filenametype, 0, usdocref.length());
    fsNT3.setStringValue(filenametype_name, filename);
    indexRep.addFS(fsNT3);
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(TxTokenizer);
