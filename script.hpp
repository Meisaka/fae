#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

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
enum class ASTSlots : uint8_t {
	// [2 1] count of fixed slots
	// [0] is var slots open
	NONE = 0,
	VAR = 1,
	SLOT1 = 2,
	SLOT1VAR = 3,
	SLOT2 = 4,
	SLOT2VAR = 5,
	SLOTSMASK = 6,
};
struct ASTNode {
	LexTokenRange block;
	Token ast_token;
	Expr asc;
	uint8_t precedence;
	WS whitespace;
	ASTSlots slots;
	ASTNode() = delete;
	ASTNode(Token token, Expr a, LexTokenRange lex_range);
	ASTNode(Token token, Expr a, ASTSlots e, LexTokenRange lex_range);
	node_ptr slot1;
	node_ptr slot2;
	expr_list_t list;
	bool open() const {
		uint8_t flags = static_cast<uint8_t>(slots);
		if(flags & 1) return true;
		uint8_t count = (flags & static_cast<uint8_t>(ASTSlots::SLOTSMASK)) >> 1;
		return (count > 0 && !slot1) || (count > 1 && !slot2);
	}
	bool slot1_open() const {
		return (slot_count() > 0 && !slot1);
	}
	bool slot2_open() const {
		return (slot_count() > 1 && slot1 && !slot2);
	}
	bool empty() const {
		return !slot1 && !slot2 && list.empty();
	}

	static ASTSlots make_open(ASTSlots s) {
		return static_cast<ASTSlots>(static_cast<uint8_t>(s) | 1);
	}
	uint8_t slot_count() const {
		return static_cast<uint8_t>(slots) >> 1;
	}
	void mark_open() {
		slots = make_open(slots);
	}
	void mark_closed() {
		slots = static_cast<ASTSlots>(static_cast<uint8_t>(slots) & 6);
	}
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

