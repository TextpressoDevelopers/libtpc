/* 
 * File:   Tpcas2Lpp.h
 * Author: mueller
 *
 * Created on February 5, 2013, 1:27 PM
 */

#ifndef TPCAS2SINGLEINDEX_H
#define	TPCAS2SINGLEINDEX_H

#include <uima/api.hpp>
#include <lucene++/targetver.h>
#include <lucene++/LuceneHeaders.h>

using namespace uima;
using namespace std;
using namespace Lucene;

class Tpcas2Bib : public Annotator {
public:
    Tpcas2Bib();
    Tpcas2Bib(const Tpcas2Bib & orig);
    virtual ~Tpcas2Bib();
    TyErrorId initialize(AnnotatorContext & rclAnnotatorContext);
    TyErrorId typeSystemInit(TypeSystem const & crTypeSystem);
    TyErrorId destroy();
    TyErrorId process(CAS & tcas, ResultSpecification const & crResultSpecification);
    

private:
    // input types and features
    vector<Type> types_;
   // map<Type, int> types;
    map< Type, vector<Feature> > features_;
    CAS *tcas;
   
//    string fulltextindexdirectory; // full clean text index
//    string tokenindexdirectory;  // token index(skipped for now)
//    string sentenceindexdirectory;  // sentence index
//    string lexicalindexdirectory; // lexicalannotation index
//    string bibindexdirectory; //bibliography index
//    
//    IndexWriterPtr fulltextwriter; //index writers
//    IndexWriterPtr tokenwriter; 
//    IndexWriterPtr sentencewriter; 
//    IndexWriterPtr lexicalwriter;  
//    IndexWriterPtr bibwriter; 
    //vector<string> excludedTypes; 
};

#endif	/* TPCAS2LPP_H */
