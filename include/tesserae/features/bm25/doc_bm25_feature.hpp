/*
 * Copyright 2018 The Tesserae authors.
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#pragma once

#include <cmath>

#include "bm25.hpp"
#include "tesserae/field_id.hpp"
#include "tesserae/forward_index.hpp"
#include "tesserae/lexicon.hpp"

class doc_bm25_feature : public doc_feature {
 protected:
  rank_bm25 ranker;

 public:
  doc_bm25_feature(Lexicon &lex) : doc_feature(lex) {
    ranker.num_docs = _num_docs;
    ranker.avg_doc_len = _avg_doc_len;
  }

  void bm25_compute(query_train &qry, doc_entry &doc, Document &doc_idx,
                    FieldIdMap &field_id_map) {
    // reset socres to 0
    reset();

    for (auto &q : qry.q_ft) {
      // skip non-existent terms
      if (lexicon.is_oov(q.first)) {
        continue;
      }

      if (doc_idx.freq(q.first) == 0) {
        continue;
      }

      _score_doc += ranker.calculate_docscore(q.second, doc_idx.freq(q.first),
                                              lexicon[q.first].document_count(),
                                              doc_idx.length());

      // Score document fields
      for (const std::string &field_str : _fields) {
        int field_id = field_id_map[field_str];
        if (field_id < 1) {
          // field is not indexed
          continue;
        }

        if (0 == doc_idx.field_len(field_id)) {
          continue;
        }
        if (doc_idx.freq(field_id, q.first) == 0) {
          continue;
        }

        double field_score = ranker.calculate_docscore(
            q.second, doc_idx.freq(field_id, q.first),
            lexicon[q.first].field_document_count(field_id),
            doc_idx.field_len(field_id));
        _accumulate_score(field_str, field_score);
      }
    }
  }
};
