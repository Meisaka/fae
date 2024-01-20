
#include <ranges>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include "script.hpp"

using namespace std::string_view_literals;

namespace Fae {

using string = std::string;

ScriptContext::ScriptContext() {}
ScriptContext::~ScriptContext() {}

void LoadFileV(const string path, string &outdata) {
	auto file = std::fstream(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	size_t where = file.tellg();
	file.seekg(std::ios_base::beg);
	outdata.resize(where);
	file.read(outdata.data(), where);
	std::cerr << "loading " << where << " bytes\n";
}

typedef uint8_t lex_t[256];
static lex_t lex_table[] {
	{0}
};
static bool contains(const std::string_view &str, char c) {
	return std::ranges::any_of(str, [&](auto sc) { return sc == c; });
}
#define DEF_TOKENS(f) \
	f(Invalid) f(Eof) f(White) f(Newline) f(Comma) f(Semi) \
	f(Ident) f(Keyword) \
	f(Block) f(Array) f(Object) f(EndDelim) \
	f(DotBlock) f(DotArray) f(DotObject) \
	f(K_If) f(K_ElseIf) f(K_Else) \
	f(K_While) f(K_Until) f(K_Loop) f(K_Continue) f(K_Break) \
	f(K_Goto) f(K_End) \
	f(K_For) f(K_In) \
	f(K_Let) f(K_Mut) \
	f(K_True) f(K_False) \
	f(K_Import) f(K_Export) \
	f(O_RefExpr) \
	f(O_Minus) f(O_Not) f(O_Length) \
	f(O_And) f(O_Or) f(O_Xor) \
	f(O_Add) f(O_Sub) f(O_Mul) f(O_Power) \
	f(O_Div) f(O_Mod) \
	f(O_DotP) f(O_Cross) \
	f(O_Rsh) f(O_RAsh) f(O_Lsh) \
	f(O_AndEq) f(O_OrEq) f(O_XorEq) \
	f(O_AddEq) f(O_SubEq) f(O_MulEq) f(O_PowerEq) \
	f(O_DivEq) f(O_ModEq) \
	f(O_CrossEq) \
	f(O_RshEq) f(O_RAshEq) f(O_LshEq) \
	f(O_DivMod) \
	f(O_Assign) \
	f(O_Less) f(O_Greater) \
	f(O_LessEq) f(O_GreaterEq) \
	f(O_EqEq) f(O_NotEq) \
	f(O_Dot) \
	f(O_Range) \
	f(Zero) f(Number) f(NumberBin) f(NumberOct) f(NumberHex) \
	f(NumberReal) \
	f(Unit) \
	f(String) \
	f(Hash) \
	f(Oper) f(OperEqual) \
	f(Less) f(LessEqual) f(LessUnit) f(LessDot) \
	f(Equal) f(EqualEqual) \
	f(Minus) \
	f(Greater) f(GreaterEqual) f(GreaterUnit) f(GreaterDot) \
	f(Arrow) f(Func) f(O_Spaceship) \
	f(Dot) f(DotDot) f(DotDotDot) f(DotUnit) f(DotIdent) \
	f(RangeIncl) \
	f(DotOper) f(DotOperEqual) \
	f(DotOperPlus) f(DotDotPlusEqual) \
	f(O_Compose) \
	f(O_LeftRot) f(O_RightRot) \
	f(O_LeftRotIn) f(O_RightRotIn) \
	f(Xor) f(XorDot) \
	f(Slash)

enum class token_t : uint8_t {
#define GENERATE(f) f,
	DEF_TOKENS(GENERATE)
#undef GENERATE
};
static std::string_view token_name(token_t tk) {
	switch(tk) {
#define GENERATE(f) case token_t::f: return #f##sv;
	DEF_TOKENS(GENERATE)
#undef GENERATE
	default: return "?"sv;
	}
}
enum class LexState {
	Free,
	Remain,
	Comment,
	CommentBlock,
	DString,
	SString,
};
enum class LexAction {
	Remain,
	Promote,
	NewToken,
	FixToken,
	DefaultNewToken,
};

struct LexNode {
	LexAction action;
	token_t first_token;
	token_t next_token;
	LexState next_state;
	constexpr LexNode(LexAction act, token_t chk) :
		action{ act },
		first_token{ chk },
		next_token{token_t::Invalid},
		next_state{LexState::Remain} {}
	constexpr LexNode(LexAction act, token_t chk, token_t next) :
		action{ act },
		first_token{ chk },
		next_token{next},
		next_state{LexState::Remain} {}
	constexpr LexNode(LexAction act, token_t chk, token_t next, LexState state) :
		action{ act },
		first_token{ chk },
		next_token{next},
		next_state{state} {}
};
struct LexCharClass {
	char begin;
	char end;
	std::vector<LexNode> nodes;
};

const LexCharClass lex_tree[] = {
LexCharClass{ '_', '_', {
	LexNode(LexAction::NewToken, token_t::Zero, token_t::Number),
	LexNode(LexAction::Remain, token_t::NumberBin),
	LexNode(LexAction::Remain, token_t::NumberHex),
	LexNode(LexAction::Remain, token_t::NumberOct),
	LexNode(LexAction::Remain, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Unit, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Less, token_t::LessUnit),
	LexNode(LexAction::Promote, token_t::Greater, token_t::GreaterUnit),
	LexNode(LexAction::DefaultNewToken, token_t::Unit),
}},{'A','F', { LexNode(LexAction::Remain, token_t::NumberHex)
}},{'a','f', { LexNode(LexAction::Remain, token_t::NumberHex)
}},{'b','b', { LexNode(LexAction::Promote, token_t::Zero, token_t::NumberBin)
}},{'B','B', { LexNode(LexAction::Promote, token_t::Zero, token_t::NumberBin)
}},{'x','x', { LexNode(LexAction::Promote, token_t::Zero, token_t::NumberHex)
}},{'X','X', { LexNode(LexAction::Promote, token_t::Zero, token_t::NumberHex)
}},{'o','o', { LexNode(LexAction::Promote, token_t::Zero, token_t::NumberOct)
}},{'O','O', { LexNode(LexAction::Promote, token_t::Zero, token_t::NumberOct)
}},{'A','Z', {
	LexNode(LexAction::FixToken, token_t::GreaterUnit, token_t::Ident),
	LexNode(LexAction::FixToken, token_t::LessUnit, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Unit, token_t::Ident),
	LexNode(LexAction::DefaultNewToken, token_t::Ident)
}},{'a','z', {
	LexNode(LexAction::FixToken, token_t::GreaterUnit, token_t::Ident),
	LexNode(LexAction::FixToken, token_t::LessUnit, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Unit, token_t::Ident),
	LexNode(LexAction::DefaultNewToken, token_t::Ident)
}},{'0','0', {
	LexNode(LexAction::Promote, token_t::Zero, token_t::Number),
	LexNode(LexAction::Promote, token_t::Unit,token_t::Ident),
	LexNode(LexAction::Promote, token_t::Ident, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Dot, token_t::NumberReal),
	LexNode(LexAction::FixToken, token_t::GreaterDot, token_t::NumberReal),
	LexNode(LexAction::FixToken, token_t::GreaterUnit, token_t::Ident),
	LexNode(LexAction::FixToken, token_t::LessDot, token_t::NumberReal),
	LexNode(LexAction::FixToken, token_t::LessUnit, token_t::Ident),
	LexNode(LexAction::FixToken, token_t::XorDot, token_t::NumberReal),
	LexNode(LexAction::Remain, token_t::Number),
	LexNode(LexAction::Remain, token_t::NumberReal),
	LexNode(LexAction::Remain, token_t::NumberBin),
	LexNode(LexAction::Remain, token_t::NumberHex),
	LexNode(LexAction::Remain, token_t::NumberOct),
	LexNode(LexAction::DefaultNewToken, token_t::Zero)
}},{'1','1', { LexNode(LexAction::Remain, token_t::NumberBin)
}},{'1','7', { LexNode(LexAction::Remain, token_t::NumberOct)
}},{'1','9', {
	LexNode(LexAction::Promote, token_t::Unit, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Ident, token_t::Ident),
	LexNode(LexAction::Promote, token_t::Dot, token_t::NumberReal),
	LexNode(LexAction::Promote, token_t::Zero, token_t::Number),
	LexNode(LexAction::FixToken, token_t::GreaterDot, token_t::NumberReal),
	LexNode(LexAction::FixToken, token_t::GreaterUnit, token_t::Ident),
	LexNode(LexAction::FixToken, token_t::LessDot, token_t::NumberReal),
	LexNode(LexAction::FixToken, token_t::LessUnit, token_t::Ident),
	LexNode(LexAction::FixToken, token_t::XorDot, token_t::NumberReal),
	LexNode(LexAction::NewToken, token_t::NumberBin, token_t::Invalid),
	LexNode(LexAction::NewToken, token_t::NumberOct, token_t::Invalid),
	LexNode(LexAction::Remain, token_t::NumberHex),
	LexNode(LexAction::DefaultNewToken, token_t::Number)
}},{'/','/', {
	LexNode(LexAction::Promote, token_t::Dot, token_t::DotOper),
	LexNode(LexAction::Promote, token_t::Slash, token_t::White, LexState::Comment),
	LexNode(LexAction::DefaultNewToken, token_t::Slash)
}},{'*','*', {
	LexNode(LexAction::Promote, token_t::DotDot, token_t::DotOper),
	LexNode(LexAction::Promote, token_t::Dot, token_t::Oper),
	LexNode(LexAction::Promote, token_t::Slash, token_t::White, LexState::Comment),
	LexNode(LexAction::DefaultNewToken, token_t::Oper)
}},{'.','.', {
	LexNode(LexAction::Promote, token_t::Less, token_t::LessDot),
	LexNode(LexAction::Promote, token_t::Greater, token_t::GreaterDot),
	LexNode(LexAction::Promote, token_t::Xor, token_t::XorDot),
	LexNode(LexAction::Promote, token_t::DotDot, token_t::DotDotDot),
	LexNode(LexAction::Promote, token_t::Dot, token_t::DotDot),
	LexNode(LexAction::DefaultNewToken, token_t::Dot)
}},{';',';', {
	LexNode(LexAction::DefaultNewToken, token_t::Semi),
}},{',',',', {
	LexNode(LexAction::DefaultNewToken, token_t::Comma),
}},{'=','=', {
	LexNode(LexAction::Promote, token_t::DotDot, token_t::RangeIncl),
	LexNode(LexAction::Promote, token_t::DotOper, token_t::DotOperEqual),
	LexNode(LexAction::Promote, token_t::DotOperPlus, token_t::DotDotPlusEqual),
	LexNode(LexAction::Promote, token_t::Less, token_t::LessEqual),
	LexNode(LexAction::Promote, token_t::Greater, token_t::GreaterEqual),
	LexNode(LexAction::Promote, token_t::Xor, token_t::O_XorEq),
	LexNode(LexAction::Promote, token_t::Minus, token_t::O_SubEq),
	LexNode(LexAction::Promote, token_t::Slash, token_t::OperEqual),
	LexNode(LexAction::Promote, token_t::Oper, token_t::OperEqual),
	LexNode(LexAction::DefaultNewToken, token_t::Equal)
}},{'>','>', {
	LexNode(LexAction::Promote, token_t::GreaterDot, token_t::O_RightRot),
	LexNode(LexAction::Promote, token_t::GreaterUnit, token_t::O_RightRotIn),
	LexNode(LexAction::Promote, token_t::LessEqual, token_t::O_Spaceship),
	LexNode(LexAction::Promote, token_t::Equal, token_t::Func),
	LexNode(LexAction::Promote, token_t::Minus, token_t::Arrow),
	LexNode(LexAction::DefaultNewToken, token_t::Greater)
}},{'<','<', {
	LexNode(LexAction::Promote, token_t::LessDot, token_t::O_LeftRot),
	LexNode(LexAction::Promote, token_t::LessUnit, token_t::O_LeftRotIn),
	LexNode(LexAction::DefaultNewToken, token_t::Less)
}},{'+','+', {
	LexNode(LexAction::Promote, token_t::DotDot, token_t::DotOperPlus),
	LexNode(LexAction::Promote, token_t::Dot, token_t::Oper),
	LexNode(LexAction::DefaultNewToken, token_t::Oper)
}},{'-','-', {
	LexNode(LexAction::Promote, token_t::Dot, token_t::DotOper),
	LexNode(LexAction::DefaultNewToken, token_t::Minus)
}},{'&','&', { LexNode(LexAction::DefaultNewToken, token_t::Oper)
}},{'|','|', { LexNode(LexAction::DefaultNewToken, token_t::Oper)
}},{'^','^', {
	LexNode(LexAction::Promote, token_t::XorDot, token_t::O_Compose),
	LexNode(LexAction::DefaultNewToken, token_t::Xor)
}},{'%','%', {
	LexNode(LexAction::Remain, token_t::DotOper),
	LexNode(LexAction::Promote, token_t::Slash, token_t::Oper),
	LexNode(LexAction::DefaultNewToken, token_t::Oper)
}},{'!','!', { LexNode(LexAction::DefaultNewToken, token_t::Oper)
}},{'#','#', { LexNode(LexAction::DefaultNewToken, token_t::Hash)
}},{'(','(', {
	LexNode(LexAction::Promote, token_t::Dot, token_t::DotBlock),
	LexNode(LexAction::DefaultNewToken, token_t::Block)
}},{'[','[', {
	LexNode(LexAction::Promote, token_t::Dot, token_t::DotArray),
	LexNode(LexAction::DefaultNewToken, token_t::Array)
}},{'{','{', {
	LexNode(LexAction::Promote, token_t::Dot, token_t::DotObject),
	LexNode(LexAction::DefaultNewToken, token_t::Object)
}},{')',')', { LexNode(LexAction::DefaultNewToken, token_t::EndDelim)
}},{']',']', { LexNode(LexAction::DefaultNewToken, token_t::EndDelim)
}},{'}','}', { LexNode(LexAction::DefaultNewToken, token_t::EndDelim)
}},{'"','"', { LexNode(LexAction::DefaultNewToken, token_t::String, token_t::String, LexState::DString)
}},{'\'','\'', { LexNode(LexAction::DefaultNewToken, token_t::String, token_t::String, LexState::SString)
}},{' ',' ', { LexNode(LexAction::DefaultNewToken, token_t::White)
}},{'\t','\t', { LexNode(LexAction::DefaultNewToken, token_t::White)
}},{'\r','\r', { LexNode(LexAction::DefaultNewToken, token_t::White)
}},{'\n','\n', {
	LexNode(LexAction::Promote, token_t::White, token_t::Newline),
	LexNode(LexAction::DefaultNewToken, token_t::Newline)
}}
};
struct ParserKeyword {
	std::string_view word;
	token_t token;
};
const ParserKeyword keyword_table[] = {
	{"if"sv, token_t::K_If},
	{"else"sv, token_t::K_Else},
	{"elif"sv, token_t::K_ElseIf},
	{"elsif"sv, token_t::K_ElseIf},
	{"elseif"sv, token_t::K_ElseIf},
	{"loop"sv, token_t::K_Loop},
	{"while"sv, token_t::K_While},
	{"until"sv, token_t::K_Until},
	{"continue"sv, token_t::K_Continue},
	{"end"sv, token_t::K_End},
	{"break"sv, token_t::K_Break},
	{"for"sv, token_t::K_For},
	{"in"sv, token_t::K_In},
	{"goto"sv, token_t::K_Goto},
	{"let"sv, token_t::K_Let},
	{"mut"sv, token_t::K_Mut},
	{"true"sv, token_t::K_True},
	{"false"sv, token_t::K_False},
	{"import"sv, token_t::K_Import},
	{"export"sv, token_t::K_Export},
	{"#"sv ,token_t::O_Length},
	{"!"sv ,token_t::O_Not},
	{"&"sv ,token_t::O_And},
	{"|"sv ,token_t::O_Or},
	{"^"sv ,token_t::O_Xor},
	{"+"sv ,token_t::O_Add},
	{"-"sv ,token_t::O_Sub},
	{"*"sv ,token_t::O_Mul},
	{"**"sv ,token_t::O_Power},
	{"/"sv ,token_t::O_Div},
	{"%"sv ,token_t::O_Mod},
	{".*"sv ,token_t::O_DotP},
	{".+"sv ,token_t::O_Cross},
	{">>"sv ,token_t::O_Rsh},
	{">>>"sv ,token_t::O_RAsh},
	{"<<"sv ,token_t::O_Lsh},
	{"&="sv ,token_t::O_AndEq},
	{"|="sv ,token_t::O_OrEq},
	{"^="sv ,token_t::O_XorEq},
	{"+="sv ,token_t::O_AddEq},
	{"-="sv ,token_t::O_SubEq},
	{"*="sv ,token_t::O_MulEq},
	{"**="sv ,token_t::O_PowerEq},
	{"/="sv ,token_t::O_DivEq},
	{"%="sv ,token_t::O_ModEq},
	{".+="sv ,token_t::O_CrossEq},
	{">>="sv ,token_t::O_RshEq},
	{">>>="sv ,token_t::O_RAshEq},
	{"<<="sv ,token_t::O_LshEq},
	{"/%"sv ,token_t::O_DivMod},
	{"-"sv ,token_t::O_Minus},
	{"<"sv ,token_t::O_Less},
	{"="sv ,token_t::O_Assign},
	{">"sv ,token_t::O_Greater},
	{"<="sv ,token_t::O_LessEq},
	{">="sv ,token_t::O_GreaterEq},
	{"=="sv ,token_t::O_EqEq},
	{"!="sv ,token_t::O_NotEq},
	{".."sv ,token_t::O_Range},
	{"."sv ,token_t::O_Dot},
	{"..="sv ,token_t::RangeIncl},
	{"./%"sv ,token_t::Ident},
};
struct Precedence {
	uint8_t prec;
	token_t token;
};
Precedence p_table[] = {
	{0, token_t::O_Assign},

	{1, token_t::O_Less},
	{1, token_t::O_Greater},
	{1, token_t::O_LessEq},
	{1, token_t::O_GreaterEq},
	{1, token_t::O_EqEq},
	{1, token_t::O_NotEq},

	{2, token_t::O_AndEq},
	{2, token_t::O_OrEq},
	{2, token_t::O_XorEq},
	{2, token_t::O_AddEq},
	{2, token_t::O_SubEq},
	{2, token_t::O_MulEq},
	{2, token_t::O_PowerEq},
	{2, token_t::O_DivEq},
	{2, token_t::O_ModEq},
	{2, token_t::O_CrossEq},
	{2, token_t::O_RshEq},
	{2, token_t::O_RAshEq},
	{2, token_t::O_LshEq},

	{3, token_t::O_And},
	{3, token_t::O_Or},
	{3, token_t::O_Xor},
	{3, token_t::O_Add},
	{3, token_t::O_Sub},

	{4, token_t::O_Rsh},
	{4, token_t::O_RAsh},
	{4, token_t::O_Lsh},

	{5, token_t::O_Mul},

	{6, token_t::O_Div},
	{6, token_t::O_Mod},
	{6, token_t::O_DivMod},

	{7, token_t::O_Power},
	{7, token_t::O_DotP},
	{7, token_t::O_Cross},

	{8, token_t::O_Range},

	{9, token_t::O_Minus},
	{9, token_t::O_Not},
	{9, token_t::O_Length},

	{10, token_t::O_RefExpr},
	{11, token_t::O_Dot},
};
constexpr auto p_table_span = std::span{p_table};
static uint8_t get_precedence(token_t token) {
	auto found = std::ranges::find(p_table_span, token, &Precedence::token);
	if(found != p_table_span.end()) {
		return found->prec;
	}
	return 0;
}
constexpr auto lex_tree_span = std::span{lex_tree};
constexpr auto keyword_table_span = std::span{keyword_table};
using source_itr = string::const_iterator;
struct LexTokenRange {
	source_itr tk_begin;
	source_itr tk_end;
	token_t token;
	std::string_view as_string() const {
		return std::string_view(this->tk_begin, this->tk_end);
	}
	std::string_view as_string(size_t offset) const {
		return std::string_view(this->tk_begin + offset, this->tk_end);
	}
};
#define DEF_EXPRESSIONS(f) \
	f(Delimiter) \
	f(CommaList) \
	f(BlockExpr) \
	f(FuncDecl) \
	f(FuncExpr) \
	f(Prefix) \
	f(RawOper) \
	f(OperExpr) \
	f(OperOpen) \
	f(KeywExpr) \
	f(Start) \
	f(End)
enum class Expr {
#define GENERATE(f) f,
	DEF_EXPRESSIONS(GENERATE)
#undef GENERATE
};
static std::string_view expr_type_name(Expr e) {
	switch(e) {
#define GENERATE(f) case Expr::f: return #f##sv;
	DEF_EXPRESSIONS(GENERATE)
#undef GENERATE
	default: return "INVALID"sv;
	}
}
struct ASTNode;
typedef std::unique_ptr<ASTNode> node_ptr;
typedef std::vector<node_ptr> expr_list_t;
struct ASTNode {
	LexTokenRange block;
	token_t ast_token;
	Expr asc;
	uint8_t meta_value;
	bool newline;
	bool whitespace;
	//std::shared_ptr<string> source;
	ASTNode() = delete;
	ASTNode(token_t token, Expr a, LexTokenRange lex_range)
		: ast_token{token}, asc{a}, block{lex_range}, newline{false}, whitespace{false}, meta_value{get_precedence(token)} {}
	ASTNode(token_t token, Expr a, uint8_t e, LexTokenRange lex_range)
		: ast_token{token}, asc{a}, block{lex_range}, newline{false}, whitespace{false}, meta_value{e} {}
	expr_list_t expressions;
};
struct ExprStackItem {
	ASTNode *block;
	expr_list_t &expr_list;
};
std::ostream &operator<<(std::ostream &os, token_t tk) {
	return os << token_name(tk);
}
std::ostream &operator<<(std::ostream &os, Expr tk) {
	return os << expr_type_name(tk);
}
static void show_node(std::ostream &os, const ASTNode &node, int depth) {
	os << "("
	<< token_name(node.ast_token)
	<< " " << expr_type_name(node.asc);
	if(node.newline) os << " NL";
	else if(node.whitespace) os << " SP";
	if(node.asc == Expr::RawOper || node.asc == Expr::OperExpr || node.asc == Expr::OperOpen)
		os << " " << static_cast<uint32_t>(node.meta_value);
	if(node.asc == Expr::KeywExpr || node.asc == Expr::BlockExpr)
		os << " " << static_cast<uint32_t>(node.meta_value) << "," << node.expressions.size();
	if(node.asc == Expr::Start) {
		os << '\"' << std::string_view(node.block.tk_begin, node.block.tk_end) << '\"';
	}
	bool first = true;
	for(auto &expr : node.expressions) {
		if(first) {first = false; os << "\n"; }
		else os << ",\n";
		for(int i = 0; i <= depth; i++) os << "  ";
		if(expr) show_node(os, *expr, depth + 1);
		else os << "nullnode";
	}
	if(node.expressions.size() > 0) {
		os << "\n";
		for(int i = 1; i <= depth; i++) os << "  ";
	}
	os << ")";
}
std::ostream &operator<<(std::ostream &os, const std::unique_ptr<ASTNode> &node) {
	if(node) show_node(os, *node, 0);
	else os << "nullnode";
	return os;
}
std::ostream &operator<<(std::ostream &os, const ASTNode &node) {
	show_node(os, node, 0);
	return os;
}
#define PUSH_INTO(d, s) (d)->expressions.emplace_back(std::move(s));
#define EXprev 2
#define REMOV_EX(n) expr_list.erase(expr_list.cend() - n)
#define REMOV_EXprev expr_list.erase(expr_list.cend() - 2)
#define REMOV_EXhead expr_list.pop_back()
#define REMOVE_EXPR(ex) ex
#define REMOVE1(ex) REMOVE_EXPR(REMOV_EX##ex)
#define CONV_N(n, t, num) n->asc = Expr::t; n->meta_value = num
#define EXTEND_TO(t, sl, sr) t->block.tk_begin = sl->block.tk_begin; \
	t->block.tk_end = sr->block.tk_end;
#define AT_EXPR_TARGET(id) (id->expressions.size() >= id->meta_value)
#define IS_TOKEN(n, t) ((n)->ast_token == token_t::t)
#define IS_CLASS(n, c) ((n)->asc == Expr::c)
#define IS2C(c1, c2) ((prev->asc == Expr::c1) && (head->asc == Expr::c2))
#define IS3C(c1, c2, c3) (third && (third->asc == Expr::c1) && (prev->asc == Expr::c2) && (head->asc == Expr::c3))
#define MAKE_NODE_FROM1(n, t, cl, s) n = std::make_unique<ASTNode>( \
token_t::t, Expr::cl, LexTokenRange{s->block.tk_begin, s->block.tk_end, token_t::t});
#define MAKE_NODE_FROM2(n, t, cl, s1, s2) n = std::make_unique<ASTNode>( \
token_t::t, Expr::cl, LexTokenRange{s1->block.tk_begin, s2->block.tk_end, token_t::t});
#define MAKE_NODE_N_FROM1(n, t, cl, num, s) n = std::make_unique<ASTNode>( \
token_t::t, Expr::cl, num, LexTokenRange{s->block.tk_begin, s->block.tk_end, token_t::t});

