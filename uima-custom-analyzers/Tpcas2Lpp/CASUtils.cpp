#include "CASUtils.h"


string getFilename(CAS& tcas) {
    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    while (aait.get().getType().getName().asUTF8().find("filename") == -1) {
        aait.moveToNext();
    }
    Type currentType = aait.get().getType();
    Feature fcontent = currentType.getFeatureByBaseName("value");
    UnicodeStringRef ucontent = aait.get().getStringValue(fcontent);
    return ucontent.asUTF8();

}

wstring getFulltext(CAS& tcas) {
    UnicodeStringRef usdocref = tcas.getDocumentText();
    wstring ws;
    UnicodeString wd;
    usdocref.extract(0, usdocref.length(), wd);
    for (int i = 0; i < wd.length(); ++i)
        ws += static_cast<wchar_t> (wd[i]);
    return ws;
}

string getCASType(CAS& tcas) {
    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    while (aait.isValid()) {
        Type currentType = aait.get().getType();
        UnicodeStringRef tnameref = currentType.getName();
        string annType = tnameref.asUTF8();
        if (annType == "org.apache.uima.textpresso.rawsource") {
            Feature fvalue = currentType.getFeatureByBaseName("value");
            UnicodeStringRef uvalue = aait.get().getStringValue(fvalue);
            cout << "castype is  " << uvalue << endl;
            return uvalue.asUTF8();
        }
        aait.moveToNext();
    }

}

string gettpfnvHash(CAS& tcas) {
    ANIndex allannindex = tcas.getAnnotationIndex();
    ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    while (aait.isValid()) {
        Type currentType = aait.get().getType();
        UnicodeStringRef tnameref = currentType.getName();
        string annType = tnameref.asUTF8();
        if (annType == "org.apache.uima.textpresso.tpfnvhash") {
            Feature fcontent = currentType.getFeatureByBaseName("content");
            UnicodeStringRef ucontent = aait.get().getStringValue(fcontent);
            return ucontent.asUTF8();
        }
        aait.moveToNext();
    }

}


wstring getCleanText(CAS & tcas) {
    string castype = getCASType(tcas);
    wstring w_fulltext(L"");
    if (castype == "pdf") { //if it's from pdf , records all PDF tag positions and skip PDF tags in clean text
        vector < pair<int, int> > pdftags;
        ANIndex allannindex = tcas.getAnnotationIndex();
        ANIterator aait = allannindex.iterator();
        aait.moveToFirst();
        while (aait.isValid()) {
            Type currentType = aait.get().getType();
            UnicodeStringRef tnameref = currentType.getName();
            string annType = tnameref.asUTF8();
            if (annType == "org.apache.uima.textpresso.pdftag") {
                int begin = aait.get().getBeginPosition();
                int end = aait.get().getEndPosition();
                pdftags.push_back(make_pair(begin, end));
            }
            aait.moveToNext();
        }
        w_fulltext = getFulltext(tcas); //get full clean text
        for (int i = 0; i < pdftags.size(); i++) {  // skip PDF tags in full text to get clean text
            int begin = pdftags[i].first;
            int end = pdftags[i].second;
            cout << "begin " << begin << " end " << end << endl;
            if (w_fulltext[begin] != '<') {
                wcout << "nomatch " << begin << "is " << w_fulltext[begin] << endl;  //error when matching a PDF tag in full text
            }
            int sub_length = end - begin + 1;
            w_fulltext.replace(begin, sub_length, L"");  //replace a PDF tag with empty string
            for (int j = i + 1; j < pdftags.size(); j++) { // shift all following PDF tags positions by the length of previously replaced PDF tag
                pdftags[j].first -= sub_length;
                pdftags[j].second -= sub_length;
            }
        }
    }
    else if (castype == "nxml") { // if it's from nxml
        ANIndex allannindex = tcas.getAnnotationIndex();
        ANIterator aait = allannindex.iterator();
        aait.moveToFirst();
        while (aait.isValid()) {
            Type currentType = aait.get().getType();
            UnicodeStringRef tnameref = currentType.getName();
            string annType = tnameref.asUTF8();
            if (annType == "org.apache.uima.textpresso.xmltag") {
                int begin = aait.get().getBeginPosition();
                int end = aait.get().getEndPosition();
                Feature fvalue = currentType.getFeatureByBaseName("value");
                UnicodeStringRef uvalue = aait.get().getStringValue(fvalue);
                if (uvalue.asUTF8() == "pcdata") {  //concatenate all pcdata strings to form clean text
                    Feature fterm = currentType.getFeatureByBaseName("term");
                    UnicodeStringRef uterm = aait.get().getStringValue(fterm);
                    UnicodeString wd;
                    uterm.extract(0, uterm.length(), wd);
                    for (int i = 0; i < wd.length(); ++i)
                        w_fulltext += static_cast<wchar_t> (wd[i]);
                }
            }
            w_fulltext += L" ";  //adding a space between words
            aait.moveToNext();
        }
    }
    return w_fulltext;
}


string getXMLstring(CAS & tcas)
{
    UnicodeStringRef usdocref = tcas.getDocumentText();
    string xmlstring = usdocref.asUTF8();
    return xmlstring;
}