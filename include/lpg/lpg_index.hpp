//
// Created by diediaz on 11-12-19.
//

#ifndef LMS_GRAMMAR_REP_HPP
#define LMS_GRAMMAR_REP_HPP

#include <pthread.h>
#include "lpg_build.hpp"
#include "grammar_tree.hpp"
#include "grid.hpp"
#include "macros.hpp"

class lpg_index {

private:
    typedef sdsl::sd_vector<> bv_y;
    typedef typename lpg_build::plain_grammar_t plain_grammar_t;
    typedef typename lpg_build::alpha_t alpha_t;

    grammar_tree_t grammar_tree;
    grid m_grid;

    bv_y Y;
    bv_y::rank_1_type rank_Y;

    sdsl::int_vector<8> symbols_map; // map a compressed terminal to its original byte symbol
    uint8_t m_sigma{}; //alphabet of terminal symbols
    uint8_t parsing_rounds{}; //number of LMS parsing rounds during the grammar construction
    bool rl_compressed{}; // is the grammar run-length compressed?

    void build_index(const std::string &i_file, plain_grammar_t &p_gram, const size_t &text_length,
                     sdsl::cache_config &config) {
        m_sigma = p_gram.sigma;
        parsing_rounds = p_gram.rules_per_level.size();

        {
            sdsl::bit_vector _y(p_gram.r, 0);
            symbols_map.resize(p_gram.sym_map.size());
            size_type ii = 0;
            for (const auto &item : p_gram.sym_map) {

                _y[item.first] = 1;
                symbols_map[ii] = item.second;
                ii++;
            }
            std::sort(symbols_map.begin(), symbols_map.end());
            Y = bv_y(_y);
            rank_Y = bv_y::rank_1_type(&Y);
#ifdef DEBUG_PRINT
            utils::pretty_printer_v(symbols_map, "sym");
            utils::pretty_printer_bv(Y, "Y");
#endif
        }



        /*symbols_map.resize(p_gram.symbols_map.size());
        for(size_t i=0; i < p_gram.symbols_map.size(); i++){
            symbols_map[i] = p_gram.symbols_map[i];
        }*/
        //build the grammar tree and grid from p_gram here!!

        utils::lenght_rules lenghts;
        size_type S;
        utils::nav_grammar NG = utils::build_nav_grammar(p_gram, S);
        grammar_tree.build(NG, p_gram, text_length, lenghts, S);
        std::vector<utils::sfx> grammar_sfx;
        const auto &T = grammar_tree.getT();
        compute_grammar_sfx(NG, p_gram, lenghts, grammar_sfx, S);
        NG.clear();
        lenghts.clear();
#ifdef DEBUG_INFO
        std::cout << "sort_suffixes[" << grammar_sfx.size() << "]\n";
#endif
        utils::sort_suffixes(i_file, grammar_sfx);
#ifdef DEBUG_PRINT
        int i = 0;
        for (const auto &sfx : grammar_sfx) {
            std::cout<<i++<<":\t"<<":O["<<sfx.off<<"]P["<<sfx.preorder<<"]"<<"R["<<sfx.rule<<"]\t";
            print_suffix_grammar(sfx.preorder);
            std::cout<<std::endl;
        }
#endif
#ifdef DEBUG_INFO
        std::cout << "compute_grid_points\n";
#endif
        std::vector<grid_point> points;
        compute_grid_points(grammar_sfx, points);

        grammar_sfx.clear();
//        grid = grid_t(points,p_gram.rules_per_level.size());
        m_grid = grid(points);
#ifdef DEBUG_INFO
        std::cout << "build grid\n";
        breakdown_space();
        std::cout << "end build index\n";
#endif
    };

    void breakdown_space() const {
        std::cout << "breakdown-space-index\n";
        std::cout << "Text-length," << grammar_tree.get_text_len() << std::endl;
        std::cout << "Number-rules," << grammar_tree.get_size_rules() << std::endl;
        std::cout << "Grammar-size," << grammar_tree.get_grammar_size() << std::endl;
        std::cout << "Grammar-Tree," << sdsl::size_in_bytes(grammar_tree) << std::endl;
        grammar_tree.breakdown_space();
        std::cout << "Grid," << sdsl::size_in_bytes(m_grid) << std::endl;;
        m_grid.breakdown_space();
        std::cout << "symbols_map," << sdsl::size_in_bytes(symbols_map);
        std::cout << "m_sigma," << sizeof(m_sigma);
        std::cout << "parsing_rounds," << sizeof(parsing_rounds);
        std::cout << "rl_compressed," << sizeof(rl_compressed);
    }

    struct parse_data {
        uint8_t lvl;
        size_t idx;
        size_t n_lms;
        bool tail;
    };

