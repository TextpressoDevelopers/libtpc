/**
    Project: libtpc
    File name: reader.h

    @author valerio
    @version 1.0 7/18/17.
*/

#include "reader.h"
#include "lucene-custom/CaseSensitiveAnalyzer.h"
#include "lucene-custom/LazySelector.h"
#include <lucene++/LuceneHeaders.h>
#include <lucene++/FieldCache.h>

using namespace std;
using namespace tpc::reader;
using namespace Lucene;

/*!
 * @brief search the Lucene index for documents matching the provided Lucene query and return a list of results sorted
 * by score
 *
 * note that while the documents are sorted by score, their matched sentences, in case of sentence searches, are not
 * sorted
 * @param index_root_dir the root dir of the Lucene indexes
 * @param search_type the type of search to be performed
 * @param query a Lucene query
 * @param literatures a list of literatures representing the set of indexes on which to perform the search
 * @param case_sensitive whether to perform a case sensitive search
 * @return the list of the documents matching the query sorted by their scores and encapsulated in a SearchResutl object
 */
SearchResult Util::search(const string& index_root_dir, QueryType search_type, const std::string& query,
                          const vector<string>& literatures, bool case_sensitive) {
    string index_type;
    String field;
    if (search_type == QueryType::document) {
        index_type = case_sensitive ? document_indexname_cs : document_indexname;
        field = L"fulltext";
    } else if (search_type == QueryType::sentence) {
        index_type = case_sensitive ? sentence_indexname_cs : sentence_indexname;
        field = L"sentence";
    }
    AnalyzerPtr analyzer;
    if (case_sensitive) {
        analyzer = newLucene<CaseSensitiveAnalyzer>(LuceneVersion::LUCENE_30);
    } else {
        analyzer = newLucene<StandardAnalyzer>(LuceneVersion::LUCENE_30);
    }
    Collection<IndexReaderPtr> subReaders = get_subreaders(literatures, index_root_dir, index_type);
    MultiReaderPtr multireader = newLucene<MultiReader>(subReaders, true);
    QueryParserPtr parser = newLucene<QueryParser>(LuceneVersion::LUCENE_30, field , analyzer);
    QueryPtr luceneQuery = parser->parse(String(query.begin(), query.end()));
    SearcherPtr searcher = newLucene<IndexSearcher>(multireader);
    TopScoreDocCollectorPtr collector = TopScoreDocCollector::create(maxHits, true);
    searcher->search(luceneQuery, collector);
    Collection<ScoreDocPtr> matchesCollection = collector->topDocs()->scoreDocs;
    SearchResult result;
    if (search_type == QueryType::document) {
        result = get_results_from_document_hit_collection(matchesCollection, subReaders, searcher);
    } else if (search_type == QueryType::sentence) {
        result = get_results_from_sentence_hit_collection(matchesCollection, subReaders, searcher);
    }
    sort(result.hit_documents.begin(), result.hit_documents.end(), document_greater_than);
    return result;
}

/*!
 * create a collection of sub-readers with multiple Lucene indexes
 * @param query a Lucene query
 * @param literatures the list of directory names for the indexed literatures
 * @param index_root_dir the name of the index to be read within the literatures
 * @param index_type the type of index to be read - i.e. the name of the index directory within the literature
 * @return a collection of readers created from the Lucene indexes
 */
Collection<IndexReaderPtr> Util::get_subreaders(const vector<string>& literatures, const string& index_root_dir,
                                                const string& index_type) {
    Collection<IndexReaderPtr> subReaders = Collection<IndexReaderPtr>::newInstance(0);
    for (string literature : literatures) {
        string index_dir = index_root_dir + "/" + literature + "/" + index_type;
        IndexReaderPtr reader = IndexReader::open(FSDirectory::open(String(index_dir.begin(), index_dir.end())), true);
        if (reader) {
            subReaders.add(reader);
        } else {
            throw new index_exception();
        }
    }
    return subReaders;
}

/*!
 * collect and return document information for a collection of matches obtained from a document search
 * @param matches_collection the collection of documents matching the search query
 * @param subreaders the readers used during the search
 * @param searcher the searcher used during the search
 * @return the list of Document objects with information related to the matching documents, encapsulated in a
 * SearchResult object
 */
