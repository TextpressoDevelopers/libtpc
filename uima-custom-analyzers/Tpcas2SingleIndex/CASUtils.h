/* 
 * File:   CASutils.h
 * Author: lyl
 *
 * Created on November 15, 2013, 3:48 PM
 */

#ifndef CASUTILS_H
#define	CASUTILS_H

#include <iostream>
#include <uima/api.hpp>
#include <lucene++/targetver.h>
#include <lucene++/LuceneHeaders.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/algorithm/string.hpp>
#include "Tpcas2SingleIndex.h"

std::wstring getCleanText(CAS& tcas);
std::wstring getFulltext(CAS& tcas);// get full texto(not clean)
std::string getCASType(uima::CAS & tcas); // get CAS file type(PDF or NXML)
std::string gettpfnvHash(uima::CAS& tcas); // get hash value
std::string getFilename(uima::CAS& tcas);
std::string getXMLstring(uima::CAS & tcas); // get xml from tpcas

#endif	/* CASUTILS_H */