    static alpha_t get_alphabet(std::string &i_file) {

        std::cout << "Reading input file" << std::endl;

        //TODO this can be done in parallel if the input is too big
        size_t alph_frq[256] = {0};
        alpha_t alphabet;

        i_file_stream<uint8_t> if_stream(i_file, BUFFER_SIZE);
        for (size_t i = 0; i < if_stream.tot_cells; i++) {
            alph_frq[if_stream.read(i)]++;
        }

        for (size_t i = 0; i < 256; i++) {
            if (alph_frq[i] > 0) alphabet.emplace_back(i, alph_frq[i]);
        }

        std::cout << "  Number of characters: " << if_stream.size() << std::endl;
        std::cout << "  Alphabet:             " << alphabet.size() << std::endl;
        std::cout << "  Smallest symbol:      " << (int) alphabet[0].first << std::endl;
        std::cout << "  Greatest symbol:      " << (int) alphabet.back().first << std::endl;

        if (if_stream.read(if_stream.size() - 1) != alphabet[0].first) {
            std::cout << "Error: sep. symbol " << alphabet[0].first << " differs from last symbol in file "
                      << if_stream.read(if_stream.size() - 1) << std::endl;
            exit(1);
        }
        return alphabet;
    }

    template<typename proc>
    static void lms_scan(proc &task, std::vector<size_t> &parse) {

        int_array<size_t> lms_phrase(2, 32);
        size_t curr_sym, prev_sym, pos;
        bool s_type, prev_s_type;

        lms_phrase.push_back(parse.back());
        pos = parse.size() - 2;
        while (pos > 0 && parse[pos] == parse[pos + 1]) {
            lms_phrase.push_back(parse[pos--]);
        }
        if (pos == 0) return;

        if (parse[pos] < parse[pos + 1]) {
            prev_s_type = S_TYPE;
        } else {
            prev_s_type = L_TYPE;
        }

        prev_sym = parse[pos];
        lms_phrase.push_back(prev_sym);
        for (size_t i = pos; i-- > 0;) {
            curr_sym = parse[i];
            if (curr_sym < prev_sym) {//S_TYPE type
                s_type = S_TYPE;
            } else if (curr_sym == prev_sym) {
                s_type = prev_s_type;
            } else {//L_TYPE type
                s_type = L_TYPE;
                if (prev_s_type == S_TYPE) {//LMS-type
                    lms_phrase.pop_back();
                    task(lms_phrase);

                    lms_phrase.clear();
                    lms_phrase.push_back(prev_sym);
                }
            }
            lms_phrase.push_back(curr_sym);
            prev_sym = curr_sym;
            prev_s_type = s_type;
        }
        task(lms_phrase);
    }

    static std::pair<std::vector<size_t>, uint8_t> compute_pattern_cut(const std::string &pattern) {

        std::vector<size_t> parse;

        //right elements are index in the first
        // level (terminals) where the S* string occur
        // left element is the index in the last level
        std::vector<size_t> lms_cuts;

        parse.reserve(pattern.size());
        for (auto const &sym : pattern) parse.push_back(sym);

        size_t rank = 0;
        parse_data p_data{};
        p_data.n_lms = parse.size();

        //hash table to hash the LMS phrases
        bit_hash_table<size_t, 44> ht;

        //lambda function to hash the LMS phrases
        auto hash_task = [&](auto &phrase) {

            if (p_data.tail) {
                p_data.tail = false;
            } else {
                phrase.mask_tail();
                if (p_data.idx > phrase.size()) {
                    ht.insert(phrase.data(), phrase.n_bits(), 0);
                }
            }

            if (p_data.idx > phrase.size()) {
                p_data.idx -= phrase.size();
                if (p_data.lvl == 0) {
                    lms_cuts.push_back(p_data.idx);
                } else {
                    lms_cuts[p_data.n_lms] = lms_cuts[p_data.idx];
                }
                p_data.n_lms++;
            }
        };

        //lambda function to create the LMS parse
        auto parse_task = [&](auto &phrase) {
            if (p_data.tail) {
                p_data.tail = false;
            } else {
                if (p_data.n_lms > 1) {
                    phrase.mask_tail();
                    auto res = ht.find(phrase.data(), phrase.n_bits());
                    assert(res.second);
                    parse[p_data.idx--] = res.first.value();
                    p_data.n_lms--;
                }
            }
        };

        //hash the LMS phrases in the text
        while (true) {

            assert(parse.size() > 1);

            p_data.idx = parse.size() - 1;
            p_data.n_lms = 0;
            p_data.tail = true;

            lms_scan(hash_task, parse);

            if (p_data.n_lms < 4) {//report the cuts
                if (p_data.n_lms != 0) lms_cuts.resize(p_data.n_lms);
                std::reverse(lms_cuts.begin(), lms_cuts.end());
                return {lms_cuts, p_data.lvl};
            } else {//TODO if we incur in more parsing rounds than the grammar, then the pattern doesn't exist
                //assign ranks to the lms phrases
                {
                    const bitstream<bit_hash_table<size_t, 44>::buff_t> &stream = ht.get_data();
                    lpg_build::key_wrapper key_w{44, ht.description_bits(), stream};
                    std::vector<size_t> sorted_phrases;
                    sorted_phrases.reserve(ht.size());

                    for (auto const &phrase : ht) {
                        sorted_phrases.push_back(phrase);
                    }
                    std::sort(sorted_phrases.begin(), sorted_phrases.end(),
                              [&](const size_t &l, const size_t &r) {
                                  return key_w.compare(l, r);
                              });
                    for (auto const &phrase_ptr : sorted_phrases) {
                        ht.insert_value_at(phrase_ptr, rank++);
                    }
                }

                p_data.tail = true;
                p_data.idx = parse.size() - 1;
                lms_scan(parse_task, parse);
                parse.erase(parse.begin(), parse.begin() + p_data.idx + 1);
                lms_cuts.pop_back();
                std::reverse(lms_cuts.begin(), lms_cuts.end());
            }
            ht.flush();
            p_data.lvl++;
        }
    }

public:
    typedef size_t size_type;

