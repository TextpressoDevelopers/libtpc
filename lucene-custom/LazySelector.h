/**
    Project: libtpc
    File name: LazySelector.h
    
    @author valerio
    @version 1.0 6/10/17.
*/

#ifndef LIBTPC_LAZYSELECTOR_H
#define LIBTPC_LAZYSELECTOR_H

#include <lucene++/LuceneHeaders.h>
#include <lucene++/FieldSelector.h>

DECLARE_SHARED_PTR(LazySelector);
class LazySelector : public FieldSelector {
public:
    LazySelector(const String& magicField) {
        this->magicFields = std::set<String>();
        this->magicFields.insert(magicField);
    }
    LazySelector(const std::set<String>& magicFields) {
        this->magicFields = magicFields;
    }
    virtual ~LazySelector() {
    }
    LUCENE_CLASS(LazySelector);
protected:
    std::set<String> magicFields;

public:
    virtual FieldSelectorResult accept(const String& fieldName) {
        if (magicFields.find(fieldName) != magicFields.end()) {
            return FieldSelector::SELECTOR_LOAD;
        } else {
            return FieldSelector::SELECTOR_NO_LOAD;
        }
    }
};

#endif //TEXTPRESSOCENTRAL_LAZYSELECTOR_H
