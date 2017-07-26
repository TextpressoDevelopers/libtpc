/* 
 * File:   TpLexiconAnnotator.cpp
 * Author: mueller
 * 
 * Created on February 6, 2013, 10:09 AM
 */

#include "TpLexiconAnnotator.h"
#include "AnnotationCounter.h"
#include "StopWords.h"
#include <sys/stat.h>

using namespace std;

TpLexiconAnnotator::TpLexiconAnnotator(void) {
}

TpLexiconAnnotator::~TpLexiconAnnotator(void) {
}

TyErrorId TpLexiconAnnotator::initialize(AnnotatorContext & rclAnnotatorContext) {
    if (!rclAnnotatorContext.isParameterDefined("LocalLexiconFile") ||
            rclAnnotatorContext.extractValue("LocalLexiconFile", locallexiconfile) != UIMA_ERR_NONE) {
        /* log the error condition */
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"LocalLexiconFile\" not found in component descriptor");
        cerr << "TpLexiconAnnotator::initialize() - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    trie = new TpLexiconTrie();
    std::ifstream f(locallexiconfile.c_str());
    string in;
    while (getline(f, in)) {
        int i = in.find_first_of('\t');
        string term = in.substr(0, i);
        string category = in.substr(i + 1, in.length() - i);
        UnicodeString uterm = UnicodeString::fromUTF8(StringPiece(term));
        UnicodeString ucat = UnicodeString::fromUTF8(StringPiece(category));
        trie->addWord(uterm, ucat);
    }
    f.close();
    s = new StopWords();
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TpLexiconAnnotator::typeSystemInit(TypeSystem const & crTypeSystem) {
    lexanntype = crTypeSystem.getType("org.apache.uima.textpresso.lexicalannotation");
    if (!lexanntype.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.lexicalannotation");
        cerr << "TpLexiconAnnotator::typeSystemInit - Error. See logfile" << endl;
        return (TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    term = lexanntype.getFeatureByBaseName("term");
    category = lexanntype.getFeatureByBaseName("category");
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TpLexiconAnnotator::destroy() {
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId TpLexiconAnnotator::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    AnnotationCounter ac(tcas);
    FSIndexRepository & indexRep = tcas.getIndexRepository();
    UnicodeStringRef ulstrDoc = tcas.getDocumentText();
    getAnnotatorContext().getLogger().logMessage("process called");
    UnicodeString dst;
    ulstrDoc.extract(0, ulstrDoc.length(), dst);
    vector<ppiiU> p = trie->searchAllWords(dst);
    vector<ppiiU>::iterator it;
    for (it = p.begin(); it < p.end(); it++) {
        pair<int32_t, int32_t> be = (*it).first;
        UnicodeString a = (*it).second;
        int32_t b = be.first;
        int32_t e = be.second + 1;
        AnnotationFS fsNewExp = tcas.createAnnotation(lexanntype, b, e);
        UnicodeString wd;
        ulstrDoc.extract(b, e - b, wd);
        std::string dummy;
        if (!s->isStopword(wd.toUTF8String<std::string >(dummy))) {
            fsNewExp.setStringValue(term, wd);
            fsNewExp.setStringValue(category, a);
            Feature faid = lexanntype.getFeatureByBaseName("aid");
            if (faid.isValid())
                fsNewExp.setIntValue(faid, ac.GetNextId());
            indexRep.addFS(fsNewExp);
        }
    }
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(TpLexiconAnnotator);
