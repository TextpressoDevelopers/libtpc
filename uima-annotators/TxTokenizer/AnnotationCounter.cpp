/* 
 * File:   AnnotationCounter.cpp
 * Author: mueller
 * 
 * Created on October 28, 2013, 3:29 PM
 */

#include "AnnotationCounter.h"

AnnotationCounter::AnnotationCounter(uima::CAS & tcas) {
    count_ = 0;
    SetCurrentId(tcas);
}

AnnotationCounter::AnnotationCounter(const AnnotationCounter & orig) {
}

AnnotationCounter::~AnnotationCounter() {
}

void AnnotationCounter::SetCurrentId(uima::CAS & tcas) {
    const UnicodeString textpresso("textpresso");
    uima::ANIndex allannindex = tcas.getAnnotationIndex();
    uima::ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    while (aait.isValid()) {
        uima::Type currentType = aait.get().getType();
        uima::UnicodeStringRef tnameref = currentType.getName();
        bool isTextpressoAnnotation = (tnameref.indexOf(textpresso) > -1);
        int32_t pi = tnameref.lastIndexOf('.');
        if (isTextpressoAnnotation) {
            uima::Feature faid = currentType.getFeatureByBaseName("aid");
            if (faid.isValid()) {
                int iaid = aait.get().getIntValue(faid);
                count_ = (iaid > count_) ? iaid : count_;
            }
        }
        aait.moveToNext();
    }
}
