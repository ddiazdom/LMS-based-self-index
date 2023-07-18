#include <chrono>
#include <thread>

#include "third-party/CLI11.hpp"
#include "lpg/lpg_index.hpp"
#include <filesystem>

void generate_random_samples(const std::string &file, const std::string &o_file, const uint32_t& len, const uint32_t& samples){

    std::string data;
    utils::readFile(file,data);
    std::fstream out_f(o_file, std::ios::out | std::ios::binary);

    std::srand(time(nullptr));
    uint32_t i = 0;
    std::set<std::string> M;
    while (i < samples){
        size_t pos = std::rand()%data.size();
        if(pos + len <= data.size()) {
            std::string ss;
            ss.resize(len);
            std::copy(data.begin()+pos,data.begin()+pos+len,ss.begin());
            auto res = ss.find('\n');
            if(M.find(ss) == M.end() && res==std::string::npos){
                M.insert(ss);
                out_f<<ss+'\n';
                i++;
            }
        }
    }
}


struct arguments{
    std::string input_file;
    std::string output_file;
    std::string patter_list_file;
    std::vector<std::string> patterns;

    std::string tmp_dir;
    size_t n_threads{};
    float hbuff_frac=0.5;
    bool ver=false;

    size_t pat_len{};
    size_t n_pat{};
    bool ind_report=false;

    std::string version="0.0.1.alpha";

};

class MyFormatter : public CLI::Formatter {
public:
    MyFormatter() : Formatter() {}
    std::string make_option_opts(const CLI::Option *) const override { return ""; }
};

static void parse_app(CLI::App& app, struct arguments& args){
    
	auto fmt = std::make_shared<MyFormatter>();

    fmt->column_width(23);
    app.formatter(fmt);

    CLI::App *index = app.add_subcommand("index", "Create an LPG self-index");
    CLI::App *search = app.add_subcommand("search", "Search for a pattern in the index");
    CLI::App *rand_pat = app.add_subcommand("rpat", "Extract random patterns from the text");

    app.set_help_all_flag("--help-all", "Expand all help");
    app.add_flag("-v,--version", args.ver, "Print the software version and exit");

    index->add_option("TEXT", args.input_file, "Input text file")->check(CLI::ExistingFile)->required();
    index->add_option("-o,--output-file", args.output_file, "Output file")->type_name("");
    index->add_option("-t,--threads", args.n_threads, "Maximum number of threads")->default_val(1);
    index->add_option("-f,--hbuff", args.hbuff_frac, "Hashing step will use at most INPUT_SIZE*f bytes. O means no limit (def. 0.5)")-> check(CLI::Range(0.0,1.0))-> default_val(0.5);
    index->add_option("-T,--tmp", args.tmp_dir, "Temporal folder (def. /tmp/lpg_index.xxxx)")->check(CLI::ExistingDirectory)->default_val("/tmp");

    search->add_option("INDEX", args.input_file, "Input LPG index file")->check(CLI::ExistingFile)->required(true);
    search->add_flag("-r,--ind-report", args.ind_report, "Flag to report the result for each pattern individually");
    CLI::Option_group *opt = search->add_option_group("Pattern options");
    opt->add_option("-p,--patterns", args.patterns, "Pattern to search for in the index");
    opt->add_option("-F,--pattern-list", args.patter_list_file, "File with a pattern list");
    opt->require_option(1, 2);
    //search->add_option("-t,--threads", args.n_threads, "Maximum number of threads")->default_val(1);
    //search->add_option("-o,--output-file", args.output_file, "Output file")->type_name("");

    rand_pat->add_option("TEXT", args.input_file, "Input text file")->check(CLI::ExistingFile)->required();
    rand_pat->add_option("PAT_LEN", args.pat_len, "Pattern length")->required()->check(CLI::Range(5, 10000));
    rand_pat->add_option("N_PATS", args.n_pat, "Pattern length")->required()->check(CLI::Range(1, std::numeric_limits<int>::max()));
    rand_pat->add_option("-o,--output-file", args.output_file, "Output file")->type_name("");

    app.require_subcommand(1);
    app.footer("By default, lpg_index will compress FILE if -c,-d or -b are not set\n\nReport bugs to <diediaz@dcc.uchile.cl>");
}

