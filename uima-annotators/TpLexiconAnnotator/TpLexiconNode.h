/* 
 * File:   TpLexiconNode.h
 * Author: mueller
 *
 * Created on February 6, 2013, 10:22 AM
 */

#ifndef TPLEXICONNODE_H
#define	TPLEXICONNODE_H

#include <uima/api.hpp>
#include <vector>

using namespace std;

class TpLexiconNode {
public:
    TpLexiconNode();
    TpLexiconNode(const TpLexiconNode & orig);
    virtual ~TpLexiconNode();
    UChar content() { return content_; }
    void setContent(UChar c) { content_ = c; }
    UnicodeString PopOneAnnotation();
    void PushBackAnnotation(UnicodeString s) { annotation_.push_back(s); }
    long unsigned int AnnotationVectorSize() { return annotation_.size(); }
    UnicodeString GetVectorElement(long unsigned int i) { return annotation_.at(i); }
    bool wordMarker() { return marker_; }
    void setWordMarker() { marker_ = true; }
    TpLexiconNode * findChild(UChar c);
    void appendChild(TpLexiconNode * child) { children_.push_back(child); }
    vector<TpLexiconNode*> children() { return children_; }
private:
    UChar content_;
    bool marker_;
    vector<TpLexiconNode*> children_;
    vector<UnicodeString> annotation_;
};

#endif	/* TPLEXICONNODE_H */