    lpg_index(std::string &input_file, std::string &tmp_folder, size_t n_threads, float hbuff_frac) {

        std::cout << "Input file: " << input_file << std::endl;
        auto alphabet = get_alphabet(input_file);

        size_t n_chars = 0;
        for (auto const &sym : alphabet) n_chars += sym.second;

        //create a temporary folder
        std::string tmp_path = tmp_folder + "/lpg_index.XXXXXX";
        char temp[200] = {0};
        tmp_path.copy(temp, tmp_path.size() + 1);
        temp[tmp_path.size() + 1] = '\0';
        auto res = mkdtemp(temp);
        if (res == nullptr) {
            std::cout << "Error trying to create a temporal folder" << std::endl;
        }
        std::cout << "Temporal folder: " << std::string(temp) << std::endl;
        //

        sdsl::cache_config config(false, temp);
        std::string g_file = sdsl::cache_file_name("g_file", config);

        //maximum amount of RAM allowed to spend in parallel for the hashing step
        auto hbuff_size = std::max<size_t>(64 * n_threads, size_t(std::ceil(float(n_chars) * hbuff_frac)));

        std::cout << "Computing the LPG grammar" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        lpg_build::compute_LPG(input_file, g_file, n_threads, config, hbuff_size, alphabet);
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  Elap. time (microsec): " << elapsed.count() << std::endl;


        //plain representation of the grammar
        plain_grammar_t plain_gram;
        plain_gram.load_from_file(g_file);
        std::cout << "Resulting grammar: " << std::endl;
        std::cout << "  Terminals:                " << (size_t) plain_gram.sigma << std::endl;
        std::cout << "  Nonterminals:             " << plain_gram.r - plain_gram.sigma << std::endl;
        std::cout << "  Size of the comp. string: " << plain_gram.c << std::endl;
        std::cout << "  Grammar size:             " << plain_gram.g - plain_gram.sigma << std::endl;

#ifdef DEBUG_PRINT
        plain_gram.print_grammar();
#endif

        std::cout << "Building the self-index" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        build_index(input_file, plain_gram, n_chars, config);
        end = std::chrono::high_resolution_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  Elap. time (microsec): " << elapsed.count() << std::endl;
        double text_size = grammar_tree.get_text_len();
        double index_size = sdsl::size_in_bytes(*this);
        std::cout << "  Index size(bytes) " << sdsl::size_in_bytes(*this) << std::endl;
        std::cout << "  Text size(bytes) " << grammar_tree.get_text_len() << std::endl;
        std::cout << "  (bits/sym) " << index_size * 8 /text_size << std::endl;

    }

    lpg_index() = default;

    lpg_index(const lpg_index &other) {
        grammar_tree = other.grammar_tree;
        m_grid = other.m_grid;
        symbols_map = other.symbols_map;
        m_sigma = other.m_sigma;
        parsing_rounds = other.parsing_rounds;
        rl_compressed = other.rl_compressed;
    }

    void swap(lpg_index &&other) {
        std::swap(grammar_tree, other.grammar_tree);
        std::swap(m_grid, other.m_grid);
        std::swap(symbols_map, other.symbols_map);
        std::swap(m_sigma, other.m_sigma);
        std::swap(parsing_rounds, other.parsing_rounds);
        std::swap(rl_compressed, other.rl_compressed);
    }

    lpg_index(lpg_index &&other) noexcept {
        swap(std::forward<lpg_index>(other));
    }

    lpg_index &operator=(lpg_index &&other) noexcept {
        swap(std::forward<lpg_index>(other));
        return *this;
    }

    lpg_index &operator=(lpg_index const &other) noexcept = default;

    //statistics about the text: number of symbols, number of documents, etc
    void text_stats(std::string &list) {
    }

    void locate(const std::string &pattern, std::set<uint64_t> &pos) const;

    //extract text[start, end] from the index
    void extract(size_t start, size_t end) {
    }

