/* 
 * File:   TpTokenizer.h
 * Author: mueller
 *
 * Created on February 4, 2013, 3:15 PM
 */

#ifndef TPTOKENIZER_H
#define	TPTOKENIZER_H

#include <set>
#include <uima/api.hpp>
#include "TpTrie.h"

using namespace uima;

class TpTokenizer : public Annotator {
public:
    TpTokenizer();
    TpTokenizer(const TpTokenizer & orig);
    virtual ~TpTokenizer();
    TyErrorId initialize(AnnotatorContext & rclAnnotatorContext);
    TyErrorId typeSystemInit(TypeSystem const & crTypeSystem);
    TyErrorId destroy();
    TyErrorId process(CAS & tcas, ResultSpecification const & crResultSpecification);
private:
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
    Type pdftagtype;
    Feature pdftagtype_tagtype;
    Feature pdftagtype_value;
    
    CAS * tcas;
    std::set<UnicodeString> dlsetToken;
    TpTrie * trieToken;
    std::set<UnicodeString> dlsetSentence;
    vector<string> disqSentence;
    int32_t maxfrontdisqcharlength;
    int32_t maxbackdisqcharlength;
    TpTrie * trieSentence;
    TpTrie * triePdfTags;
    std::set<UnicodeString> PdfTags;
};

#endif	/* TPTOKENIZER_H */
