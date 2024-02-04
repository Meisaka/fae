
#include "script.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace Fae {
bool LoadFileV(const string_view path, string &outdata) {
	auto file = std::fstream(string(path), std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	if(!file.is_open()) return false;
	size_t file_length = file.tellg();
	file.seekg(std::ios_base::beg);
	char *data = new char[file_length];
	file.read(data, file_length);
	outdata.assign(data, file_length);
	//std::cerr << "loading " << outdata.size() << " bytes\n";
	return true;
}
}

Fae::module_source_ptr load_syntax_tree(std::string_view const file) {
	std::string source;
	if(!Fae::LoadFileV(file, source)) return nullptr;
	return std::move(Fae::load_syntax_tree(std::move(source)));
}

int main(int argc, char**argv) {
	std::vector<std::string_view> str_args;
	auto script = std::make_unique<Fae::ScriptContext>();
	for(int i = 0; i < argc; i++) {
		str_args.emplace_back(std::string_view(argv[i]));
	}
	bool once = true;
	bool f_only_parse = false;
	bool f_only_compile = false;
	bool f_flag_errors = false;
	bool f_syntax_tree = false;
	uint32_t verbose = 0;
	bool any_files_were_processed = false;
	Fae::module_source_ptr test_syntax_tree;
	auto arg_itr = str_args.cbegin() + 1;
	auto end = str_args.cend();
	auto nullout = std::ostream(nullptr);
	for(;arg_itr != end; arg_itr++) {
		auto &arg = *arg_itr;
		if(arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
			for(size_t sw = 1; sw < arg.size(); sw++) {
				switch(arg[sw]) {
				case 'c': f_only_compile = true; break;
				case 'p': f_only_parse = true; break;
				case 'T': f_syntax_tree = true; break;
				case 'v': verbose++; break;
				default:
					std::cerr << "unknown flag: " << arg << '\n';
					f_flag_errors = true;
				}
			}
		} else if(!f_flag_errors) {
			if(f_syntax_tree) {
				f_syntax_tree = false;
				test_syntax_tree = load_syntax_tree(arg);
				if(!test_syntax_tree) return 1;
			} else if(f_only_parse) {
				f_only_parse = false;
				any_files_were_processed = true;
				auto parsed = Fae::test_parse_sourcefile(verbose >= 2 ? std::cerr : nullout, arg);
				if(!parsed) return 1;
				auto &parsed_source = Fae::get_source(parsed);
				if(test_syntax_tree) {
					if(verbose) {
						Fae::show_source_tree(std::cerr, *test_syntax_tree);
						Fae::show_source_tree(std::cout, parsed_source);
					}
					if(!Fae::show_node_diff(std::cerr, *parsed_source.root_tree, *parsed_source.root_tree)) {
						std::cerr << "equality function has failed\n";
						return 2;
					}
					if(!Fae::show_node_diff(std::cerr, *test_syntax_tree->root_tree, *test_syntax_tree->root_tree)) {
						std::cerr << "equality function has failed\n";
						return 2;
					}

					return Fae::show_node_diff(std::cerr, *test_syntax_tree->root_tree, *parsed_source.root_tree)
						? 0 : 1;
				}
				if(verbose) {
					//auto dbg_file = std::fstream("./debug.txt", std::ios_base::out | std::ios_base::trunc);
					Fae::show_source_tree(std::cout, parsed_source);
				}
			} else if(f_only_compile) {
				f_only_compile = false;
				any_files_were_processed = true;
				auto dbg_file = std::fstream("./debug.txt", std::ios_base::out | std::ios_base::trunc);
				Fae::test_compile_sourcefile(dbg_file, arg);
			} else {
				any_files_were_processed = true;
				script->LoadScriptFile(arg, arg);
				script->FunctionCall(arg);
			}
		}
	}
	if(test_syntax_tree && verbose && !any_files_were_processed)
		Fae::show_source_tree(std::cerr, *test_syntax_tree);
	return 0;
}