    //search for a list of patterns
    void search(std::vector<std::string> &list) {
        std::cout << "Locate pattern list["<<list.size()<<"]" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        size_t total_occ = 0;
        for (auto const &pattern : list) {
#ifdef DEBUG_PRINT
            std::cout << pattern << ":";
#endif
            std::set<size_type> occ;
            locate(pattern, occ);
            total_occ += occ.size();
#ifdef DEBUG_PRINT
            std::cout <<"-\n";
            for (const auto &item : occ) {
                std::cout << item << " ";
            }
            std::cout << std::endl;
#endif
        }

        double text_size = grammar_tree.get_text_len();
        double index_size = sdsl::size_in_bytes(*this);

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Elap. time (microsec): " << elapsed.count() << std::endl;
        std::cout << "Total occ: " << total_occ << std::endl;
        double time_per_occ = (double)elapsed.count()/(double)total_occ;
        std::cout << "  Elap. time/occ (microsec): " << time_per_occ << std::endl;
        std::cout << "Index size " << sdsl::size_in_bytes(*this) << std::endl;
        std::cout << "Text size " << grammar_tree.get_text_len() << std::endl;
        std::cout << "Bps " << index_size * 8 /text_size << std::endl;


    }

    //search for a list of pattern (input is a file each line is a pattern)
    void search(std::string &input_file) {
        std::fstream in(input_file, std::ios::in);
        std::string pattern;
        long total_time = 0;
        while (in.good() && std::getline(in, pattern)) {
            auto start = std::chrono::high_resolution_clock::now();
                std::set<size_type> occ;
                locate(pattern, occ);
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = (end - start).count();
            total_time += elapsed;
        }
        std::cout << "  Elap. time (secs): " << total_time << std::endl;

    }

    void load(std::istream &in) {
        grammar_tree.load(in);
        m_grid.load(in);
        symbols_map.load(in);
        sdsl::read_member(m_sigma, in);
        sdsl::read_member(parsing_rounds, in);
        sdsl::read_member(rl_compressed, in);
        Y.load(in);
        rank_Y.load(in);


        rank_Y = bv_y::rank_1_type(&Y);
    }

    size_type serialize(std::ostream &out, sdsl::structure_tree_node *v, std::string name) const {
        sdsl::structure_tree_node *child = sdsl::structure_tree::add_child(v, name, sdsl::util::class_name(*this));
        size_t written_bytes = 0;
        written_bytes += grammar_tree.serialize(out, child, "grammar_tree");
        written_bytes += m_grid.serialize(out, child, "m_grid");
        written_bytes += symbols_map.serialize(out, child, "symbols_map");
        written_bytes += sdsl::write_member(m_sigma, out, child, "sigma");
        written_bytes += sdsl::write_member(parsing_rounds, out, child, "sigma");
        written_bytes += sdsl::write_member(rl_compressed, out, child, "sigma");
        written_bytes += Y.serialize(out, child, "Y");
        written_bytes += rank_Y.serialize(out, child, "rank_Y");
        return written_bytes;
    }
    template<typename F>
    bool dfs_mirror_leaf_base_case(const uint64_t &preorder_node, const uint64_t &node, const F &f) const {
        size_type _x = grammar_tree.get_rule_from_preorder_node(preorder_node);//get the rule of the leaf
        if (is_terminal(_x)) {//check if the rule is a terminal symbol
            //terminal case
            bool keep = f(preorder_node, node, _x);//call process function
            return keep; // keep says if we must to stop the expansion of the rule
        }
        //second occ.
        auto fpre_node = grammar_tree.first_occ_from_rule(_x);//get preorder of first mention node
        const auto &T = grammar_tree.getT();
        auto fnode = T[fpre_node];//preorder select

        return dfs_mirror_leaf(fpre_node, fnode, f); // call recursive in the new node...
    }

    template<typename F>
    bool dfs_leaf_base_case(const uint64_t &preorder_node, const uint64_t &node, const F &f) const {
        size_type _x = grammar_tree.get_rule_from_preorder_node(preorder_node);//get the rule of the leaf
        if (is_terminal(_x)) {//check if the rule is a terminal symbol
            //terminal case
            bool keep = f(preorder_node, node, _x);//call process function
            return keep; // keep says if we must to stop the expansion of the rule
        }
        //second occ.
        auto fpre_node = grammar_tree.first_occ_from_rule(_x);//get preorder of first mention node
        const auto &T = grammar_tree.getT();
        auto fnode = T[fpre_node];//preorder select

        return dfs_leaf(fpre_node, fnode, f); // call recursive in the new node...
    }

