/**
    Project: libtpc
    File name: TpcIndexReader.cpp
    
    @author valerio
    @version 1.0 7/25/17.
*/

#include "IndexManager.h"
#include "Utils.h"
#include "lucene-custom/CaseSensitiveAnalyzer.h"
#include "lucene-custom/LazySelector.h"
#include "uima-custom-analyzers/Tpcas2SingleIndex/CASUtils.h"
#include <lucene++/LuceneHeaders.h>
#include <lucene++/FieldCache.h>
#include <lucene++/CompressionTools.h>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <chrono>
#include <numeric>
#include <unordered_map>
#include <uima/exceptions.hpp>
#include <uima/resmgr.hpp>
#include <uima/engine.hpp>
#include <xercesc/util/XMLString.hpp>
#include <uima/xmideserializer.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace tpc::index;
using namespace Lucene;
using namespace xercesc;
using namespace boost::filesystem;

IndexManager::IndexManager(const string& index_path, bool read_only) {
    index_dir = index_path;
    vector<string> all_literatures;
    std::ifstream lit_file (index_path + "/subindex.config");
    string line;
    vector<string> index_types = {"fulltext", "sentence", "fulltext_cs", "sentence_cs"};
    if (lit_file.is_open()) {
        while (getline(lit_file, line)) {
            for (const string& index_type : index_types) {
                string index_dir;
                index_dir.append(index_path);
                index_dir.append("/");
                index_dir.append(line);
                index_dir.append("/");
                index_dir.append(index_type);
                if (exists(path(index_dir + "/segments.gen"))) {
                    IndexReaderPtr reader = IndexReader::open(
                            FSDirectory::open(String(index_dir.begin(), index_dir.end())), read_only);
                    if (reader) {
                        readers_vec_map[line.substr(0, line.find_first_of("_")) + "_" + index_type].push_back(reader);
                        readers_map[line + "_" + index_type] = reader;
                        available_literatures.insert(line.substr(0, line.find_first_of("_")));
                        available_subindices.insert(line);
                    } else {
                        throw index_exception();
                    }
                }
            }
        }
        lit_file.close();
    }
}

