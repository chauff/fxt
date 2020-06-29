/*
 * Copyright 2018 The Fxt authors.
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#pragma once

#include "doc_bm25_feature.hpp"

/**
 * BM25 using parameters suggested by Robertson, et al. in TREC 3.
 */
class doc_bm25_trec3_feature : public doc_bm25_feature {
 public:
  doc_bm25_trec3_feature(Lexicon &lex) : doc_bm25_feature(lex) {}

  void compute(query_train &qry, doc_entry &doc, Document &doc_idx,
               FieldIdMap &field_id_map) {
    ranker.set_k1(1.2);
    ranker.set_b(0.75);

    bm25_compute(qry, doc, doc_idx, field_id_map);

    doc.bm25_trec3 = _score_doc;
    doc.bm25_trec3_body = _score_body;
    doc.bm25_trec3_title = _score_title;
    doc.bm25_trec3_heading = _score_heading;
    doc.bm25_trec3_inlink = _score_inlink;
    doc.bm25_trec3_a = _score_a;
  }
};
