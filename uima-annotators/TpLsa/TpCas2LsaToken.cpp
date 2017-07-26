/* 
 * File:   TpCas2LsaToken.cpp
 * Author: mueller
 * 
 * Created on June 27, 2014, 2:40 PM
 */

#include "TpCas2LsaToken.h"

#include "xercesc/util/XMLString.hpp"
#include <uima/api.hpp>
#include "uima/xmideserializer.hpp"

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/filesystem.hpp>

enum rawsourcetype {
    unknown, nxml, pdf
};

namespace {

    std::string uncompressGzip2(std::string gzFile) {
        std::ifstream filein(gzFile.c_str(), std::ios_base::in | std::ios_base::binary);
        boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
        in.push(boost::iostreams::gzip_decompressor());
        in.push(filein);
        char tmpname[L_tmpnam];
        char * pDummy = tmpnam(tmpname);
        std::string tmpfile(tmpname);
        while (boost::filesystem::exists(tmpfile)) {
            char * pDummy = tmpnam(tmpname);
            tmpfile = std::string(tmpname);
        }
        std::ofstream out(tmpfile.c_str());
        boost::iostreams::copy(in, out);
        out.close();
        return tmpfile;
    }

    //[ Uima related

    uima::AnalysisEngine * CreateUimaEngine(const char * descriptor) {
        uima::ErrorInfo errorInfo;
        uima::AnalysisEngine * ret = uima::Framework::createAnalysisEngine(descriptor, errorInfo);
        if (errorInfo.getErrorId() != UIMA_ERR_NONE) {
            std::cerr << std::endl
                    << "  Error string  : "
                    << uima::AnalysisEngine::getErrorIdAsCString(errorInfo.getErrorId())
                    << std::endl
                    << "  UIMACPP Error info:" << std::endl
                    << errorInfo << std::endl;
        }
        return ret;
    }

    uima::CAS * GetCas(const char * pszInputFile, uima::AnalysisEngine * pEngine) {
        uima::CAS * ret = pEngine->newCAS();
        if (ret == NULL) {
            std::cerr << "pEngine_->newCAS() failed." << std::endl;
        } else {
            try {
                /* initialize from an xmicas */
                XMLCh * native = XMLString::transcode(pszInputFile);
                LocalFileInputSource fileIS(native);
                XMLString::release(&native);
                uima::XmiDeserializer::deserialize(fileIS, * ret, true);
            } catch (uima::Exception e) {
                uima::ErrorInfo errInfo = e.getErrorInfo();
                std::cerr << "Error " << errInfo.getErrorId() << " " << errInfo.getMessage() << std::endl;
                std::cerr << errInfo << std::endl;
            }
        }
        return ret;
    }
    //] Uima related


}

