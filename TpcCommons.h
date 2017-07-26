/**
    Project: libtpc
    File name: reader.h

    @author valerio
    @version 1.0 7/18/17.
*/

#ifndef LIBTPC_LIBRARY_H
#define LIBTPC_LIBRARY_H

#include <string>
#include <vector>
#include <set>

namespace tpc {

    static const std::string document_indexname("fulltext");
    static const std::string sentence_indexname("sentence");
    static const std::string document_indexname_cs("fulltext_cs");
    static const std::string sentence_indexname_cs("sentence_cs");

    static const int maxHits(1000000);
    static const int field_cache_min_hits(10000);

    static const int max_num_sentenceids_in_query(100);

    static const std::set<std::string> document_fields_detailed{"accession_compressed", "title_compressed",
                                                                "author_compressed", "journal_compressed", "year",
                                                                "abstract_compressed", "filepath",
                                                                "literature_compressed", "identifier",
                                                                "fulltext_compressed", "fulltext_cat_compressed"};
    static const std::set<std::string> sentence_fields_detailed{"sentence_id", "begin", "end",
                                                                "sentence_compressed", "sentence_cat_compressed"};
}

#endif