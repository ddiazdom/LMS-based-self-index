//
// Created by Diaz, Diego on 9.1.2022.
//
#include "lc_gram_algo.hpp"

int main(int argc, char** argv) {
    dictionary dict;
    sdsl::load_from_file(dict, "/Users/ddiaz/dictionary_tmp");

    vector_t sa;
    sdsl::load_from_file(sa, "/Users/ddiaz/sa_tmp");

    gram_info_t p_gram;
    std::string gfile = "/Users/ddiaz/p_gram_tmp";
    p_gram.load_from_file(gfile);

    ivb_t rules("fake_rules", std::ios::out);
    bvb_t r_lim("fale_r_lim", std::ios::out);
    phrase_map_t map;

    compress_dictionary(dict, sa, p_gram, r_lim, rules, map);
}