    template<typename F>
    bool dfs_mirror_leaf(const uint64_t &preorder_node, const uint64_t &node, const F &f) const {
        if (grammar_tree.isLeaf(preorder_node)) { //leaf case
            return dfs_mirror_leaf_base_case(preorder_node, node, f);
        } else {
            auto len = grammar_tree.is_run(preorder_node);
            const auto &T = grammar_tree.getT();
            if (len) {
                for (uint32_t i = 0; i < len; ++i) {
                    auto chnode = T.child(node, 1);
                    bool keep = dfs_mirror_leaf(T.pre_order(chnode), chnode, f);
                    if (!keep) return keep;
                }
                return true;
            } else {
                uint32_t n = T.children(node); //number of children
                for (uint32_t i = n; i > 0; --i) { // from right to left
                    auto chnode = T.child(node, i); //get i-child node
                    bool keep = dfs_mirror_leaf(T.pre_order(chnode), chnode,
                                                f);// compute preorder and visit recursively
                    if (!keep) return keep; // keep says if we must to stop the expansion of the rule
                }
                return true;
            }

        }
    }

    template<typename F>
    bool dfs_leaf(const uint64_t &preorder_node, const uint64_t &node, const F &f) const {
        if (grammar_tree.isLeaf(preorder_node)) { //leaf case
            return dfs_leaf_base_case(preorder_node, node, f);
        } else {
            auto len = grammar_tree.is_run(preorder_node);
            const auto &T = grammar_tree.getT();
            if (len) {
                auto chnode = T.child(node, 1);
                for (uint32_t i = 0; i < len; ++i) {
                    bool keep = dfs_leaf(T.pre_order(chnode), chnode, f);
                    if (!keep) return keep;
                }
            } else {
                uint32_t n = T.children(node); //number of children
                for (uint32_t i = 0; i < n; ++i) { // from left to right
                    auto chnode = T.child(node, i + 1); //get i-child node
                    bool keep = dfs_leaf(T.pre_order(chnode), chnode, f);// compute preorder and visit recursively
                    if (!keep) return keep; // keep says if we must to stop the expansion of the rule
                }
            }
            return true;
        }
    }

    int cmp_prefix_rule(const size_type &preorder_node, const std::string &str, const uint32_t &i) const {
        long ii = i;
        int r = 0;
        bool match = false;
        const auto &m_tree = grammar_tree.getT();
        auto node = m_tree[preorder_node];
        auto cmp = [&str, &r, &match, &ii, this](const size_type &preorder_node, const size_type &node,
                                                 const size_type &X) {
            // X is a terminal rule
            auto c1 = get_symbol(X);//symbols_map[X];
            auto c2 = (uint8_t) str[ii];

            if (c1 > c2) {
                r = 1;
                return false;
            }
            if (c1 < c2) {
                r = -1;
                return false;
            }
            --ii;
            if (ii < 0) {
                match = true;
                return false;
            }
            return true;
        };
        dfs_mirror_leaf(preorder_node, node, cmp);
        // case : rev(grammar rule)  is longer than the rev(string prefix) and rev(string prefix)  is prefix of rev(grammar rule)
        if (r == 0 && match) return 0;
        // case : rev(string prefix) is longer than the rev(grammar rule) and rev(grammar rule) is prefix of rev(string prefix)
        if (r == 0 && !match) return -1;
        // in other case they have a at least a symbol diferent
        return r;

    }
    void print_suffix_grammar(const size_type &preorder_node) const {
        auto cmp = [this](const uint64_t &prenode, const uint64_t &node,const uint64_t &X) {
            auto c1 = get_symbol(X);//symbols_map[X];
            std::cout<<(char)c1;
            return true;
        };
        process_suffix_grammar(preorder_node,cmp);
    }
    template<typename F>
    int process_suffix_grammar(const size_type &preorder_node, const F & f) const{

        const auto &T = grammar_tree.getT();
        auto node = T[preorder_node];//preorder select

        auto fopen = T.bps.find_open(node - 1); // this is to compute parent and childrank
        auto parent = T.pred0(fopen) + 1;//  compute parent
        auto pre_parent = T.pre_order(parent);//  compute parent
        size_type len = grammar_tree.is_run(pre_parent);
        if(len){
            for (size_type j = 1; j < len; ++j){
                auto cnode = T.child(parent, j);
                dfs_leaf(pre_parent + 1, cnode, f);
            }
        }else{
            uint32_t ch = T.children(parent);//  compute children
            uint32_t chr = T.succ0(fopen) - fopen;//  compute childrank
            for (uint32_t j = chr; j <= ch; ++j) {
                auto cnode = T.child(parent, j);
                auto pcnode = T.pre_order(cnode);
                dfs_leaf(pcnode, cnode, f);
            }
        }
        return 1;
    }
    int cmp_suffix_grammar(const size_type &preorder_node, const std::string &str, const uint32_t &i) const {
        uint32_t ii = i, sfx_len = str.size();
        int r = 0;
        bool match = false;
        const auto &T = grammar_tree.getT();
        auto node = T[preorder_node];//preorder select
        auto cmp = [&str, &r, &match, &ii, &sfx_len, this](const uint64_t &prenode, const uint64_t &node,
                                                           const uint64_t &X) {
            auto c1 = get_symbol(X);//symbols_map[X];
            auto c2 = (uint8_t) str[ii];
            if (c1 > c2) {
                r = 1;
                return false;
            }
            if (c1 < c2) {
                r = -1;
                return false;
            }
            ii++;
            if (ii == sfx_len) {
                match = true;
                return false;
            }
            return true;
        };

        auto fopen = T.bps.find_open(node - 1); // this is to compute parent and childrank
        auto parent = T.pred0(fopen) + 1;//  compute parent
        auto pre_parent = T.pre_order(parent);//  compute parent


        size_type len = grammar_tree.is_run(pre_parent);
        if(len){
            for (size_type j = 1; j < len; ++j){
                auto cnode = T.child(parent, j);
                dfs_leaf(pre_parent + 1, cnode, cmp);
                // if they have a at least a symbol diferent return r
                if (r != 0) return r;
                // case : grammar sfx  is longer than the string sfx and string sfx  is prefix of grammar sfx
                if (r == 0 && match) return 0;
                // if r == 0 and !match we process next sibling
            }
            // case : string sfx is longer than the grammar sfx and grammar sfx is prefix of string sfx
            if (r == 0 && !match) return -1;
            // in other case they have a at least a symbol diferent
            return r;

        }else{
            uint32_t ch = T.children(parent);//  compute children
            uint32_t chr = T.succ0(fopen) - fopen;//  compute childrank
            for (uint32_t j = chr; j <= ch; ++j) {
                auto cnode = T.child(parent, j);
                auto pcnode = T.pre_order(cnode);
                dfs_leaf(pcnode, cnode, cmp);
                // if they have a at least a symbol diferent return r
                if (r != 0) return r;
                // case : grammar sfx  is longer than the string sfx and string sfx  is prefix of grammar sfx
                if (r == 0 && match) return 0;
                // if r == 0 and !match we process next sibling
            }
            // case : string sfx is longer than the grammar sfx and grammar sfx is prefix of string sfx
            if (r == 0 && !match) return -1;
            // in other case they have a at least a symbol diferent
            return r;
        }
    }

