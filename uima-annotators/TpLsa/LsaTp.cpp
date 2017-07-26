/* 
 * File:   LsaTp.cpp
 * Author: mueller
 * 
 * Created on July 2, 2014, 10:36 AM
 */

#include "LsaTp.h"
#include <sys/time.h>
#include "redsvdFile.hpp"
namespace {

    double getSec() {
        timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec + (double) tv.tv_usec * 1e-6;
    }

    void writeMatrix_(const std::string & fn, const Eigen::MatrixXf & M) {
        FILE* outfp = fopen(fn.c_str(), "wb");
        if (outfp == NULL) {
            throw std::string("cannot open ") + fn;
        }

        for (int i = 0; i < M.rows(); ++i) {
            for (int j = 0; j < M.cols(); ++j) {
                fprintf(outfp, "%+f ", M(i, j));
            }
            fprintf(outfp, "\n");
        }

        fclose(outfp);
    }

    void writeVector_(const std::string& fn, const Eigen::VectorXf& V) {
        FILE* outfp = fopen(fn.c_str(), "wb");
        if (outfp == NULL) {
            throw std::string("cannot open ") + fn;
        }

        for (int i = 0; i < V.rows(); ++i) {
            fprintf(outfp, "%+f\n", V(i));
        }
        fclose(outfp);
    }

    void ReadX2Id(std::string fn, std::map<std::string, int> & map) {
        std::ifstream ifs(fn.c_str());
        if (!ifs) {
            throw std::string("cannot open ") + fn;
        }
        for (std::string line; getline(ifs, line);) {
            std::stringstream ss(line);
            std::string s;
            int i;
            while (ss >> s) {
                ss >> i;
                map[s] = i;
            }
        }
        ifs.close();
    }

}

LsaTp::LsaTp() {
    setsfrompairs_.ClearPairs();
}

int LsaTp::getId(const std::string & str) {
    std::map<std::string, int>::const_iterator it = word2id_.find(str);
    if (it == word2id_.end()) {
        int newId = (int) word2id_.size();
        word2id_[str] = newId;
        return newId;
    } else {
        return it->second;
    }
}

void LsaTp::AddWordCount2Wcs(std::string docname, std::map<std::string, int> & tokencount) {
    std::map<std::string, int>::iterator it;
    wordcount_t * pwc = new wordcount_t;
    for (it = tokencount.begin(); it != tokencount.end(); it++) {
        (*pwc)[getId(it->first)] += it->second;
    }
    FILE * of = Open2WriteWcs();
    WriteLineWcs(of, pwc);
    CloseFile(of);
    getDocId(docname);
    delete pwc;
}

void LsaTp::CalculateFVS() {
    PopulateGlobalWeights(entropy);
    FILE * of = OpenFile("FVS", "wb");
    std::string saux = prefix_ + ".WCS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".WCS";
    }
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        while (ss >> i) {
            ss >> j;
            float fij = LocalWeight(j, log) * globalweight_[i];
            fprintf(of, "%u %f ", i, fij);
        }
        fprintf(of, "\n");
    }
    ifs.close();
    CloseFile(of);
}

void LsaTp::CalculateA() {
    ConvertFromFvs2A();
}

void LsaTp::ConvertFromFvs2A() {
    int maxID = 0;
    size_t nonZeroNum = 0;
    std::string saux = prefix_ + ".FVS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".FVS";
    }
    int rowcount = 0;
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i;
        float f;
        int countcol = 0;
        while (ss >> i) {
            ss >> f;
            countcol++;
            maxID = std::max(i + 1, maxID);
        }
        nonZeroNum += countcol;
        rowcount++;
    }
    ifs.close();
    A_.resize(rowcount, maxID);
    A_.reserve(nonZeroNum);

    std::ifstream ifs2(saux.c_str());
    if (!ifs2) {
        throw std::string("cannot open ") + prefix_ + ".FVS";
    }
    rowcount = 0;
    for (std::string line; getline(ifs2, line);) {
        A_.startVec(rowcount);
        std::stringstream ss(line);
        int i;
        float f;
        while (ss >> i) {
            ss >> f;
            if (exclusionid_.find(rowcount) != exclusionid_.end())
                A_.insertBack(rowcount, i) = 0.f;
            else
                A_.insertBack(rowcount, i) = f;
        }
        rowcount++;
    }
    ifs2.close();
}