TpCas2LsaToken::TpCas2LsaToken(std::string tpcasfilename) {
    (void) uima::ResourceManager::createInstance("TPCAS2TPCENTRALAE");
    //time_t now = time(0);
    uima::AnalysisEngine * pEngine = CreateUimaEngine(TPCAS2TPCENTRALDESCRIPTOR);
    std::string tmpfl = uncompressGzip2(tpcasfilename);
    uima::CAS * pcas = GetCas(tmpfl.c_str(), pEngine);
    boost::filesystem::remove(tmpfl);
    rawsourcetype rawsource = unknown;
    LoadStopwords("/home/mueller/NetBeansProjects/TpUimaAnnotators/TpLsa/resources/stopwords");
    uima::Type rawtype = pcas->getTypeSystem().getType(UnicodeString("org.apache.uima.textpresso.rawsource"));
    uima::ANIndex allannindex = pcas->getAnnotationIndex(rawtype);
    //    uima::ANIndex allannindex = pcas->getAnnotationIndex();
    uima::ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    while (aait.isValid()) {
        //[1
        //        uima::Type currentType = aait.get().getType();
        //        uima::UnicodeStringRef tnameref = currentType.getName();
        //        bool isTextpressoAnnotation = (tnameref.indexOf(UnicodeString("textpresso")) > -1);
        //]1
        //        if (isTextpressoAnnotation) {
        //[2
        //            if (tnameref.compare("org.apache.uima.textpresso.rawsource") == 0) {
        uima::Feature fvalue = aait.get().getType().getFeatureByBaseName("value");
        if (fvalue.isValid()) {
            uima::UnicodeStringRef uvalue = aait.get().getStringValue(fvalue);
            if (uvalue.compare("nxml") == 0)
                rawsource = nxml;
            else if (uvalue.compare("pdf") == 0)
                rawsource = pdf;
        }
        //           }
        //]2
        //       }
        aait.moveToNext();
    }
    if (rawsource == nxml) {
        //[3
        uima::Type xmltype = pcas->getTypeSystem().getType(UnicodeString("org.apache.uima.textpresso.xmltag"));
        uima::ANIndex allannindex2 = pcas->getAnnotationIndex(xmltype);
        uima::ANIterator aait2 = allannindex2.iterator();
        aait2.moveToFirst();
        while (aait2.isValid()) {
            uima::Type currentType = aait2.get().getType();
            uima::Feature f = currentType.getFeatureByBaseName("value");
            uima::UnicodeStringRef value = aait2.get().getStringValue(f);
            if (value.compare("pcdata") == 0) {
                uima::Feature f = currentType.getFeatureByBaseName("term");
                uima::UnicodeStringRef term = aait2.get().getStringValue(f);
                if (term.length() > 0) {
                    int32_t curr = 0;
                    int32_t old = 0;
                    curr = term.indexOf(' ', old);
                    while ((curr > 0) && (curr < term.length())) {
                        UnicodeString extract;
                        term.extract(old, curr - old, extract);
                        std::string nword = Normalize(uima::UnicodeStringRef(extract).asUTF8());
                        if (nword.size() != 0)
                            if (!stopwords_[nword])
                                tokencount_["token:" + nword]++;
                        old = curr + 1;
                        curr = term.indexOf(' ', old);
                    }
                    // take care of last element
                    UnicodeString extract;
                    term.extract(old, term.length() - old, extract);
                    std::string nword = Normalize(uima::UnicodeStringRef(extract).asUTF8());
                    if (nword.size() != 0)
                        if (!stopwords_[nword])
                            tokencount_["token:" + nword]++;
                }
            }
            aait2.moveToNext();
        }
        uima::Type lexanntype = pcas->getTypeSystem().getType(UnicodeString("org.apache.uima.textpresso.lexicalannotation"));
        uima::ANIndex allannindex3 = pcas->getAnnotationIndex(lexanntype);
        uima::ANIterator aait3 = allannindex3.iterator();
        aait3.moveToFirst();
        while (aait3.isValid()) {
            uima::Feature f = aait3.get().getType().getFeatureByBaseName("category");
            uima::UnicodeStringRef category = aait3.get().getStringValue(f);
            tokencount_["category:" + category.asUTF8()]++;
            aait3.moveToNext();
        }
        //]3
    } else if (rawsource == pdf) {
        //[4
        uima::ANIndex allannindex4 = pcas->getAnnotationIndex();
        uima::ANIterator aait4 = allannindex4.iterator();
        aait4.moveToFirst();
        int32_t blockeduntil = 0;
        while (aait4.isValid()) {
            uima::Type currentType = aait4.get().getType();
            uima::UnicodeStringRef tnameref = currentType.getName();
            if (tnameref.compare("org.apache.uima.textpresso.token") == 0) {
                int32_t begin = aait4.get().getBeginPosition();
                if (begin > blockeduntil) {
                    uima::Feature f = currentType.getFeatureByBaseName("content");
                    uima::UnicodeStringRef content = aait4.get().getStringValue(f);
                    std::string nword = Normalize(content.asUTF8());
                    if (nword.size() != 0)
                        if (!stopwords_[nword])
                            tokencount_["token:" + nword]++;
                }
            } else if (tnameref.compare("org.apache.uima.textpresso.pdftag") == 0) {
                blockeduntil = aait4.get().getEndPosition();
            } else if (tnameref.compare("org.apache.uima.textpresso.lexicalannotation") == 0) {

                uima::Feature f = currentType.getFeatureByBaseName("category");
                uima::UnicodeStringRef category = aait4.get().getStringValue(f);
                tokencount_["category:" + category.asUTF8()]++;
            }
            aait4.moveToNext();
        }
        //]4
    }
    delete pcas;
}

void TpCas2LsaToken::WriteTokenCount2Ostream(std::ostream & sout) {
    std::map<std::string, int>::iterator it;

    for (it = tokencount_.begin(); it != tokencount_.end(); it++)
        sout << it->first << "\t" << it->second << std::endl;
}

void TpCas2LsaToken::LoadStopwords(const char * fn) {
    std::ifstream ifs(fn);
    if (!ifs) {
        throw std::string("cannot open ") + fn;
    }
    std::string word;
    while (ifs >> word) {

        std::string nword = Normalize(word);
        stopwords_[nword] = true;
    }
    ifs.close();
}

std::string TpCas2LsaToken::Normalize(const std::string & word) {
    std::string ret;
    size_t p = 0;
    while (p < word.size() && !isalnum(word[p])) {
        ++p;
    }
    for (; p < word.size(); ++p) {
        //if (!isalnum(word[p])) break;
        if (isalnum(word[p]))
            ret += tolower(word[p]);
    }
    return ret;
}