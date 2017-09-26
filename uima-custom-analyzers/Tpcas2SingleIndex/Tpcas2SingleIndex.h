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
#include "../../lucene-custom/CaseSensitiveAnalyzer.h"
#include "../../CASManager.h"

using namespace uima;
using namespace std;
using namespace Lucene;

struct LexMapping {
    wstring term;
    long begin;
    long end;
    set<wstring> categories;

    bool operator < (const LexMapping& mapping) const {
        return (begin < mapping.begin);
    }
};

class Tpcas2SingleIndex : public Annotator {
public:
    Tpcas2SingleIndex();
    Tpcas2SingleIndex(const Tpcas2SingleIndex & orig);
    virtual ~Tpcas2SingleIndex();
    TyErrorId initialize(AnnotatorContext & rclAnnotatorContext);
    TyErrorId typeSystemInit(TypeSystem const & crTypeSystem);
    TyErrorId destroy();
    TyErrorId process(CAS & tcas, ResultSpecification const & crResultSpecification);
    vector<String> GetBib(string fullfilename);
    static wstring RemoveTags(wstring w_cleantext);
    

private:
    // input types and features
    vector<Type> types_;
    map< Type, vector<Feature> > features_;
    CAS *tcas;
   
    string fulltextindexdirectory; // full clean text index
    string tokenindexdirectory;  // token index(skipped for now)
    string sentenceindexdirectory;  // sentence index

    string fulltextindexdirectory_casesens;
    string tokenindexdirectory_casesens;
    string sentenceindexdirectory_casesens;
    string lexicalindexdirectory_casesens;

    string tempDir;
    
    IndexWriterPtr fulltextwriter; //index writers
    IndexWriterPtr sentencewriter; 
    IndexWriterPtr fulltextwriter_casesens; //index writers
    IndexWriterPtr sentencewriter_casesens;

    std::string root_dir;
};

#endif	/* TPCAS2LPP_H */