int main(int argc, char** argv) {

	arguments args;

    CLI::App app("A grammar-based self-index");
    parse_app(app, args);

    CLI11_PARSE(app, argc, argv);

    if(app.got_subcommand("index")) {

        lpg_index g(args.input_file, args.tmp_dir, args.n_threads, args.hbuff_frac);

        if(args.output_file.empty()){
            args.output_file = std::filesystem::path(args.input_file).filename();
        }
        args.output_file = std::filesystem::path(args.output_file).replace_extension(".lpg_idx");

        std::cout<<"Saving the self-index to file "<<args.output_file<<std::endl;
        sdsl::store_to_file(g, args.output_file);
    }else if(app.got_subcommand("search")){

        std::cout<<"Searching for patterns in the self-index"<<std::endl;
        lpg_index g;
        sdsl::load_from_file(g, args.input_file);
        std::cout<<"Index stats"<<std::endl;
        std::cout<<"  Index name:                                              "<<args.input_file<<std::endl;
        std::cout<<"  Index size:                                              "<<sdsl::size_in_bytes(g)<<" bytes "<<std::endl;
        std::cout<<"  Orig. text len:                                          "<<g.text_size()<<std::endl;
        std::cout<<"  Number of bits in the index per input text symbol (bps): "<<g.bps()<<std::endl;
        std::set<std::string> patterns_set;

        if (!args.patter_list_file.empty()){

            std::fstream in(args.patter_list_file,std::ios::in|std::ios::binary);

            std::string line;
            while (std::getline(in, line)) {
                args.patterns.push_back(line);
            }
            /*if(in.good()){
                uint32_t len, samples;
                in.read((char *)&len,sizeof (uint32_t));
                in.read((char *)&samples,sizeof (uint32_t));
                char *buff = new char[len];
                for (uint32_t i = 0; i < samples ; ++i) {
		//	std::cout<<i<<" "<<len<<std::endl;
			        in.read(buff,len);
                    std::string ss;ss.resize(len);
                    std::copy(buff,buff+len,ss.begin());
                    patterns_set.insert(ss);
                    args.patterns.push_back(ss);
                }
                delete[] buff;
            }*/
        }
#ifdef CHECK_OCC
        std::string file; file.resize(args.input_file.size() - 8);
        std::copy(args.input_file.begin(),args.input_file.end()-8,file.begin());
        std::cout<<"file:"<<file<<std::endl;
#endif
//
//        uint64_t rules = g.grammar_tree.Z.size();
//        for (uint64_t i = 0; i < rules - 1 ; ++i) {
//            uint64_t X = g.grammar_tree.get_rule_from_preorder_node(i+1);
//            std::cout<<i+1<<"["<<X<<"]"<<"->";
//            g.print_prefix_rule(i+1,1000);
//
//        }
     if(!args.patterns.empty()){
         std::cout<<"Searching for the patterns "<<std::endl;
         g.search(args.patterns, args.ind_report
#ifdef CHECK_OCC
			                    ,file
#endif
        );

//         g.search_split_time(patterns
//#ifdef CHECK_OCC
//                 ,file
//#endif
//         );


//std::cout<<"search_all_cuts"<<std::endl;
//         g.search_all_cuts(args.patterns
//#ifdef CHECK_OCC
//                 ,file
//#endif
//         );
    }
	// g.search(args.patterns);
    // g.search(args.patter_list_file);
    } else if(app.got_subcommand("rpat")){
        if(args.output_file.empty()){
            args.output_file = std::filesystem::path(args.input_file).filename();
            std::string ext = std::to_string(args.n_pat)+"_"+std::to_string(args.pat_len);
            args.output_file = std::filesystem::path(args.output_file).replace_extension(ext);
        }
        std::cout<<"Extracting "<<args.n_pat<<" random patterns of length "<<args.pat_len<<" from "<<args.input_file<<std::endl;
        generate_random_samples(args.input_file, args.output_file, args.pat_len, args.n_pat) ;
        std::cout<<"The random patterns were stored in "<<args.output_file<<std::endl;
    }
    return 0;
}
