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

//typedef std::pair<UnicodeString, UnicodeString> AttributeNVPair;

class TpLexiconNode {
public:
    TpLexiconNode();
    TpLexiconNode(const TpLexiconNode & orig);
    virtual ~TpLexiconNode();
    UChar content() { return content_; }
    void setContent(UChar c) { content_ = c; }
    UnicodeString PopOneAnnotation();
    void PushBackAnnotation(long unsigned int i) { annotationid_.push_back(i); }
    long unsigned int AnnotationVectorSize() { return annotationid_.size(); }
    long unsigned int GetVectorElement(long unsigned int i) { return annotationid_.at(i); }
//    void PushBackAttributes(std::vector<AttributeNVPair> & a) { attributes_.push_back(a); }
//    std::vector<AttributeNVPair> GetAttributes(long unsigned int i) { return attributes_.at(i); }
    bool wordMarker() { return marker_; }
    void setWordMarker() { marker_ = true; }
    TpLexiconNode * findChild(UChar c);
    void appendChild(TpLexiconNode * child) { children_.push_back(child); }
    std::vector<TpLexiconNode*> children() { return children_; }
private:
    UChar content_;
    bool marker_;
    std::vector<TpLexiconNode*> children_;
    std::vector<long unsigned int> annotationid_;
//    std::vector< std::vector<AttributeNVPair> > attributes_;
};

#endif	/* TPLEXICONNODE_H */
