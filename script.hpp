#pragma once
#include <string>
#include <memory>
#include <unordered_map>

namespace Fae {

using string = std::string;
using string_view = std::string_view;

bool LoadFileV(const string_view path, string &outdata);
enum class WS : uint8_t { NONE, SP, NL };
enum class Expr : uint8_t;
enum class Token : uint8_t;
struct ASTNode;
using source_itr = string::const_iterator;
struct LexTokenRange {
	source_itr tk_begin;
	source_itr tk_end;
	Token token;
	std::string_view as_string() const;
	std::string_view as_string(size_t offset) const;
};

typedef std::unique_ptr<ASTNode> node_ptr;
typedef std::vector<node_ptr> expr_list_t;
struct ASTNode {
	LexTokenRange block;
	uint32_t meta_value;
	Token ast_token;
	Expr asc;
	uint8_t precedence;
	WS whitespace;
	ASTNode() = delete;
	ASTNode(Token token, Expr a, LexTokenRange lex_range);
	ASTNode(Token token, Expr a, uint8_t e, LexTokenRange lex_range);
	expr_list_t expressions;
};
struct ModuleSource {
	string source;
	node_ptr root_tree;
};
typedef std::unique_ptr<ModuleSource> module_source_ptr;
module_source_ptr load_syntax_tree(string &&source);

struct ModuleContext;
typedef std::shared_ptr<ModuleContext> module_ptr;
ModuleSource& get_source(const module_ptr &);
bool show_node_diff(std::ostream &out, const ASTNode &expected, const ASTNode &actual);
void show_source_tree(std::ostream &out, const ModuleSource &source);
void show_scopes(std::ostream &out, const ModuleContext &module);
void show_lines(std::ostream &out, const ModuleContext &module);
void show_string_table(std::ostream &out, const ModuleContext &module);
module_ptr compile_sourcefile(string file_source);
module_ptr test_parse_sourcefile(std::ostream &dbg, const string_view file_path);
module_ptr test_compile_sourcefile(std::ostream &dbg, const string_view file_path);

class ScriptContext {
public:

	ScriptContext();
	virtual ~ScriptContext();

	virtual void FunctionCall(const string_view i);
	virtual void LoadScriptFile(const string_view f, const string_view i);
private:
	std::unordered_map<string, std::shared_ptr<ModuleContext>> script_map;
};
}

