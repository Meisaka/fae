
#include <ranges>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
using namespace std::string_view_literals;

namespace Fae {

using string = std::string;

class ScriptContext {
public:

	ScriptContext();
	virtual ~ScriptContext();

	virtual void FunctionCall(std::string);
	virtual void LoadScriptFile(string f, string i);
};
ScriptContext::ScriptContext() {}
ScriptContext::~ScriptContext() {}

void ScriptContext::FunctionCall(std::string) {}

void LoadFileV(const string path, string &outdata) {
	auto file = std::fstream(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	size_t where = file.tellg();
	file.seekg(std::ios_base::beg);
	outdata.reserve(where);
	file.read(outdata.data(), where);
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
	int prec;
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

	{10, token_t::O_Dot},
};
constexpr auto p_table_span = std::span{p_table};
static int get_precedence(token_t token) {
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
	union {
		int precedence;
		int expr_target;
	};
	//std::shared_ptr<string> source;
	ASTNode() = delete;
	ASTNode(token_t token, Expr a, LexTokenRange lex_range)
		: ast_token{token}, asc{a}, block{lex_range}, precedence{get_precedence(token)} {}
	ASTNode(token_t token, Expr a, int e, LexTokenRange lex_range)
		: ast_token{token}, asc{a}, block{lex_range}, expr_target{e} {}
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
	if(node.asc == Expr::RawOper || node.asc == Expr::OperExpr || node.asc == Expr::OperOpen)
		os << " " << node.precedence;
	if(node.asc == Expr::KeywExpr)
		os << " " << node.expr_target << "," << node.expressions.size();
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
#define CONV_N(n, t, num) n->asc = Expr::t; n->expr_target = num;
#define EXTEND_TO(t, sl, sr) t->block.tk_begin = sl->block.tk_begin; \
	t->block.tk_end = sr->block.tk_end;
#define AT_EXPR_TARGET(id) (id->expressions.size() >= id->expr_target)
#define IS_TOKEN(n, t) (n->ast_token == token_t::t)
#define IS_CLASS(n, c) (n->asc == Expr::c)
#define IS2C(c1, c2) ((prev->asc == Expr::c1) && (head->asc == Expr::c2))
#define IS3C(c1, c2, c3) (third && (third->asc == Expr::c1) && (prev->asc == Expr::c2) && (head->asc == Expr::c3))
#define MAKE_NODE_FROM1(n, t, cl, s) n = std::make_unique<ASTNode>( \
token_t::t, Expr::cl, LexTokenRange{s->block.tk_begin, s->block.tk_end, token_t::t});
#define MAKE_NODE_FROM2(n, t, cl, s1, s2) n = std::make_unique<ASTNode>( \
token_t::t, Expr::cl, LexTokenRange{s1->block.tk_begin, s2->block.tk_end, token_t::t});
#define MAKE_NODE_N_FROM1(n, t, cl, num, s) n = std::make_unique<ASTNode>( \
token_t::t, Expr::cl, num, LexTokenRange{s->block.tk_begin, s->block.tk_end, token_t::t});

