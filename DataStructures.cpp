/**
    Project: libtpc
    File name: Query.cpp
    
    @author valerio
    @version 1.0 11/1/17.
*/

#include "DataStructures.h"

using namespace std;
using namespace tpc::index;

string Query::get_query_text() const {
    string query_text;
    if (type == QueryType::document) {
        add_field_to_text_if_not_empty("fulltext", keyword, query_text);
        add_field_to_text_if_not_empty("-fulltext", exclude_keyword, query_text);
    } else {
        add_field_to_text_if_not_empty("sentence", keyword, query_text);
        add_field_to_text_if_not_empty("-sentence", exclude_keyword, query_text);
    }
    add_field_to_text_if_not_empty("year", year, query_text);
    if (!journal.empty()) {
        string journal_modified = journal;
        if (exact_match) {
            if (journal.substr(0, 1) == "\""){
                journal_modified = journal_modified.substr(1, journal_modified.size() - 1);
            }
            if (journal.substr(journal.size() - 1, 1) == "\""){
                journal_modified = journal_modified.substr(0, journal_modified.size() - 1);
            }
            journal_modified = "\"BEGIN " + journal_modified + " END\"";
        }
        add_field_to_text_if_not_empty("journal", journal_modified, query_text);
    }
    add_field_to_text_if_not_empty("accession", accession, query_text);
    add_field_to_text_if_not_empty("type", paper_type, query_text);
}

void Query::add_field_to_text_if_not_empty(const std::string& field_name, const std::string& field_value,
                                           string& query_text) const {
    if (!field_value.empty() && !field_name.empty()) {
        if (!query_text.empty()) {
            query_text.append(" AND ");
        }
        query_text.append(field_name);
        query_text.append(": ");
        query_text.append(field_value);
    }
}

// TODO rewrite as a 'friend' method in C++ std style
void SearchResults::update(const SearchResults& other) {
    copy(other.hit_documents.begin(), other.hit_documents.end(), back_inserter(hit_documents));
    if (query.type == QueryType::sentence) {
        total_num_sentences += other.total_num_sentences;
    }
    max_score = max(max_score, other.max_score);
    min_score = min(min_score, other.min_score);
}