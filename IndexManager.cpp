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
#include <db_cxx.h>
#include <dbstl_map.h>
#include <dbstl_vector.h>
#include "DataStructures.h"

using namespace std;
using namespace tpc::index;
using namespace Lucene;
using namespace xercesc;
using namespace boost::filesystem;

SearchResults IndexManager::search_documents(const Query& query, bool matches_only, const set<string>& doc_ids,
                                             const SearchResults& partialResults)
{
    Collection<ScoreDocPtr> matchesCollection;
    Collection<ScoreDocPtr> externalMatchesCollection;
    Collection<IndexReaderPtr> subReaders = get_subreaders(query.type, query.case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, false);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    if ((!partialResults.partialIndexMatches || partialResults.partialIndexMatches.empty()) && (!partialResults.partialExternalMatches ||
            partialResults.partialExternalMatches.empty())) {
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
        string query_text = query.get_query_text();
        String query_str = String(query_text.begin(), query_text.end());
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
        matchesCollection = partialResults.partialIndexMatches;
        if (has_external_index()) {
            externalMatchesCollection = partialResults.partialExternalMatches;
        }
    }
    SearchResults result = SearchResults();
    SearchResults externalResults = SearchResults();
    if (!matches_only) {
        if (query.type == QueryType::document) {
            result = read_documents_summaries(matchesCollection, query.sort_by_year);
            if (has_external_index() && externalMatchesCollection) {
                externalResults = externalIndexManager->read_documents_summaries(externalMatchesCollection,
                                                                                query.sort_by_year);
                result.update(externalResults);
            }
        } else if (query.type == QueryType::sentence) {
            result = read_sentences_summaries(matchesCollection, query.sort_by_year);
            result.query = query;
            result.total_num_sentences = matchesCollection.size();
            if (has_external_index() && externalMatchesCollection) {
                externalResults = externalIndexManager->read_sentences_summaries(externalMatchesCollection,
                                                                                 query.sort_by_year);
                externalResults.total_num_sentences = externalMatchesCollection.size();
                result.update(externalResults);
            }
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
        result.partialIndexMatches = matchesCollection;
        if (has_external_index()) {
            result.partialExternalMatches = externalIndexManager->search_documents(query, true, doc_ids).partialIndexMatches;
        }
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
    return subReaders;
}

SearchResults IndexManager::read_documents_summaries(const Collection<ScoreDocPtr> &matches_collection,
                                                     bool sort_by_year)
{
    SearchResults result = SearchResults();
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb = NULL;
    try {
        env.set_error_stream(&cerr);
        env.open((index_dir + "/db").c_str(), DB_INIT_MPOOL, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map_year;
        if (sort_by_year) {
            pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
            pdb->open(NULL, "doc_map.db", NULL, DB_BTREE, DB_RDONLY, 0);
            huge_map_year = HugeMap(pdb, &env);
        }
        for (const auto& docresult : matches_collection) {
            DocumentSummary document;
            if (external) {
                document.documentType = DocumentType::external;
            }
            document.lucene_internal_id = docresult->doc;
            document.score = docresult->score;
            if (sort_by_year) {
                document.year = huge_map_year[docresult->doc];
            }
            result.hit_documents.push_back(document);
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
        exit(EXIT_FAILURE);
    } catch (std::exception& e) {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
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
                                                     bool sort_by_year)
{
    SearchResults result = SearchResults();
    unordered_map<string, DocumentSummary> doc_map;
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb;
    try {
        env.set_error_stream(&cerr);
        env.open((index_dir + "/db").c_str(), DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "sent_map.db", NULL, DB_BTREE, DB_RDONLY, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        for (const auto& scoredoc : matches_collection) {
            vector<string> id_year_arr;
            string line = huge_map[scoredoc->doc];
            boost::algorithm::split(id_year_arr, line, boost::is_any_of("|"));
            if (doc_map.find(id_year_arr[0]) == doc_map.end()) {
                DocumentSummary document;
                if (external) {
                    document.documentType = DocumentType::external;
                }
                document.identifier = id_year_arr[0];
                if (sort_by_year) {
                    document.year = id_year_arr[1];
                }
                document.score = 0;
                doc_map.insert({id_year_arr[0], document});
            }
            doc_map[id_year_arr[0]].score += scoredoc->score;
            SentenceSummary sentence;
            sentence.lucene_internal_id = scoredoc->doc;
            sentence.score = scoredoc->score;
            doc_map[id_year_arr[0]].matching_sentences.push_back(sentence);
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
        exit(EXIT_FAILURE);
    } catch (std::exception& e) {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
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
    vector<DocumentSummary> summaries;
    bool use_lucene_internal_ids = all_of(doc_summaries.begin(), doc_summaries.end(), [](DocumentSummary d) {
        return d.lucene_internal_id != -1;
    });
    copy_if(doc_summaries.begin(), doc_summaries.end(), back_inserter(summaries), [this](DocumentSummary doc) {
        return (external && doc.documentType == DocumentType::main);
    });
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
    results = read_documents_details(doc_summaries, docParser, searcher, doc_fsel, doc_f, use_lucene_internal_ids,
                                     docMultireader);
    map<string, DocumentSummary> doc_summaries_map;
    for (const auto &doc_summary : doc_summaries) {
        if (use_lucene_internal_ids) {
            doc_summaries_map[to_string(doc_summary.lucene_internal_id)] = doc_summary;
        } else {
            doc_summaries_map[doc_summary.identifier] = doc_summary;
        }
    }
    if (include_sentences) {
        for (DocumentDetails &docDetails : results) {
            if (use_lucene_internal_ids) {
                update_sentences_details_for_document(doc_summaries_map[to_string(docDetails.lucene_internal_id)],
                                                      docDetails, sentParser,
                                                      sent_searcher, sent_fsel, sent_f, true,
                                                      sentMultireader);
            } else {
                update_sentences_details_for_document(doc_summaries_map[docDetails.identifier],
                                                      docDetails, sentParser,
                                                      sent_searcher, sent_fsel, sent_f, true,
                                                      sentMultireader);
            }
        }
    }
    docMultireader->close();
    if (include_sentences) {
        sentMultireader->close();
    }
    if (!external && has_external_index()) {
        auto externalResults = externalIndexManager->get_documents_details(doc_summaries, sort_by_year,
                                                                           include_sentences, include_doc_fields,
                                                                           include_match_sentences_fields,
                                                                           exclude_doc_fields,
                                                                           exclude_match_sentences_fields);
        move(externalResults.begin(), externalResults.end(), back_inserter(results));
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

vector<DocumentDetails> IndexManager::read_documents_details(const vector<DocumentSummary> &doc_summaries,
                                                             QueryParserPtr doc_parser,
                                                             SearcherPtr searcher,
                                                             FieldSelectorPtr fsel,
                                                             const set<String> &fields, bool use_lucene_internal_ids,
                                                             MultiReaderPtr doc_reader)
{
    vector<DocumentDetails> results;
    if (use_lucene_internal_ids) {
        for (const auto &docSummary : doc_summaries) {
            DocumentDetails documentDetails = DocumentDetails();
            if (external) {
                documentDetails.documentType = DocumentType::external;
            }
            DocumentPtr docPtr = doc_reader->document(docSummary.lucene_internal_id, fsel);
            for (const auto &f : fields) {
                update_document_details(documentDetails, f, docPtr);
            }
            documentDetails.score = docSummary.score;
            results.push_back(documentDetails);
        }
    } else {
        vector<string> identifiers;
        map<string, double> scoremap;
        for (const auto &docSummary : doc_summaries) {
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
                if (external) {
                    documentDetails.documentType = DocumentType::external;
                }
                DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
                for (const auto &f : fields) {
                    update_document_details(documentDetails, f, docPtr);
                }
                documentDetails.score = scoremap[documentDetails.identifier];
                results.push_back(documentDetails);
            }
            identifiersItBegin = identifiersItEnd;
        }
    }
    sort(results.begin(), results.end(), document_score_gt);
    return results;
}


void IndexManager::update_sentences_details_for_document(const DocumentSummary &doc_summary,
                                                         DocumentDetails &doc_details,
                                                         QueryParserPtr sent_parser,
                                                         SearcherPtr searcher,
                                                         FieldSelectorPtr fsel, const set<String> &fields,
                                                         bool use_lucene_internal_ids, MultiReaderPtr sent_reader)
{
    vector<string> sentencesIds;
    map<int, double> sentScoreMap;
    if (use_lucene_internal_ids) {
        for (const SentenceSummary &sent : doc_summary.matching_sentences) {
            SentenceDetails sentenceDetails = SentenceDetails();
            for (const auto &f : fields) {
                if (f == L"sentence_id") {
                    sentenceDetails.sentence_id = StringUtils::toInt(
                            sent_reader->document(sent.lucene_internal_id, fsel)->get(L"sentence_id"));
                } else if (f == L"begin") {
                    sentenceDetails.doc_position_begin = StringUtils::toInt(
                            sent_reader->document(sent.lucene_internal_id, fsel)->get(L"begin"));
                } else if (f == L"end") {
                    sentenceDetails.doc_position_end = StringUtils::toInt(sent_reader->document(
                            sent.lucene_internal_id, fsel)->get(L"end"));
                } else if (f == L"sentence_compressed") {
                    String sentence = CompressionTools::decompressString(
                            sent_reader->document(sent.lucene_internal_id, fsel)->getBinaryValue(
                                    L"sentence_compressed"));
                    sentenceDetails.sentence_text = string(sentence.begin(), sentence.end());
                } else if (f == L"sentence_cat_compressed") {
                    String sentence_cat = CompressionTools::decompressString(
                            sent_reader->document(sent.lucene_internal_id, fsel)->getBinaryValue(
                                    L"sentence_cat_compressed"));
                    sentenceDetails.categories_string = string(sentence_cat.begin(), sentence_cat.end());
                }
            }
            sentenceDetails.score = sent.score;
            doc_details.sentences_details.push_back(sentenceDetails);
        }

    } else {
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
            string sent_query_str = "sentence_id:\"" +
                                    boost::algorithm::join(vector<string>(sentencesIdsItBegin, sentencesIdsItEnd),
                                                           "\" OR sentence_id:\"") + "\"";
            string docid_query_str = "doc_id:\"" + doc_details.identifier + "\"";
            BooleanQueryPtr booleanQuery = newLucene<BooleanQuery>();
            QueryPtr key_luceneQuery = sent_parser->parse(String(docid_query_str.begin(), docid_query_str.end()));
            QueryPtr luceneQuery = sent_parser->parse(String(sent_query_str.begin(), sent_query_str.end()));
            booleanQuery->add(luceneQuery, BooleanClause::MUST);
            booleanQuery->add(key_luceneQuery, BooleanClause::MUST);
            collector = TopScoreDocCollector::create(MAX_HITS, true);
            searcher->search(booleanQuery, collector);
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

}

void IndexManager::create_index_from_existing_cas_dir(const string &input_cas_dir, const set<string>& file_list,
                                                      int max_num_papers_per_subindex)
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
        string file_id = dir_it->path().parent_path().parent_path().filename().string()
                         + "/" + dir_it->path().parent_path().filename().string();
        if (is_regular_file(dir_it->status()) && boost::algorithm::ends_with(
                dir_it->path().filename().string(), ".tpcas.gz") && (file_list.empty() ||
                file_list.find(file_id) != file_list.end())) {
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

        string file_id = boost::filesystem::path(file_path).parent_path().parent_path().filename() + "/" +
                boost::filesystem::path(file_path).parent_path().filename() + "/" +
                boost::filesystem::path(file_path).filename();
        add_doc_and_sentences_to_bdb(file_id, false);
        add_doc_and_sentences_to_bdb(file_id, true);

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
    string doc_id = remove_document_from_index(identifier, false);
    // document - case sensitive index
    remove_document_from_index(identifier, true);
    // sentence - case insensitive index
    remove_sentences_for_document(doc_id, false);
    // sentence - case sensitive index
    remove_sentences_for_document(doc_id, true);
}

void IndexManager::add_doc_and_sentences_to_bdb(const std::string& identifier, bool case_sensitive) {
    Collection<IndexReaderPtr> subreaders = get_subreaders(QueryType::sentence, case_sensitive);
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
    FieldSelectorPtr fsel = newLucene<LazySelector>(set<String>({L"doc_id", L"year"}));
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb;
    try {
        env.open((index_dir + "/db").c_str(), DB_CREATE | DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "sent_map.db", NULL, DB_BTREE, DB_RDWRMASTER, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        for (const auto& document : matchesCollection) {
            multireader->deleteDocument(document->doc);
            if (huge_map.find(document->doc) != huge_map.end()) {
                huge_map.erase(document->doc);
            }
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
    } catch (std::exception& e) {
        cerr << e.what() << endl;
    }
    multireader->close();
}

string IndexManager::remove_document_from_index(const string& identifier, bool case_sensitive)
{
    // remove doc
    Collection<IndexReaderPtr> subreaders = get_subreaders(QueryType::document, case_sensitive);
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
    FieldSelectorPtr fsel = newLucene<LazySelector>(set<String>({L"doc_id", L"year"}));
    String doc_id = multireader->document(matchesCollection[0]->doc, fsel)->get(L"doc_id");
    String year = multireader->document(matchesCollection[0]->doc, fsel)->get(L"year");
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb;
    try {
        env.open((index_dir + "/db").c_str(), DB_CREATE | DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "doc_map.db", NULL, DB_BTREE, DB_RDWRMASTER, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        huge_map[matchesCollection[0]->doc] = string(year.begin(), year.end());
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
    } catch (std::exception& e) {
        cerr << e.what() << endl;
    }
    multireader->close();

    // remove doc sentences
    subreaders = get_subreaders(QueryType::sentence, case_sensitive);
    multireader = newLucene<MultiReader>(subreaders, false);
    if (case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    parser = newLucene<QueryParser>(LuceneVersion::LUCENE_30, L"doc_id", analyzer);
    query_str = L"doc_id:" + doc_id;
    luceneQuery = parser->parse(query_str);
    searcher = newLucene<IndexSearcher>(multireader);
    collector = TopScoreDocCollector::create(MAX_HITS, true);
    searcher->search(luceneQuery, collector);
    matchesCollection = collector->topDocs()->scoreDocs;
    try {
        env.open((index_dir + "/db").c_str(), DB_CREATE | DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "sent_map.db", NULL, DB_BTREE, DB_RDWRMASTER, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        for (auto& sentence : matchesCollection) {
            huge_map[sentence->doc] = string(doc_id.begin(), doc_id.end()) + "|" + string(year.begin(), year.end());
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
    } catch (std::exception& e) {
        cerr << e.what() << endl;
    }
    multireader->close();

}

void IndexManager::remove_sentences_for_document(const std::string &doc_id, bool case_sensitive) {
    Collection<IndexReaderPtr> subreaders = get_subreaders(QueryType::sentence, case_sensitive);
    MultiReaderPtr multireader = newLucene<MultiReader>(subreaders, false);
    AnalyzerPtr analyzer;
    if (case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_30, L"doc_id", analyzer);
    String query_str = L"doc_id:" + String(doc_id.begin(), doc_id.end());
    QueryPtr luceneQuery = parser->parse(query_str);
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(MAX_HITS, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb;
    try {
        env.open((index_dir + "/db").c_str(), DB_CREATE | DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "sent_map.db", NULL, DB_BTREE, DB_RDWRMASTER, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        for (const auto& document : matchesCollection) {
            multireader->deleteDocument(document->doc);
            if (huge_map.find(document->doc) != huge_map.end()) {
                huge_map.erase(document->doc);
            }
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
    } catch (std::exception& e) {
        cerr << e.what() << endl;
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

std::vector<std::string> IndexManager::get_additional_corpora() {
    vector<string> corpora_vec;
    load_corpus_counter();
    for (const auto& cd_conter_map : corpus_doc_counter) {
        corpora_vec.push_back(cd_conter_map.first);
    }
    return corpora_vec;
}

vector<string> IndexManager::get_external_corpora() {
    if (has_external_index()) {
        return externalIndexManager->get_additional_corpora();
    }
}

int IndexManager::get_num_articles_in_corpus(const string &corpus, bool external) {
    if (external) {
        if (has_external_index()) {
            return externalIndexManager->get_num_articles_in_corpus(corpus);
        } else {
            return 0;
        }
    } else {
        load_corpus_counter();
        return corpus_doc_counter[corpus];
    }
}

void IndexManager::save_corpus_counter() {
    std::ofstream ofs(index_dir + "/" + CORPUS_COUNTER_FILENAME);
    {
        boost::archive::text_oarchive oa(ofs);
        oa << corpus_doc_counter;
    }
}

void IndexManager::load_corpus_counter() {
    std::ifstream ifs(index_dir + "/" + CORPUS_COUNTER_FILENAME, std::ios::binary);
    if (ifs) {
        boost::archive::text_iarchive ia(ifs);
        ia >> corpus_doc_counter;
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

void IndexManager::calculate_and_save_corpus_counter() {
    update_corpus_counter();
    save_corpus_counter();
}

void IndexManager::save_all_doc_ids_for_sentences_to_db() {
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb;
    try {
        env.set_error_stream(&cerr);
        env.open((index_dir + "/db").c_str(), DB_CREATE | DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "sent_map.db", NULL, DB_BTREE, DB_CREATE, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        Collection<IndexReaderPtr> subReaders = get_subreaders(QueryType::sentence, false);
        MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, false);
        FieldSelectorPtr fsel = newLucene<LazySelector>(set<String>({L"doc_id", L"sentence_id", L"year"}));
        for (int i = 0; i < multireader->maxDoc(); i++) {
            String doc_id = multireader->document(i, fsel)->get(L"doc_id");
            String year = multireader->document(i, fsel)->get(L"year");
            huge_map[i] = string(doc_id.begin(), doc_id.end()) + "|" + string(year.begin(), year.end());
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
    } catch (std::exception& e) {
        cerr << e.what() << endl;
    }
}

void IndexManager::save_all_years_for_documents_to_db() {
    DbEnv env(DB_CXX_NO_EXCEPTIONS);
    Db* pdb;
    try {
        env.open((index_dir + "/db").c_str(), DB_CREATE | DB_INIT_MPOOL, 0);
        pdb = new Db(&env, DB_CXX_NO_EXCEPTIONS);
        pdb->open(NULL, "doc_map.db", NULL, DB_BTREE, DB_CREATE, 0);
        typedef dbstl::db_map<int, string> HugeMap;
        HugeMap huge_map(pdb, &env);
        Collection<IndexReaderPtr> subReaders = get_subreaders(QueryType::document, false);
        MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, false);
        FieldSelectorPtr fsel = newLucene<LazySelector>(set<String>({L"year"}));
        for (int i = 0; i < multireader->maxDoc(); i++) {
            String year = multireader->document(i, fsel)->get(L"year");
            huge_map[i] = string(year.begin(), year.end());
        }
        if (pdb != NULL) {
            pdb->close(0);
            delete pdb;
        }
        env.close(0);
    } catch (DbException& e) {
        cerr << "DbException: " << e.what() << endl;
    } catch (std::exception& e) {
        cerr << e.what() << endl;
    }
}

void IndexManager::set_external_index(std::string external_idx_path) {
    externalIndexManager = make_shared<IndexManager>(external_idx_path, true, true);
}

void IndexManager::remove_external_index() {
    externalIndexManager.reset();

}

bool IndexManager::has_external_index() {
    return externalIndexManager != nullptr;
}

