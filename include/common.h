//
// Created by Diaz, Diego on 23.11.2021.
//

#ifndef LPG_COMPRESSOR_COMMON_H
#define LPG_COMPRESSOR_COMMON_H
#include <sdsl/bit_vectors.hpp>
#include "cdt/hash_table.hpp"
#include "cdt/int_array.h"
#include "cdt/file_streams.hpp"

#define BUFFER_SIZE 8388608 //8MB of buffer
typedef sdsl::bit_vector                        bv_t;
typedef sdsl::bit_vector::rank_1_type           bv_rs_t;
typedef sdsl::bit_vector::select_1_type         bv_ss_t;
typedef sdsl::int_vector_buffer<1>              bvb_t;
typedef sdsl::int_vector_buffer<>               ivb_t;
typedef sdsl::int_vector<>                      vector_t;
typedef int_array<size_t>                       string_t;
typedef bit_hash_table<size_t,44>               phrase_map_t;
typedef bit_hash_table<bool,1>                  dict_t;
typedef typename dict_t::buff_t                 ht_buff_t;
typedef std::vector<std::pair<uint8_t, size_t>> alpha_t;

struct string_collection{
    std::vector<size_t> suf_pos;
    std::vector<std::pair<uint8_t, size_t>> alphabet;
    size_t n_syms=0;
};


// the phrases are stored in a bit-compressed hash table:
// this wrapper reinterprets the bits back as phrases
struct key_wrapper{
    size_t width;
    size_t d_bits;//bits used to describe the string
    const bitstream<ht_buff_t>& stream;

    //offset is the bit where the key description starts
    [[nodiscard]] inline size_t read(size_t offset, size_t idx) const {
        return stream.read(offset + d_bits + (idx * width),
                           offset + d_bits + ((idx + 1) * width - 1));
    }

    //offset is the bit where the key description starts
    [[nodiscard]] inline size_t size(size_t offset) const{
        return stream.read(offset, offset + d_bits - 1) / width;
    }

    //compare two phrases are different positions of the bit stream
    [[nodiscard]] inline bool compare(size_t a, size_t b) const{

        size_t a_bits = stream.read(a, a + d_bits - 1);
        size_t b_bits = stream.read(b, b + d_bits - 1);
        size_t min_bits = std::min<size_t>(a_bits, b_bits);

        size_t a_pos = a+d_bits+a_bits-1;
        size_t b_pos = b+d_bits+b_bits-1;
        size_t rm_diff = stream.inv_com_segments(a_pos, b_pos, min_bits);

        if(rm_diff < min_bits){
            a_pos = a+d_bits+(((a_bits - rm_diff-1) / width) * width);
            b_pos = b+d_bits+(((b_bits - rm_diff-1) / width) * width);
            size_t sym_a = stream.read(a_pos, a_pos+width-1);
            size_t sym_b = stream.read(b_pos, b_pos+width-1);
            return sym_a<sym_b;
        }else{
            return a_bits>b_bits;
        }
    }
};

struct dictionary {
    size_t min_sym;
    size_t max_sym;
    size_t alphabet;
    size_t n_phrases;
    vector_t dict;
    bv_t d_lim;
    typedef size_t size_type;

    dictionary(phrase_map_t &mp_map, size_t _min_sym, size_t _max_sym,
               key_wrapper &key_w, size_t dict_syms): min_sym(_min_sym),
                                                      max_sym(_max_sym),
                                                      alphabet(max_sym-min_sym+1),
                                                      n_phrases(mp_map.size()),
                                                      dict(dict_syms, 0, sdsl::bits::hi(alphabet-1)+1),
                                                      d_lim(dict_syms, false){
        size_t j=0;
        for (auto const &ptr : mp_map) {
            for(size_t i=key_w.size(ptr);i-->0;){
                dict[j] = key_w.read(ptr, i)-min_sym;
                d_lim[j++] = false;
            }
            d_lim[j-1] = true;
        }
        assert(j==dict_syms);
    }

    size_type serialize(std::ostream& out, sdsl::structure_tree_node * v=nullptr, std::string name="") const{
        sdsl::structure_tree_node* child = sdsl::structure_tree::add_child( v, name, sdsl::util::class_name(*this));
        size_type written_bytes= sdsl::write_member(min_sym, out, child, "min_sym");
        written_bytes+= sdsl::write_member(max_sym, out, child, "max_sym");
        written_bytes+= sdsl::write_member(alphabet, out, child, "alphabet");
        written_bytes+= sdsl::write_member(n_phrases, out, child, "n_phrases");
        dict.serialize(out, child);
        d_lim.serialize(out, child);
        return written_bytes;
    }

    void load(std::istream& in){
        sdsl::read_member(min_sym, in);
        sdsl::read_member(max_sym, in);
        sdsl::read_member(alphabet, in);
        sdsl::read_member(n_phrases, in);
        dict.load(in);
        d_lim.load(in);
    }
};
#endif //LPG_COMPRESSOR_COMMON_H
