/* 
 * File:   TpLexiconAnnotator.h
 * Author: mueller
 *
 * Created on February 6, 2013, 10:09 AM
 */

#ifndef TPLEXICONANNOTATOR_H
#define	TPLEXICONANNOTATOR_H

#include <uima/api.hpp>
#include "TpLexiconTrie.h"
#include "StopWords.h"

using namespace uima;

class TpLexiconAnnotator : public Annotator {
private:
  Type lexanntype;
  Feature term;
  Feature category;
  CAS *tcas;
  string locallexiconfile;
  UnicodeString cat_string;
  TpLexiconTrie * trie;
  StopWords * s;

public:
  TpLexiconAnnotator(void);
  ~TpLexiconAnnotator(void);
  TyErrorId initialize(AnnotatorContext & rclAnnotatorContext); 
  TyErrorId typeSystemInit(TypeSystem const & crTypeSystem);
  TyErrorId destroy();
  TyErrorId process(CAS & tcas, ResultSpecification const & crResultSpecification);
};

#endif	/* TPLEXICONANNOTATOR_H */
