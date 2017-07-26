/* 
 * File:   TpLexiconTrie.cpp
 * Author: mueller
 * 
 * Created on February 6, 2013, 10:21 AM
 */

#include "TpLexiconTrie.h"
#include "../uimaglobaldefinitions.h"

using namespace std;

TpLexiconTrie::TpLexiconTrie() {
    idcounter_ = 0;
    root = new TpLexiconNode();
    dlsetToken = set<UnicodeString>(G_initT, G_initT + G_initT_No);
    set<UnicodeString>::iterator it;
    trieToken = new TpTrie();
    for (it = dlsetToken.begin(); it != dlsetToken.end(); it++) {
        trieToken->addWord(*it);
    }
}

TpLexiconTrie::TpLexiconTrie(const TpLexiconTrie & orig) {
}

TpLexiconTrie::~TpLexiconTrie() {
}

void TpLexiconTrie::addWord(UnicodeString s, UnicodeString annotation) {
    TpLexiconNode * current = root;
    if (s.length() == 0) return;
    for (int32_t i = 0; i < s.length(); i++) {
        TpLexiconNode * child = current->findChild(s[i]);
        if (child != NULL) {
            current = child;
        } else {
            TpLexiconNode * tmp = new TpLexiconNode();
            tmp->setContent(s[i]);
            current->appendChild(tmp);
            current = tmp;
        }
        if (i == s.length() - 1) {
            current->setWordMarker();
            current->PushBackAnnotation(annotation2id(annotation));
        }
    }
}

vector<ppiiU> TpLexiconTrie::searchAllWords(UnicodeString s) {
    vector<ppiiU> result;
    result.clear();
    vector< pair<int32_t, int32_t> > dls = trieToken->searchAllWords(s);
    sort(dls.begin(), dls.end());
    map<int32_t, bool> fb;
    map<ppiiU, bool> alreadyseen;
    vector< pair<int32_t, int32_t> >::iterator it;
    for (it = dls.begin(); it != dls.end(); it++) fb[(*it).first] = true;
    for (it = dls.begin(); it != dls.end(); it++) {
        int32_t j = (*it).second + 1;
        TpLexiconNode * current = root;
        if (current != NULL) {
            bool itsnotabreak = true;
            for (int32_t i = j; i < s.length(); i++) {
                if (current->wordMarker()) {
                    if (fb[i]) {
                        pair<int32_t, int32_t> p = make_pair(j, i - 1);
                        for (long unsigned int k = 0; k < current->AnnotationVectorSize(); k++) {
                            UnicodeString a = id2annotation(current->GetVectorElement(k));
                            ppiiU aux = make_pair(p, a);
                            if (!alreadyseen[aux]) {
                                alreadyseen[aux] = true;
                                result.push_back(make_pair(p, a));
                            }
                        }
                    }
                }
                TpLexiconNode * tmp = current->findChild(s[i]);
                if (tmp == NULL) {
                    itsnotabreak = false;
                    break;
                }
                current = tmp;
            }
            if (itsnotabreak)
                if (current->wordMarker()) {
                    pair<int32_t, int32_t> p = make_pair(j, s.length() - 1);
                    for (long unsigned int k = 0; k < current->AnnotationVectorSize(); k++) {
                        UnicodeString a = id2annotation(current->GetVectorElement(k));
                        ppiiU aux = make_pair(p, a);
                        if (!alreadyseen[aux]) {
                            alreadyseen[aux] = true;
                            result.push_back(make_pair(p, a));
                        }
                    }
                }
        }
    }
    return result;
}

long unsigned int TpLexiconTrie::annotation2id(UnicodeString annotation) {
    std::map<UnicodeString, long unsigned int>::iterator it;
    it = a2i_.find(annotation);
    if (it == a2i_.end()) {
        a2i_[annotation] = idcounter_;
        i2a_.push_back(annotation);
        idcounter_++;
    }
    return a2i_[annotation];
}