SearchResults IndexManager::search_documents(const Query& query, const set<string>& doc_ids)
{
    AnalyzerPtr analyzer;
    if (query.case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    Collection<IndexReaderPtr> subReaders = get_subreaders(query.literatures, query.type, query.case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, false);
    QueryParserPtr parser = newLucene<QueryParser>(
            LuceneVersion::LUCENE_30, query.type == QueryType::document ? L"fulltext" : L"sentence", analyzer);
    String query_str = String(query.query_text.begin(), query.query_text.end());
    if (!doc_ids.empty()) {
        string joined_ids = boost::algorithm::join(doc_ids, " OR identifier:");
        query_str += L" AND (identifier:" + String(joined_ids.begin(), joined_ids.end()) + L")";
    }
    QueryPtr luceneQuery = parser->parse(query_str);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(maxHits, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    SearchResults result = SearchResults();
    if (query.type == QueryType::document) {
        result = read_documents_summaries(matchesCollection, subReaders, searcher);
    } else if (query.type == QueryType::sentence_with_ids || query.type == QueryType::sentence_without_ids) {
        result = read_sentences_summaries(matchesCollection, subReaders, searcher, query.sort_by_year,
                                          query.type == QueryType::sentence_with_ids);
        result.total_num_sentences = matchesCollection.size();
    }
    result.query = query;
    if (query.sort_by_year) {
        // sort by year (greater first) and by score (greater first)
        sort(result.hit_documents.begin(), result.hit_documents.end(), document_year_score_gt);
    } else {
        // sort by score (greater first)
        sort(result.hit_documents.begin(), result.hit_documents.end(), document_score_gt);
    }
    multireader->close();
    return result;
}

set<String> IndexManager::compose_field_set(const set<string> &include_fields, const set<string> &exclude_fields,
                                              const set<string> &required_fields)
{
    set<String> fields;
    for (const auto& f : include_fields) {
        fields.insert(String(f.begin(), f.end()));
    }
    for (const auto& f : exclude_fields) {
        fields.erase(String(f.begin(), f.end()));
    }
    for (const auto& f : required_fields) {
        fields.insert(String(f.begin(), f.end()));
    }
    return fields;
}

Collection<IndexReaderPtr> IndexManager::get_subreaders(const vector<string>& literatures, QueryType type,
                                                        bool case_sensitive)
{
    string index_type;
    if (type == QueryType::document) {
        index_type = case_sensitive ? document_indexname_cs : document_indexname;
    } else if (type == QueryType::sentence_with_ids || type == QueryType::sentence_without_ids) {
        index_type = case_sensitive ? sentence_indexname_cs : sentence_indexname;
    }
    Collection<IndexReaderPtr> subReaders = Collection<IndexReaderPtr>::newInstance(0);
    for (const string& literature : literatures) {
        try {
            for (const auto& subreader : readers_vec_map[literature + "_" + index_type]) {
                subReaders.add(subreader);
            }
        } catch (out_of_range e) {
            throw index_exception();
        }
    }
    return subReaders;
}

SearchResults IndexManager::read_documents_summaries(
        const Collection<ScoreDocPtr> &matches_collection, const Collection<IndexReaderPtr> &subreaders,
        SearcherPtr searcher, bool sort_by_year)
{
    SearchResults result = SearchResults();
    // for small searches, read the fields with a lazy loader
    if (matches_collection.size() < field_cache_min_hits) {
        set<String> fields;
        if (sort_by_year) {
            fields = {L"identifier", L"year"};
        } else {
            fields = {L"identifier"};
        }
        FieldSelectorPtr fsel = newLucene<LazySelector>(fields);
        for (const auto& scoredoc : matches_collection) {
            DocumentSummary document;
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            String identifier = docPtr->get(L"identifier");
            document.identifier = string(identifier.begin(), identifier.end());
            document.score = scoredoc->score;
            if (sort_by_year) {
                String year = docPtr->get(L"year");
                document.year = string(year.begin(), year.end());
            }
            result.hit_documents.push_back(document);
        }
    } else { // for big searches, read the fields from the fieldcaches
        vector<int32_t> docids;
        map<int32_t, double> scores;
        for (const auto& scoredoc : matches_collection) {
            docids.push_back(scoredoc->doc);
            scores.insert({scoredoc->doc, scoredoc->score});
        }
        sort(docids.begin(), docids.end());
        int readerIndex = 0;
        Collection<String> fieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"identifier");
        Collection<String> yearFieldCache;
        if (sort_by_year) {
            yearFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"year");
        }
        int offset = 0;
        for (auto docid : docids) {
            while ((docid - offset) >= fieldCache.size()) {
                offset += fieldCache.size();
                fieldCache = FieldCache::DEFAULT()->getStrings(subreaders[++readerIndex], L"identifier");
                if (sort_by_year) {
                    yearFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"year");
                }
            }
            String identifier = fieldCache[docid - offset];
            DocumentSummary document;
            document.identifier = string(identifier.begin(), identifier.end());
            document.score = scores[docid];
            if (sort_by_year) {
                String year = yearFieldCache[docid - offset];
                document.year = string(year.begin(), year.end());
            }
            result.hit_documents.push_back(document);
        }
    }
    // check and update max and min scores for result
    for (const DocumentSummary& doc : result.hit_documents) {
        if (doc.score > result.max_score) {
            result.max_score = doc.score;
        } else if (doc.score < result.min_score) {
            result.min_score = doc.score;
        }
    }
    return result;
}