#define DEF_VARIABLE_TYPES(f) \
	f(Unset) f(Unit) f(Integer) f(Bool) f(Function)
enum class VarType {
#define GENERATE(f) f,
	DEF_VARIABLE_TYPES(GENERATE)
#undef GENERATE
};
std::string_view const variable_type_names[] = {
#define GENERATE(f) (#f##sv),
	DEF_VARIABLE_TYPES(GENERATE)
#undef GENERATE
};
auto const variable_type_names_span = std::span(variable_type_names);
struct Variable {
	uint64_t value;
	VarType vtype;
	Variable() : value{0}, vtype{VarType::Unset} {}
	Variable(uint64_t v) : value{v}, vtype{VarType::Integer} {}
	Variable(int64_t v) : value{static_cast<uint64_t>(v)}, vtype{VarType::Integer} {}
	Variable(uint64_t v, VarType t) : value{v}, vtype{t} {}
};
std::ostream &operator<<(std::ostream &os, const Variable &v) {
	size_t vtype = static_cast<size_t>(v.vtype);
	if(vtype >= variable_type_names_span.size()) vtype = 0;
	os << "Var{"
		<< variable_type_names[vtype]
		<< ":" << static_cast<int64_t>(v.value) << "}";
	return os;
}

#define DEF_OPCODES(f) \
	f(LoadConst) f(LoadBool) f(LoadVariable) f(LoadClosed) f(LoadString) \
	f(LoadClosure) f(AssignLocal) f(AssignClosed) f(AssignNamed) f(LoadUnit) \
	f(PushRegister) f(PopRegister) f(LoadStack) f(StoreStack) f(PopStack) \
	f(NamedLookup) f(NamedArgLookup) \
	f(CallExpression) f(ExitScope) f(Jump) f(JumpIf) \
	f(AddInteger) f(SubInteger) f(MulInteger) f(DivInteger) f(ModInteger) f(PowInteger) \
	f(Negate) f(Not) \
	f(AndInteger) f(OrInteger) f(XorInteger) \
	f(LSHInteger) f(RSHLInteger) f(RSHAInteger) \
	f(CompareLess) f(CompareGreater) f(CompareEqual)
