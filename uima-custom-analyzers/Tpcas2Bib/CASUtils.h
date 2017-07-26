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

using namespace std;
using namespace uima;
using namespace Lucene;


#endif	/* CASUTILS_H */

extern wstring getCleanText(CAS& tcas); //get clean text
extern wstring getFulltext(CAS& tcas);// get full texto(not clean)
extern string getCASType(CAS & tcas); // get CAS file type(PDF or NXML)
extern string gettpfnvHash(CAS& tcas); // get hash value
extern string getFilename(CAS& tcas); // get filename
extern string getXMLstring(CAS & tcas); // get xml from tpcas
//extern string getTempDir();// generate a temp dir under /run/shm to store all temp files for each run. using year+month+day+min