SearchResults IndexManager::read_sentences_summaries(const Collection<ScoreDocPtr> &matches_collection,
                                                       const Collection<IndexReaderPtr> &subreaders,
                                                       SearcherPtr searcher, bool sort_by_year,
                                                       bool return_match_sentences_ids)
{
    SearchResults result = SearchResults();
    unordered_map<string, DocumentSummary> doc_map;
    doc_map.reserve(100000);
    // for small searches, read the fields with a lazy loader
    if (matches_collection.size() < field_cache_min_hits) {
        set<String> fields =  {L"identifier"};
        if (sort_by_year) {
            fields.insert(L"year");
        }
        if (return_match_sentences_ids) {
            fields.insert(L"sentence_id");
        }
        FieldSelectorPtr fsel = newLucene<LazySelector>(fields);
        for (const auto& scoredoc : matches_collection) {
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            String identifier = docPtr->get(L"identifier");
            string identifier_str = string(identifier.begin(), identifier.end());
            if (doc_map.find(string(identifier.begin(), identifier.end())) == doc_map.end()) {
                DocumentSummary document;
                document.identifier = identifier_str;
                if (sort_by_year) {
                    String year = docPtr->get(L"year");
                    document.year = string(year.begin(), year.end());
                }
                document.score = 0;
                doc_map.insert({document.identifier, document});
            }
            doc_map[identifier_str].score += scoredoc->score;
            SentenceSummary sentence;
            if (return_match_sentences_ids) {
                sentence.sentence_id = StringUtils::toInt(docPtr->get(L"sentence_id"));
            }
            sentence.score = scoredoc->score;
            doc_map[identifier_str].matching_sentences.push_back(sentence);
        }
    } else { // for big searches, read the fields from the fieldcaches
        vector<int> docids;
        vector<double> scores;
        for (const auto& scoredoc : matches_collection) {
            docids.push_back(scoredoc->doc);
            scores.push_back(scoredoc->score);
        }
        //map<int, double> scores;
        std::vector<int> idx_vec(docids.size());
        std::iota(idx_vec.begin(), idx_vec.end(), 0);
        auto comparator = [&docids](int a, int b){ return docids[a] < docids[b]; };
        std::sort(idx_vec.begin(), idx_vec.end(), comparator);
        //sort(docids.begin(), docids.end());
        int readerIndex = 0;
        Collection<String> docIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"identifier");
        Collection<int> sentIdFieldCache;
        if (return_match_sentences_ids) {
            sentIdFieldCache = FieldCache::DEFAULT()->getInts(subreaders[readerIndex], L"sentence_id");
        }
        Collection<String> yearFieldCache;
        if (sort_by_year) {
            yearFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"year");
        }
        int offset = 0;
        for (int idx : idx_vec) {
            int docid = docids[idx];
            while ((docid - offset) >= docIdFieldCache.size()) {
                offset += docIdFieldCache.size();
                ++readerIndex;
                docIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"identifier");
                if (return_match_sentences_ids) {
                    sentIdFieldCache = FieldCache::DEFAULT()->getInts(subreaders[readerIndex], L"sentence_id");
                }
                if (sort_by_year) {
                    yearFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"year");
                }
            }
            String docIdentifier = docIdFieldCache[docid - offset];
            string identifier_str = string(docIdentifier.begin(), docIdentifier.end());
            if (doc_map.find(identifier_str) == doc_map.end()) {
                DocumentSummary document;
                document.identifier = identifier_str;
                if (sort_by_year) {
                    String year = yearFieldCache[docid - offset];
                    document.year = string(year.begin(), year.end());
                }
                document.score = 0;
                doc_map.insert({identifier_str, document});
            }
            doc_map[identifier_str].score += scores[idx];
            SentenceSummary sentence;
            if (return_match_sentences_ids) {
                sentence.sentence_id = sentIdFieldCache[docid - offset];
            }
            sentence.score = scores[idx];
            doc_map[identifier_str].matching_sentences.push_back(sentence);
        }
    }
    std::transform(doc_map.begin(), doc_map.end(), std::back_inserter(result.hit_documents),
                   boost::bind(&map<string, DocumentSummary>::value_type::second, _1));
    // check and update max and min scores for result
    for (const DocumentSummary& doc : result.hit_documents) {
        if (doc.score > result.max_score) {
            result.max_score = doc.score;
        } else if (doc.score < result.min_score) {
            result.min_score = doc.score;
        }
    }
    return result;
}