SearchResult Util::get_results_from_document_hit_collection(Collection<ScoreDocPtr> matches_collection,
                                                            Collection<IndexReaderPtr> subreaders,
                                                            SearcherPtr searcher)
{
    SearchResult result;
    result.query_type = QueryType::document;
    if (matches_collection.size() < field_cache_min_hits) {
        FieldSelectorPtr fsel = newLucene<LazySelector>(L"identifier");
        for (auto scoredoc : matches_collection) {
            Document document;
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            String identifier = docPtr->get(L"identifier");
            document.identifier = StringUtils::toUTF8(identifier);
            document.score = scoredoc->score;
            result.hit_documents.push_back(document);
        }
    } else {
        vector<int32_t> docids;
        map<int32_t, double> scores;
        for (auto scoredoc : matches_collection) {
            docids.push_back(scoredoc->doc);
            scores.insert({scoredoc->doc, scoredoc->score});
        }
        sort(docids.begin(), docids.end());
        int readerIndex = 0;
        Collection<String> fieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"identifier");
        int offset = 0;
        for (auto docid : docids) {
            while ((docid - offset) >= fieldCache.size()) {
                // load one field cache at a time to avoid wasting memory - sorted docids reduces
                // the number of loadings - the offset value is needed since docids count the docs
                // altoghether
                offset += fieldCache.size();
                fieldCache = FieldCache::DEFAULT()->getStrings(subreaders[++readerIndex], L"identifier");
            }
            String identifier = fieldCache[docid - offset];
            Document document;
            document.identifier = StringUtils::toUTF8(identifier);
            document.score = scores[docid];
            result.hit_documents.push_back(document);
        }
    }
    return result;
}

/*!
 * collect and return document information for a collection of matches obtained from a sentence search
 * @param matches_collection the collection of sentences matching the search query
 * @param subreaders the readers used during the search
 * @param searcher the searcher used during the search
 * @return the list of Document objects with information related to the matching sentences and their respective
 * documents, encapsulated in a SearchResult object
 */
SearchResult Util::get_results_from_sentence_hit_collection(Collection<ScoreDocPtr> matches_collection,
                                                            Collection<IndexReaderPtr> subreaders,
                                                            SearcherPtr searcher)
{
    SearchResult result;
    result.query_type = QueryType::sentence;
    map<string, Document> doc_map;
    if (matches_collection.size() < field_cache_min_hits) {
        set<String> fields = {L"identifier", L"sentence_id"};
        FieldSelectorPtr fsel = newLucene<LazySelector>(fields);
        for (auto scoredoc : matches_collection) {
            DocumentPtr docPtr = searcher->doc(scoredoc->doc, fsel);
            String identifier = docPtr->get(L"identifier");
            if (doc_map.find(StringUtils::toUTF8(identifier)) == doc_map.end()) {
                Document document;
                document.identifier = StringUtils::toUTF8(identifier);
                document.score = 0;
                doc_map.insert({StringUtils::toUTF8(identifier), document});
            }
            doc_map[StringUtils::toUTF8(identifier)].score += scoredoc->score;
            Sentence sentence;
            sentence.identifier = StringUtils::toUTF8(docPtr->get(L"sentence_id"));
            sentence.score = scoredoc->score;
            doc_map[StringUtils::toUTF8(identifier)].matching_sentences.push_back(sentence);
        }
    } else {
        vector<int32_t> docids;
        map<int32_t, double> scores;
        for (auto scoredoc : matches_collection) {
            docids.push_back(scoredoc->doc);
            scores.insert({scoredoc->doc, scoredoc->score});
        }
        sort(docids.begin(), docids.end());
        int readerIndex = 0;
        Collection<String> docIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"identifier");
        Collection<String> sentIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex],
                                                                                L"sentence_id");
        int offset = 0;
        for (auto docid : docids) {
            while ((docid - offset) >= docIdFieldCache.size()) {
                // load one field cache at a time to avoid wasting memory - sorted docids reduces
                // the number of loadings - the offset value is needed since docids count the docs
                // altoghether
                offset += docIdFieldCache.size();
                ++readerIndex;
                docIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"identifier");
                sentIdFieldCache = FieldCache::DEFAULT()->getStrings(subreaders[readerIndex], L"sentence_id");
            }
            String docIdentifier = docIdFieldCache[docid - offset];
            String sentIdentifier = sentIdFieldCache[docid - offset];

            if (doc_map.find(StringUtils::toUTF8(docIdentifier)) == doc_map.end()) {
                Document document;
                document.identifier = StringUtils::toUTF8(docIdentifier);
                document.score = 0;
                doc_map.insert({StringUtils::toUTF8(docIdentifier), document});
            }
            doc_map[StringUtils::toUTF8(docIdentifier)].score += scores[docid];
            Sentence sentence;
            sentence.identifier = StringUtils::toLong(sentIdentifier);
            sentence.score = scores[docid];
            doc_map[StringUtils::toUTF8(docIdentifier)].matching_sentences.push_back(sentence);
        }
    }
    std::transform(doc_map.begin(), doc_map.end(), std::back_inserter(result.hit_documents),
                   boost::bind(&map<string, Document>::value_type::second, _1));
    return result;
}