    inline bool is_terminal(const size_type &X) const {
        return Y[X];
    }

    inline uint8_t get_symbol(const size_type &X) const {
        return symbols_map[rank_Y(X)];
    }

    /**
     * Find the m_grid range to search
     * @return grid_query
     * */

    bool search_grid_range(const char *pattern, const uint32_t &len, const uint32_t &p, const uint32_t &level,
                           grid_query &q) const {
        // search rules range....
        auto cmp_rev_prefix_rule = [&p, &pattern, this](const size_type &rule_id) {
            // compute node definiton preorder of the rule
            uint64_t prenode = grammar_tree.first_occ_from_rule(rule_id);
            return cmp_prefix_rule(prenode, pattern, p - 1);
        };
        uint64_t row_1 = 0, row_2 = grammar_tree.get_size_rules() - 2;
        //search lower
        if (!utils::lower_bound(row_1, row_2, cmp_rev_prefix_rule)) return false;
        q.row1 = row_1;
        //search upper
        row_2 = grammar_tree.get_size_rules() - 2;
        if (!utils::upper_bound(row_1, row_2, cmp_rev_prefix_rule)) return false;
        q.row2 = row_2;
        //search suffixes
        auto cmp_suffix_grammar_rule = [&p, &pattern, this](const size_type &suffix_id) {
            //val is just to use the std lower bound method
            // compute node definiton preorder of the rule
            uint64_t prenode = m_grid.first_label_col(suffix_id);
            return cmp_suffix_grammar(prenode, pattern, p);
        };


///         multiple grid versions
//        auto cmp_suffix_grammar_rule = [ &level, &p, &pattern,this](const size_type &suffix_id,const size_type &val) {
//            //val is just to use the std lower bound method
//            // compute node definiton preorder of the rule
//            uint64_t prenode = m_grid.get_preorder_node_from_suffix(suffix_id,level);
//            return cmp_suffix_grammar(prenode, pattern, p);
//        };
//        std::vector<size_type> sfx_by_level;
//        this->grid.map_suffixes_levels(level,sfx_by_level);
//        //search lower
//        auto it_low = std::lower_bound(sfx_by_level.begin(),sfx_by_level.end(),0,cmp_suffix_grammar_rule);
//        //check we found a match...
//        if(it_low == sfx_by_level.end() || cmp_suffix_grammar_rule(*it_low,0) != 0) return false;
//        q.col1 = *it_low;
//        //search upper
//        //remove elements less than lower bound
//        sfx_by_level.erase(sfx_by_level.begin(),it_low);
//        auto it_upper = std::lower_bound(sfx_by_level.begin(),sfx_by_level.end(),0,cmp_suffix_grammar_rule);
//        //check we found a match...
//        if(it_upper == sfx_by_level.end() || cmp_suffix_grammar_rule(*it_upper,0) != 0) return false;
//        q.col2 = *it_upper;

        uint64_t col_1 = 1, col_2 = m_grid.size_cols();
        //search lower
        if (!utils::lower_bound(col_1, col_2, cmp_suffix_grammar_rule)) return false;
        q.col1 = col_1;
        //search upper
        col_2 = m_grid.size_cols();
        if (!utils::upper_bound(col_1, col_2, cmp_suffix_grammar_rule)) return false;
        q.col2 = col_2;
        //search suffixes
        return true;
    }