vector<DocumentDetails> IndexManager::get_documents_details(const vector<DocumentSummary> &doc_summaries,
                                                              const vector<string> &subindices,
                                                              bool sort_by_year,
                                                              bool include_sentences,
                                                              set<string> include_doc_fields,
                                                              set<string> include_match_sentences_fields,
                                                              const set<string> &exclude_doc_fields,
                                                              const set<string> &exclude_match_sentences_fields)
{
    vector<DocumentDetails> results;
    set<String> doc_f = compose_field_set(include_doc_fields, exclude_doc_fields, {"year"});
    FieldSelectorPtr doc_fsel = newLucene<LazySelector>(doc_f);
    AnalyzerPtr analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    Collection<IndexReaderPtr> docSubReaders = get_subreaders(subindices, QueryType::document);
    MultiReaderPtr docMultireader = newLucene<MultiReader>(docSubReaders, false);
    QueryParserPtr docParser = newLucene<QueryParser>(LuceneVersion::LUCENE_30,
                                                      String(document_indexname.begin(), document_indexname.end()),
                                                      analyzer);
    SearcherPtr searcher = newLucene<IndexSearcher>(docMultireader);
    set<String> sent_f;
    FieldSelectorPtr sent_fsel;
    SearcherPtr sent_searcher;
    Collection<IndexReaderPtr> sentSubReaders;
    MultiReaderPtr sentMultireader;
    QueryParserPtr sentParser;
    if (include_sentences) {
        sent_f = compose_field_set(include_match_sentences_fields, exclude_match_sentences_fields);
        sent_fsel = newLucene<LazySelector>(sent_f);
        sentSubReaders = get_subreaders(subindices, QueryType::sentence_without_ids);
        sentMultireader = newLucene<MultiReader>(sentSubReaders, false);
        sentParser = newLucene<QueryParser>(LuceneVersion::LUCENE_30,
                                            String(sentence_indexname.begin(), sentence_indexname.end()),
                                            analyzer);
        sent_searcher = newLucene<IndexSearcher>(sentMultireader);
    }
    results = read_documents_details(doc_summaries, docParser, searcher, doc_fsel, doc_f);
    map<string, DocumentSummary> doc_summaries_map;
    for (const auto& doc_summary : doc_summaries) {
        doc_summaries_map[doc_summary.identifier] = doc_summary;
    }
    if (include_sentences) {
        for (DocumentDetails& docDetails : results ) {
            update_sentences_details_for_document(doc_summaries_map[docDetails.identifier], docDetails, sentParser,
                                                  sent_searcher, sent_fsel, sent_f);
        }
    }
    docMultireader->close();
    if (include_sentences) {
        sentMultireader->close();
    }
    if (sort_by_year) {
        sort(results.begin(), results.end(), document_year_score_gt);
    } else {
        sort(results.begin(), results.end(), document_score_gt);
    }
    return results;
}

DocumentDetails IndexManager::get_document_details(const DocumentSummary& doc_summary,
                                                     const vector<string>& subindices,
                                                     bool include_sentences,
                                                     set<string> include_doc_fields,
                                                     set<string> include_match_sentences_fields,
                                                     const set<string>& exclude_doc_fields,
                                                     const set<string>& exclude_match_sentences_fields)
{
    return get_documents_details({doc_summary}, subindices, false, include_sentences,
                                 include_doc_fields, include_match_sentences_fields, exclude_doc_fields,
                                 exclude_match_sentences_fields)[0];
}

void IndexManager::update_document_details(DocumentDetails &doc_details, String field, DocumentPtr doc_ptr) {
    if (field == L"identifier") {
        String identifier = doc_ptr->get(StringUtils::toString("identifier"));
        doc_details.identifier = string(identifier.begin(), identifier.end());
    } else if (field == L"year") {
        String year = doc_ptr->get(StringUtils::toString("year"));
        doc_details.year = string(year.begin(), year.end());
    } else if (field == L"filepath") {
        String filepath = doc_ptr->get(StringUtils::toString("filepath"));
        doc_details.filepath = string(filepath.begin(), filepath.end());
    } else if (field == L"accession_compressed") {
        String accession = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("accession_compressed")));
        doc_details.accession = string(accession.begin(), accession.end());
    } else if (field == L"title_compressed") {
        String title = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("title_compressed")));
        doc_details.title = string(title.begin(), title.end());
    } else if (field == L"author_compressed") {
        String author = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("author_compressed")));
        doc_details.author = string(author.begin(), author.end());
    } else if (field == L"journal_compressed") {
        String journal = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("journal_compressed")));
        doc_details.journal = string(journal.begin(), journal.end());
    } else if (field == L"abstract_compressed") {
        String abstract = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("abstract_compressed")));
        doc_details.abstract = string(abstract.begin(), abstract.end());
    } else if (field == L"literature_compressed") {
        String literature = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("literature_compressed")));
        doc_details.literature = string(literature.begin(), literature.end());
    } else if (field == L"fulltext_compressed") {
        String fulltext = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("fulltext_compressed")));
        doc_details.fulltext = string(fulltext.begin(), fulltext.end());
    } else if (field == L"fulltext_cat_compressed") {
        String fulltext_cat = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("fulltext_cat_compressed")));
        doc_details.categories_string = string(fulltext_cat.begin(), fulltext_cat.end());
    }
}

