/* 
 * File:   TpNode.h
 * Author: mueller
 *
 * Created on February 4, 2013, 3:34 PM
 */

#ifndef TPNODE_H
#define	TPNODE_H

#include <uima/api.hpp>
#include <vector>

using namespace std;

class TpNode {
public:
    TpNode();
    TpNode(const TpNode& orig);
    virtual ~TpNode();
    UChar content() { return content_; }
    void setContent(UChar c) { content_ = c; }
    bool wordMarker() { return marker_; }
    void setWordMarker() { marker_ = true; }
    TpNode * findChild(UChar c);
    void appendChild(TpNode * child) { children_.push_back(child); }
    vector<TpNode*> children() { return children_; }
private:
    UChar content_;
    bool marker_;
    vector<TpNode*> children_;

};

#endif	/* TPNODE_H */

