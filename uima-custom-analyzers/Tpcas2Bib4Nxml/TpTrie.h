/* 
 * File:   TpTrie.h
 * Author: mueller
 *
 * Created on February 4, 2013, 3:22 PM
 */

#ifndef TPTRIE_H
#define	TPTRIE_H

#include <vector>
#include "TpNode.h"

using namespace std;


class TpTrie {
public:
    TpTrie();
    TpTrie(const TpTrie& orig);
    virtual ~TpTrie();
    void addWord(UnicodeString s);
    vector< pair<int32_t, int32_t> > searchAllWords(UnicodeString s);
private:
    TpNode * root;

};

#endif	/* TPTRIE_H */
