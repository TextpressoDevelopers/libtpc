/* 
 * File:   AnnotationCounter.h
 * Author: mueller
 *
 * Created on October 28, 2013, 3:29 PM
 */

#ifndef ANNOTATIONCOUNTER_H
#define	ANNOTATIONCOUNTER_H

#include <uima/api.hpp>

class AnnotationCounter {
public:
    AnnotationCounter(uima::CAS & tcas);
    AnnotationCounter(const AnnotationCounter & orig);
    virtual ~AnnotationCounter();
    int GetNextId() { return ++count_; }
    int GetCurrentId() { return count_; }
    
private:
    int count_;
    void SetCurrentId(uima::CAS & tcas);
};

#endif	/* ANNOTATIONCOUNTER_H */

