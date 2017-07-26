/* 
 * File:   Tpcas2TpCentral.h
 * Author: mueller
 *
 * Created on February 13, 2013, 4:41 PM
 */

#ifndef TPCAS2TPCENTRAL_H
#define	TPCAS2TPCENTRAL_H

#include <uima/api.hpp>

using namespace uima;

class Tpcas2TpCentral : public Annotator {
public:
    Tpcas2TpCentral();
    Tpcas2TpCentral(const Tpcas2TpCentral & orig);
    virtual ~Tpcas2TpCentral();
    TyErrorId initialize(AnnotatorContext & rclAnnotatorContext);
    TyErrorId typeSystemInit(TypeSystem const & crTypeSystem);
    TyErrorId destroy();
    TyErrorId process(CAS & tcas, ResultSpecification const & crResultSpecification);
    
private:

};

#endif	/* TPCAS2TPCENTRAL_H */