float LsaTp::LocalWeight(int count, localweight w) {
    switch (w) {
        case lbinary:
            return (count > 0) ? 1.f : 0.f;
            break;
        case termfrequency:
            return float(count);
            break;
        case log:
            return logf(float(count + 1));
            break;
        default:
            return (count > 0) ? 1.f : 0.f;
            break;
    }
}

void LsaTp::PopulateGlobalWeights(globalweight w) {
    switch (w) {
        case gbinary:
            PopulateBinaryGlobalWeights();
            break;
        case normal:
            PopulateNormGlobalWeights();
            break;
        case gfidf:
            PopulatFiDfGlobalWeights();
            break;
        case idf:
            PopulateIdfGlobalWeights();
            break;
        case entropy:
            PopulateEntropyGlobalWeights();
            break;
        default:
            PopulateBinaryGlobalWeights();
            break;
    }
}

void LsaTp::PopulateBinaryGlobalWeights() {
    std::string saux = prefix_ + ".WCS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".WCS";
    }
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        while (ss >> i) {
            ss >> j;
            globalweight_[i] = 1.f;
        }
    }
    ifs.close();
}

void LsaTp::PopulateNormGlobalWeights() {
    std::string saux = prefix_ + ".WCS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".WCS";
    }
    std::map<int, long> sum2;
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        while (ss >> i) {
            ss >> j;
            if (i == 7031) std::cout << j << std::endl;

            sum2[i] += j*j;
        }
    }
    ifs.close();
    std::map<int, long>::iterator it;
    for (it = sum2.begin(); it != sum2.end(); it++) {
        globalweight_[it->first] = 1.f / sqrt(float(it->second));
    }
}

void LsaTp::PopulatFiDfGlobalWeights() {
    std::string saux = prefix_ + ".WCS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".WCS";
    }
    std::map<int, long> gfi;
    std::map<int, long> dfi;
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        while (ss >> i) {
            ss >> j;
            gfi[i] += j;
            if (j > 0) dfi[i]++;
        }
    }
    ifs.close();
    std::map<int, long>::iterator it;
    for (it = dfi.begin(); it != dfi.end(); it++) {
        globalweight_[it->first] = gfi[it->first] / float(it->second);
    }
}

void LsaTp::PopulateIdfGlobalWeights() {
    std::string saux = prefix_ + ".WCS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".WCS";
    }
    std::map<int, long> dfi;
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        while (ss >> i) {
            ss >> j;
            if (j > 0) dfi[i]++;
        }
    }
    ifs.close();

    std::map<int, long>::iterator it;
    float nf = float(dfi.size());
    for (it = dfi.begin(); it != dfi.end(); it++) {
        globalweight_[it->first] = log2f(nf / (1.f + float(it->second)));
    }
}

void LsaTp::PopulateEntropyGlobalWeights() {
    std::string saux = prefix_ + ".WCS";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".WCS";
    }
    std::map<int, long> gfi;
    std::map<int, float> sum1;
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        while (ss >> i) {
            ss >> j;
            gfi[i] += j;
            if (j > 0) {
                sum1[i] += float(j) * logf(float(j));
            }
        }
    }
    ifs.close();
    float lognf = logf(gfi.size());

    std::map<int, long>::iterator it;
    for (it = gfi.begin(); it != gfi.end(); it++) {
        float aux = float(it->second);
        globalweight_[it->first] = 1.f - (sum1[it->first] - logf(aux) * aux)
                / lognf / aux;
    }
}

void LsaTp::DoLsa(const int r) {
    svdOfA_ = REDSVD::RedSVD(A_, r);
}

int LsaTp::getDocId(const std::string & str) {
    std::map<std::string, int>::const_iterator it = doc2id_.find(str);
    if (it == doc2id_.end()) {
        int newId = (int) doc2id_.size();
        doc2id_[str] = newId;
        //id2doc_[newId] = str;
        return newId;
    } else {
        return it->second;
    }
}

