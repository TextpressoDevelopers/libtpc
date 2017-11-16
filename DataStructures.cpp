/**
    Project: libtpc
    File name: Query.cpp
    
    @author valerio
    @version 1.0 11/1/17.
*/

#include <boost/algorithm/string.hpp>
#include "DataStructures.h"
#include <regex>

using namespace std;
using namespace tpc::index;

string Query::get_query_text() const {
    string query_text;
    if (type == QueryType::document) {
        add_field_to_text_if_not_empty("fulltext", keyword, false, query_text);
        add_field_to_text_if_not_empty("-fulltext", exclude_keyword, false, query_text);

    } else {
        add_field_to_text_if_not_empty("sentence", keyword, false, query_text);
        add_field_to_text_if_not_empty("-sentence", exclude_keyword, false, query_text);
    }
    add_field_to_text_if_not_empty("year", year, false, query_text);
    add_field_to_text_if_not_empty("accession", accession, false, query_text);
    add_field_to_text_if_not_empty("type", paper_type, false, query_text);
    add_field_to_text_if_not_empty("author", author, exact_match_author, query_text);
    add_field_to_text_if_not_empty("journal", journal, exact_match_journal, query_text);
    add_categories_to_text(query_text);
    return query_text;
}

void Query::add_field_to_text_if_not_empty(const std::string& field_name, const std::string& field_value,
                                           bool exact_match_field, string& query_text) const {
    if (!field_value.empty() && !field_name.empty()) {
        string field_modified = field_value;
        if (exact_match_field) {
            boost::replace_all(field_modified, "\"", "");
            field_modified = "\"BEGIN " + field_modified + " END\"";
        } else {
            // add space for quoted words not separated by whitespace
            field_modified.clear();
            regex r("\\w+\"\\w+");
            regex_replace(field_modified, r, "\" ");
            vector<pair<size_t, size_t>> positions;
            size_t pos = field_modified.find("\"", 0);
            pair<size_t, size_t> begin_end_quote;
            bool first = true;
            while(pos != string::npos) {
                if (first) {
                    positions.emplace_back(pos, 0);
                    first = false;
                } else {
                    positions[positions.size() - 1].second = pos;
                }
                pos = field_modified.find("\"", pos + 1);
            }
            vector<string> words_arr;
            boost::split(words_arr, field_modified, boost::is_any_of(" "));
            size_t actual_pos = 0;
            for (auto word : words_arr) {
                bool starts_with_quote = false;
                bool ends_with_quote = false;
                bool inside_quoted_phrase = false;
                if (word.at(0) == '\"') {
                    starts_with_quote = true;
                }
                if (word.at(0) == '\"'){
                    ends_with_quote = true;
                }
                // delete all double quotes
                boost::replace_all(word, "\"", "");
                if (!(starts_with_quote && ends_with_quote)) {
                    for (auto& pos_pair : positions) {
                        if (actual_pos >= pos_pair.first && actual_pos <= pos_pair.second) {
                            inside_quoted_phrase = true;
                            break;
                        } else if (actual_pos < pos_pair.first) {
                            break;
                        }
                    }
                }
                if (starts_with_quote || !inside_quoted_phrase) {
                    field_modified.append("\"");
                }
                field_modified.append(word);
                if (ends_with_quote || !inside_quoted_phrase) {
                    field_modified.append("\"");
                }
                field_modified.append(" ");
                ++actual_pos;
            }
        }
        if (!query_text.empty()) {
            query_text.append(" AND ");
        }
        query_text.append(field_name);
        query_text.append(": ");
        query_text.append(field_modified);
    }
}

void Query::add_categories_to_text(std::string &query_text) const {
    if (!categories.empty()) {
        if (!query_text.empty()) {
            query_text.append(" AND ");
        }
        query_text.append("(");
        for (auto &category : categories) {
            if (query_text.substr(query_text.size() - 1, 1) != "(") {
                if (categories_and_ed) {
                    query_text.append(" AND");
                } else {
                    query_text.append(" OR");
                }
            }
            if (type == QueryType::document) {
                query_text.append("fulltext_cat: \"");
            } else {
                query_text.append("sentence_cat: \"");
            }
            query_text.append(category);
            query_text.append("\"");
        }
        query_text.append(")");
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