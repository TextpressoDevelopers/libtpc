/* 
 * File:   TpCas2LsaToken.h
 * Author: mueller
 *
 * Created on June 27, 2014, 2:40 PM
 */

#ifndef TPCAS2LSATOKEN_H
#define	TPCAS2LSATOKEN_H

#define TPCAS2TPCENTRALDESCRIPTOR "/usr/local/uima_descriptors/Tpcas2TpCentral.xml"

#include <map>
#include <string>

class TpCas2LsaToken {
public:
    TpCas2LsaToken(std::string tpcasfilename);
    std::map<std::string, int> & TokenCount() { return tokencount_; }
    void WriteTokenCount2Ostream(std::ostream & sout);
private:
    void LoadStopwords(const char * fn);
    std::string Normalize(const std::string & word);
    std::map<std::string, int> tokencount_;
    std::map<std::string, bool> stopwords_;
};

#endif	/* TPCAS2LSATOKEN_H */

