/* 
 * File:   Utils.h
 * Author: lyl
 *
 * Created on November 15, 2013, 3:48 PM
 */

#ifndef UTILS_H
#define	UTILS_H

#include <iostream>
#include <uima/api.hpp>
#include "targetver.h"
#include <lucene++/LuceneHeaders.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

using namespace std;
using namespace uima;
using namespace Lucene;


extern const char* newindexflag;  //new index lock/falg
#endif	/* UTILS_H */


extern string uncompressGzip(string gzFile); // uncompress gz file


