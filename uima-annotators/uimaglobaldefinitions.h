// Global file containing all global definitions.

#ifndef UIMAGLOBALDEFINITIONS_H
#define UIMAGLOBALDEFINITIONS_H

#include <uima/api.hpp>

// If a composite delimiter exists, then there cannot be another delimiter
// that is a subset of that composite token delimiter. Decompose it accordingly.
// This applies to token and sentence delimiter
UnicodeString G_initT[] = {
    " ", "\n", "\t", "'", "\"",
    "/", "—", "(", ")", "[",
    "]", "{", "}", ":", ". ",
    "; ", ", ", "! ", "? "
};
const int G_initT_No = 19;

UnicodeString G_initS[] = {
    ".\n", "!\n", "?\n", ". ", "! ", "? ",
    ".\t", "!\t", "?\t", ".<", "!<", "?<"
};
const int G_initS_No = 12;

const set<UnicodeString> sectionArticleB{
    "Beginning of Article\n"};

const set<UnicodeString> sectionArticleE{
    "End of Article\n"};

const set<UnicodeString> sectionAbstract{
    "Abstract\n",
    "A b s t r a c t\n",
    "ABSTRACT\n",
    "A B S T R A C T\n"};

const set<UnicodeString> sectionIntroduction{
    "Introduction\n",
    "I n t r o d u c t i o n\n",
    "INTRODUCTION\n",
    "I N T R O D U C T I O N\n"};

const set<UnicodeString> sectionResult{
    "Result\n",
    "R e s u l t\n",
    "RESULT\n",
    "R E S U L T\n",
    "Results\n",
    "R e s u l t s\n",
    "RESULTS\n",
    "R E S U L T S\n"};

const set<UnicodeString> sectionDiscussion{
    "Discussion\n",
    "D i s c u s s i o n\n",
    "DISCUSSION\n",
    "D I S C U S S I O N\n"};

const set<UnicodeString> sectionConclusion{
    "Conclusion\n",
    "C o n c l u s i o n\n",
    "CONCLUSION\n",
    "C O N C L U S I O N\n",
    "Conclusions\n",
    "C o n c l u s i o n s\n",
    "CONCLUSIONS\n",
    "C O N C L U S I O N S\n"};

const set<UnicodeString> sectionBackground{
    "Background\n",
    "B a c k g r o u n d\n",
    "BACKGROUND\n",
    "B A C K G R O U N D\n"};

const set<UnicodeString> sectionMaterialsMethods{
    "Material\n",
    "M a t e r i a l\n",
    "MATERIAL\n",
    "M A T E R I A L\n",
    "Materials\n",
    "M a t e r i a l s\n",
    "MATERIALS\n",
    "M A T E R I A L S\n",
    "Method\n",
    "M e t h o d\n",
    "METHOD\n",
    "M E T H O D\n",
    "Methods\n",
    "M e t h o d s\n",
    "METHODS\n",
    "M E T H O D S\n",
    "Material and Method\n",
    "M a t e r i a l   a n d   M e t h o d\n",
    "MATERIAL AND METHOD\n",
    "M A T E R I A L   A N D   M E T H O D\n",
    "Material and Methods\n",
    "M a t e r i a l   a n d   M e t h o d s\n",
    "MATERIAL AND METHODS\n",
    "M A T E R I A L   A N D   M E T H O D S\n",
    "Materials and Method\n",
    "M a t e r i a l s   a n d  M e t h o d\n",
    "MATERIALS AND METHOD\n",
    "M A T E R I A L S   A N D   M E T H O D\n",
    "Materials and Methods\n",
    "M a t e r i a l s   a n d   M e t h o d s\n",
    "MATERIALS AND METHODS\n",
    "M A T E R I A L S   A N D   M E T H O D S\n",
    "Material And Method\n",
    "M a t e r i a l   A n d   M e t h o d\n",
    "Material And Methods\n",
    "M a t e r i a l   A n d   M e t h o d s\n",
    "Materials And Method\n",
    "M a t e r i a l s   A n d   M e t h o d\n",
    "Materials And Methods\n",
    "M a t e r i a l s   A n d   M e t h o d s",
    "Material and method\n",
    "M a t e r i a l   a n d   m e t h o d\n",
    "Material and methods\n",
    "M a t e r i a l   a n d   m e t h o d s\n",
    "Materials and method\n",
    "M a t e r i a l s   a n d   m e t h o d\n",
    "Materials and methods\n",
    "M a t e r i a l s   a n d   m e t h o d s\n"};

const set<UnicodeString> sectionDesign{
    "Design\n",
    "D e s i g n\n",
    "DESIGN\n",
    "D E S I G N\n",
    "Designs\n",
    "D e s i g n s\n",
    "DESIGNS\n",
    "D E S I G N S\n"};

const set<UnicodeString> sectionAcknowledgments{
    "Acknowledgment\n",
    "A c k n o w l e d g m e n t\n",
    "ACKNOWLEDGMENT\n",
    "A C K N O W L E D G M E N T\n",
    "Acknowledgments\n",
    "A c k n o w l e d g m e n t s\n",
    "ACKNOWLEDGMENTS\n",
    "A C K N O W L E D G M E N T S\n",
    "Acknowledgement\n",
    "A c k n o w l e d g e m e n t\n",
    "ACKNOWLEDGEMENT\n",
    "A C K N O W L E D G E M E N T\n",
    "Acknowledgements\n",
    "A c k n o w l e d g e m e n t s\n",
    "ACKNOWLEDGEMENTS\n",
    "A C K N O W L E D G E M E N T S\n"};

const set<UnicodeString> sectionReferences{
    "Reference\n",
    "R e f e r e n c e\n",
    "REFERENCE\n",
    "R E F E R E N C E\n",
    "References\n",
    "R e f e r e n c e s\n",
    "REFERENCES\n",
    "R E F E R E N C E S\n"};

UnicodeString G_initP[] = {"<_pdf _image", "<_pdf _sbr", "<_pdf _hbr",
    "<_pdf _fsc", "<_pdf _fnc", "<_pdf _ydiff", "<_pdf _cr", "<_pdf _page"};
const int G_initP_No = 8;

const std::string ServerNames[] = {"http://goldturtle.caltech.edu/cgi-bin/ReceivePost.cgi",
    "http://go-genkisugi.rhcloud.com/capella", "http://localhost/cgi-bin/ReceivePost.cgi"};
const int ServerNames_No = 3;

const UnicodeString usG_pdftagstart("<_pdf ");
const UnicodeString usG_pdftagend("/>");

#endif