    void grid_search(const grid_query &range, const uint64_t &pattern_off, const uint32_t &m, const uint32_t &level,
                     std::vector<utils::primaryOcc> &occ) const {
        std::vector<uint64_t> sfx;
///        multiples grid version
//        m_grid.search(range,level,sfx);
        m_grid.search_2d(range, sfx);
        occ.reserve(sfx.size());
        const auto &T = grammar_tree.getT();
        for (size_type i = 0; i < sfx.size(); ++i) {
            size_type preorder_node = m_grid.first_label_col(sfx[i]);
            size_type node = T[preorder_node];
            size_type leaf = 0;
            size_type off = grammar_tree.offset_node(node, leaf);
            assert(grammar_tree.offset_node(node) - pattern_off >= 0);
            size_type parent = T.parent(node);
            size_type parent_off = grammar_tree.offset_node(parent);
            size_type parent_preorder = T.pre_order(parent);
            size_type run_len = grammar_tree.is_run(parent_preorder);
            if (run_len) {
                // add run length primary occ
                size_type first_child_size = off - grammar_tree.offset_node(parent);
                size_type num_leaves = grammar_tree.num_leaves();
                size_type end_node = leaf == num_leaves ?
                        grammar_tree.get_text_len() : grammar_tree.selectL(leaf + 1) - 1;

                uint32_t pattern_tail = m - pattern_off;
                while (end_node >= off + pattern_tail - 1 ) {
                    occ.emplace_back(parent, parent_preorder, parent_off, (off - parent_off) - pattern_off, true);
                    off += first_child_size ;
                }
            } else {
                size_type prefix_size = off - grammar_tree.offset_node(parent);
                occ.emplace_back(parent, parent_preorder, parent_off, prefix_size - pattern_off, true);
            }

        }
    }

    void find_secondary_occ(const utils::primaryOcc &p_occ, std::set<size_type> &occ) const {

        //queue for node processing
        std::deque<utils::primaryOcc> Q;
        const auto &T = grammar_tree.getT();
        //auxiliar functions
        auto insert_second_mentions = [&Q, &T, this](const utils::primaryOcc &occ) {
            grammar_tree.visit_secondary_occ(occ.preorder, [&T, &Q, &occ, this](const size_type &preorder) {
                size_type node = T[preorder];
                size_type parent = T.parent(node);
                size_type pre_parent = T.pre_order(parent);
                if(grammar_tree.is_run(pre_parent)){
//                    std::cout<<"run"<<pre_parent<<std::endl;
                    // if it is the first child of a run
                    if( pre_parent + 1 == preorder) // we only put the first child
                    { //the rest of the run len is handle in the while...
                        size_type node_off = grammar_tree.offset_node(T[preorder]);
                        Q.emplace_back(node, preorder, node_off, occ.off_pattern);
                    }
                }else{
                    size_type node_off = grammar_tree.offset_node(T[preorder]);
                    Q.emplace_back(node, preorder, node_off, occ.off_pattern);
                }
            });
        };
        //initialize the queue
        if (p_occ.preorder == 1) {
            occ.insert(p_occ.off_pattern);
            return;
        }
        Q.emplace_back(p_occ); // insert a primary occ for the node and all its second mentions
//        std::cout<<"----------\n PRIMARY OCC:"<<Q.size()<<std::endl;
        insert_second_mentions(p_occ);
//        std::cout<<"SECOND MENTIONS:"<<Q.size()<<std::endl;

        while (!Q.empty()) {
            auto top = Q.front(); //first element
            if (top.preorder == 1) { //base case
                occ.insert(top.off_pattern);
            } else {
                //check if the node is a run - length node of a secondary occ
                if (!top.primary && (top.run_len > 0 || grammar_tree.is_run(top.preorder))) {
                    if (top.run_len > 0) {
                        //if fchild_len y len are != 0 then use precomputed values....
                        utils::primaryOcc s_occ(top);
                        //insert himself with all new offsets
                        for (size_type i = 0; i < top.run_len - 1; ++i) {
                            s_occ.off_pattern += top.fchild_len;
                            s_occ.primary = true;
                            Q.push_back(s_occ);
                        }
                    }
                    else {
                        // we arrive from first child
                        // compute child len
                        size_type fchild_len = top.off_node - grammar_tree.offset_node(T.child(top.node, 2));
                        //compute len of the run
                        size_type rlen = grammar_tree.is_run(top.preorder);
                        utils::primaryOcc s_occ(top);
                        s_occ.run_len = rlen;
                        s_occ.fchild_len = fchild_len;
                        s_occ.primary = true;
                        //insert himself with all new offsets
                        for (size_type i = 0; i < rlen - 1; ++i) {
                            s_occ.off_pattern += fchild_len;
                            Q.push_back(s_occ);
                        }
                    }
                }
                // insert in Q parent
                size_type parent = T.parent(top.node);
                size_type preorder_parent = T.pre_order(parent);
                size_type off_parent = grammar_tree.offset_node(parent);

                utils::primaryOcc s_occ(parent, preorder_parent, off_parent,
                                        top.off_pattern + (top.off_node - off_parent));
                Q.push_back(s_occ);
                // insert in Q second occ of the node
                insert_second_mentions(s_occ);
            }
            Q.pop_front();
        }
    }


