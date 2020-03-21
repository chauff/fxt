/*
 * Copyright 2018 The Tesserae authors.
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#include "CLI/CLI.hpp"
#include "cereal/archives/binary.hpp"

#include "indri/QueryEnvironment.hpp"
#include "indri/Repository.hpp"

#include "tesserae/inverted_index.hpp"
#include "tesserae/util.hpp"

int main(int argc, char const *argv[]) {
  std::string repo_path;
  std::string inverted_index_file;

  CLI::App app{"Inverted index generator."};
  app.add_option("repo_path", repo_path, "Indri repo path")->required();
  app.add_option("inverted_index_file", inverted_index_file,
                 "Inverted index file")
      ->required();
  CLI11_PARSE(app, argc, argv);

  std::ofstream os(inverted_index_file, std::ios::binary);
  cereal::BinaryOutputArchive archive(os);

  indri::collection::Repository repo;
  repo.openRead(repo_path);
  indri::collection::Repository::index_state state = repo.indexes();
  const auto &index = (*state)[0];

  {
    // dump size of vector
    size_t len = index->uniqueTermCount();
    archive(len);
  }

  ProgressPresenter pp(index->uniqueTermCount(), 1, 10000, "terms processed: ");
  indri::index::DocListFileIterator *iter = index->docListFileIterator();
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
    pl.add_list(docs, freqs);
    archive(pl);
    pp.progress();
    iter->nextEntry();
  }

  delete iter;

  return 0;
}
