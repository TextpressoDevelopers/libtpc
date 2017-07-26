/* 
 * File:   TpLexiconAnnotatorFromPg.h
 * Author: mueller
 *
 * Created on October 10, 2013, 2:36 PM
 */

#ifndef TPLEXICONANNOTATORFROMPG_H
#define	TPLEXICONANNOTATORFROMPG_H

#include <uima/api.hpp>
#include "TpLexiconTrie.h"
#include "StopWords.h"

class TpLexiconAnnotatorFromPg : public uima::Annotator {
public:
  TpLexiconAnnotatorFromPg(void);
  ~TpLexiconAnnotatorFromPg(void);
  uima::TyErrorId initialize(uima::AnnotatorContext & rclAnnotatorContext); 
  uima::TyErrorId typeSystemInit(uima::TypeSystem const & crTypeSystem);
  uima::TyErrorId destroy();
  uima::TyErrorId process(uima::CAS & tcas, uima::ResultSpecification const & crResultSpecification);
private:
  uima::Type lexanntype_;
  uima::Feature term_;
  uima::Feature category_;
  uima::CAS *tcas_;
  string lexicontablename_;
  UnicodeString cat_string_;
  TpLexiconTrie * trie_;
  StopWords * s_;

};

#endif	/* TPLEXICONANNOTATORFROMPG_H */