enum class Opcode {
#define GENERATE(f) f,
	DEF_OPCODES(GENERATE)
#undef GENERATE
};
std::string_view const opcode_names[] = {
#define GENERATE(f) (#f##sv),
	DEF_OPCODES(GENERATE)
#undef GENERATE
};
auto const opcode_names_span = std::span(opcode_names);
std::ostream &operator<<(std::ostream &os, const Opcode &v) {
	if(static_cast<size_t>(v) < opcode_names_span.size())
		os << opcode_names[static_cast<size_t>(v)];
	else os << "Opcode::???"sv;
	return os;
}

struct Instruction {
	Opcode opcode;
	union {
		uint64_t param;
		struct {
			uint32_t param_a;
			uint32_t param_b;
		};
	};
	Instruction(Opcode o) : opcode{o}, param{0} {}
	Instruction(Opcode o, uint64_t p) : opcode{o}, param{p} {}
	Instruction(Opcode o, uint32_t a, uint32_t b) : opcode{o}, param_a{a}, param_b{b} {}
};

std::ostream &operator<<(std::ostream &os, const Instruction &v) {
	os << "I(" << v.opcode << " " << v.param << ")";
	return os;
}

struct FrameContext;

struct VariableDeclaration {
	size_t name_index;
	size_t stack_pos;
};
std::ostream &operator<<(std::ostream &os, const VariableDeclaration &v) {
	os << "Decl{" << v.name_index << "}";
	return os;
}

struct FrameContext {
	std::shared_ptr<FrameContext> up;
	size_t frame_index;
	std::vector<VariableDeclaration> var_declarations;
	size_t current_depth;
	std::vector<size_t> scope_depths;
	std::vector<Instruction> instructions;
	FrameContext(size_t scope_id) : frame_index{scope_id}, current_depth{0} {}
	FrameContext(size_t scope_id, std::shared_ptr<FrameContext> &up_ptr)
		: frame_index{scope_id}, up{up_ptr}, current_depth{0} {}
};
struct ModuleContext {
	std::string file_source;
	std::vector<size_t> line_positions;
	std::vector<std::string_view> string_table;
	std::shared_ptr<FrameContext> root_context;
	std::vector<std::shared_ptr<FrameContext>> frames;

	size_t find_or_put_string(std::string_view s) {
		auto found = std::ranges::find(this->string_table, s);
		if(found != this->string_table.cend()) {
			return found - this->string_table.cbegin();
		}
		this->string_table.push_back(s);
		return this->string_table.size() - 1; // newly inserted string
	}
};

bool convert_number(std::string_view raw_number, uint64_t &output) {
	output = 0;
	for(auto c : raw_number) {
		if(c == '_') continue;
		output *= 10;
		output += c - '0';
	}
	return true;
}
bool convert_number_hex(std::string_view raw_number, uint64_t &output) {
	output = 0;
	for(auto c : std::span(raw_number.cbegin() + 2, raw_number.cend())) {
		if(c == '_') continue;
		output <<= 4;
		if(c > '9') output |= c - 'A' + 10;
		else output |= c - '0';
	}
	return true;
}
bool convert_number_oct(std::string_view raw_number, uint64_t &output) {
	output = 0;
	for(auto c : std::span(raw_number.cbegin() + 2, raw_number.cend())) {
		if(c == '_') continue;
		output <<= 3;
		output |= c - '0';
	}
	return true;
}
bool convert_number_bin(std::string_view raw_number, uint64_t &output) {
	output = 0;
	for(auto c : std::span(raw_number.cbegin() + 2, raw_number.cend())) {
		if(c == '_') continue;
		output <<= 1;
		output |= c & 1;
	}
	return true;
}

