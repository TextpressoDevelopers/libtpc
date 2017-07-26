/* 
 * File:   Tpcas2TpCentral.cpp
 * Author: mueller
 * 
 * Created on February 13, 2013, 4:41 PM
 */

#include "Tpcas2TpCentral.h"

Tpcas2TpCentral::Tpcas2TpCentral() {
}

Tpcas2TpCentral::Tpcas2TpCentral(const Tpcas2TpCentral& orig) {
}

Tpcas2TpCentral::~Tpcas2TpCentral() {
}

TyErrorId Tpcas2TpCentral::initialize(AnnotatorContext & rclAnnotatorContext) {
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2TpCentral::typeSystemInit(TypeSystem const & crTypeSystem) {
    return (TyErrorId) UIMA_ERR_NONE;
}
        
TyErrorId Tpcas2TpCentral::destroy() {
    return (TyErrorId) UIMA_ERR_NONE;
}

TyErrorId Tpcas2TpCentral::process(CAS & tcas, ResultSpecification const & crResultSpecification) {
    return (TyErrorId) UIMA_ERR_NONE;
}

MAKE_AE(Tpcas2TpCentral);


