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
#include <regex>
#include "CASManager.h"
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

using namespace std;
using namespace tpc::index;
using namespace Lucene;
using namespace xercesc;
using namespace boost::filesystem;

SearchResults IndexManager::search_documents(const Query& query, bool matches_only, const set<string>& doc_ids,
                                             const Collection<ScoreDocPtr>& indexMatches)
{
    Collection<ScoreDocPtr> matchesCollection;
    Collection<IndexReaderPtr> subReaders = get_subreaders(query.type, query.case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, false);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    if (!indexMatches || indexMatches.size() == 0) {
        if (query.literatures.empty()) {
            throw tpc_exception("no literature information provided in the query object");
        }
        AnalyzerPtr analyzer;
        if (query.case_sensitive) {
            analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
        } else {
            analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
        }
        QueryParserPtr parser = newLucene<QueryParser>(
                LuceneVersion::LUCENE_30, query.type == QueryType::document ? L"fulltext" : L"sentence", analyzer);
        String query_str = String(query.query_text.begin(), query.query_text.end());
        string joined_lit = boost::algorithm::join(query.literatures, "ED\" OR corpus:\"BG");
        query_str = L"(corpus:\"BG" +  String(joined_lit.begin(), joined_lit.end()) + L"ED\") AND (" + query_str + L")";
        QueryPtr luceneQuery = parser->parse(query_str);
        String key_query_str;
        TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(MAX_HITS, true);
        if (!doc_ids.empty()) {
            AnalyzerPtr keyword_analyzer = newLucene<KeywordAnalyzer>();
            QueryParserPtr keyword_parser = newLucene<QueryParser>(
                    LuceneVersion::LUCENE_30, query.type == QueryType::document ? L"fulltext" : L"sentence",
                    keyword_analyzer);
            string joined_ids = boost::algorithm::join(doc_ids, " OR doc_id:");
            key_query_str += L" doc_id:" + String(joined_ids.begin(), joined_ids.end());
            BooleanQueryPtr booleanQuery = newLucene<BooleanQuery>();
            QueryPtr key_luceneQuery = keyword_parser->parse(key_query_str);
            booleanQuery->add(luceneQuery, BooleanClause::MUST);
            booleanQuery->add(key_luceneQuery, BooleanClause::MUST);
            searcher->search(booleanQuery, collector);
        } else {
            searcher->search(luceneQuery, collector);
        }
        matchesCollection = collector->topDocs()->scoreDocs;
    } else {
        matchesCollection = indexMatches;
    }
    SearchResults result = SearchResults();
    if (!matches_only) {
        if (query.type == QueryType::document) {
            result = read_documents_summaries(matchesCollection, subReaders, searcher);
        } else if (query.type == QueryType::sentence) {
            result = read_sentences_summaries(matchesCollection, subReaders, searcher, query.sort_by_year);
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
    } else {
        result.indexMatches = matchesCollection;
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

Collection<IndexReaderPtr> IndexManager::get_subreaders(QueryType type, bool case_sensitive)
{

    string index_type;
    if (type == QueryType::document) {
        index_type = case_sensitive ? DOCUMENT_INDEXNAME_CS : DOCUMENT_INDEXNAME;
    } else if (type == QueryType::sentence) {
        index_type = case_sensitive ? SENTENCE_INDEXNAME_CS : SENTENCE_INDEXNAME;
    }
    Collection<IndexReaderPtr> subReaders = Collection<IndexReaderPtr>::newInstance(0);
    directory_iterator enditr;
    // add subindices for main index
    for(directory_iterator itr(index_dir); itr != enditr; itr++) {
        if (is_directory(itr->status()) && regex_match(itr->path().string(), regex(".*\\/" + SUBINDEX_NAME +
                                                                                           "\\_[0-9]+"))) {
            string index_id(itr->path().string());
            index_id.append("/");
            index_id.append(index_type);
            if (readers_map.find(itr->path().string() + "/" + index_type) == readers_map.end()) {
                if (exists(path(index_id + "/segments.gen"))) {
                    readers_map[index_id] = IndexReader::open(FSDirectory::open(String(index_id.begin(),
                                                                                       index_id.end())), readonly);
                }
            }
            subReaders.add(readers_map[index_id]);
        }
    }
    // add subindices for external indices
    for (const auto& extra_idx_dir : extra_index_dirs) {
        for (directory_iterator itr(extra_idx_dir); itr != enditr; itr++) {
            if (is_directory(itr->status()) && regex_match(itr->path().string(), regex(".*\\/" + SUBINDEX_NAME +
                                                                                       "\\_[0-9]+"))) {
                string index_id(itr->path().string());
                index_id.append("/");
                index_id.append(index_type);
                if (readers_map.find(itr->path().string() + "/" + index_type) == readers_map.end()) {
                    if (exists(path(index_id + "/segments.gen"))) {
                        readers_map[index_id] = IndexReader::open(FSDirectory::open(String(index_id.begin(),
                                                                                           index_id.end())), readonly);
                    }
                }
                subReaders.add(readers_map[index_id]);
            }
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
    if (matches_collection.size() < FIELD_CACHE_MIN_HITS) {
        set<String> fields;
        if (sort_by_year) {
            fields = {L"doc_id", L"year"};
        } else {
            fields = {L"doc_id"};
        }
        FieldSelectorPtr fsel = newLucene<LazySelector>(fields);
        for (const auto& scoredoc : matches_collection) {
            DocumentSummary document;
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            String identifier = docPtr->get(L"doc_id");
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
        Collection<String> fieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"doc_id");
        Collection<String> yearFieldCache;
        if (sort_by_year) {
            yearFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"year");
        }
        int offset = 0;
        for (auto docid : docids) {
            while ((docid - offset) >= fieldCache.size()) {
                offset += fieldCache.size();
                FieldCache::DEFAULT()->purge(subreaders[readerIndex]);
                fieldCache = FieldCache::DEFAULT()->getStrings(subreaders[++readerIndex], L"doc_id");
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
        for (auto& reader : readers_map) {
            FieldCache::DEFAULT()->purge(reader.second);
        }
        docids.clear();
        scores.clear();
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
                                                       SearcherPtr searcher, bool sort_by_year)
{
    SearchResults result = SearchResults();
    unordered_map<string, DocumentSummary> doc_map;
    doc_map.reserve(100000);
    // for small searches, read the fields with a lazy loader
    if (matches_collection.size() < FIELD_CACHE_MIN_HITS) {
        set<String> fields =  {L"doc_id", L"sentence_id"};
        if (sort_by_year) {
            fields.insert(L"year");
        }
        FieldSelectorPtr fsel = newLucene<LazySelector>(fields);
        for (const auto& scoredoc : matches_collection) {
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            String identifier = docPtr->get(L"doc_id");
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
            sentence.sentence_id = StringUtils::toInt(docPtr->get(L"sentence_id"));
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
        Collection<String> docIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex],
                                                                               L"doc_id");
        Collection<int> sentIdFieldCache;
        sentIdFieldCache = FieldCache::DEFAULT()->getInts(subreaders[readerIndex], L"sentence_id");
        Collection<String> yearFieldCache;
        if (sort_by_year) {
            yearFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"year");
        }
        int offset = 0;
        for (int idx : idx_vec) {
            int docid = docids[idx];
            while ((docid - offset) >= docIdFieldCache.size()) {
                offset += docIdFieldCache.size();
                FieldCache::DEFAULT()->purge(subreaders[readerIndex]);
                ++readerIndex;
                docIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"doc_id");
                sentIdFieldCache = FieldCache::DEFAULT()->getInts(subreaders[readerIndex], L"sentence_id");
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
            sentence.sentence_id = sentIdFieldCache[docid - offset];
            sentence.score = scores[idx];
            doc_map[identifier_str].matching_sentences.push_back(sentence);
        }
        for (auto& reader : readers_map) {
            FieldCache::DEFAULT()->purge(reader.second);
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
    AnalyzerPtr analyzer = newLucene<KeywordAnalyzer>();
    Collection<IndexReaderPtr> docSubReaders = get_subreaders(QueryType::document);
    MultiReaderPtr docMultireader = newLucene<MultiReader>(docSubReaders, false);
    QueryParserPtr docParser = newLucene<QueryParser>(LuceneVersion::LUCENE_30,
                                                      String(DOCUMENT_INDEXNAME.begin(), DOCUMENT_INDEXNAME.end()),
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
        sentSubReaders = get_subreaders(QueryType::sentence);
        sentMultireader = newLucene<MultiReader>(sentSubReaders, false);
        sentParser = newLucene<QueryParser>(LuceneVersion::LUCENE_30,
                                            String(SENTENCE_INDEXNAME.begin(), SENTENCE_INDEXNAME.end()),
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
                                                     bool include_sentences,
                                                     set<string> include_doc_fields,
                                                     set<string> include_match_sentences_fields,
                                                     const set<string>& exclude_doc_fields,
                                                     const set<string>& exclude_match_sentences_fields)
{
    return get_documents_details({doc_summary}, false, include_sentences,
                                 include_doc_fields, include_match_sentences_fields, exclude_doc_fields,
                                 exclude_match_sentences_fields)[0];
}

void IndexManager::update_document_details(DocumentDetails &doc_details, String field, DocumentPtr doc_ptr) {
    if (field == L"doc_id") {
        String identifier = doc_ptr->get(StringUtils::toString("doc_id"));
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
    } else if (field == L"corpus") {
        String literature = doc_ptr->get(StringUtils::toString("corpus"));
        string raw_lit = string(literature.begin(), literature.end());
        raw_lit = raw_lit.substr(2, raw_lit.length() - 4);
        boost::split_regex(doc_details.corpora, raw_lit, boost::regex("ED BG"));
    } else if (field == L"fulltext_compressed") {
        String fulltext = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("fulltext_compressed")));
        doc_details.fulltext = string(fulltext.begin(), fulltext.end());
    } else if (field == L"type_compressed") {
        String type = CompressionTools::decompressString(
                doc_ptr->getBinaryValue(StringUtils::toString("type_compressed")));
        doc_details.type = string(type.begin(), type.end());
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
    string doc_query_str = "doc_id:" + doc_summary.identifier;
    QueryPtr luceneQuery = doc_parser->parse(String(doc_query_str.begin(), doc_query_str.end()));
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(MAX_HITS, true);
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
        identifiersItEnd = distance(identifiersItBegin, identifiers.end()) <= MAX_NUM_DOCIDS_IN_QUERY ?
                           identifiers.end() : identifiersItBegin + MAX_NUM_DOCIDS_IN_QUERY;
        string doc_query_str = "doc_id:\"" + boost::algorithm::join(vector<string>(identifiersItBegin,
                                                                                     identifiersItEnd),
                                                                      "\" OR doc_id:\"");
        doc_query_str.append("\"");
        QueryPtr luceneQuery = doc_parser->parse(String(doc_query_str.begin(), doc_query_str.end()));
        TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(MAX_HITS, true);
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
        sentencesIdsItEnd = distance(sentencesIdsItBegin, sentencesIds.end())  <= MAX_NUM_SENTENCES_IN_QUERY ?
                            sentencesIds.end() : sentencesIdsItBegin + MAX_NUM_SENTENCES_IN_QUERY;
        string sent_query_str = "doc_id:\"" + doc_summary.identifier + "\" AND (sentence_id:\"" +
                                boost::algorithm::join(vector<string>(sentencesIdsItBegin, sentencesIdsItEnd),
                                                       "\" OR sentence_id:\"") + "\")";
        QueryPtr luceneQuery = sent_parser->parse(String(sent_query_str.begin(), sent_query_str.end()));
        collector = TopScoreDocCollector::create(MAX_HITS, true);
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
    string out_dir = index_dir + "/" + SUBINDEX_NAME;
    string subindex_dir;
    int counter_cas_files(0);
    bool first_paper;
    TmpConf tmp_conf = TmpConf();
    recursive_directory_iterator it_end;
    for (recursive_directory_iterator dir_it(input_cas_dir_path); dir_it != it_end; ++dir_it) {
        if (counter_cas_files % max_num_papers_per_subindex == 0 && first_paper == false) {
            // create new subindex
            subindex_dir = out_dir + "_" + to_string(counter_cas_files / max_num_papers_per_subindex);
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
            cout << "total number of cas files added: "
                 << to_string(counter_cas_files) << endl;
        }
    }
    update_corpus_counter();
    save_corpus_counter();
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

int IndexManager::add_cas_file_to_index(const char* file_path, string index_descriptor, string temp_dir_path) {
    std::string gzfile(file_path);
    string source_file = gzfile;
    boost::replace_all(source_file, ".tpcas.gz", ".bib");
    string bib_file = source_file;
    path source = source_file;
    string filename = source.filename().string();
    boost::replace_all(filename, ".tpcas.gz", ".bib");
    if(gzfile.find(".tpcas") == std::string::npos) {
        //std::cerr << "No .tpcas file found for file " << source.filename().string() << endl;
        return 0;
    }
    if(!exists(bib_file)) {
        //std::cerr << "No .bib file found for file " << source.filename().string() << endl;
        return 0;
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
        return 1;
    } catch (uima::Exception e) {
        std::cerr << "Exception: " << e << std::endl;
        return 0;
    }
}

bool IndexManager::process_single_file(const string& filepath, bool& first_paper, const TmpConf& tmp_conf) {
    if (filepath.find(".tpcas.gz") == std::string::npos)
        return false;
    cout << "processing cas file: " << filepath << endl;
    if (first_paper) {
        if (add_cas_file_to_index(filepath.c_str(), tmp_conf.index_descriptor, tmp_conf.tmp_dir) == 1) {
            first_paper = false;
            boost::filesystem::remove(tmp_conf.new_index_flag);
        } else {
            return false;
        }
    } else {
        if (add_cas_file_to_index(filepath.c_str(), tmp_conf.index_descriptor, tmp_conf.tmp_dir) == 0) {
            return false;
        }
    }
    return true;
}

void IndexManager::add_file_to_index(const std::string &file_path, int max_num_papers_per_subindex)
{
    string out_dir = index_dir + "/" + SUBINDEX_NAME;
    string subindex_dir;
    int counter_cas_files(0);
    int largest_subindex_num(0);
    for (directory_iterator dir_it(index_dir); dir_it != directory_iterator(); ++dir_it) {
        string actual_subidx_name = dir_it->path().string().substr(0, dir_it->path().string().find_last_of("_"));
        string actual_subidx_num = dir_it->path().string().substr(dir_it->path().string().find_last_of("_") + 1,
                                                                  dir_it->path().string().size());
        if (actual_subidx_name == SUBINDEX_NAME && stoi(actual_subidx_num) > largest_subindex_num) {
                largest_subindex_num = stoi(actual_subidx_num);
            }
        }
        IndexReaderPtr indexReader = readers_map[SUBINDEX_NAME + "_" + to_string(largest_subindex_num) + "_fulltext"];
        counter_cas_files = indexReader->numDocs();
    bool first_paper;
    TmpConf tmp_conf = TmpConf();
    if (counter_cas_files % max_num_papers_per_subindex == 0) {
        // create new subindex
        subindex_dir = out_dir + "_" + to_string(counter_cas_files / max_num_papers_per_subindex);
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
    cout << "total number of cas files added: " << to_string(counter_cas_files) << endl;
}

void IndexManager::remove_file_from_index(const std::string &identifier) {
    // document - case insensitive index
    remove_document_from_index(identifier, QueryType::document, false);
    // document - case sensitive index
    remove_document_from_index(identifier, QueryType::document, true);
    // sentence - case insensitive index
    remove_document_from_index(identifier, QueryType::sentence, false);
    // sentence - case sensitive index
    remove_document_from_index(identifier, QueryType::sentence, true);
}

void IndexManager::remove_document_from_index(const string& identifier, QueryType type, bool case_sensitive)
{
    Collection<IndexReaderPtr> subreaders = get_subreaders(type, case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subreaders, false);
    AnalyzerPtr analyzer;
    if (case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_30, L"filepath", analyzer);
    String query_str = L"filepath:" + String(identifier.begin(), identifier.end());
    QueryPtr luceneQuery = parser->parse(query_str);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(MAX_HITS, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    for (const auto& document : matchesCollection) {
        multireader->deleteDocument(document->doc);
    }
    multireader->close();
}

std::vector<std::string> IndexManager::get_available_corpora() {
    vector<string> corpora_vec;
    for (const auto &cat_regex : tpc::cas::PMCOA_CAT_REGEX) {
        corpora_vec.push_back(cat_regex.first);
    }
    corpora_vec.push_back(tpc::cas::PMCOA_UNCLASSIFIED);
    corpora_vec.push_back(tpc::cas::CELEGANS);
    corpora_vec.push_back(tpc::cas::CELEGANS_SUP);
    return corpora_vec;
}

std::vector<std::string> IndexManager::get_corpora_for_external_index(const std::string &external_idx_location) {
    vector<string> corpora_vec;
    load_corpus_counter(external_idx_location);
    for (const auto& cd_conter_map : external_corpus_doc_counter_map[external_idx_location]) {
        corpora_vec.push_back(cd_conter_map.first);
    }
    return corpora_vec;
}

int IndexManager::get_num_articles_in_corpus(const string &corpus, const string& external_idx_location) {
    if (external_idx_location.empty()) {
        load_corpus_counter();
        return corpus_doc_counter[corpus];
    } else {
        load_corpus_counter(external_idx_location);
        return external_corpus_doc_counter_map[external_idx_location][corpus];
    }
}

void IndexManager::save_corpus_counter() {
    std::ofstream ofs(index_dir + "/" + CORPUS_COUNTER_FILENAME);
    {
        boost::archive::text_oarchive oa(ofs);
        oa << corpus_doc_counter;
    }
}

void IndexManager::load_corpus_counter(const string& external_idx_location) {
    if (external_idx_location.empty()) {
        std::ifstream ifs(index_dir + "/" + CORPUS_COUNTER_FILENAME, std::ios::binary);
        if (ifs) {
            boost::archive::text_iarchive ia(ifs);
            ia >> corpus_doc_counter;
        }
    } else {
        std::ifstream ifs(external_idx_location + "/" + CORPUS_COUNTER_FILENAME, std::ios::binary);
        if (ifs) {
            boost::archive::text_iarchive ia(ifs);
            ia >> external_corpus_doc_counter_map[external_idx_location];
        }
    }
}

void IndexManager::update_corpus_counter() {
    if (!external) {
        for (const auto &corpus_regex : tpc::cas::PMCOA_CAT_REGEX) {
            corpus_doc_counter[corpus_regex.first] = get_num_docs_in_corpus_from_index(corpus_regex.first);
        }
        corpus_doc_counter[tpc::cas::CELEGANS] = get_num_docs_in_corpus_from_index(tpc::cas::CELEGANS);
        corpus_doc_counter[tpc::cas::CELEGANS_SUP] = get_num_docs_in_corpus_from_index(tpc::cas::CELEGANS_SUP);
    } else {
        map<string, set<string>> external_paper_lit_map;
        std::ifstream ifs(index_dir + "/../uploadedfiles/lit.cfg", std::ios::binary);
        if (ifs) {
            boost::archive::text_iarchive ia(ifs);
            ia >> external_paper_lit_map;
        }
        for (const auto& lit : external_paper_lit_map["__all_literatures__"]) {
            corpus_doc_counter[lit] = get_num_docs_in_corpus_from_index(lit);
        }
    }
}

int IndexManager::get_num_docs_in_corpus_from_index(const string& corpus) {
    Collection<ScoreDocPtr> matchesCollection;
    Collection<IndexReaderPtr> subReaders = get_subreaders(QueryType::document, false);
    MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, false);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    AnalyzerPtr analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_30, L"fulltext", analyzer);
    String query_str = L"corpus:\"BG" + String(corpus.begin(), corpus.end()) + L"ED\"";
    QueryPtr luceneQuery = parser->parse(query_str);
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(MAX_HITS, true);
    searcher->search(luceneQuery, collector);
    matchesCollection = collector->topDocs()->scoreDocs;
    return matchesCollection.size();
}

int IndexManager::get_num_subindices() {
    int subidx_counter(0);
    directory_iterator enditr;
    for(directory_iterator itr(index_dir); itr != enditr; itr++) {
        if (is_directory(itr->status()) && regex_match(itr->path().string(), regex(SUBINDEX_NAME + "_([0-9]+)"))) {
            ++subidx_counter;
        }
    }
    for (const auto& external_idx_dir : extra_index_dirs) {
        for(directory_iterator itr(external_idx_dir); itr != enditr; itr++) {
            if (is_directory(itr->status()) && regex_match(itr->path().string(), regex(SUBINDEX_NAME + "_([0-9]+)"))) {
                ++subidx_counter;
            }
        }
    }
    return subidx_counter;
}

void IndexManager::add_external_index(const std::string &index_dir) {
    extra_index_dirs.insert(index_dir);
}

void IndexManager::remove_external_index(const std::string &index_dir) {
    extra_index_dirs.erase(index_dir);
}

void IndexManager::remove_all_external_indices() {
    for (auto& index_loc : extra_index_dirs) {
        remove_external_index(index_loc);
    }
}





