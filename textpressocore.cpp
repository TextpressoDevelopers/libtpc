/**
    Project: libtpc
    File name: reader.h

    @author valerio
    @version 1.0 7/18/17.
*/

#include "textpressocore.h"
#include "lucene-custom/CaseSensitiveAnalyzer.h"
#include "lucene-custom/LazySelector.h"
#include <lucene++/LuceneHeaders.h>
#include <lucene++/FieldCache.h>
#include <lucene++/CompressionTools.h>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <chrono>
#include <numeric>
#include <unordered_map>

using namespace std;
using namespace tpc::reader;
using namespace Lucene;

SearchResults TpcIndexReader::search_documents(const string& index_root_dir, const Query& query,
                                                 const set<string>& doc_ids)
{
    AnalyzerPtr analyzer;
    if (query.case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    Collection<IndexReaderPtr> subReaders = get_subreaders(query.literatures, index_root_dir, query.type,
                                                           query.case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, true);
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

set<String> TpcIndexReader::compose_field_set(const set<string> &include_fields, const set<string> &exclude_fields,
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

Collection<IndexReaderPtr> TpcIndexReader::get_subreaders(const vector<string>& literatures,
                                                            const string& index_root_dir, QueryType type,
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
        string index_dir;
        index_dir.append(index_root_dir); index_dir.append("/"); index_dir.append(literature);
        index_dir.append("/"); index_dir.append(index_type);
        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(String(index_dir.begin(), index_dir.end())), true);
        if (reader) {
            subReaders.add(reader);
        } else {
            throw index_exception();
        }
    }
    return subReaders;
}

SearchResults TpcIndexReader::read_documents_summaries(
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

SearchResults TpcIndexReader::read_sentences_summaries(const Collection<ScoreDocPtr> &matches_collection,
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

vector<DocumentDetails> TpcIndexReader::get_documents_details(const vector<DocumentSummary> &doc_summaries,
                                                                const string &index_root_dir,
                                                                const vector<string> &literatures,
                                                                bool sort_by_year,
                                                                bool include_sentences,
                                                                set<string> include_doc_fields,
                                                                set<string> include_match_sentences_fields,
                                                                const set<string> &exclude_doc_fields,
                                                                const set<string> &exclude_match_sentences_fields)
{
    map<string, DocumentDetails> doc_map;
    set<String> doc_f = compose_field_set(include_doc_fields, exclude_doc_fields, {"year"});
    FieldSelectorPtr doc_fsel = newLucene<LazySelector>(doc_f);
    AnalyzerPtr analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    Collection<IndexReaderPtr> docSubReaders = get_subreaders(literatures, index_root_dir, QueryType::document);
    MultiReaderPtr docMultireader = newLucene<MultiReader>(docSubReaders, true);
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
        sentSubReaders = get_subreaders(literatures, index_root_dir, QueryType::sentence_without_ids);
        sentMultireader = newLucene<MultiReader>(sentSubReaders, true);
        sentParser = newLucene<QueryParser>(LuceneVersion::LUCENE_30,
                                            String(sentence_indexname.begin(), sentence_indexname.end()),
                                            analyzer);
        sent_searcher = newLucene<IndexSearcher>(sentMultireader);
    }
    vector<DocumentDetails> documentsDetails = read_documents_details(doc_summaries, docParser, searcher,
                                                                      doc_fsel, doc_f);
    map<string, DocumentSummary> doc_summaries_map;
    for (const auto& doc_summary : doc_summaries) {
        doc_summaries_map[doc_summary.identifier] = doc_summary;
    }
    for (DocumentDetails& docDetails : documentsDetails ) {
        if (include_sentences) {
            update_sentences_details_for_document(doc_summaries_map[docDetails.identifier], docDetails, sentParser,
                                                  sent_searcher, sent_fsel, sent_f);
        }
        doc_map[docDetails.identifier] = docDetails;
    }
    docMultireader->close();
    if (include_sentences) {
        sentMultireader->close();
    }
    vector<DocumentDetails> results;
    std::transform(doc_map.begin(), doc_map.end(), std::back_inserter(results),
                   boost::bind(&map<string, DocumentDetails>::value_type::second, _1));
    if (sort_by_year) {
        sort(results.begin(), results.end(), document_year_score_gt);
    } else {
        sort(results.begin(), results.end(), document_score_gt);
    }
    return results;
}

DocumentDetails TpcIndexReader::get_document_details(const DocumentSummary& doc_summary,
                                                       const string& index_root_dir,
                                                       const vector<string>& literatures,
                                                       bool include_sentences,
                                                       set<string> include_doc_fields,
                                                       set<string> include_match_sentences_fields,
                                                       const set<string>& exclude_doc_fields,
                                                       const set<string>& exclude_match_sentences_fields)
{
    return get_documents_details({doc_summary}, index_root_dir, literatures, false, include_sentences,
                                 include_doc_fields, include_match_sentences_fields, exclude_doc_fields,
                                 exclude_match_sentences_fields)[0];
}

void TpcIndexReader::update_document_details(DocumentDetails &doc_details, String field, DocumentPtr doc_ptr) {
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

DocumentDetails TpcIndexReader::read_document_details(const DocumentSummary &doc_summary,
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

vector<DocumentDetails> TpcIndexReader::read_documents_details(const vector<DocumentSummary> &doc_summaries,
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
    string doc_query_str = "identifier:" + boost::algorithm::join(identifiers, " OR identifier:");
    QueryPtr luceneQuery = doc_parser->parse(String(doc_query_str.begin(), doc_query_str.end()));
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(maxHits, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    for (const auto& scoredoc : matchesCollection) {
        DocumentDetails documentDetails = DocumentDetails();
        DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
        for (const auto &f : fields) {
            update_document_details(documentDetails, f, docPtr);
        }
        documentDetails.score = scoremap[documentDetails.identifier];
        results.push_back(documentDetails);
    }
    sort(results.begin(), results.end(), document_score_gt);
    return results;
}


void TpcIndexReader::update_sentences_details_for_document(const DocumentSummary &doc_summary,
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