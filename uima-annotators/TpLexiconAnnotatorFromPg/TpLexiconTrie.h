/* 
 * File:   TpLexiconTrie.h
 * Author: mueller
 *
 * Created on February 6, 2013, 10:21 AM
 */

#ifndef TPLEXICONTRIE_H
#define	TPLEXICONTRIE_H

#include <vector>
#include "TpLexiconNode.h"
#include "../TpTokenizer/TpTrie.h"

using namespace std;

typedef pair<pair<int32_t, int32_t>, UnicodeString> ppiiU;

class TpLexiconTrie {
public:
    TpLexiconTrie();
    TpLexiconTrie(const TpLexiconTrie & orig);
    virtual ~TpLexiconTrie();
    void addWord(UnicodeString s, UnicodeString annotation);
    vector<ppiiU> searchAllWords(UnicodeString s);
private:
    TpLexiconNode * root;
    std::set<UnicodeString> dlsetToken;
    TpTrie * trieToken;
    long unsigned int idcounter_;
    std::map<UnicodeString, long unsigned int> a2i_;
    std::vector<UnicodeString> i2a_;
    long unsigned int annotation2id(UnicodeString annotation);
    UnicodeString id2annotation(long unsigned int i) { return i2a_[i]; }
};

#endif	/* TPLEXICONTRIE_H */