void LsaTp::ClearFile(std::string extension) {
    FILE * outfp = fopen((prefix_ + "." + extension).c_str(), "wb");
    if (outfp == NULL) {
        throw std::string("cannot open ") + prefix_ + "." + extension;
    }
    fclose(outfp);
}

FILE * LsaTp::OpenFile(std::string extension, const char * mode) {
    FILE * outfp = fopen((prefix_ + "." + extension).c_str(), mode);
    if (outfp == NULL) {
        throw std::string("cannot open ") + prefix_ + "." + extension;
    }
    return outfp;
}

FILE * LsaTp::Open2WriteWcs() {
    FILE * outfp = OpenFile("WCS", "ab");
    return outfp;
}

void LsaTp::WriteLineWcs(FILE * outfp, wordcount_t * pwc) {
    std::map<int, int>::iterator it2;
    for (it2 = (*pwc).begin(); it2 != (*pwc).end(); it2++)
        fprintf(outfp, "%u %u ", it2->first, it2->second);
    fprintf(outfp, "\n");
}

void LsaTp::CloseFile(FILE * outfp) {
    fclose(outfp);
}

void LsaTp::WriteSVD(bool u, bool s, bool v) {
    if (u) writeMatrix_(prefix_ + ".U", svdOfA_.matrixU());
    if (s) writeVector_(prefix_ + ".S", svdOfA_.singularValues());
    if (v) writeMatrix_(prefix_ + ".V", svdOfA_.matrixV());
}

void LsaTp::WriteWord2Id() {
    std::filebuf fb;
    fb.open((prefix_ + ".WD2ID").c_str(), std::ios::out);
    std::ostream os(&fb);
    std::map<std::string, int>::iterator it;
    for (it = word2id_.begin(); it != word2id_.end(); it++) {
        os << it->first << "\t" << it->second << std::endl;
    }
    fb.close();
}

void LsaTp::WriteDoc2Id() {
    std::filebuf fb;
    fb.open((prefix_ + ".DOC2ID").c_str(), std::ios::out);
    std::ostream os(&fb);
    std::map<std::string, int>::iterator it;
    for (it = doc2id_.begin(); it != doc2id_.end(); it++) {
        os << it->first << "\t" << it->second << std::endl;
    }
    fb.close();
}

void LsaTp::WriteAll(bool u, bool s, bool v, float scalarproductthreshold) {
    WriteScalarProducts2File(scalarproductthreshold, true);
    WriteSVD(u, s, v);
    WriteDoc2Id();
    WriteWord2Id();
}

void LsaTp::ReadWcs() {
}

void LsaTp::ReadWord2Id() {
    ReadX2Id(prefix_ + ".WD2ID", word2id_);
}

void LsaTp::ReadDoc2Id() {
    ReadX2Id(prefix_ + ".DOC2ID", doc2id_);
}

void LsaTp::ReadExclusionIds() {
    if (doc2id_.empty()) ReadDoc2Id();
    std::string fn = prefix_ + ".EXC";
    std::ifstream ifs(fn.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + fn;
    }
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        std::string s;
        while (ss >> s) {
            if (doc2id_.find(s) != doc2id_.end())
                exclusionid_.insert(doc2id_[s]);
        }
    }
    ifs.close();
}

float LsaTp::ScalarProduct(int i, int j, const REDSVD::RedSVD & svdOfA) {
    float sc = 0.0;
    float nsq1 = 0.0;
    float nsq2 = 0.0;
    for (int k = 0; k < svdOfA.matrixU().cols(); ++k) {
        float x1 = svdOfA.matrixU()(i, k);
        float x2 = svdOfA.matrixU()(j, k);
        nsq1 += x1*x1;
        nsq2 += x2*x2;
        sc += x1*x2;
    }
    if (nsq1 * nsq2 > 0.) sc /= sqrt(nsq1 * nsq2);
    return sc;
}