DocumentDetails IndexManager::read_document_details(const DocumentSummary &doc_summary,
                                                      QueryParserPtr doc_parser,
                                                      SearcherPtr searcher,
                                                      FieldSelectorPtr fsel,
                                                      const set<String> &fields)
{
    string doc_query_str = "identifier:" + doc_summary.identifier;
    QueryPtr luceneQuery = doc_parser->parse(String(doc_query_str.begin(), doc_query_str.end()));
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(maxHits, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    auto scoredoc = matchesCollection[0];
    DocumentDetails documentDetails = DocumentDetails();
    DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
    for (const auto& f : fields) {
        update_document_details(documentDetails, f, docPtr);
    }
    return documentDetails;
}

vector<DocumentDetails> IndexManager::read_documents_details(const vector<DocumentSummary> &doc_summaries,
                                                               QueryParserPtr doc_parser,
                                                               SearcherPtr searcher,
                                                               FieldSelectorPtr fsel,
                                                               const set<String> &fields)
{
    vector<DocumentDetails> results;
    vector<string> identifiers;
    map<string, double> scoremap;
    for (const auto& docSummary : doc_summaries) {
        identifiers.push_back(docSummary.identifier);
        scoremap[docSummary.identifier] = docSummary.score;
    }
    auto identifiersItBegin = identifiers.begin();
    auto identifiersItEnd = identifiers.begin();
    while (identifiersItEnd != identifiers.end()) {
        identifiersItEnd = distance(identifiersItBegin, identifiers.end()) <= max_num_docids_in_query ?
                           identifiers.end() : identifiersItBegin + max_num_docids_in_query;
        string doc_query_str = "identifier:" + boost::algorithm::join(vector<string>(identifiersItBegin,
                                                                                     identifiersItEnd),
                                                                      " OR identifier:");
        QueryPtr luceneQuery = doc_parser->parse(String(doc_query_str.begin(), doc_query_str.end()));
        TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(maxHits, true);
        searcher->search(luceneQuery, collector);
        Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
        for (const auto &scoredoc : matchesCollection) {
            DocumentDetails documentDetails = DocumentDetails();
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            for (const auto &f : fields) {
                update_document_details(documentDetails, f, docPtr);
            }
            documentDetails.score = scoremap[documentDetails.identifier];
            results.push_back(documentDetails);
        }
        identifiersItBegin = identifiersItEnd;
    }
    sort(results.begin(), results.end(), document_score_gt);
    return results;
}


void IndexManager::update_sentences_details_for_document(const DocumentSummary &doc_summary,
                                                           DocumentDetails &doc_details,
                                                           QueryParserPtr sent_parser,
                                                           SearcherPtr searcher,
                                                           FieldSelectorPtr fsel, const set<String> &fields)
{
    vector<string> sentencesIds;
    map<int, double> sentScoreMap;
    for (const SentenceSummary &sent : doc_summary.matching_sentences) {
        sentencesIds.push_back(to_string(sent.sentence_id));
        sentScoreMap[sent.sentence_id] = sent.score;
    }

    TopScoreDocCollectorPtr collector;
    auto sentencesIdsItBegin = sentencesIds.begin();
    auto sentencesIdsItEnd = sentencesIds.begin();
    while (sentencesIdsItEnd != sentencesIds.end()) {
        sentencesIdsItEnd = distance(sentencesIdsItBegin, sentencesIds.end())  <= max_num_sentenceids_in_query ?
                            sentencesIds.end() : sentencesIdsItBegin + max_num_sentenceids_in_query;
        string sent_query_str = "identifier:" + doc_summary.identifier + " AND (sentence_id:" +
                                boost::algorithm::join(vector<string>(sentencesIdsItBegin, sentencesIdsItEnd),
                                                       " OR sentence_id:") + ")";
        QueryPtr luceneQuery = sent_parser->parse(String(sent_query_str.begin(), sent_query_str.end()));
        collector = TopScoreDocCollector::create(maxHits, true);
        searcher->search(luceneQuery, collector);
        Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
        for (const auto &sentscoredoc : matchesCollection) {
            SentenceDetails sentenceDetails = SentenceDetails();
            DocumentPtr sentPtr = searcher->doc(sentscoredoc->doc, fsel);
            for (const auto &f : fields) {
                if (f == L"sentence_id") {
                    sentenceDetails.sentence_id = StringUtils::toInt(
                            sentPtr->get(StringUtils::toString("sentence_id")));
                } else if (f == L"begin") {
                    sentenceDetails.doc_position_begin = StringUtils::toInt(
                            sentPtr->get(StringUtils::toString("begin")));
                } else if (f == L"end") {
                    sentenceDetails.doc_position_end = StringUtils::toInt(sentPtr->get(
                            StringUtils::toString("end")));
                } else if (f == L"sentence_compressed") {
                    String sentence = CompressionTools::decompressString(
                            sentPtr->getBinaryValue(StringUtils::toString("sentence_compressed")));
                    sentenceDetails.sentence_text = string(sentence.begin(), sentence.end());
                } else if (f == L"sentence_cat_compressed") {
                    String sentence_cat = CompressionTools::decompressString(
                            sentPtr->getBinaryValue(StringUtils::toString("sentence_cat_compressed")));
                    sentenceDetails.categories_string = string(sentence_cat.begin(), sentence_cat.end());
                }
            }
            sentenceDetails.score = sentScoreMap[sentenceDetails.sentence_id];
            doc_details.sentences_details.push_back(sentenceDetails);
        }
        sentencesIdsItBegin = sentencesIdsItEnd;
    }
}

void IndexManager::create_index_from_existing_cas_dir(const std::string &input_cas_dir, int max_num_papers_per_subindex)
{
    path input_cas_dir_path(input_cas_dir);
    string current_lit = input_cas_dir_path.filename().string();
    string out_dir = index_dir + "/" + current_lit;
    string subindex_dir;
    int counter_cas_files(0);
    bool first_paper;
    TmpConf tmp_conf = TmpConf();
    available_literatures.insert(input_cas_dir_path.filename().string());
    for (directory_iterator dir_it(input_cas_dir_path); dir_it != directory_iterator(); ++dir_it) {
        if (counter_cas_files % max_num_papers_per_subindex == 0) {
            // create new subindex
            subindex_dir = out_dir + "_" + to_string(counter_cas_files / max_num_papers_per_subindex);
            available_subindices.insert(path(subindex_dir).filename().string());
            tmp_conf = write_tmp_conf_files(subindex_dir);
            first_paper = true;
            if (!exists(tmp_conf.new_index_flag)) {
                std::ofstream f_newindexflag(tmp_conf.new_index_flag.c_str());
                f_newindexflag << "newindexflag";
                f_newindexflag.close();
            }
            create_subindex_dir_structure(subindex_dir);
        }
        if (is_regular_file(dir_it->status())) {
            std::string filepath(dir_it->path().string());
            if (!process_single_file(filepath, first_paper, tmp_conf)) {
                continue;
            }
            ++counter_cas_files;
            cout << "total number of cas files added for " << input_cas_dir_path.filename().string() << ": "
                 << to_string(counter_cas_files) << endl;
        } else if (is_directory(dir_it->status())) {
            path subdir(dir_it->path().string().c_str());
            for (directory_iterator dir_it2(subdir); dir_it2 != directory_iterator(); ++dir_it2) {
                if (is_regular_file(dir_it2->status())) {
                    std::string filepath(dir_it2->path().string().c_str());
                    if (!process_single_file(filepath, first_paper, tmp_conf)) {
                        continue;
                    }
                    ++counter_cas_files;
                    cout << "total number of cas files added for "
                         << input_cas_dir_path.filename().string() << ": " << to_string(counter_cas_files) << endl;
                }
            }
        }
    }
    remove(index_dir + "/subindex.config");
    std::ofstream ofs;
    ofs.open (index_dir + "/subindex.config", std::ofstream::out | std::ofstream::app);
    for (const auto& subindex : available_subindices) {
        ofs << subindex << endl;
    }
    ofs.close();
}

void IndexManager::create_subindex_dir_structure(const string &index_path) {
    if (!exists(index_path)) {
        create_directories(index_path);
        create_directories(index_path + "/fulltext");
        create_directories(index_path + "/fulltext_cs");
        create_directories(index_path + "/sentence");
        create_directories(index_path + "/sentence_cs");
    }
}

TmpConf IndexManager::write_tmp_conf_files(const string &index_path) {
    // temp conf files
    std::string temp_dir;
    bool dir_created = false;
    while (!dir_created) {
        temp_dir = Utils::get_temp_dir_path();
        dir_created = create_directories(temp_dir);
    }
    Utils::write_index_descriptor(index_path, temp_dir + "/Tpcas2SingleIndex.xml", temp_dir);
    TmpConf tmpConf = TmpConf();
    tmpConf.index_descriptor = temp_dir + "/Tpcas2SingleIndex.xml";
    tmpConf.new_index_flag = temp_dir + "/newindexflag";
    tmpConf.tmp_dir = temp_dir;
    return tmpConf;
}

void IndexManager::add_cas_file_to_index(const char* file_path, string index_descriptor, string temp_dir_path) {
    std::string gzfile(file_path);
    string source_file = gzfile;
    boost::replace_all(source_file, ".tpcas.gz", ".bib");
    string bib_file = source_file;
    path source = source_file;
    string filename = source.filename().string();
    boost::replace_all(filename, ".tpcas.gz", ".bib");
    if(gzfile.find(".tpcas") == std::string::npos) {
        //std::cerr << "No .tpcas file found for file " << source.filename().string() << endl;
        return;
    }
    if(!exists(bib_file)) {
        //std::cerr << "No .bib file found for file " << source.filename().string() << endl;
        return;
    }
    string bib_file_temp = temp_dir_path + "/" + filename;
    path dst  = temp_dir_path + "/" + filename;
    boost::filesystem::copy_file(source,dst,copy_option::overwrite_if_exists);
    const char * descriptor = index_descriptor.c_str();
    string tpcasfile = Utils::decompress_gzip(gzfile, temp_dir_path);

    try {
        /* Create/link up to a UIMACPP resource manager instance (singleton) */
        (void) uima::ResourceManager::createInstance("TPCAS2LINDEXAE");
        uima::ErrorInfo errorInfo;
        uima::AnalysisEngine * pEngine
                = uima::Framework::createAnalysisEngine(descriptor, errorInfo);
        if (errorInfo.getErrorId() != UIMA_ERR_NONE) {
            std::cerr << std::endl
                      << "  Error string  : "
                      << uima::AnalysisEngine::getErrorIdAsCString(errorInfo.getErrorId())
                      << std::endl
                      << "  UIMACPP Error info:" << std::endl
                      << errorInfo << std::endl;
            exit((int) errorInfo.getErrorId());
        }
        uima::TyErrorId utErrorId; // Variable to store UIMACPP return codes
        /* Get a new CAS */
        uima::CAS* cas = pEngine->newCAS();
        if (cas == nullptr) {
            std::cerr << "pEngine->newCAS() failed." << std::endl;
            exit(1);
        }
        /* process input / cas */
        try {
            /* initialize from an xmicas */
            XMLCh* native = XMLString::transcode(tpcasfile.c_str());
            LocalFileInputSource fileIS(native);
            XMLString::release(&native);
            uima::XmiDeserializer::deserialize(fileIS, *cas, true);
            std::string filename(tpcasfile);
            string filehash = gettpfnvHash(*cas);
            /* process the CAS */
            auto text = getFulltext(*cas);
            if (text.length() > 0) {
                ((uima::AnalysisEngine *) pEngine)->process(*cas);
            } else {
                cout << "Skip file." << endl;
            }
        } catch (uima::Exception e) {
            uima::ErrorInfo errInfo = e.getErrorInfo();
            std::cerr << "Error " << errInfo.getErrorId() << " " << errInfo.getMessage() << std::endl;
            std::cerr << errInfo << std::endl;
        }
        /* call collectionProcessComplete */
        utErrorId = pEngine->collectionProcessComplete();
        /* Free annotator */
        utErrorId = pEngine->destroy();
        delete cas;
        delete pEngine;
        std::remove(tpcasfile.c_str()); //delete uncompressed temp casfile
        std::remove(bib_file_temp.c_str());
    } catch (uima::Exception e) {
        std::cerr << "Exception: " << e << std::endl;
    }
}

bool IndexManager::process_single_file(const string& filepath, bool& first_paper, const TmpConf& tmp_conf) {
    if (filepath.find(".tpcas") == std::string::npos)
        return false;
    cout << "processing cas file: " << filepath << endl;
    if (first_paper) {
        add_cas_file_to_index(filepath.c_str(), tmp_conf.index_descriptor, tmp_conf.tmp_dir);
        first_paper = false;
        boost::filesystem::remove(tmp_conf.new_index_flag);
    } else {
        add_cas_file_to_index(filepath.c_str(), tmp_conf.index_descriptor, tmp_conf.tmp_dir);
    }
    return true;
}

void IndexManager::add_file_to_index(const std::string &file_path, const std::string &literature,
                                     int max_num_papers_per_subindex)
{
    string out_dir = index_dir + "/" + literature;
    string subindex_dir;
    int counter_cas_files(0);
    if (available_literatures.find(literature) != available_literatures.end()) {
        string largest_subindex_name = literature + "_0";
        for (directory_iterator dir_it(index_dir); dir_it != directory_iterator(); ++dir_it) {
            string lit = dir_it->path().string().substr(0, dir_it->path().string().find_last_of("_"));
            if (lit == literature && dir_it->path().string() > largest_subindex_name) {
                largest_subindex_name = dir_it->path().string();
            }
        }
        IndexReaderPtr indexReader = readers_map[largest_subindex_name + "_fulltext"];
        counter_cas_files = indexReader->numDocs();
    } else {
        available_literatures.insert(literature);
    }

    bool first_paper;
    TmpConf tmp_conf = TmpConf();
    if (counter_cas_files % max_num_papers_per_subindex == 0) {
        // create new subindex
        subindex_dir = out_dir + "_" + to_string(counter_cas_files / max_num_papers_per_subindex);
        available_subindices.insert(path(subindex_dir).filename().string());
        tmp_conf = write_tmp_conf_files(subindex_dir);
        first_paper = true;
        if (!exists(tmp_conf.new_index_flag)) {
            std::ofstream f_newindexflag(tmp_conf.new_index_flag.c_str());
            f_newindexflag << "newindexflag";
            f_newindexflag.close();
        }
        create_subindex_dir_structure(subindex_dir);
    }
    process_single_file(file_path, first_paper, tmp_conf);
    ++counter_cas_files;
    cout << "total number of cas files added for " << literature << ": " << to_string(counter_cas_files) << endl;
    remove(index_dir + "/subindex.config");
    std::ofstream ofs;
    ofs.open (index_dir + "/subindex.config", std::ofstream::out | std::ofstream::app);
    for (const auto& subindex : available_subindices) {
        ofs << subindex << endl;
    }
    ofs.close();
}

void IndexManager::remove_file_from_index(const std::string &identifier, const string& literature) {
    // document - case insensitive index
    remove_document_from_specific_subindex(identifier, literature, QueryType::document, false);
    // document - case sensitive index
    remove_document_from_specific_subindex(identifier, literature, QueryType::document, true);
    // sentence - case insensitive index
    remove_document_from_specific_subindex(identifier, literature, QueryType::sentence_with_ids, false);
    // sentence - case sensitive index
    remove_document_from_specific_subindex(identifier, literature, QueryType::sentence_with_ids, true);
}

void IndexManager::remove_document_from_specific_subindex(const string& identifier, const string& literature,
                                                          QueryType type, bool case_sensitive)
{
    Collection<IndexReaderPtr> subreaders = get_subreaders({literature}, type, case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subreaders, false);
    AnalyzerPtr analyzer;
    if (case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_30, L"identifier", analyzer);
    String query_str = L"identifier:" + String(identifier.begin(), identifier.end());
    QueryPtr luceneQuery = parser->parse(query_str);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(maxHits, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    for (const auto& document : matchesCollection) {
        multireader->deleteDocument(document->doc);
    }
    multireader->close();
}