bool walk_expression(std::ostream &dbg, ModuleContext *module_ctx, std::shared_ptr<FrameContext> &ctx, const node_ptr &expr) {
	auto show_position = [&](const LexTokenRange &block) {
		auto file_start = module_ctx->file_source.cbegin();
		size_t offset = block.tk_begin - file_start;
		auto found = std::ranges::lower_bound(module_ctx->line_positions, offset);
		if(found == module_ctx->line_positions.cend()) {
			dbg << "char:" << offset;
			return;
		}
		auto lines_start = module_ctx->line_positions.cbegin();
		size_t prev_pos = 0;
		if(found != lines_start) prev_pos = *(found - 1);
		size_t line = (found - lines_start) + 1;
		dbg << line << ":" << (offset - prev_pos) << ": ";
	};
	auto show_syn_error = [&](const std::string_view what, const node_ptr &node) {
		dbg << '\n' << what << " syntax error: ";
		show_position(node->block);
		dbg << node->block.as_string() << "\n";
	};
	dbg << "Expr:" << expr->ast_token << " ";
	switch(expr->asc) {
	case Expr::BlockExpr: {
		size_t current_depth = ctx->current_depth;
		ctx->scope_depths.push_back(current_depth);
		for(auto &walk_node : expr->expressions) {
			if(!walk_expression(dbg, module_ctx, ctx, walk_node))
				return false;
		}
		if(current_depth != ctx->current_depth)
			ctx->instructions.emplace_back(Instruction{Opcode::ExitScope, current_depth});
		//for(auto &ins : block_context->instructions) dbg << ins << '\n';
		dbg << "block end\n";
		break;
	}
	case Expr::KeywExpr:
		if(!AT_EXPR_TARGET(expr) && expr->expressions.size() >= 2) {
			show_syn_error("Keyword expression", expr);
			return false;
		}
		switch(expr->ast_token) {
		case token_t::K_Let: {
			if(expr->expressions.empty()) {
				show_syn_error("Let", expr);
				return false;
			}
			auto &item = expr->expressions.front();
			if(IS_TOKEN(item, O_Assign)) {
				if(!IS_TOKEN(item->expressions.front(), Ident)) {
					show_syn_error("Let assignment expression", item);
					return false;
				}
				size_t string_index = module_ctx->find_or_put_string(
					item->expressions.front()->block.as_string() );
				if(!walk_expression(dbg, module_ctx, ctx, item->expressions[1])) return false;
				size_t var_index = ctx->current_depth++;
				size_t decl_index = ctx->var_declarations.size();
				ctx->var_declarations.emplace_back(VariableDeclaration{string_index, var_index});
				dbg << "let decl " << ctx->var_declarations.back() << " " << module_ctx->string_table[string_index] << " = \n";
				// assign the value
				ctx->instructions.emplace_back(Instruction{Opcode::AssignLocal, decl_index});
				return true;
			} else if(IS_TOKEN(item, Ident)) {
				size_t string_index = module_ctx->find_or_put_string(item->block.as_string());
				dbg << "TODO let decl " << "\n";
				return true;
			} else {
				show_syn_error("Let expression", item);
			}
			return false;
		}
		case token_t::K_If: {
			auto walk_expr = expr->expressions.cbegin();
			if(!walk_expression(dbg, module_ctx, ctx, *walk_expr)) return false;
			walk_expr++;
			ctx->instructions.emplace_back(Instruction{Opcode::Not, 0});
			size_t skip_pos = ctx->instructions.size();
			ctx->instructions.emplace_back(Instruction{Opcode::JumpIf, 0});
			if(!walk_expression(dbg, module_ctx, ctx, *walk_expr)) return false;
			walk_expr++;
			std::vector<size_t> exit_positions;
			exit_positions.push_back(ctx->instructions.size());
			ctx->instructions.emplace_back(Instruction{Opcode::Jump, 0});
			if(walk_expr == expr->expressions.cend()) { // no else or elseif
				ctx->instructions[skip_pos].param = ctx->instructions.size();
				ctx->instructions.emplace_back(Instruction{Opcode::LoadConst, 0});
				// TODO the unit type
			} else while(walk_expr != expr->expressions.cend()) {
				if(IS_TOKEN((*walk_expr), K_ElseIf)) {
					if((*walk_expr)->expressions.size() != 2) {
						show_syn_error("ElseIf", *walk_expr);
						return false;
					}
					// fixup the previous test's jump
					ctx->instructions[skip_pos].param = ctx->instructions.size();
					// test expression
					if(!walk_expression(dbg, module_ctx, ctx, (*walk_expr)->expressions[0])) return false;
					ctx->instructions.emplace_back(Instruction{Opcode::Not, 0});
					skip_pos = ctx->instructions.size();
					ctx->instructions.emplace_back(Instruction{Opcode::JumpIf, 0});
					if(!walk_expression(dbg, module_ctx, ctx, (*walk_expr)->expressions[1])) return false;
					exit_positions.push_back(ctx->instructions.size());
					ctx->instructions.emplace_back(Instruction{Opcode::Jump, 0});
				} else if(IS_TOKEN((*walk_expr), K_Else)) {
					if((*walk_expr)->expressions.size() != 1) {
						show_syn_error("Else", *walk_expr);
						return false;
					}
					ctx->instructions[skip_pos].param = ctx->instructions.size();
					if(!walk_expression(dbg, module_ctx, ctx, (*walk_expr)->expressions[0])) return false;
					break;
				} else {
					show_syn_error("If-Else", *walk_expr);
					return false;
				}
				walk_expr++;
			}
			// fixup the exit points
			for(auto pos : exit_positions) {
				ctx->instructions[pos].param = ctx->instructions.size();
			}
			return true;
		}
		case token_t::K_Until:
		case token_t::K_While: {
			if(expr->expressions.size() != 2) {
				if(IS_TOKEN(expr, K_Until))
					show_syn_error("Until expression", expr);
				else show_syn_error("While expression", expr);
				return false;
			}
			size_t current_depth = ctx->var_declarations.size();
			auto walk_expr = expr->expressions.cbegin();
			if(!walk_expression(dbg, module_ctx, ctx, *walk_expr)) return false;
			walk_expr++;
			if(IS_TOKEN(expr, K_Until))
				ctx->instructions.emplace_back(Instruction{Opcode::Not, 0});
			ctx->instructions.emplace_back(Instruction{Opcode::JumpIf, 0});
			size_t jump_ins = ctx->instructions.size() - 1;
			if(!walk_expression(dbg, module_ctx, ctx, *walk_expr)) return false;
			ctx->instructions.emplace_back(Instruction{Opcode::Jump, 0});
			ctx->instructions[jump_ins].param = ctx->instructions.size();
			if(current_depth != ctx->var_declarations.size())
				ctx->instructions.emplace_back(Instruction{Opcode::ExitScope, current_depth});
			// for(auto &ins : ctx->instructions) dbg << ins << '\n';
			return true;
		}
		default:
			dbg << "unhandled keyword\n";
			return false;
		}
		return true;
	case Expr::FuncDecl:
	case Expr::RawOper:
	case Expr::OperOpen:
		show_syn_error("Expression", expr);
		return false;
	case Expr::OperExpr:
		if(IS_TOKEN(expr, O_RefExpr)) { // function calls
			if(expr->expressions.size() != 2) {
				show_syn_error("Function call", expr);
				return false;
			}
			dbg << "reference: ";
			if(!walk_expression(dbg, module_ctx, ctx, expr->expressions[0])) return false;
			dbg << "\ncall with: ";
			// arguments
			auto &args = expr->expressions[1];
			bool func_save = false;
			if(IS_TOKEN(args, Block)) {
				if(!args->expressions.empty()) {
					ctx->instructions.emplace_back(Instruction{Opcode::PushRegister, 0});
					func_save = true;
				}
				for(auto &arg : args->expressions) {
					if(!walk_expression(dbg, module_ctx, ctx, arg)) return false;
					ctx->instructions.emplace_back(Instruction{Opcode::PushRegister, 0});
				}
				if(!args->expressions.empty())
					ctx->instructions.emplace_back(Instruction{Opcode::LoadStack, args->expressions.size()});
			} else {
				dbg << "unhandled function call\n";
				return false;
			}
			ctx->instructions.emplace_back(Instruction{Opcode::CallExpression, args->expressions.size()});
			if(func_save)
				ctx->instructions.emplace_back(Instruction{Opcode::PopStack, 0});
			dbg << "\n";
			return true;
		}
		dbg << "operator: " << expr->ast_token;
		if(expr->expressions.size() >= 1) {
			if(IS_TOKEN(expr, O_Dot) && expr->expressions.size() == 1) {
				if(!IS_TOKEN(expr->expressions[0], Ident)) {
					show_syn_error("Dot operator", expr);
					return false;
				}
				size_t string_index = module_ctx->find_or_put_string(
					expr->expressions[0]->block.as_string() );
				ctx->instructions.emplace_back(Instruction{Opcode::NamedArgLookup, string_index});
				return true;
			}
			dbg << "\nexpr L: ";
			if(!walk_expression(dbg, module_ctx, ctx, expr->expressions[0])) return false;
		}
		if(IS_TOKEN(expr, O_Dot)) {
			if(expr->expressions.size() < 2 || !IS_TOKEN(expr->expressions[1], Ident)) {
				show_syn_error("Dot operator", expr);
				return false;
			}
			size_t string_index = module_ctx->find_or_put_string(
				expr->expressions[1]->block.as_string() );
			ctx->instructions.emplace_back(Instruction{Opcode::NamedLookup, string_index});
			return true;
		}
		if(expr->expressions.size() >= 2) {
			dbg << "\nexpr R: ";
			ctx->instructions.emplace_back(Instruction{Opcode::PushRegister, 0});
			if(!walk_expression(dbg, module_ctx, ctx, expr->expressions[1])) return false;
		}
		switch(expr->ast_token) {
		case token_t::O_Add:
			ctx->instructions.emplace_back(Instruction{Opcode::AddInteger, 0});
			return true;
		case token_t::O_Sub:
			ctx->instructions.emplace_back(Instruction{Opcode::SubInteger, 0});
			return true;
		case token_t::O_Mul:
			ctx->instructions.emplace_back(Instruction{Opcode::MulInteger, 0});
			return true;
		case token_t::O_Div:
			ctx->instructions.emplace_back(Instruction{Opcode::DivInteger, 0});
			return true;
		case token_t::O_Mod:
			ctx->instructions.emplace_back(Instruction{Opcode::ModInteger, 0});
			return true;
		case token_t::O_Power:
			ctx->instructions.emplace_back(Instruction{Opcode::PowInteger, 0});
			return true;
		case token_t::O_And:
			ctx->instructions.emplace_back(Instruction{Opcode::AndInteger, 0});
			return true;
		case token_t::O_Or:
			ctx->instructions.emplace_back(Instruction{Opcode::OrInteger, 0});
			return true;
		case token_t::O_Xor:
			ctx->instructions.emplace_back(Instruction{Opcode::XorInteger, 0});
			return true;
		case token_t::O_RAsh:
			ctx->instructions.emplace_back(Instruction{Opcode::RSHAInteger, 0});
			return true;
		case token_t::O_Rsh:
			ctx->instructions.emplace_back(Instruction{Opcode::RSHLInteger, 0});
			return true;
		case token_t::O_Lsh:
			ctx->instructions.emplace_back(Instruction{Opcode::LSHInteger, 0});
			return true;
		case token_t::O_Minus:
			ctx->instructions.emplace_back(Instruction{Opcode::Negate, 0});
			return true;
		case token_t::O_Not:
			ctx->instructions.emplace_back(Instruction{Opcode::Not, 0});
			return true;
		case token_t::O_Less:
			ctx->instructions.emplace_back(Instruction{Opcode::CompareLess, 0});
			return true;
		case token_t::O_Greater:
			ctx->instructions.emplace_back(Instruction{Opcode::CompareGreater, 0});
			return true;
		case token_t::O_EqEq:
			ctx->instructions.emplace_back(Instruction{Opcode::CompareEqual, 0});
			return true;
		default:
			dbg << "unhandled operator: " << expr->ast_token << "\n";
		}
		return false;
	case Expr::Start:
		switch(expr->ast_token) {
		case token_t::Zero: 
			ctx->instructions.emplace_back(Instruction{Opcode::LoadConst, 0});
			return true;
		case token_t::Number: {
			uint64_t value = 0;
			if(!convert_number(expr->block.as_string(), value)) {
				dbg << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			dbg << "number value: " << value << "\n";
			ctx->instructions.emplace_back(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case token_t::NumberHex: {
			uint64_t value = 0;
			if(!convert_number_hex(expr->block.as_string(), value)) {
				dbg << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			dbg << "number value: " << value << " from " << expr->block.as_string() << "\n";
			ctx->instructions.emplace_back(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case token_t::NumberOct: {
			uint64_t value = 0;
			if(!convert_number_oct(expr->block.as_string(), value)) {
				dbg << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			dbg << "number value: " << value << " from " << expr->block.as_string() << "\n";
			ctx->instructions.emplace_back(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case token_t::NumberBin: {
			uint64_t value = 0;
			if(!convert_number_bin(expr->block.as_string(), value)) {
				dbg << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			dbg << "number value: " << value << " from " << expr->block.as_string() << "\n";
			ctx->instructions.emplace_back(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case token_t::K_True:
			ctx->instructions.emplace_back(Instruction{Opcode::LoadBool, 1});
			return true;
		case token_t::K_False:
			ctx->instructions.emplace_back(Instruction{Opcode::LoadBool, 0});
			return true;
		case token_t::Ident: {
			size_t string_index = module_ctx->find_or_put_string(expr->block.as_string());
			auto search_context = ctx.get();
			uint32_t up_count = 0;
			uint32_t decl_index = 0;
			for(;;) {
				if(search_context == nullptr) {
					dbg << "variable not found: " << module_ctx->string_table[string_index] << "\n";
					return false;
				}
				auto found = std::ranges::find(search_context->var_declarations, string_index, &VariableDeclaration::name_index);
				if(found == search_context->var_declarations.cend()) {
					search_context = search_context->up.get();
					up_count++;
					continue;
				}
				// found variable case
				decl_index = static_cast<uint32_t>(found - search_context->var_declarations.cbegin());
				break;
			}
			ctx->instructions.emplace_back(
				Instruction{Opcode::LoadVariable, up_count, decl_index});
			dbg << "load variable [" << string_index  << "]" << expr->block.as_string() << "->" << up_count << "," << decl_index << "\n";
			return true;
		}
		case token_t::String: {
			auto s = expr->block.as_string(1);
			for(auto c : s) {
				if(c == '\\') {
					dbg << "unhandled string: " << s << "\n";
					return false;
				}
			}
			size_t string_index = module_ctx->find_or_put_string(s);
			ctx->instructions.emplace_back(Instruction{Opcode::LoadString, string_index});
			return true;
		}
		default:
			dbg << "unhandled start token: " << expr->ast_token << "\n";
		}
		return false;
	case Expr::End: {
		ctx->instructions.emplace_back(Instruction{Opcode::LoadUnit, 0});
		return true;
	}
	case Expr::FuncExpr: {
		if(!AT_EXPR_TARGET(expr)) {
			show_syn_error("Function expression", expr);
			return false;
		}
		dbg << "TODO function start " << expr->block.as_string() << '\n';
		auto function_context = std::make_shared<FrameContext>(module_ctx->frames.size(), ctx);
		module_ctx->frames.push_back(function_context);
		ctx->instructions.emplace_back(Instruction{Opcode::LoadClosure, function_context->frame_index});
		// expr->expressions[0] // TODO named argument list for the function declaration
		// expr->expressions[1] // expression or block forming the function body
		if(IS_TOKEN(expr->expressions[1], Block)) {
			for(auto &walk_node : expr->expressions[1]->expressions) {
				if(!walk_expression(dbg, module_ctx, function_context, walk_node))
					return false;
			}
		} else {
			if(!walk_expression(dbg, module_ctx, function_context, expr->expressions[1]))
				return false;
		}
		dbg << "function end\n";
		break;
	}
	default:
		dbg << "Unhandled expression " << expr->ast_token << ":" << expr->asc << ": " << std::string_view(expr->block.tk_begin, expr->block.tk_end);
		return false;
	}
	return true;
};

void ScriptContext::LoadScriptFile(string file_path, string into_name) {
	// load contents of script file at "file_path", put into namespace "into_name"
	auto dbg_file = std::fstream("./debug.txt", std::ios_base::out | std::ios_base::trunc);
	auto &dbg = dbg_file;
	std::shared_ptr<ModuleContext> root_module = std::make_shared<ModuleContext>();
	LoadFileV(file_path, root_module->file_source);
	token_t current_token = token_t::White;
	const auto file_start = root_module->file_source.cbegin();
	source_itr token_start = file_start;
	const auto file_end = root_module->file_source.cend();
	auto cursor = token_start;
	LexState current_state = LexState::Free;

	std::vector<LexTokenRange> token_buffer;
	node_ptr root_node =
		std::make_unique<ASTNode>(token_t::Block, Expr::BlockExpr,
			LexTokenRange{token_start, file_end, token_t::Eof}
		);
	std::vector<std::unique_ptr<ExprStackItem>> block_stack;
	std::unique_ptr<ExprStackItem> current_block =
		std::make_unique<ExprStackItem>(ExprStackItem {
			root_node.get(), root_node->expressions
		});
	bool is_start_expr = false;
	auto push_token = [&](LexTokenRange &lex_range) {
		auto token_string = std::string_view(lex_range.tk_begin, lex_range.tk_end);
		bool show = true;
		auto collapse_expr = [&](bool terminating) {
			auto &expr_list = current_block->expr_list;
			node_ptr expr_hold;
			// try to collapse the expression list
			while(true) {
				if(expr_list.size() < 2) break;
				auto &head = expr_list.back();
				auto &prev = *(expr_list.end() - 2);
				//dbg << "|";
				//dbg << "<|" << prev << "," << head << "|>";
				node_ptr empty;
				auto &third = expr_list.size() >= 3 ? *(expr_list.end() - 3) : empty;
				auto &fourth = expr_list.size() >= 4 ? *(expr_list.end() - 4) : empty;
				if(!head || !prev) return;
				if(IS_CLASS(head, RawOper)
					&& (IS_CLASS(prev, Start) || IS_CLASS(prev, BlockExpr)))
				{
					// before: (value Start) (oper Raw)
					//  after: (oper OperOpen (value Start))
					head->asc = Expr::OperOpen;
					PUSH_INTO(head, prev);
					REMOVE1(prev);
					dbg << "\n<op_to_expr>";
				} else if(terminating &&
					IS_CLASS(prev, FuncExpr) && !AT_EXPR_TARGET(prev)
					&& (
						IS_CLASS(head, BlockExpr)
						|| IS_CLASS(head, OperExpr)
						|| IS_CLASS(head, Start))
					) {
					EXTEND_TO(prev, prev, head);
					prev->newline = head->newline;
					PUSH_INTO(prev, head);
					REMOVE1(head);
					continue;
				} else if(third
					&& IS_TOKEN(third, Ident)
					&& IS_CLASS(head, FuncDecl)
					&& (IS_TOKEN(prev, Ident) || IS_CLASS(prev, CommaList))
				) {
					// check third for another ident or let
					// TODO operator names
					// TODO also need to check for newline
					// named function with one arg or comma arg list
					node_ptr comma_list;
					if(IS_CLASS(prev, CommaList)) {
						dbg << "\n<name func comma args>";
						comma_list = std::move(prev);
					} else {
						dbg << "\n<name func one arg>";
						MAKE_NODE_FROM1(comma_list, Comma, CommaList, prev);
						PUSH_INTO(comma_list, prev);
					}
					auto MAKE_NODE_N_FROM1(let_expr, K_Let, KeywExpr, 1, third);
					auto MAKE_NODE_FROM1(let_assign, O_Assign, OperOpen, third);
					PUSH_INTO(let_assign, third);
					third = std::move(let_expr);
					prev = std::move(let_assign);
					CONV_N(head, FuncExpr, 2);
					PUSH_INTO(head, comma_list);
				} else if(third
					&& (IS_TOKEN(third, K_Let) && AT_EXPR_TARGET(third) && IS_TOKEN(third->expressions.back(), Ident))
					&& IS_CLASS(head, FuncDecl)
					&& (IS_TOKEN(prev, Ident) || IS_CLASS(prev, CommaList))
				) {
					// TODO check let is well formed
					// let function with one argument or comma arg list
					node_ptr comma_list;
					if(IS_CLASS(prev, CommaList)) {
						dbg << "\n<let func comma args>";
						comma_list = std::move(prev);
					} else {
						dbg << "\n<let func one arg>";
						MAKE_NODE_FROM1(comma_list, Comma, CommaList, prev);
						PUSH_INTO(comma_list, prev);
					}
					auto MAKE_NODE_FROM1(let_assign, O_Assign, OperOpen, third);
					PUSH_INTO(let_assign, third->expressions.back());
					third->expressions.pop_back();
					prev = std::move(let_assign);
					CONV_N(head, FuncExpr, 2);
					PUSH_INTO(head, comma_list);
				} else if(
					IS_CLASS(head, FuncDecl)
					&& (IS_TOKEN(prev, Ident) || IS_CLASS(prev, CommaList))
				) {
					auto MAKE_NODE_N_FROM1(let_expr, K_Let, KeywExpr, 1, prev);
					auto MAKE_NODE_FROM1(let_assign, O_Assign, OperOpen, prev);
					PUSH_INTO(let_assign, prev);
					prev = std::move(let_expr);
					auto MAKE_NODE_FROM1(comma_list, Func, CommaList, head);
					CONV_N(head, FuncExpr, 2);
					PUSH_INTO(head, comma_list);
					expr_list.insert(expr_list.cend() - 1, std::move(let_assign));
					// named function with no args or anon with comma arg list
					dbg << "\n<func no args?>";
				} else if(IS_CLASS(head, FuncDecl) && (IS_TOKEN(prev, Ident) || IS_CLASS(prev, CommaList))) {
					dbg << "\n<func no third>";
					CONV_N(head, FuncExpr, 2);
					PUSH_INTO(head, prev);
					REMOVE1(prev);
				} else if(IS2C(BlockExpr, FuncDecl)) {
					// anon function with blocked arg list
					// or named function with blocked arg list?
					dbg << "\n<func block>";
					CONV_N(head, FuncExpr, 2);
					PUSH_INTO(head, prev);
					REMOVE1(prev);
				} else if(terminating &&
					IS_CLASS(prev, KeywExpr)
					&& (
						IS_CLASS(head, Start)
						|| IS_CLASS(head, OperExpr)
						|| IS_CLASS(head, BlockExpr))
				) {
					if(!AT_EXPR_TARGET(prev)) {
						prev->newline = head->newline;
						PUSH_INTO(prev, head);
						REMOVE1(head);
						continue;
					} else {
						dbg << "\n<unhandled KeywExpr *Expr>";
					}
				} else if((
					IS_TOKEN(prev, Ident)
					|| IS_TOKEN(prev, O_RefExpr)
					|| IS_TOKEN(prev, O_Dot)
					|| IS_TOKEN(prev, Block)
					) && !prev->whitespace && !prev->newline && (
						IS_TOKEN(head, Array)
						|| IS_TOKEN(head, Block)
						|| IS_TOKEN(head, Object)
						)
				) {
					auto MAKE_NODE_FROM2(block_expr, O_RefExpr, OperExpr, prev, head);
					PUSH_INTO(block_expr, prev);
					PUSH_INTO(block_expr, head);
					REMOVE1(head);
					expr_list.back() = std::move(block_expr);
					continue;
				} else if(IS_TOKEN(head, DotArray) && IS_CLASS(head, BlockExpr)) {
					head->asc = Expr::Start;
					dbg << "<DotArray>";
					continue;
				} else if(IS_TOKEN(prev, DotArray) && IS_CLASS(prev, BlockExpr)) {
					prev->asc = Expr::Start;
					dbg << "<DotArray>";
					continue;
				} else if(terminating && IS_CLASS(prev, OperOpen) && (
					IS_CLASS(head, Start)
					|| (IS_CLASS(head, FuncExpr) && AT_EXPR_TARGET(head))
				)) {
					// before: (oper OperOpen (value Start)) (value Start)
					//  after: (oper OperExpr (value Start) (value Start))
					dbg << "<OpPut>";
					prev->asc = Expr::OperExpr;
					prev->newline = head->newline;
					PUSH_INTO(prev, head);
					REMOVE1(head);
					continue;
				} else if(IS2C(OperExpr, RawOper)) {
					// before: (oper1 OperExpr (value1 Start) (value2 Start)) (oper2 Raw)
					if(head->meta_value <= prev->meta_value) {
						// new is same precedence: (1+2)+ => ((1+2)+_)
						// new is lower precedence: (1*2)+ => ((1*2)+_)
						//  after: (oper2 OperOpen
						//             oper1 OperExpr (value1 Start) (value2 Start) )
						// put oper1 inside oper2
						//  ex2 push_into ex1
						head->asc = Expr::OperOpen;
						PUSH_INTO(head, prev);
						REMOVE1(prev);
					} else {
						// new is higher precedence: (1+2)* => (1+(2*_)) || (1+_) (2*_)
						//  after: (oper1 OperOpen (value1 Start)) (oper2 OperOpen (value2 Start))
						// convert both operators to open expressions
						// ex2back push_into ex1
						prev->asc = Expr::OperOpen;
						head->asc = Expr::OperOpen;
						// pop value2 from oper1 push into oper2
						PUSH_INTO(head, prev->expressions.back());
						prev->expressions.pop_back();
						// TODO have to decend or collapse the tree at some point
					}
				} else if(IS2C(KeywExpr, KeywExpr)) {
					if(IS_TOKEN(head, K_If)
						&& IS_TOKEN(prev, K_Else)
						&& prev->expressions.empty()
					) {
						head->ast_token = token_t::K_ElseIf;
						EXTEND_TO(head, prev, head);
						REMOVE1(prev);
						continue;
					} else if(
						terminating
						&& IS_TOKEN(prev, K_If)
						&& AT_EXPR_TARGET(prev)
						&& IS_TOKEN(head, K_Else)
						&& AT_EXPR_TARGET(head)
					) {
						PUSH_INTO(prev, head);
						REMOVE1(head);
						continue;
					} else if(
						IS_TOKEN(prev, K_If)
						&& AT_EXPR_TARGET(prev)
						&& (IS_TOKEN(head, K_Else) || IS_TOKEN(head, K_ElseIf))
						&& !AT_EXPR_TARGET(head)
					) {
						if(terminating) {
							dbg << "\n<if/else syntax error>";
						}
					} else if(third) {
						if(
							IS_TOKEN(third, K_If)
							&& IS_TOKEN(prev, K_ElseIf)
							&& AT_EXPR_TARGET(third)
							&& AT_EXPR_TARGET(prev)
						) {
							PUSH_INTO(third, prev);
							REMOVE1(prev);
						} else {
							dbg << "\n<unhandled Key Key\nT:" << third << "\nL:" << prev << "\nR:" << head << ">";
						}
					} else {
						dbg << "\n<unhandled KeywExpr KeywExpr\nL:" << prev << "\nR:" << head << ">";
					}
				} else if(
					(IS_CLASS(prev, Start)
						|| IS_CLASS(prev, BlockExpr)
						|| (IS_CLASS(prev, FuncExpr) && AT_EXPR_TARGET(prev))
						|| IS_CLASS(prev, OperExpr))
					&& (IS_CLASS(head, Start)
						|| IS_CLASS(head, KeywExpr)
						|| IS_CLASS(head, BlockExpr)
						|| IS_CLASS(head, OperExpr))
					) {
					if(third) {
						if(IS_CLASS(third, KeywExpr) && !AT_EXPR_TARGET(third)) {
							// ex2 push_info ex3
							PUSH_INTO(third, prev);
							REMOVE1(prev);
							continue;
						}
					}
					if(!terminating && !expr_hold) {
						//dbg << "\n<collapse_expr before " << head << ">";
						terminating = true;
						expr_hold = std::move(expr_list.back());
						REMOVE1(head);
						continue;
					}
				} else if(!terminating && IS_TOKEN(head, Semi) && !expr_hold) {
					dbg << "\n<collapse_expr semi>";
					terminating = true;
					expr_hold = std::move(expr_list.back());
					REMOVE1(head);
					continue;
				} else if((
						IS_CLASS(head, Start)
						//|| IS_CLASS(head, OperExpr)
					) && IS_CLASS(prev, Prefix)
				) {
					// TODO have to decend or collapse the tree here maybe?
					dbg << "<pfx>";
					// ex1 pushinto ex2
					prev->asc = Expr::OperExpr;
					PUSH_INTO(prev, head);
					REMOVE1(head);
					continue;
				} else if(terminating && IS_CLASS(prev, RawOper) && (
						IS_CLASS(head, Start) ||
						IS_CLASS(head, OperExpr) ||
						IS_CLASS(head, BlockExpr)
					)) {
					if(IS_TOKEN(prev, O_Sub) || IS_TOKEN(prev, O_Dot)) {
						if(IS_TOKEN(prev, O_Sub))
							prev->ast_token = token_t::O_Minus;
						prev->asc = Expr::Prefix;
						prev->meta_value = get_precedence(prev->ast_token);
						continue;
					}
				} else if((
					IS_CLASS(prev, OperOpen)
					|| IS_CLASS(prev, Delimiter)
					|| IS_CLASS(prev, KeywExpr)
					|| IS_CLASS(prev, Prefix)
					)
					&& IS_CLASS(head, RawOper)
				) {
					if(IS_TOKEN(head, O_Sub) || IS_TOKEN(head, O_Dot)) {
						if(IS_TOKEN(head, O_Sub))
							head->ast_token = token_t::O_Minus;
						head->asc = Expr::Prefix;
						head->meta_value = get_precedence(head->ast_token);
					}
				} else if(terminating && IS2C(OperOpen, OperExpr)) {
					// collapse the tree one level
					dbg << "<OPCol>";

					if(head->meta_value <= prev->meta_value) {
						// pop the prev item and put into the head item LHS
						//dbg << "<LEP\nL:" << prev << "\nR:" << head << "\n>";
						// ex1front push_into ex2
						// ex2 insert_into ex1front
						prev->asc = Expr::OperExpr;
						auto decend_item = &head;
						while(IS_CLASS(*decend_item, OperExpr) && (*decend_item)->meta_value <= prev->meta_value) {
							dbg << "<LED>";
							decend_item = &(*decend_item)->expressions.front();
						}
						PUSH_INTO(prev, *decend_item);
						prev.swap(*decend_item);
						REMOVE1(prev);
						continue; // re-eval the top of stack
					} else {
						//dbg << "<GP\nL:" << *prev << "\nR:" << *head << "\n>";
						prev->asc = Expr::OperExpr;
						PUSH_INTO(prev, head);
						REMOVE1(head);
						continue; // re-eval the top of stack
					}
				} else if(IS_CLASS(prev, End)) {
					dbg << "<End>";
					// no op
				} else if(IS2C(CommaList, Delimiter)) {
				} else if((
					IS_CLASS(prev, Start)
					|| IS_CLASS(prev, OperExpr)
					) && IS_TOKEN(head, Comma)) {
					if(third
						&& IS_CLASS(third, Delimiter)
						&& IS_TOKEN(third, Comma)
						&& fourth
						&& IS_CLASS(fourth, CommaList)) {
						// take prev, leave head
						EXTEND_TO(fourth, fourth, head);
						PUSH_INTO(fourth, prev);
						expr_list.erase(expr_list.cend() - 3, expr_list.cend() - 1);
					} else {
						auto MAKE_NODE_FROM2(comma_list, Comma, CommaList, prev, head);
						PUSH_INTO(comma_list, prev);
						prev.swap(comma_list); // comma_list -> prev
					}
				} else if(terminating &&
					(IS_TOKEN(prev, K_Let) || IS_TOKEN(prev, K_Mut))
					&& (IS_TOKEN(head, Ident) || IS_CLASS(head, CommaList))
					&& !AT_EXPR_TARGET(prev)
				) {
					PUSH_INTO(prev, head);
					REMOVE1(head);
					continue;
				} else if(terminating && IS_TOKEN(prev, Comma) && 
					(IS_CLASS(head, Start)
					|| IS_CLASS(head, OperExpr)
					|| IS_CLASS(head, BlockExpr)
					)
				) {
					if(third && IS_CLASS(third, CommaList)) {
						EXTEND_TO(third, third, head);
						third->newline = head->newline;
						PUSH_INTO(third, head);
						expr_list.erase(expr_list.cend() - 2, expr_list.cend());
						continue;
					} else if(third && (
						IS_CLASS(third, Start)
						|| IS_CLASS(third, OperExpr)
						|| IS_CLASS(third, BlockExpr)
						)) {
						prev->asc = Expr::CommaList;
						EXTEND_TO(prev, third, head);
						prev->newline = head->newline;
						PUSH_INTO(prev, third);
						PUSH_INTO(prev, head);
						REMOVE1(head); // "head"
						REMOVE1(prev); // "third" after head removed
						continue;
					} else {
						dbg << "<ERROR Comma!?>";
					}
				} else if(IS2C(OperOpen, Start) || IS2C(OperOpen, OperExpr)) {
					dbg << "<NOPS>";
				} else if(IS2C(Delimiter, Start)) {
					if(terminating) {
						dbg << "<TODO Delimiter>";
					} else dbg << "<NOP>";
				} else {
					dbg << "\n<<collapse_expr " << terminating << " unhandled " << prev << ":" << head << ">>";
				}
				break;
			}
			if(expr_hold) {
				//dbg << "\n<END collapse_expr before " << expr_hold << ">";
				expr_list.emplace_back(std::move(expr_hold));
			}
		};
		auto start_expr = [&](std::unique_ptr<ASTNode> new_node) {
			// push any existing start_expression
			auto &expr_list = current_block->expr_list;
			expr_list.emplace_back(std::move(new_node));
			collapse_expr(false);
		};
		auto terminal_expr = [&]() {
			dbg << "\n<TE " << lex_range.token << " " << token_string << ">";
			collapse_expr(true);
		};
		switch(lex_range.token) {
		case token_t::Block: // () [] {}
		case token_t::Array:
		case token_t::Object:
		case token_t::DotBlock: // .() .[] .{}
		case token_t::DotArray:
		case token_t::DotObject: {
			auto block = std::make_unique<ASTNode>(lex_range.token, Expr::BlockExpr, lex_range);
			auto ptr_block = block.get();
			current_block->expr_list.emplace_back(std::move(block));
			collapse_expr(false);
			block_stack.emplace_back(std::move(current_block));
			current_block = std::make_unique<ExprStackItem>(ExprStackItem{ptr_block, ptr_block->expressions});
			dbg << "(begin block '" << token_string << "' ";
			show = false;
			break;
		}
		case token_t::EndDelim: {
			char block_char = '\\';
			terminal_expr();
			auto &expr_list = current_block->expr_list;
			//for(auto &expr : expr_list) dbg << "expr: " << expr << "\n";
			//expr_list.clear(); // TODO use these
			switch(current_block->block->ast_token) {
			case token_t::Block:
			case token_t::DotBlock: block_char = ')'; break;
			case token_t::Array:
			case token_t::DotArray: block_char = ']'; break;
			case token_t::Object:
			case token_t::DotObject: block_char = '}'; break;
			default: break;
			}
			if(token_string[0] != block_char) {
				dbg << "\nInvalid closing for block: "
					<< current_block->block->ast_token << "!=" << token_string[0] << "\n";
				return true;
			}
			current_block->block->block.tk_end = lex_range.tk_end;
			show = false;
			if(!block_stack.empty()) {
				current_block.swap(block_stack.back());
				block_stack.pop_back();
				collapse_expr(false);
				dbg << " end block)\n";
			} else {
				dbg << "[empty stack!]\n";
				show = true;
			}
			break;
		}
		case token_t::Newline:
			if(!current_block->expr_list.empty())
				current_block->expr_list.back()->newline = true;
			// fallthrough
		case token_t::White:
			if(!current_block->expr_list.empty())
				current_block->expr_list.back()->whitespace = true;
			break;
		case token_t::Comma: {
			//terminal_expr();
			start_expr(std::make_unique<ASTNode>(token_t::Comma, Expr::Delimiter, lex_range));
			break;
		}
		case token_t::Ident: {
			auto found = std::ranges::find(keyword_table_span, token_string, &ParserKeyword::word);
			if(found != keyword_table_span.end()) {
				lex_range.token = found->token;
				switch(lex_range.token) {
				case token_t::K_If:
				case token_t::K_ElseIf:
				case token_t::K_While:
				case token_t::K_Until:
					start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::KeywExpr, 2, lex_range));
					break;
				case token_t::K_Else:
					start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::KeywExpr, 1, lex_range));
					break;
				case token_t::K_Let:
				case token_t::K_Mut:
					start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::KeywExpr, 1, lex_range));
					break;
				case token_t::K_Break:
				case token_t::K_Continue:
				case token_t::K_Loop:
				case token_t::K_For:
				case token_t::K_In:
				case token_t::K_Import:
				case token_t::K_Export:
				case token_t::K_Goto:
					// TODO all of these are
					break;
				case token_t::K_True:
				case token_t::K_False:
					start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::Start, lex_range));
					break;
				}
			} else {
				// definitely not a keyword
				start_expr(std::make_unique<ASTNode>(token_t::Ident, Expr::Start, lex_range));
			}
			break;
		}
		case token_t::Number:
		case token_t::NumberBin:
		case token_t::NumberHex:
		case token_t::NumberOct:
		case token_t::NumberReal:
		case token_t::Zero:
		case token_t::K_True:
		case token_t::K_False:
		case token_t::DotIdent:
		case token_t::String: {
			start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::Start, lex_range));
			dbg << "<Num>";
			break;
		}
		case token_t::DotDot:
		case token_t::Oper:
		case token_t::Minus:
		case token_t::Arrow:
		case token_t::RangeIncl:
		case token_t::Dot:
		case token_t::DotOper:
		case token_t::DotOperEqual:
		case token_t::DotOperPlus:
		case token_t::DotDotPlusEqual:
		case token_t::O_SubEq:
		case token_t::Hash:
		case token_t::OperEqual:
		case token_t::Less:
		case token_t::Xor:
		case token_t::XorDot:
		case token_t::LessDot:
		case token_t::LessUnit:
		case token_t::Greater:
		case token_t::GreaterEqual:
		case token_t::GreaterUnit:
		case token_t::GreaterDot:
		case token_t::LessEqual:
		case token_t::Equal:
		case token_t::EqualEqual:
		case token_t::Slash: {
			auto found = std::ranges::find(keyword_table_span, token_string, &ParserKeyword::word);
			if(found == keyword_table_span.end()) {
				dbg << "\nInvalid operator\n";
				return true;
			}
			lex_range.token = found->token;
			switch(lex_range.token) {
			case token_t::O_Length:
			case token_t::O_Not:
				start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::Prefix, lex_range));
				break;
			case token_t::O_Assign:
			case token_t::O_Dot:
			case token_t::O_Add:
			case token_t::O_Sub:
			case token_t::O_Mul:
			case token_t::O_Div:
			case token_t::O_Or:
			case token_t::O_And:
			case token_t::O_Xor:
			case token_t::O_Mod:
			case token_t::O_Rsh:
			case token_t::O_Lsh:
			case token_t::O_RAsh:
			case token_t::O_Power:
			case token_t::O_DotP:
			case token_t::O_Cross:
			case token_t::O_Less:
			case token_t::O_LessEq:
			case token_t::O_Greater:
			case token_t::O_GreaterEq:
			case token_t::O_NotEq:
			case token_t::O_EqEq:
				start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::RawOper, lex_range));
				dbg << "<Op2" << lex_range.token << ">";
				break;
			default:
				dbg << "Unhandled:" << lex_range.token;
				break;
			}
			break;
		}
		case token_t::Semi: {
			terminal_expr();
			start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::End, lex_range));
			break;
		}
		case token_t::Func: {
			terminal_expr();
			start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::FuncDecl, lex_range));
			break;
		}
		case token_t::Invalid: {
			dbg << "\nInvalid token\n";
			return true;
		}
		default: break;
		}
		token_buffer.emplace_back(std::move(lex_range));
		return show;
	};
	auto emit_token = [&](string::const_iterator cursor) {
		size_t tk_start = token_start - file_start;
		size_t tk_end = cursor - file_start;
		auto original = current_token;
		auto range = LexTokenRange{token_start, cursor, current_token};
		if(push_token(range)) {
			//if(!token_buffer.empty()) parsed = token_buffer.back().token;
			if(current_token == token_t::Newline) {
				dbg << "\n";
			} else if(current_token != token_t::White) {
				auto token_string = std::string_view(token_start, cursor);
				dbg << "["sv << tk_start << ',' << tk_end << ':'
					<< original;
				if(range.token != original)
					dbg << ':' << range.token;
				dbg << " \""sv << token_string << "\"]";
			}
		}
		token_start = cursor;
	};
	for(;; cursor++) {
		token_t next_token = current_token;
		char c = 0;
		if(cursor == file_end) {
			c = 0;
			next_token = token_t::Eof;
		} else {
			c = *cursor;
			if(c == '\n') {
				root_module->line_positions.push_back((cursor - file_start));
			}
			switch(current_state) {
			default:
			case LexState::Free:
				//std::ranges::take_while_view(
				//std::ranges::subrange(cursor, file_end), [&](auto &c) { return true; });
				if(std::ranges::find_if(lex_tree_span, [&](LexCharClass const &lex_class) {
					if((c < lex_class.begin) || (c > lex_class.end)) return false;
					for(LexNode const &node : lex_class.nodes) {
						if(node.action == LexAction::DefaultNewToken) {
							next_token = node.first_token;
							if(node.next_state != LexState::Remain)
								current_state = node.next_state;
							return true;
						}
						if(current_token == node.first_token) {
							switch(node.action) {
							case LexAction::Promote:
								current_token = next_token = node.next_token;
								break;
							case LexAction::NewToken:
								next_token = node.next_token;
								break;
							case LexAction::FixToken:
								switch(current_token) {
								case token_t::LessDot:
								case token_t::LessUnit:
									current_token = token_t::Less;
									break;
								case token_t::GreaterDot:
								case token_t::GreaterUnit:
									current_token = token_t::Greater;
									break;
								case token_t::XorDot:
									current_token = token_t::Xor;
									break;
								default:
									current_token = token_t::Invalid;
									break;
								}
								emit_token(token_start + 1);
								next_token = current_token = node.next_token;
								break;
							default:
							case LexAction::Remain:
								break;
							}
							if(node.next_state != LexState::Remain)
								current_state = node.next_state;
							return true;
						}
					}
					return false;
				}) == lex_tree_span.end()) {
					next_token = token_t::Invalid;
				}
				break;
			case LexState::Comment:
				if(contains("\r\n"sv, c)) {
					next_token = token_t::Newline;
					current_state = LexState::Free;
				}
				break;
			case LexState::DString:
				if(c == '"') {
					next_token = token_t::White;
					current_state = LexState::Free;
				}
				break;
			case LexState::SString:
				if(c == '\'') {
					next_token = token_t::White;
					current_state = LexState::Free;
				}
				break;
			}
		}
		if((next_token != current_token)
			|| (current_token == token_t::Block)
			|| (current_token == token_t::Array)
			|| (current_token == token_t::Object)
			|| (current_token == token_t::EndDelim)) {
			emit_token(cursor);
			current_token = next_token;
		}
		if(cursor == file_end) break;
	}
	dbg << "\nComplete\n" << root_node << '\n';
	dbg << "\nTree Walk\n";
	root_module->root_context = std::make_shared<FrameContext>(FrameContext{0});
	root_module->frames.push_back(root_module->root_context);
	dbg << "\nLine table:\n";
	for(auto itr = root_module->line_positions.cbegin(); itr != root_module->line_positions.cend(); itr++) {
		dbg << "[" << itr - root_module->line_positions.cbegin() << "]" << *itr << '\n';
	}
	walk_expression(dbg, root_module.get(), root_module->root_context, root_node);
	dbg << "\nString table:\n";
	for(auto itr = root_module->string_table.cbegin(); itr != root_module->string_table.cend(); itr++) {
		dbg << "[" << itr - root_module->string_table.cbegin() << "]" << *itr << '\n';
	}
	dbg << "\nScopes:\n";
	for(auto scope = root_module->frames.cbegin(); scope != root_module->frames.cend(); scope++) {
		dbg << "Scope: " << scope - root_module->frames.cbegin() << "\n";
		auto ins_begin = (*scope)->instructions.cbegin();
		for(auto ins = ins_begin; ins != (*scope)->instructions.cend(); ins++)
			dbg << "[" << ins - ins_begin << "] " << *ins << '\n';
	}
	this->script_map[into_name] = root_module;
}
struct StackFrame;
struct StackFrame {
	std::shared_ptr<FrameContext> context;
	std::shared_ptr<StackFrame> up;
	std::vector<Instruction>::const_iterator current_instruction;
	std::vector<Variable> closed_vars;
	std::vector<Variable> local_vars;
	std::vector<Variable> value_stack;
	StackFrame(
		std::shared_ptr<FrameContext> ctx,
		std::shared_ptr<StackFrame> up_ptr)
		: context{ctx}, up{up_ptr} {}
};

