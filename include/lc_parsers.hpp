//
// Created by Diaz, Diego on 24.11.2021.
//

#ifndef LPG_COMPRESSOR_LC_PARSERS_HPP
#define LPG_COMPRESSOR_LC_PARSERS_HPP

#define U_TYPE 0 //unknown type
#define L_TYPE 1
#define S_TYPE 2

template<class stream_t,
         class string_t>
struct lms_parsing{

    typedef stream_t                       stream_type;
    typedef typename stream_type::sym_type sym_type;

    const std::vector<size_t>& suf_pos;//which positions of the input text store symbols that expand to suffixes
    const size_t min_sym; //smallest symbol of the last parsing round
    const size_t max_sym; //greatest symbol of the last parsing round

    explicit lms_parsing(const std::vector<size_t>& suf_pos_, size_t min_sym_, size_t max_sym_): suf_pos(suf_pos_),
                                                                                                 min_sym(min_sym_),
                                                                                                 max_sym(max_sym_){};

    inline bool out_of_alphabet(sym_type sym) const{
        return sym < min_sym;
    }

    /***
    * Find the greatest position j such that ifs[j]<=ifs[idx] and ifs[j] is S*
    * @param idx : the scan starts from this position
    * @param ifs : input file stream
    * @return
    */
    long long prev_break(long long idx, stream_t& ifs) const {

        if(idx<=0) return 0;

        size_t p_idx = std::distance(suf_pos.begin(),
                                     std::lower_bound(suf_pos.begin(), suf_pos.end(), idx));
        size_t sym=ifs.read(idx), prev_sym=sym;
        size_t pos = idx;

        while(pos<suf_pos[p_idx-1] && prev_sym == sym) {
            prev_sym = ifs.read(pos++);
        }

        bool type, prev_type = sym < prev_sym;
        prev_sym = sym;

        while(true) {
            idx--;
            if(idx<0) return idx;

            sym = ifs.read(idx);
            type = sym==prev_sym? prev_type : sym < prev_sym;

            if(idx==(long long)suf_pos[p_idx-1] || (type==L_TYPE && prev_type==S_TYPE)){
                return idx+1;
            }
            prev_sym = sym;
            prev_type = type;
        }
    }

    void operator()(stream_t& ifs,
                    size_t start, size_t end,
                    std::function<void(string_t&)> task) const {

        //get the rightmost position j before T[end] such that T[j]
        // expands to a suffix of some string
        size_t p_idx = std::distance(suf_pos.begin(),
                                     std::lower_bound(suf_pos.begin(), suf_pos.end(), end))-1;
        size_t prev_suf_pos = suf_pos[p_idx];

        //if end is no the last symbol, we will assume that end is
        // the symbol to the left of a local minima
        uint8_t s_type, prev_s_type;
        sym_type curr_sym, prev_sym;
        string_t curr_lms(2, sdsl::bits::hi(max_sym)+1);

        if(ifs.read(end)==10){
            end--;
            prev_s_type = U_TYPE;
        }else{
            prev_s_type = L_TYPE;
        }

        prev_sym = ifs.read(end);
        curr_lms.push_back(prev_sym);

        //TODO testing
        for (size_t i = end+1; i-- > start;) {
            if(ifs.read(i)==10){
                std::cout<<"$"<<"";
            }else{
                std::cout<<(char)ifs.read(i)<<"";
            }
        }
        std::cout<<""<<std::endl;
        //

        for (size_t i = end; i-- > start;) {

            curr_sym = ifs.read(i);

            //                                     L_TYPE   S_TYPE*
            //                                        ---- ----
            //this is a junction between two strings = ...$ $...
            if (i==prev_suf_pos ||
                out_of_alphabet(curr_sym)) {

                //TODO testing
                for(size_t j=0;j<curr_lms.size();j++){
                    if(j==(curr_lms.size()-1)){
                        std::cout<<"*";
                    }else{
                        if(curr_lms[j]==10){
                            std::cout<<"$"<<"";
                        }else{
                            std::cout<<(char)curr_lms[j]<<"";
                        }
                    }
                }
                //

                //get the previous text position j such that
                // T[j] recursively expands to a suffix of
                // some string in the input collection
                if(i==prev_suf_pos){
                    prev_suf_pos = suf_pos[--p_idx];
                    curr_lms.push_back(0);
                }

                task(curr_lms);
                curr_lms.clear();

                if(curr_sym==10){
                    std::cout<<" ";
                    i--;
                    curr_sym = ifs.read(i);
                }
                s_type = U_TYPE;
            } else {
                if (curr_sym < prev_sym) {//S_TYPE type
                    s_type = S_TYPE;
                } else if (curr_sym == prev_sym) {
                    s_type = prev_s_type;
                } else {//L_TYPE type
                    s_type = L_TYPE;
                    if (prev_s_type == S_TYPE) {//Left-most S suffix
                        task(curr_lms);

                        //TODO testing
                        for(size_t j=0;j<curr_lms.size();j++){
                            if(j==(curr_lms.size()-1)){
                                std::cout<<"*";
                            }else{
                                if(curr_lms[j]==10){
                                   std::cout<<"$"<<"";
                                }else{
                                    std::cout<<(char)curr_lms[j]<<"";
                                }
                            }
                        }
                        //

                        curr_lms.clear();
                    }
                }
            }
            curr_lms.push_back(curr_sym);
            prev_sym = curr_sym;
            prev_s_type = s_type;
        }
        assert(curr_lms[0]!=1);
        task(curr_lms);
        std::cout<<""<<std::endl;
    }

    std::vector<std::pair<size_t, size_t>> partition_text(size_t n_chunks,
                                                          std::string& i_file) const {
        std::vector<std::pair<size_t, size_t>> thread_ranges;

        stream_type is(i_file, BUFFER_SIZE);
        size_t n_chars = is.tot_cells;
        assert(n_chars>0);
        size_t sym_per_chunk = INT_CEIL(n_chars, n_chunks);
        size_t start, end;
        size_t eff_threads = INT_CEIL(n_chars, sym_per_chunk);

        for(size_t i=0;i<eff_threads;i++){
            start = (i * sym_per_chunk);
            end = std::min<size_t>(((i + 1) * sym_per_chunk), n_chars-1);

            start = start==0? 0 : size_t(prev_break(start, is));
            if(end!=n_chars-1){
                long long tmp_end = prev_break(end, is);
                end = tmp_end<0?  0 : size_t(tmp_end);
                //if(is_suffix(is.read(end-1))) end--;
            }

            //std::cout<<"range: "<<start<<" "<<end<<std::endl;
            if(start<end){
                thread_ranges.emplace_back(start, end);
            }
        }
        is.close();
        return thread_ranges;
    }
};

#endif //LPG_COMPRESSOR_LC_PARSERS_HPP
