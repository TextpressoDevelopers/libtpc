
/* 
 * File:   TpTrie.cpp
 * Author: mueller
 * 
 * Created on February 4, 2013, 3:22 PM
 */

#include "TpTrie.h"



using namespace std;

TpTrie::TpTrie() {
    root = new TpNode();
}

TpTrie::TpTrie(const TpTrie& orig) {
}

TpTrie::~TpTrie() {
}

void TpTrie::addWord(UnicodeString s) {
    TpNode * current = root;
    if (s.length() == 0) {
        current->setWordMarker(); // an empty word
        return;
    }
    for (int32_t i = 0; i < s.length(); i++) {
        TpNode * child = current->findChild(s[i]);
        if (child != NULL) {
            current = child;
        } else {
            TpNode * tmp = new TpNode();
            tmp->setContent(s[i]);
            current->appendChild(tmp);
            current = tmp;
        }
        if (i == s.length() - 1)
            current->setWordMarker();
    }
}

vector< pair<int32_t, int32_t> > TpTrie::searchAllWords(UnicodeString s) {
    vector< pair<int32_t, int32_t> > result;
    result.clear();
    for (int32_t j = 0; j < s.length(); j++) {
        TpNode * current = root;
        if (current != NULL) {
            bool itsnotabreak = true;
            for (int32_t i = j; i < s.length(); i++) {
                if (current->wordMarker())
                    result.push_back(make_pair(j, i - 1));
                TpNode * tmp = current->findChild(s[i]);
                if (tmp == NULL) {
                    itsnotabreak = false;
                    break;
                }
                current = tmp;
            }
            if (itsnotabreak)
                if (current->wordMarker())
                    result.push_back(make_pair(j, s.length() - 1));
        }
    }
    return result;
}