void ScriptContext::FunctionCall(string func_name) {
	auto found = this->script_map.find(func_name);
	if(found == this->script_map.cend()) {
		std::cerr << "call to non-existant function: " << func_name << '\n';
		return;
	}
	auto dbg_file = std::fstream("../debug-run.txt", std::ios_base::out | std::ios_base::trunc);
	auto &dbg = dbg_file;
	auto current_module = found->second;
	auto current_context = current_module->root_context;
	auto current_instruction = current_context->instructions.cbegin();
	auto end_of_instructions = current_context->instructions.cend();
	std::vector<std::shared_ptr<StackFrame>> scope_stack;
	std::vector<std::shared_ptr<StackFrame>> closure_frames;
	auto current_frame = std::make_shared<StackFrame>(current_context, std::shared_ptr<StackFrame>());
	Variable accumulator;
	auto show_accum = [&]() {
		dbg << "A: " << accumulator << '\n';
	};
	auto show_vars = [&]() {
		dbg << "Stack:";
		if(current_frame->value_stack.empty()) dbg << "<E!>";
		else for(auto &v : current_frame->value_stack) { dbg << " " << v; }
		dbg << " A: " << accumulator;
		dbg << " Locals:";
		if(current_frame->local_vars.empty()) dbg << "<E!>";
		else for(auto &v : current_frame->local_vars) { dbg << " " << v; }
		dbg << '\n';
	};
	auto pop_value = [&]() {
		auto value = current_frame->value_stack.back();
		current_frame->value_stack.pop_back();
		return std::move(value);
	};
	for(;;) {
		if(current_instruction == end_of_instructions) {
			if(!scope_stack.empty()) {
				show_vars();
				auto next_scope = std::move(scope_stack.back());
				scope_stack.pop_back();
				current_instruction = next_scope->current_instruction;
				end_of_instructions = next_scope->context->instructions.cend();
				current_context = next_scope->context;
				current_frame = std::move(next_scope);
				dbg << "Exit to frame: " << current_context->frame_index
					<< " ins " << (current_instruction - current_context->instructions.cbegin())
					<< "/" << current_context->instructions.size() << '\n';
				show_vars();
				current_instruction++;
				continue;
			}
			break;
		}
		auto &ins = current_instruction;
		auto param = current_instruction->param;
		auto first_instruction = current_context->instructions.cbegin();
		dbg << "EXEC: [" << current_instruction - first_instruction << "]" << current_instruction->opcode << " " << param << '\n';
		switch(current_instruction->opcode) {
		case Opcode::CallExpression: {
			size_t closure_frame = accumulator.value >> 32;
			size_t frame_index = accumulator.value & 0xffffffffull;
			if(accumulator.vtype != VarType::Function || frame_index >= current_module->frames.size()) {
				dbg << "call to non-function value\n";
				show_vars();
				return;
			}
			auto next_context = current_module->frames[frame_index];
			auto &prev_stack = current_frame->value_stack;
			auto closure_frame_ptr = closure_frames[closure_frame];
			current_frame->current_instruction = current_instruction;
			scope_stack.push_back(current_frame);
			current_frame = std::make_shared<StackFrame>(next_context, closure_frame_ptr);
			if(param > 0) {
				// TODO take the argument's values from the stack
				// and actually give them to the function
				if(param > prev_stack.size()) {
					dbg << "stack underflow!\n";
					return;
				}
				prev_stack.erase(prev_stack.cend() - param, prev_stack.cend());
			}
			current_instruction = next_context->instructions.cbegin();
			end_of_instructions = next_context->instructions.cend();
			current_context = next_context;
			continue;
		}
		case Opcode::LoadClosure: {
			dbg << "load closure: " << param << " TODO capture scope variables\n";
			// param is scope_id
			size_t current_closure = closure_frames.size();
			closure_frames.push_back(current_frame);
			accumulator = Variable{param | (current_closure << 32), VarType::Function};
			show_accum();
			break;
		}
		case Opcode::LoadConst:
			accumulator = Variable{param, VarType::Integer};
			break;
		case Opcode::PushRegister:
			current_frame->value_stack.push_back(accumulator);
			show_vars();
			break;
		case Opcode::PopRegister:
			accumulator = pop_value();
			show_vars();
			break;
		case Opcode::PopStack:
			pop_value();
			show_vars();
			break;
		case Opcode::LoadStack:
			if(param >= current_frame->value_stack.size()) {
				accumulator = Variable{};
			} else {
				accumulator = *(current_frame->value_stack.crbegin() + param);
			}
			break;
		case Opcode::StoreStack:
			if(param < current_frame->value_stack.size()) {
				*(current_frame->value_stack.rend() + param) = accumulator;
			}
			break;
		case Opcode::AddInteger:
			accumulator = Variable{pop_value().value + accumulator.value};
			show_vars();
			break;
		case Opcode::SubInteger:
			accumulator = Variable{pop_value().value - accumulator.value};
			show_vars();
			break;
		case Opcode::MulInteger:
			accumulator = Variable{
				static_cast<int64_t>(pop_value().value)
				* static_cast<int64_t>(accumulator.value)};
			show_vars();
			break;
		case Opcode::DivInteger:
			accumulator = Variable{pop_value().value / accumulator.value};
			show_vars();
			break;
		case Opcode::ModInteger:
			accumulator = Variable{pop_value().value % accumulator.value};
			show_vars();
			break;
		case Opcode::PowInteger: {
			int64_t ex = static_cast<int64_t>(accumulator.value);
			int64_t left = pop_value().value;
			int64_t result = 0;
			if(ex < 0) {
				result = 0;
			} else {
				result = 1;
				while(ex != 0) {
					if(ex & 1) {
						result *= left;
					}
					left *= left;
					ex >>= 1;
				}
			}
			accumulator = Variable{result};
			show_vars();
			break;
		}
		case Opcode::Negate:
			accumulator = Variable{-static_cast<int64_t>(accumulator.value)};
			show_accum();
			break;
		case Opcode::OrInteger:
			accumulator = Variable{pop_value().value | accumulator.value};
			show_vars();
			break;
		case Opcode::AndInteger:
			accumulator = Variable{pop_value().value & accumulator.value};
			show_vars();
			break;
		case Opcode::XorInteger:
			accumulator = Variable{pop_value().value ^ accumulator.value};
			show_vars();
			break;
		case Opcode::RSHLInteger:
			accumulator = Variable{pop_value().value >> accumulator.value};
			show_vars();
			break;
		case Opcode::RSHAInteger:
			accumulator =
				Variable{static_cast<int64_t>(pop_value().value)
				>> accumulator.value};
			show_vars();
			break;
		case Opcode::LSHInteger:
			accumulator =
				Variable{pop_value().value << accumulator.value};
			show_vars();
			break;
		case Opcode::CompareLess:
			accumulator = Variable{
				pop_value().value < accumulator.value
				? uint64_t{1} : uint64_t{0}
				, VarType::Bool};
			show_vars();
			break;
		case Opcode::CompareGreater:
			accumulator = Variable{
				pop_value().value > accumulator.value
				? uint64_t{1} : uint64_t{0}
				, VarType::Bool};
			show_vars();
			break;
		case Opcode::Not:
			accumulator = Variable{accumulator.value == 0
				? uint64_t{1} : uint64_t{0}, VarType::Bool};
			show_vars();
			break;
		case Opcode::Jump:
			if(param > current_context->instructions.size()) {
				param = current_context->instructions.size();
			}
			current_instruction = first_instruction + param;
			continue;
		case Opcode::JumpIf:
			if(param > current_context->instructions.size()) {
				param = current_context->instructions.size();
			}
			if(accumulator.value == 0) break;
			current_instruction = first_instruction + param;
			continue;
		case Opcode::LoadBool:
			accumulator = Variable{param, VarType::Bool};
			break;
		case Opcode::LoadUnit:
			accumulator = Variable{0, VarType::Unit};
			break;
		case Opcode::LoadVariable: {
			uint32_t up_count = ins->param_a;
			dbg << "load variable: " << up_count << "," << ins->param_b << ": ";
			auto search_context = current_frame.get();
			for(;up_count != 0; up_count--) {
				search_context = search_context->up.get();
				if(search_context == nullptr) {
					dbg << "frame depth error\n";
					return;
				}
			}
			auto &var_list = search_context->context->var_declarations;
			if(ins->param_b >= var_list.size()) {
				dbg << "reference out of bounds\n";
				return;
			}
			auto &var_decl = var_list.at(ins->param_b);
			// found variable case
			accumulator = search_context->local_vars[var_decl.stack_pos];
			show_accum();
			break;
		}
		case Opcode::AssignLocal: {
			auto &var_decl = current_context->var_declarations[param];
			dbg << "assign local variable: [" << var_decl.stack_pos << "]"
				<< current_module->string_table.at(var_decl.name_index)
				<< '\n';
			if(var_decl.stack_pos == current_frame->local_vars.size()) {
				current_frame->local_vars.push_back(accumulator);
			} else {
				current_frame->local_vars.at(var_decl.stack_pos) = accumulator;
			}
			show_vars();
			break;
		}
		case Opcode::ExitScope: {
			current_frame->local_vars.resize(param);
			show_vars();
			break;
		}
		default:
			dbg << "not implemented: " << current_instruction->opcode << '\n';
		}
		current_instruction++;
	}
}

}
