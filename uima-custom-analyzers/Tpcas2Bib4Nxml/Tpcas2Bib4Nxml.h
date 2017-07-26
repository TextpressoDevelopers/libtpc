/* 
 * File:   Tpcas2Bib4Nxml.h
 * Author: mueller
 *
 * Created on October 26, 2016, 12:05 PM
 */

#ifndef TPCAS2BIB4NXML_H
#define	TPCAS2BIB4NXML_H

#include <uima/api.hpp>

class Tpcas2Bib4Nxml : public uima::Annotator {
public:
    Tpcas2Bib4Nxml();
    Tpcas2Bib4Nxml(const Tpcas2Bib4Nxml& orig);
    virtual ~Tpcas2Bib4Nxml();
    uima::TyErrorId initialize(uima::AnnotatorContext & rclAnnotatorContext);
    uima::TyErrorId typeSystemInit(uima::TypeSystem const & crTypeSystem);
    uima::TyErrorId destroy();
    uima::TyErrorId process(uima::CAS & tcas, uima::ResultSpecification const & crResultSpecification);
private:
};

#endif	/* TPCAS2BIB4NXML_H */
