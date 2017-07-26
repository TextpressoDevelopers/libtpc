/* 
 * File:   CASutils.h
 * Author: lyl
 *
 * Created on November 15, 2013, 3:48 PM
 */

#ifndef CASUTILS_H
#define	CASUTILS_H

#include <uima/api.hpp>

extern std::string getFilename(uima::CAS& tcas); // get filename
extern std::string getXMLstring(uima::CAS & tcas); // get xml from tpcas

#endif	/* CASUTILS_H */