void ScriptContext::LoadScriptFile(string file_path, string into_name) {
	// load contents of script file at "file_path", put into namespace "into_name"
	auto dbg_file = std::fstream("./debug.txt", std::ios_base::out | std::ios_base::trunc);
	auto &dbg = dbg_file;
	std::shared_ptr<string> file_source = std::make_shared<string>();
	LoadFileV(file_path, *file_source);
	token_t current_token = token_t::White;
	source_itr token_start = file_source->cbegin();
	const auto file_end = file_source->cend();
	auto cursor = token_start;
	LexState current_state = LexState::Free;

	std::vector<LexTokenRange> token_buffer;
	node_ptr root_node =
		std::make_unique<ASTNode>(token_t::Object, Expr::End,
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
				if(expr_list.size() == 1 && expr_list.back()->ast_token == token_t::Newline) {
					expr_list.pop_back();
				}
				if(expr_list.size() < 2) break;
				dbg << "||";
				auto &head = expr_list.back();
				auto &prev = *(expr_list.end() - 2);
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
					(IS_TOKEN(prev, K_Let) || IS_TOKEN(prev, K_Mut))
					&& IS_TOKEN(head, Ident)
					&& !AT_EXPR_TARGET(prev)
				) {
					PUSH_INTO(prev, head);
					REMOVE1(head);
					continue;
				} else if(
					IS_CLASS(prev, FuncExpr) && !AT_EXPR_TARGET(prev)
					&& (
						IS_CLASS(head, BlockExpr)
						|| IS_CLASS(head, OperExpr)
						|| IS_CLASS(head, Start))
					) {
					if(terminating) {
						PUSH_INTO(prev, head);
						REMOVE1(head);
					} else {
						dbg << "\n<func expr>";
					}
				} else if(IS3C(FuncExpr, BlockExpr, Start)) {
					PUSH_INTO(third, prev);
					REMOVE1(prev);
				} else if(third && IS_CLASS(head, FuncDecl) && (IS_TOKEN(prev, Ident) || IS_CLASS(prev, CommaList))) {
					// check third for another ident or let
					if(IS_TOKEN(third, Ident)) {
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
						PUSH_INTO(let_expr, third);
						third = std::move(let_expr);
						CONV_N(head, FuncExpr, 2)
						PUSH_INTO(head, comma_list);
						REMOVE1(prev);
					} else if(IS_TOKEN(third, K_Let)) {
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
						CONV_N(head, FuncExpr, 2)
						PUSH_INTO(head, comma_list);
						REMOVE1(prev);
					} else {
						auto MAKE_NODE_N_FROM1(let_expr, K_Let, KeywExpr, 1, prev);
						PUSH_INTO(let_expr, prev);
						prev = std::move(let_expr);
						auto MAKE_NODE_FROM1(comma_list, Func, CommaList, head);
						CONV_N(head, FuncExpr, 2)
						PUSH_INTO(head, comma_list);
						// named function with no args or anon with comma arg list
						dbg << "\n<func no args?>";
					}
				} else if(IS_CLASS(head, FuncDecl) && (IS_TOKEN(prev, Ident) || IS_CLASS(prev, CommaList))) {
					dbg << "\n<func no third>";
					CONV_N(head, FuncExpr, 2)
					PUSH_INTO(head, prev);
					REMOVE1(prev);
				} else if(IS2C(BlockExpr, FuncDecl)) {
					// anon function with blocked arg list
					// or named function with blocked arg list?
					dbg << "\n<func block>";
					CONV_N(head, FuncExpr, 2)
					PUSH_INTO(head, prev);
					REMOVE1(prev);
				} else if(
					IS_CLASS(prev, KeywExpr)
					&& (
						IS_CLASS(head, Start)
						|| IS_CLASS(head, OperExpr)
						|| IS_CLASS(head, BlockExpr))
				) {
					if((terminating) && !AT_EXPR_TARGET(prev)) {
						PUSH_INTO(prev, head);
						REMOVE1(head);
						continue;
					}
				} else if((
					IS_TOKEN(prev, Ident)
					|| IS_TOKEN(prev, O_RefExpr)
					|| IS_TOKEN(prev, O_Dot)
					) && (
						IS_TOKEN(head, Array)
						|| IS_TOKEN(head, Block)
						|| IS_TOKEN(head, Object)
						)
				) {
					auto MAKE_NODE_FROM2(block_expr, O_RefExpr, OperExpr, prev, head);
					block_expr->precedence = 20;
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
				} else if(IS2C(OperOpen, Start)) {
					// before: (oper OperOpen (value Start)) (value Start)
					//  after: (oper OperExpr (value Start) (value Start))
					prev->asc = Expr::OperExpr;
					PUSH_INTO(prev, head);
					REMOVE1(head);
				} else if(IS2C(OperExpr, RawOper)) {
					// before: (oper1 OperExpr (value1 Start) (value2 Start)) (oper2 Raw)
					if(head->precedence <= prev->precedence) {
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
					if(!terminating) {
						dbg << "\n<collapse_expr prev>";
						terminating = true;
						expr_hold = std::move(expr_list.back());
						REMOVE1(head);
						continue;
					}
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
						prev->precedence = get_precedence(prev->ast_token);
						continue;
					}
				} else if(IS_CLASS(head, RawOper) && (
					IS_CLASS(prev, OperOpen)
					|| IS_CLASS(prev, Delimiter)
					|| IS_CLASS(prev, Prefix)
					)) {
					if(IS_TOKEN(head, O_Sub) || IS_TOKEN(head, O_Dot)) {
						if(IS_TOKEN(head, O_Sub))
							head->ast_token = token_t::O_Minus;
						head->asc = Expr::Prefix;
						head->precedence = get_precedence(head->ast_token);
					}
				} else if(IS2C(OperOpen, OperExpr)) {
					// collapse the tree one level
					dbg << "<OPCol>";
					if(head->precedence <= prev->precedence) {
						// pop the prev item and put into the head item LHS
						//dbg << "<LEP\nL:" << prev << "\nR:" << head << "\n>";
						// ex1front push_into ex2
						// ex2 insert_into ex1front
						prev->asc = Expr::OperExpr;
						PUSH_INTO(prev, head->expressions.front());
						prev.swap(head->expressions.front());
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
				} else if(IS2C(Start, Delimiter) && IS_TOKEN(head, Comma)) {
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
				} else if(terminating && IS_TOKEN(head, Newline)) {
					REMOVE1(head);
					continue;
				} else if(IS_TOKEN(prev, Newline) && IS_TOKEN(head, Newline)) {
					REMOVE1(head);
					continue;
				} else if(IS2C(Delimiter, Start)) {
					if(terminating && IS_TOKEN(prev, Comma)) {
						if(third && IS_CLASS(third, CommaList)) {
							EXTEND_TO(third, third, head);
							PUSH_INTO(third, head);
							expr_list.erase(expr_list.cend() - 2, expr_list.cend());
						} else if(third && IS_CLASS(third, Start)) {
							prev->asc = Expr::CommaList;
							EXTEND_TO(prev, third, head);
							PUSH_INTO(prev, third);
							PUSH_INTO(prev, head);
							REMOVE1(head); // "head"
							REMOVE1(prev); // "third" after head removed
						} else {
							dbg << "<ERROR Comma!?>";
						}
					} else if(terminating) {
						dbg << "<TODO Delimiter>";
					} else dbg << "<NOP>";
				} else {
					dbg << "\n<<collapse_expr unhandled " << prev << ":" << head << ">>";
				}
				break;
			}
			if(expr_hold) {
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
			for(auto &expr : expr_list) {
				dbg << "expr: " << expr << "\n";
			}
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
		case token_t::Newline: {
			start_expr(std::make_unique<ASTNode>(token_t::Newline, Expr::Delimiter, lex_range));
			break;
		}
		case token_t::Comma: {
			terminal_expr();
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
		size_t tk_start = token_start - file_source->cbegin();
		size_t tk_end = cursor - file_source->cbegin();
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
}


}
