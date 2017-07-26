/* 
 * File:   Tpcas2Lpp.h
 * Author: mueller
 *
 * Created on February 5, 2013, 1:27 PM
 */

#ifndef TPCAS2LPP_H
#define	TPCAS2LPP_H

#include <uima/api.hpp>
#include <lucene++/targetver.h>
#include <lucene++/LuceneHeaders.h>

using namespace uima;
using namespace std;
using namespace Lucene;

class Tpcas2Lpp : public Annotator {
public:
    Tpcas2Lpp();
    Tpcas2Lpp(const Tpcas2Lpp & orig);
    virtual ~Tpcas2Lpp();
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
   
    string fulltextindexdirectory; // full clean text index
    string tokenindexdirectory;  // token index(skipped for now)
    string sentenceindexdirectory;  // sentence index
    string lexicalindexdirectory; // lexicalannotation index
    string bibindexdirectory; //bibliography index

    string fulltextindexdirectory_casesens;
    string tokenindexdirectory_casesens;
    string sentenceindexdirectory_casesens;
    string lexicalindexdirectory_casesens;
    string bibindexdirectory_casesens;

    IndexWriterPtr fulltextwriter; //index writers
    IndexWriterPtr tokenwriter; 
    IndexWriterPtr sentencewriter; 
    IndexWriterPtr lexicalwriter;  
    IndexWriterPtr bibwriter;

    IndexWriterPtr fulltextwriter_casesens; //index writers
    IndexWriterPtr tokenwriter_casesens;
    IndexWriterPtr sentencewriter_casesens;
    IndexWriterPtr lexicalwriter_casesens;
    IndexWriterPtr bibwriter_casesens;
    //vector<string> excludedTypes; 
};

#endif	/* TPCAS2LPP_H */
