/* 
 * File:   LsaTp.h
 * Author: mueller
 *
 * Created on July 2, 2014, 10:36 AM
 */

#ifndef LSATP_H
#define	LSATP_H

#include "redsvd.hpp"
#include "SetsFromPairs.h"

class LsaTp {
    typedef std::map<int, int> wordcount_t;

    enum localweight {
        lbinary, termfrequency, log
    };

    enum globalweight {
        gbinary, normal, gfidf, idf, entropy
    };
public:
    LsaTp();

    void SetPrefix(std::string s) {
        prefix_ = s;
    }

    std::string GetPrefix() {
        return prefix_;
    }
    void AddWordCount2Wcs(std::string docname, std::map<std::string, int> & tokencount);
    void CalculateFVS();
    void CalculateA();
    void DoLsa(const int r);
    void ClearFile(std::string extension);
    FILE * Open2WriteWcs();
    void WriteLineWcs(FILE * outfp, wordcount_t * pwc);
    void CloseFile(FILE * outfp);
    void WriteSVD(bool u, bool s, bool v);
    void WriteWord2Id();
    void WriteDoc2Id();
    void WriteAll(bool u, bool s, bool v, float scalarproductthreshold);
    void WriteScalarProducts2File(float threshold, bool relative);
    void ReadWcs();
    void ReadWord2Id();
    void ReadDoc2Id();
    void ReadExclusionIds();
    void ReadScalarProductsAndIdentifySets(float t);
    void ReadScalarProductsAndIdentifySimilars(float t);
    void PrintScalarProductsWithOutsiders(std::string filelist);
private:
    std::string prefix_;
    SetsFromPairs setsfrompairs_;
    FILE * OpenFile(std::string extension, const char * mode);
    REDSVD::SMatrixXf A_;
    void ConvertFromFvs2A();
    REDSVD::RedSVD svdOfA_;
    std::map<std::string, int> word2id_;
    std::map<std::string, int> doc2id_;
    std::map<int, float> globalweight_;
    std::set<int> exclusionid_;
    int getId(const std::string & str);
    int getDocId(const std::string & str);
    float LocalWeight(int count, localweight w);
    void PopulateGlobalWeights(globalweight w);
    void PopulateBinaryGlobalWeights();
    void PopulateNormGlobalWeights();
    void PopulatFiDfGlobalWeights();
    void PopulateIdfGlobalWeights();
    void PopulateEntropyGlobalWeights();
    float ScalarProduct(int i, int j, const REDSVD::RedSVD & svdOfA);
    void PrintSets();
    void PrintPair(FILE * outfp, int i, int j);
};

#endif	/* LSATP_H */
