/* 
 * File:   TpLexiconNode.cpp
 * Author: mueller
 * 
 * Created on February 6, 2013, 10:22 AM
 */

#include "TpLexiconNode.h"

#include <cstddef>

using namespace std;

TpLexiconNode::TpLexiconNode() {
    content_ = '\0'; 
    marker_ = false;
}

TpLexiconNode::TpLexiconNode(const TpLexiconNode & orig) {
}

TpLexiconNode::~TpLexiconNode() {
}

TpLexiconNode* TpLexiconNode::findChild(UChar c) {
    for (int i = 0; i < children_.size(); i++) {
        TpLexiconNode * tmp = children_.at(i);
        if (tmp->content() == c) {
            return tmp;
        }
    }
    return NULL;
}
