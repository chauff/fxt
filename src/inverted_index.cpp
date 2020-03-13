/*
 * Copyright 2020 The Tesserae authors.
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#include "tesserae/inverted_index.hpp"

#include "FastPFor/headers/codecfactory.h"
#include "FastPFor/headers/deltautil.h"

using namespace FastPForLib;

namespace {
IntegerCODEC &codec = *CODECFactory::getFromName("simdfastpfor256");
};

void PostingList::add_list(std::vector<uint32_t> &docs,
                           std::vector<uint32_t> &freqs) {
  assert(docs.size() == freqs.size());

  m_size = docs.size();
  m_docs.resize(m_size * 2);
  m_freqs.resize(m_size * 2);

  size_t compressedsize = m_docs.size();
  Delta::deltaSIMD(docs.data(), docs.size());
  codec.encodeArray(docs.data(), docs.size(), m_docs.data(), compressedsize);
  m_docs.resize(compressedsize);
  m_docs.shrink_to_fit();

  compressedsize = m_freqs.size();
  codec.encodeArray(freqs.data(), freqs.size(), m_freqs.data(), compressedsize);
  m_freqs.resize(compressedsize);
  m_freqs.shrink_to_fit();
}

std::pair<std::vector<uint32_t>, std::vector<uint32_t>> PostingList::list() {
  std::vector<uint32_t> docs(m_size);
  std::vector<uint32_t> freqs(m_size);

  size_t recoveredsize = docs.size();
  codec.decodeArray(m_docs.data(), m_docs.size(), docs.data(), recoveredsize);
  docs.resize(recoveredsize);
  Delta::inverseDeltaSIMD(docs.data(), docs.size());

  recoveredsize = freqs.size();
  codec.decodeArray(m_freqs.data(), m_freqs.size(), freqs.data(),
                    recoveredsize);
  freqs.resize(recoveredsize);
  return std::make_pair(docs, freqs);
}
