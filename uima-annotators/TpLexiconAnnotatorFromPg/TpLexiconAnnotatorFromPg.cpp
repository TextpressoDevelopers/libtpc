/* 
 * File:   TpLexiconAnnotatorFromPg.cpp
 * Author: mueller
 * 
 * Created on October 10, 2013, 2:36 PM
 */

#include "TpLexiconAnnotatorFromPg.h"
#include "AllMyParents.h"
#include <sys/stat.h>
#include "../globaldefinitions.h"
#include "AnnotationCounter.h"
#include <pqxx/pqxx>
#include <boost/algorithm/string.hpp>

TpLexiconAnnotatorFromPg::TpLexiconAnnotatorFromPg(void) {
}

TpLexiconAnnotatorFromPg::~TpLexiconAnnotatorFromPg(void) {
}

uima::TyErrorId TpLexiconAnnotatorFromPg::initialize(uima::AnnotatorContext & rclAnnotatorContext) {
    if (!rclAnnotatorContext.isParameterDefined("LexiconTableName") ||
            rclAnnotatorContext.extractValue("LexiconTableName", lexicontablename_) != UIMA_ERR_NONE) {
        /* log the error condition */
        rclAnnotatorContext.getLogger().logError(
                "Required configuration parameter \"LocalLexiconFile\" not found in component descriptor");
        cerr << "TpLexiconAnnotator::initialize() - Error. See logfile." << endl;
        return UIMA_ERR_USER_ANNOTATOR_COULD_NOT_INIT;
    }
    std::cerr << lexicontablename_ << std::endl;
    // database connection
    pqxx::connection cn(PGONTOLOGY);
    //std::cerr << "Connected to " << cn.dbname() << std::endl;
    pqxx::work w(cn);
    pqxx::result r;
    std::stringstream pc;
    // select term,lexicalvariations,category from PGONTOLOGYTABLENAME
    pc << "select term,lexicalvariations,category from ";
    pc << lexicontablename_;
    r = w.exec(pc.str());
    std::vector<std::string> term;
    std::vector<std::string> lexicalvariations;
    std::vector<std::string> category;
    if (r.size() != 0) {
        //std::cerr << r.size() << " rows retrieved." << std::endl;
        for (pqxx::result::size_type i = 0; i != r.size(); i++) {
            std::string saux;
            r[i]["term"].to(saux);
            term.push_back(saux);
            r[i]["lexicalvariations"].to(saux);
            lexicalvariations.push_back(saux);
            r[i]["category"].to(saux);
            category.push_back(saux);
        }
    }
    w.commit();
    cn.disconnect();
    trie_ = new TpLexiconTrie();
    AllMyParents * amp = new AllMyParents();
    std::multimap<std::string, std::string> mmamp(amp->GetCPs());
    while (!term.empty()) {
        UnicodeString uterm = UnicodeString::fromUTF8(StringPiece(term.back()));
        UnicodeString ucat = UnicodeString::fromUTF8(StringPiece(category.back()));
        trie_->addWord(uterm, ucat);
        trie_->addWord(uterm, "pAaCh" + ucat);
        std::pair<std::multimap<std::string, std::string>::iterator,
                std::multimap<std::string, std::string>::iterator> ppp;
        std::string dummy("");
        ppp = mmamp.equal_range(ucat.toUTF8String<std::string>(dummy));
        for (std::multimap<std::string, std::string>::iterator it2 = ppp.first; it2 != ppp.second; ++it2) {
            UnicodeString upcat = "pAaCh" + UnicodeString::fromUTF8(StringPiece((*it2).second));
            trie_->addWord(uterm, upcat);
        }
        // do lexical variations here;
        std::vector<std::string> splitsterms;
        boost::split(splitsterms, lexicalvariations.back(), boost::is_any_of("|"));
        while (!splitsterms.empty()) {
            std::string aux = splitsterms.back();
            boost::trim(aux);
            uterm = UnicodeString::fromUTF8(StringPiece(aux));
            trie_->addWord(uterm, ucat);
            trie_->addWord(uterm, "pAaCh" + ucat);
            std::string dummy("");
            ppp = mmamp.equal_range(ucat.toUTF8String<std::string>(dummy));
            for (std::multimap<std::string, std::string>::iterator it2 = ppp.first; it2 != ppp.second; ++it2) {
                UnicodeString upcat = "pAaCh" + UnicodeString::fromUTF8(StringPiece((*it2).second));
                trie_->addWord(uterm, upcat);
            }
            splitsterms.pop_back();
        }
        term.pop_back();
        lexicalvariations.pop_back();
        category.pop_back();
    }
    s_ = new StopWords();
    delete amp;
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

uima::TyErrorId TpLexiconAnnotatorFromPg::typeSystemInit(uima::TypeSystem const & crTypeSystem) {
    lexanntype_ = crTypeSystem.getType("org.apache.uima.textpresso.lexicalannotation");
    if (!lexanntype_.isValid()) {
        getAnnotatorContext().getLogger().logError(
                "Error getting Type object for org.apache.uima.textpresso.lexicalannotation");
        cerr << "TpLexiconAnnotator::typeSystemInit - Error. See logfile" << endl;
        return (uima::TyErrorId) UIMA_ERR_RESMGR_INVALID_RESOURCE;
    }
    term_ = lexanntype_.getFeatureByBaseName("term");
    category_ = lexanntype_.getFeatureByBaseName("category");
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

uima::TyErrorId TpLexiconAnnotatorFromPg::destroy() {
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

uima::TyErrorId TpLexiconAnnotatorFromPg::process(uima::CAS & tcas, uima::ResultSpecification const & crResultSpecification) {
    AnnotationCounter ac(tcas);
    uima::FSIndexRepository & indexRep = tcas.getIndexRepository();
    uima::UnicodeStringRef ulstrDoc = tcas.getDocumentText();
    getAnnotatorContext().getLogger().logMessage("process called");
    UnicodeString dst;
    ulstrDoc.extract(0, ulstrDoc.length(), dst);
    vector<ppiiU> p = trie_->searchAllWords(dst);
    vector<ppiiU>::iterator it;
    for (it = p.begin(); it < p.end(); it++) {
        pair<int32_t, int32_t> be = (*it).first;
        UnicodeString a = (*it).second;
        int32_t b = be.first;
        int32_t e = be.second + 1;
        uima::AnnotationFS fsNewExp = tcas.createAnnotation(lexanntype_, b, e);
        UnicodeString wd;
        ulstrDoc.extract(b, e - b, wd);
        std::string dummy;
        if (!s_->isStopword(wd.toUTF8String<std::string > (dummy))) {
            fsNewExp.setStringValue(term_, wd);
            fsNewExp.setStringValue(category_, a);
            uima::Feature faid = lexanntype_.getFeatureByBaseName("aid");
            if (faid.isValid())
                fsNewExp.setIntValue(faid, ac.GetNextId());
            indexRep.addFS(fsNewExp);
        }
    }
    return (uima::TyErrorId) UIMA_ERR_NONE;
}

using namespace uima;
MAKE_AE(TpLexiconAnnotatorFromPg);
