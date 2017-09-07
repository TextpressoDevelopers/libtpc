#include "CASUtils.h"

std::string getFilename(uima::CAS& tcas) {
    // FSIndexRepository & indices = tcas.getIndexRepository();
    uima::ANIndex allannindex = tcas.getAnnotationIndex();
    uima::ANIterator aait = allannindex.iterator();
    aait.moveToFirst();
    while (aait.get().getType().getName().asUTF8().find("filename") == -1)
        aait.moveToNext();
    uima::Type currentType = aait.get().getType();
    uima::Feature fcontent = currentType.getFeatureByBaseName("value");
    uima::UnicodeStringRef ucontent = aait.get().getStringValue(fcontent);
    return ucontent.asUTF8();

}

std::string getXMLstring(uima::CAS & tcas) {
    uima::UnicodeStringRef usdocref;
    usdocref = tcas.getDocumentText();
    if (usdocref.getBuffer() != NULL) {
        std::string xmlstring = usdocref.asUTF8();
        return xmlstring;
    } else {
        return "";
    }
}
