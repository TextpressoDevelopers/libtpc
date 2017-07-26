
/* 
 * File:   TpNode.cpp
 * Author: mueller
 * 
 * Created on February 4, 2013, 3:34 PM
 */

#include "TpNode.h"
#include <cstddef>

using namespace std;

TpNode::TpNode() {
    content_ = '\0'; 
    marker_ = false;
}

TpNode::TpNode(const TpNode & orig) {
}

TpNode::~TpNode() {
}

TpNode * TpNode::findChild(UChar c) {
    for (int i = 0; i < children_.size(); i++) {
        TpNode * tmp = children_.at(i);
        if (tmp->content() == c) {
            return tmp;
        }
    }

    return NULL;
}
