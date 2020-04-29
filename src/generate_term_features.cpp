/*
 * Copyright 2018 The Tesserae authors.
 *
 * For the full copyright and license information, please view the LICENSE file
 * that was distributed with this source code.
 */

#include <chrono>
#include <iostream>
#include <limits>
#include <string>

#include "CLI/CLI.hpp"
#include "cereal/archives/binary.hpp"

#include "tesserae/doc_lens.hpp"
#include "tesserae/inverted_index.hpp"
#include "tesserae/term_feature.hpp"

int main(int argc, char **argv) {
    size_t done      = 0;
    size_t freq      = 0;
    double tfidf_max = 0.0;
    double bm25_max  = 0.0;
    double pr_max    = 0.0;
    double be_max    = 0.0;
    double dfr_max   = 0.0;
    double dph_max   = 0.0;
    double lm_max    = -std::numeric_limits<double>::max();

    std::string inverted_index_file;
    std::string doc_lens_file;
    std::string output_file;

    CLI::App app{"Term features generation."};
    app.add_option("-i,--inverted-index", inverted_index_file, "Inverted index filename")
        ->required();
    app.add_option("-d,--doc-lens", doc_lens_file, "Document lens filename")->required();
    app.add_option("-o,--out-file", output_file, "Output filename")->required();
    CLI11_PARSE(app, argc, argv);

    using clock = std::chrono::high_resolution_clock;
    InvertedIndex inv_idx;
    DocLens       doc_lens;

    {

        auto start = clock::now();

        // load inv_idx
        std::ifstream              ifs_inv(inverted_index_file);
        cereal::BinaryInputArchive iarchive_inv(ifs_inv);
        iarchive_inv(inv_idx);

        auto stop      = clock::now();
        auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        std::cerr << "Loaded " << inverted_index_file << " in " << load_time.count() << " ms"
                  << std::endl;
    }
    {
        auto start = clock::now();

        // load doc_lens
        std::ifstream              ifs_len(doc_lens_file);
        cereal::BinaryInputArchive iarchive_len(ifs_len);
        iarchive_len(doc_lens);
        auto stop      = clock::now();
        auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        std::cerr << "Loaded " << doc_lens_file << " in " << load_time.count() << " ms"
                  << std::endl;
    }

    std::ofstream outfile(output_file, std::ofstream::app);
    outfile << std::fixed << std::setprecision(6);

    size_t clen     = std::accumulate(doc_lens.begin(), doc_lens.end(), size_t(0));
    size_t ndocs    = doc_lens.size();
    double avg_dlen = (double)clen / ndocs;
    std::cout << "Avg Document Length: " << avg_dlen << std::endl;
    std::cout << "N. docs: " << ndocs << std::endl;
    std::cout << "Collection Length " << clen << std::endl;

    for (auto &&pl : inv_idx) {
        feature_t feature;
        feature.term = pl.term();
        feature.cf   = pl.term_count();
        feature.cdf  = pl.length();
        auto list    = pl.get();

        /* Min count is set to 4 or IQR computation goes boom. */
        if (pl.length() >= 4) {
            feature.geo_mean = compute_geo_mean(list.frequency);
            compute_tfidf_stats(feature, doc_lens, list, ndocs, tfidf_max);
            compute_bm25_stats(feature, doc_lens, list, ndocs, avg_dlen, bm25_max);
            compute_lm_stats(feature, doc_lens, list, clen, pl.term_count(), lm_max);
            compute_prob_stats(feature, doc_lens, list, pr_max);
            compute_be_stats(feature, doc_lens, list, ndocs, avg_dlen, pl.term_count(), be_max);
            compute_dph_stats(feature, doc_lens, list, ndocs, avg_dlen, pl.term_count(), dph_max);
            compute_dfr_stats(feature, doc_lens, list, ndocs, avg_dlen, pl.term_count(), dfr_max);
            outfile << feature;
            freq++;
        }
        done++;
        if (done % 10000 == 0) {
            std::cout << "Processed " << done << " terms." << std::endl;
        }
    }
    std::cout << "Inv Lists Processed = " << done << std::endl;
    std::cout << "Inv Lists > 4 = " << freq << std::endl;
    std::cout << "TFIDF Max Score = " << tfidf_max << std::endl;
    std::cout << "BM25 Max Score = " << bm25_max << std::endl;
    std::cout << "LM Max Score = " << lm_max << std::endl;
    std::cout << "PR Max Score = " << pr_max << std::endl;
    std::cout << "BE Max Score = " << be_max << std::endl;
    std::cout << "DPH Max Score = " << dph_max << std::endl;
    std::cout << "DFR Max Score = " << dfr_max << std::endl;
    return 0;
}
