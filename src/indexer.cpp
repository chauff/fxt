/*
 * Copyright 2020 The Tesserae authors.
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#include <filesystem>
#include <iostream>
#include <string>

#include "cereal/archives/binary.hpp"
#include "indri/QueryEnvironment.hpp"
#include "indri/Repository.hpp"

#include "tesserae/doc_lens.hpp"
#include "tesserae/field_map.hpp"
#include "tesserae/forward_index.hpp"
#include "tesserae/forward_index_interactor.hpp"
#include "tesserae/inverted_index.hpp"
#include "tesserae/lexicon.hpp"
#include "tesserae/util.hpp"

namespace fs = std::filesystem;

static const std::vector<std::string> _fields = {"body", "title", "heading",
                                                 "inlink", "a"};
class IndriIndexAdapter {
 public:
  indri::collection::Repository repo;
  indri::index::Index *index = nullptr;

  IndriIndexAdapter() = default;

  void open(const std::string path) {
    repo.openRead(path);
    indri::collection::Repository::index_state state = repo.indexes();
    index = (*state)[0];
  }
};

class IndexerInteractor {
  const std::string sep = "/";  // assume unix like filesystem
  const std::string lexicon_file = "lexicon";
  const std::string doclen_file = "doclen";
  const std::string fwdidx_file = "forward_index";
  const std::string invidx_file = "inverted_index";
  const IndriIndexAdapter &indri;
  std::string outpath;

 public:
  IndexerInteractor(const IndriIndexAdapter &index, const std::string path)
      : indri(index), outpath(path) {}

  // Build the lexicon and serialize to file.
  void lexicon() {
    std::string outfile =
        outpath + std::string(sep) + std::string(lexicon_file);
    std::ofstream os(outfile, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);
    FieldMap fields;
    fields.insert(*indri.index, _fields);

    indri::index::VocabularyIterator *iter = indri.index->vocabularyIterator();
    iter->startIteration();

    Lexicon lexicon(
        Counts(indri.index->documentCount(), indri.index->termCount()));
    ProgressPresenter pp(indri.index->uniqueTermCount(), 1, 10000,
                         "terms processed: ");

    while (!iter->finished()) {
      indri::index::DiskTermData *entry = iter->currentEntry();
      indri::index::TermData *termData = entry->termData;

      FieldCounts field_counts;
      for (const int &field_id : fields.values()) {
        Counts c(termData->fields[field_id - 1].documentCount,
                 termData->fields[field_id - 1].totalCount);
        field_counts.insert(std::make_pair(field_id, c));
      }

      Counts counts(termData->corpus.documentCount,
                    termData->corpus.totalCount);
      lexicon.push_back(termData->term, counts, field_counts);
      pp.progress();
      iter->nextEntry();
    }

    delete iter;
    archive(lexicon);
  }

  // Collect a vector of document lengths and serialize to file.
  void document_length() {
    std::string outfile = outpath + std::string(sep) + std::string(doclen_file);
    std::ofstream os(outfile, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);

    DocLens doc_lens;
    doc_lens.reserve(indri.index->documentCount() + 1);
    doc_lens.push_back(0);

    ProgressPresenter pp(indri.index->documentCount(),
                         indri.index->documentBase(), 10000,
                         "documents processed: ");
    indri::index::TermListFileIterator *iter =
        indri.index->termListFileIterator();
    iter->startIteration();
    while (!iter->finished()) {
      indri::index::TermList *list = iter->currentEntry();
      auto &doc_terms = list->terms();
      doc_lens.push_back(doc_terms.size());
      pp.progress();
      iter->nextEntry();
    }

    delete iter;
    archive(doc_lens);
  }

  // Construct a document forward index with positional and field information.
  void forward_index() {
    std::string outfile = outpath + std::string(sep) + std::string(fwdidx_file);
    std::ofstream os(outfile, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);

    ForwardIndexInteractor interactor;
    FieldMap fields;
    fields.insert(*indri.index, _fields);

    {
      // dump size of vector
      size_t len = indri.index->documentCount();
      // add 1 for the zero padded document
      len += 1;
      // pad document index zero (unused)
      Document zero;

      archive(len);
      archive(zero);
    }

    ProgressPresenter pp(indri.index->documentCount(),
                         indri.index->documentBase(), 10000,
                         "documents processed: ");
    indri::index::TermListFileIterator *iter =
        indri.index->termListFileIterator();
    iter->startIteration();

    while (!iter->finished()) {
      indri::index::TermList *list = iter->currentEntry();
      auto &doc_terms = list->terms();
      auto &doc_fields = list->fields();
      Document document;

      std::vector<uint32_t> terms(doc_terms.begin(), doc_terms.end());
      document.set_terms(terms);

      std::unordered_map<uint16_t, std::vector<indri::index::FieldExtent>>
          fid_extentlist;
      for (auto &f : doc_fields) {
        if (fields.get().find(indri.index->field(f.id)) != fields.get().end()) {
          fid_extentlist[f.id].push_back(f);
        }
      }

      std::vector<uint16_t> fv;
      for (auto &f : fields.get()) {
        fv.push_back(f.second);
      }
      document.set_fields(fv);

      std::unordered_map<size_t, std::unordered_map<uint32_t, uint32_t>>
          field_freqs;
      for (const auto &curr : fid_extentlist) {
        for (const auto &f : curr.second) {
          auto d_len = f.end - f.begin;
          interactor.process_field_len(document, f.id, d_len);
          interactor.process_field_len_sum_sqrs(document, f.id, d_len);
          interactor.process_field_max_len(document, f.id, d_len);
          interactor.process_field_min_len(document, f.id, d_len);
          document.set_tag_count(f.id, document.tag_count(f.id) + 1);

          for (size_t i = f.begin; i < f.end; ++i) {
            field_freqs[f.id][doc_terms[i]] += 1;
          }
        }
      }
      for (auto &&freq : field_freqs) {
        for (auto &&f : freq.second) {
          document.set_freq(freq.first, f.first, f.second);
        }
      }

      document.compress();
      archive(document);
      pp.progress();
      iter->nextEntry();
    }

    delete iter;
  }

  // Build an inverted index with compression and serialize to file.
  void inverted_index() {
    std::string outfile = outpath + std::string(sep) + std::string(invidx_file);
    std::ofstream os(outfile, std::ios::binary);
    cereal::BinaryOutputArchive archive(os);

    {
      // dump size of vector
      size_t len = indri.index->uniqueTermCount();
      archive(len);
    }

    ProgressPresenter pp(indri.index->uniqueTermCount(), 1, 10000,
                         "terms processed: ");
    indri::index::DocListFileIterator *iter =
        indri.index->docListFileIterator();
    iter->startIteration();
    while (!iter->finished()) {
      indri::index::DocListFileIterator::DocListData *entry =
          iter->currentEntry();
      entry->iterator->startIteration();
      indri::index::TermData *termData = entry->termData;
      PostingList pl(termData->term, termData->corpus.totalCount);
      std::vector<uint32_t> docs;
      std::vector<uint32_t> freqs;

      while (!entry->iterator->finished()) {
        indri::index::DocListIterator::DocumentData *doc =
            entry->iterator->currentEntry();
        docs.push_back(doc->document);
        freqs.push_back(doc->positions.size());
        entry->iterator->nextEntry();
      }
      pl.set(docs, freqs);
      archive(pl);
      pp.progress();
      iter->nextEntry();
    }

    delete iter;
  }
};

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << "<indri_index> <index>" << std::endl;
    return 1;
  }

  std::string indri_path = argv[1];
  std::string tess_path = argv[2];

  if (fs::exists(tess_path)) {
    std::cerr << "error index path exists" << std::endl;
    return 1;
  }

  if (!fs::create_directory(tess_path)) {
    std::cerr << "error creating directory" << std::endl;
    return 1;
  }

  IndriIndexAdapter indri;
  indri.open(indri_path);

  // 1. Build lexicon
  // 2. Document lengths
  // 3. Forward index
  // 4. Inverted index
  IndexerInteractor indexer(indri, tess_path);
  indexer.lexicon();
  indexer.document_length();
  indexer.forward_index();
  indexer.inverted_index();

  return 0;
}