    void compute_grammar_sfx(utils::nav_grammar &grammar, lpg_build::plain_grammar_t &G,
                             utils::lenght_rules &len, std::vector<utils::sfx> &grammar_sfx,
                             const size_t &init_r) const;

};

void lpg_index::compute_grammar_sfx(
        utils::nav_grammar & grammar,
        lpg_build::plain_grammar_t& G,
        utils::lenght_rules& len,
        std::vector<utils::sfx>& grammar_sfx,
        const size_t & init_r
)const{

    utils::cuts_rules cuts;
    utils::build_nav_cuts_rules(G,cuts); //read the cuts of each rule
    uint64_t preorder = 0;
    std::set<uint64_t> mark;

    const auto& m_tree = grammar_tree.getT();

    for (const auto &r : grammar) {
        if(G.sym_map.find(r.first) == G.sym_map.end() && r.second.size() > 1) {

            size_type preorder = grammar_tree.first_occ_from_rule(r.first);

            auto node = m_tree[preorder];//preorder select

            size_type acc_len = 0;

            for (size_type i = 1; i < r.second.size(); ++i) {
                auto _child = m_tree.child(node, i + 1);
                auto _ch_pre = m_tree.pre_order(_child);
                auto off  = grammar_tree.offset_node(_child);
                acc_len += len[r.second[i-1]].second;
                utils::sfx s(
                        off,//rule off
                        len[r.first].second - acc_len,// len of parent - len of prev-sibling
                        r.second[i-1], //prev-sibling id
                        _ch_pre,//preorder
                        cuts[r.first][i-1]//cut level
                );
                grammar_sfx.push_back(s);

            }
        }
    }


//    utils::dfs(init_r,grammar,[&G,&grammar,&m_tree, &mark,&preorder,&cuts,&len,&grammar_sfx,this](const size_type& id){
//
//        auto it_rule = grammar.find(id);
//        ++preorder;
//        if(mark.find(id) != mark.end())
//            return false; // stop descending if it is second mention
//        mark.insert(id);
//
//        if(is_terminal(id)) return false;
//
//        size_type n_children = it_rule->second.size();
//
//        if(n_children <= 0) return false; // if leaf break and stop descending
//
//        auto node = m_tree[preorder];//preorder select
//        size_type acc_len = 0;
//
//        for(size_type j = 1; j < n_children; ++j){
//            auto _child = m_tree.child(node, j + 1);
//            acc_len += len[it_rule->second[j-1]].second;
//            utils::sfx s(
//                    grammar_tree.offset_node(_child),//rule off
//                    len[it_rule->first].second - acc_len,// len of parent - len of prev-sibling
//                    it_rule->second[j-1], //prev-sibling id
//                    m_tree.pre_order(_child),//preorder
//                    cuts[it_rule->first][j-1]//cut level
//            );
//
//            grammar_sfx.push_back(s);
//#ifdef DEBUG_PRINT
//            std::cout<<"id:"<<id<<":P["<<m_tree.pre_order(_child)<<"]"<<"R["<<it_rule->second[j-1]<<"]";
//            print_suffix_grammar(m_tree.pre_order(_child));
//            std::cout<<std::endl;
//#endif
//        }
//        return true;
//    });
#ifdef DEBUG_INFO
    std::cout<<"compute_grammar_sfx\n";
#endif
}

void lpg_index::locate(const std::string &pattern, std::set<uint64_t> &pos)  const {
        auto partitions  = compute_pattern_cut(pattern);

//        std::cout<<partitions.first.size()<<std::endl;
        uint32_t level = partitions.second;
//        for (const auto &item : partitions.first) {
        for(uint item = 0; item < pattern.size() - 1;++item){
            //find primary occ
            grid_query range{};
            //range search
            if(search_grid_range(pattern.c_str(),pattern.size(),item + 1,level, range)){
                std::vector<utils::primaryOcc> pOcc;
                // grid search
                grid_search(range,item + 1,pattern.size(),level,pOcc);
                // find secondary occ

//                std::cout<<"find_secondary_occ:n_p:"<<pOcc.size()<<std::endl;
                for (const auto &occ : pOcc) {
                    find_secondary_occ(occ,pos);
                }
            }
        }
}


#endif //LMS_GRAMMAR_REP_HPP