void LsaTp::WriteScalarProducts2File(float threshold, bool relative) {
    float ave = 0.0f;
    float sigma = 1.0f;
    if (relative) {
        float sum, sum2;
        int ntotal = 0;
        for (int i = 0; i < svdOfA_.matrixU().rows(); ++i)
            for (int j = i + 1; j < svdOfA_.matrixU().rows(); ++j) {
                float sc = ScalarProduct(i, j, svdOfA_);
                sum += sc;
                sum2 += sc*sc;
                ntotal++;
            }
        float ave = float(sum) / float(ntotal);
        float sigma = sqrt(float(sum2) / float(ntotal) - float(ave) * float(ave));
        std::cout << "A/S: " << ave << " " << sigma << std::endl;
    }
    FILE * outfp = OpenFile("SCA", "wb");
    for (int i = 0; i < svdOfA_.matrixU().rows(); ++i)
        for (int j = i + 1; j < svdOfA_.matrixU().rows(); ++j) {
            float sc = 0.0f;
            if (relative) {
                if (sigma > 0.f)
                    sc = (ScalarProduct(i, j, svdOfA_) - ave) / sigma;
            } else {
                sc = ScalarProduct(i, j, svdOfA_);
            }
            if (sc > threshold) {
                //setsfrompairs_.AddPair(i, j);
                fprintf(outfp, "%u\t%u\t%f\n", i, j, sc);
            }
        }
    CloseFile(outfp);
}

void LsaTp::PrintSets() {
    if (doc2id_.empty()) ReadDoc2Id();
    std::map<int, std::string> id2doc;
    for (std::map<std::string, int>::iterator it = doc2id_.begin(); it != doc2id_.end(); it++) {
        id2doc.insert(std::make_pair((*it).second, (*it).first));
    }
    int count = 0;
    std::vector< std::set<int>*>::iterator vit;
    for (vit = setsfrompairs_.Sets().begin(); vit != setsfrompairs_.Sets().end(); vit++) {
        std::stringstream saux;
        saux << count++;
        std::string setfile = "SET." + saux.str();
        FILE * outfp2 = OpenFile(setfile, "wb");
        for (std::set<int>::iterator it = (*(*vit)).begin(); it != (*(*vit)).end(); it++)
            fprintf(outfp2, "%s\n", id2doc[*it].c_str());
        CloseFile(outfp2);
    }
}

void LsaTp::ReadScalarProductsAndIdentifySets(float t) {
    setsfrompairs_.ClearPairs();
    setsfrompairs_.ClearSets();
    std::string saux = prefix_ + ".SCA";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".SCA";
    }
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        float f;
        while (ss >> i) {
            ss >> j;
            ss >> f;
            if (f >= t)
                setsfrompairs_.AddPair(i, j);
        }
    }
    ifs.close();
    setsfrompairs_.IdentifySets();
    if (!setsfrompairs_.Sets().empty()) PrintSets();
}

void LsaTp::ReadScalarProductsAndIdentifySimilars(float t) {
    if (doc2id_.empty()) ReadDoc2Id();
    std::string saux = prefix_ + ".SCA";
    std::ifstream ifs(saux.c_str());
    if (!ifs) {
        throw std::string("cannot open ") + prefix_ + ".SCA";
    }
    std::string setfile = "SIM";
    FILE * outfp = OpenFile(setfile, "wb");
    for (std::string line; getline(ifs, line);) {
        std::stringstream ss(line);
        int i, j;
        float f;
        while (ss >> i) {
            ss >> j;
            ss >> f;
            if (f >= t) PrintPair(outfp, i, j);
        }
    }
    CloseFile(outfp);
    ifs.close();
}

void LsaTp::PrintPair(FILE * outfp, int i, int j) {
    std::map<std::string, int>::iterator it;
    for (it = doc2id_.begin(); it != doc2id_.end(); it++) {
        if ((*it).second == i) fprintf(outfp, "%s\t", (*it).first.c_str());
        if ((*it).second == j) fprintf(outfp, "%s\t", (*it).first.c_str());
    }
    fprintf(outfp, "\n");
}

void LsaTp::PrintScalarProductsWithOutsiders(std::string filelist) {
    if (word2id_.empty()) ReadWord2Id();
    // Create local LsaTP
    // read in tpcas (see main), 
    // load WCS(local) and FVS(local) and save, but only for words that are in 
    // this LsaTp using this word2id
    // calculate A(local) from FVS(local)
    // create D(local) =  A(local)*T(this)*S^(-1)(this)
    // calculate and print scalarproducts(relative = false) of D(local) and
    // D(this) vectors.
}