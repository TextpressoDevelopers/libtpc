/* 
 * File:   TxTokenizer.h
 * Author: mueller
 *
 * Created on April 30, 2013, 1:28 PM
 */

#ifndef TXTOKENIZER_H
#define	TXTOKENIZER_H

#include <set>
#include <uima/api.hpp>
#include "../TpTokenizer/TpTrie.h"
#include "pugixml.hpp"

using namespace uima;

class TxTokenizer : public Annotator {
public:
    TxTokenizer();
    TxTokenizer(const TxTokenizer & orig);
    virtual ~TxTokenizer();
    TyErrorId initialize(AnnotatorContext & rclAnnotatorContext);
    TyErrorId typeSystemInit(TypeSystem const & crTypeSystem);
    TyErrorId destroy();
    TyErrorId process(CAS & tcas, ResultSpecification const & crResultSpecification);
private:
    void TraverseTree(pugi::xml_node pnode, UnicodeString docstring,
            int32_t pos, CAS & tcas, Type t1, Feature f1, Feature f2, Feature f3);
    void FindAndWriteOutXMLTags(CAS & tcas, const UnicodeStringRef usdocref,
            Type t1, Feature f1, Feature f2, Feature f3);
    Type rawsourcetype;
    Feature rawsourcetype_type;
    Type filenametype;
    Feature filenametype_name;
    Type tokentype;
    Type tokendelimitertype;
    Feature tokentype_content;
    Feature tokendelimitertype_content;
    Type sentencetype;
    Type sentencedelimitertype;
    Feature sentencetype_content;
    Feature sentencedelimitertype_content;
    Type tpfnvhashtype;
    Feature tpfnvhashtype_content;
    Type xmltagtype;
    Feature xmltagtype_value;
    Feature xmltagtype_content;
    Feature xmltagtype_term;

    CAS * tcas;
    std::set<UnicodeString> dlsetToken;
    TpTrie * trieToken;
    std::set<UnicodeString> dlsetSentence;
    vector<string> disqSentence;
    int32_t maxfrontdisqcharlength;
    int32_t maxbackdisqcharlength;
    TpTrie * trieSentence;
    int32_t FindStartTag(const UnicodeString docstring, const UnicodeString name, int32_t pos);
    int32_t FindEndTag(const UnicodeString docstring, const UnicodeString name, int32_t pos);
};

#endif	/* TXTOKENIZER_H */
