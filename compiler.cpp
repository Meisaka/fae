
#include <ranges>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <string_view>
#include <vector>
#include <optional>
#include <unordered_map>
#include <assert.h>
#include "script.hpp"

#define DEBUG_LEXPARSE 0
#define DEBUG_WALK
//#define DEBUG_PARSE

using namespace std::string_view_literals;

namespace Fae {

template<class R, class P>
auto find_last_if(const R &r, P pred) {
	auto end_itr = r.cend();
	auto search_itr = end_itr;
	auto begin_itr = r.cbegin();
	while(search_itr != begin_itr) {
		search_itr--;
		if(pred(*search_itr)) return search_itr;
	}
	return end_itr;
};
ScriptContext::ScriptContext() {}
ScriptContext::~ScriptContext() {}

typedef uint8_t lex_t[256];
static lex_t lex_table[] {
	{0}
};
static bool contains(const std::string_view &str, char c) {
	return std::ranges::any_of(str, [&](auto sc) { return sc == c; });
}
const auto invalid_name_string = "?"sv;
#define GENERATE_ENUM_LIST(f) f,
#define GENERATE_STRING_LIST(f) #f##sv,
#define MAKE_ENUM(ty, gen) \
enum class ty : uint8_t { gen(GENERATE_ENUM_LIST) }; \
std::string_view ty##_table[] = { gen(GENERATE_STRING_LIST) }; \
constexpr auto ty##_table_span = std::span(ty##_table); \
static std::string_view name_of(ty tk) { \
	size_t index = static_cast<size_t>(tk); \
	if(index >= ty##_table_span.size()) return invalid_name_string; \
	return *(ty##_table_span.begin() + index); \
} \
std::ostream &operator<<(std::ostream &os, ty tk) { return os << name_of(tk); }

#define DEF_TOKENS(f) \
	f(Invalid) f(Eof) f(White) f(Newline) f(Comma) f(Semi) \
	f(Ident) f(Keyword) f(TypeTag) \
	f(Block) f(Array) f(Object) f(EndDelim) \
	f(TypeBlock) f(TypeArray) f(TypeObject) \
	f(DotBlock) f(DotArray) f(DotObject) \
	f(K_If) f(K_ElseIf) f(K_Else) \
	f(K_While) f(K_Until) f(K_Loop) f(K_Continue) f(K_Break) \
	f(K_Goto) f(K_End) \
	f(K_For) f(K_In) \
	f(K_Let) f(K_Mut) \
	f(K_True) f(K_False) \
	f(K_Import) f(K_Export) \
	f(K_Type) f(K_Enum) f(O_Sum) f(O_Union) \
	f(O_Decl) f(O_DeclAssign) f(O_CallExpr) \
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
	f(Colon) \
	f(O_Compose) \
	f(O_LeftRot) f(O_RightRot) \
	f(O_LeftRotIn) f(O_RightRotIn) \
	f(Xor) f(XorDot) \
	f(Slash)

MAKE_ENUM(Token, DEF_TOKENS);

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
	Token first_token;
	Token next_token;
	LexState next_state;
	constexpr LexNode(LexAction act, Token chk) :
		action{ act },
		first_token{ chk },
		next_token{Token::Invalid},
		next_state{LexState::Remain} {}
	constexpr LexNode(LexAction act, Token chk, Token next) :
		action{ act },
		first_token{ chk },
		next_token{next},
		next_state{LexState::Remain} {}
	constexpr LexNode(LexAction act, Token chk, Token next, LexState state) :
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
	LexNode(LexAction::NewToken, Token::Zero, Token::Number),
	LexNode(LexAction::Remain, Token::NumberBin),
	LexNode(LexAction::Remain, Token::NumberHex),
	LexNode(LexAction::Remain, Token::NumberOct),
	LexNode(LexAction::Remain, Token::Ident),
	LexNode(LexAction::Promote, Token::Unit, Token::Ident),
	LexNode(LexAction::Promote, Token::Less, Token::LessUnit),
	LexNode(LexAction::Promote, Token::Greater, Token::GreaterUnit),
	LexNode(LexAction::DefaultNewToken, Token::Unit),
}},{'A','F', { LexNode(LexAction::Remain, Token::NumberHex)
}},{'a','f', { LexNode(LexAction::Remain, Token::NumberHex)
}},{'b','b', { LexNode(LexAction::Promote, Token::Zero, Token::NumberBin)
}},{'B','B', { LexNode(LexAction::Promote, Token::Zero, Token::NumberBin)
}},{'x','x', { LexNode(LexAction::Promote, Token::Zero, Token::NumberHex)
}},{'X','X', { LexNode(LexAction::Promote, Token::Zero, Token::NumberHex)
}},{'o','o', { LexNode(LexAction::Promote, Token::Zero, Token::NumberOct)
}},{'O','O', { LexNode(LexAction::Promote, Token::Zero, Token::NumberOct)
}},{'A','Z', {
	LexNode(LexAction::FixToken, Token::GreaterUnit, Token::Ident),
	LexNode(LexAction::FixToken, Token::LessUnit, Token::Ident),
	LexNode(LexAction::Promote, Token::Unit, Token::Ident),
	LexNode(LexAction::DefaultNewToken, Token::Ident)
}},{'a','z', {
	LexNode(LexAction::FixToken, Token::GreaterUnit, Token::Ident),
	LexNode(LexAction::FixToken, Token::LessUnit, Token::Ident),
	LexNode(LexAction::Promote, Token::Unit, Token::Ident),
	LexNode(LexAction::DefaultNewToken, Token::Ident)
}},{'0','0', {
	LexNode(LexAction::Promote, Token::Zero, Token::Number),
	LexNode(LexAction::Promote, Token::Unit,Token::Ident),
	LexNode(LexAction::Promote, Token::Ident, Token::Ident),
	LexNode(LexAction::Promote, Token::Dot, Token::NumberReal),
	LexNode(LexAction::FixToken, Token::GreaterDot, Token::NumberReal),
	LexNode(LexAction::FixToken, Token::GreaterUnit, Token::Ident),
	LexNode(LexAction::FixToken, Token::LessDot, Token::NumberReal),
	LexNode(LexAction::FixToken, Token::LessUnit, Token::Ident),
	LexNode(LexAction::FixToken, Token::XorDot, Token::NumberReal),
	LexNode(LexAction::Remain, Token::Number),
	LexNode(LexAction::Remain, Token::NumberReal),
	LexNode(LexAction::Remain, Token::NumberBin),
	LexNode(LexAction::Remain, Token::NumberHex),
	LexNode(LexAction::Remain, Token::NumberOct),
	LexNode(LexAction::DefaultNewToken, Token::Zero)
}},{'1','1', { LexNode(LexAction::Remain, Token::NumberBin)
}},{'1','7', { LexNode(LexAction::Remain, Token::NumberOct)
}},{'1','9', {
	LexNode(LexAction::Promote, Token::Unit, Token::Ident),
	LexNode(LexAction::Promote, Token::Ident, Token::Ident),
	LexNode(LexAction::Promote, Token::Dot, Token::NumberReal),
	LexNode(LexAction::Promote, Token::Zero, Token::Number),
	LexNode(LexAction::FixToken, Token::GreaterDot, Token::NumberReal),
	LexNode(LexAction::FixToken, Token::GreaterUnit, Token::Ident),
	LexNode(LexAction::FixToken, Token::LessDot, Token::NumberReal),
	LexNode(LexAction::FixToken, Token::LessUnit, Token::Ident),
	LexNode(LexAction::FixToken, Token::XorDot, Token::NumberReal),
	LexNode(LexAction::NewToken, Token::NumberBin, Token::Invalid),
	LexNode(LexAction::NewToken, Token::NumberOct, Token::Invalid),
	LexNode(LexAction::Remain, Token::NumberHex),
	LexNode(LexAction::DefaultNewToken, Token::Number)
}},{'/','/', {
	LexNode(LexAction::Promote, Token::Dot, Token::DotOper),
	LexNode(LexAction::Promote, Token::Slash, Token::White, LexState::Comment),
	LexNode(LexAction::DefaultNewToken, Token::Slash)
}},{'*','*', {
	LexNode(LexAction::Promote, Token::DotDot, Token::DotOper),
	LexNode(LexAction::Promote, Token::Dot, Token::Oper),
	LexNode(LexAction::Promote, Token::Slash, Token::White, LexState::Comment),
	LexNode(LexAction::DefaultNewToken, Token::Oper)
}},{'.','.', {
	LexNode(LexAction::Promote, Token::Less, Token::LessDot),
	LexNode(LexAction::Promote, Token::Greater, Token::GreaterDot),
	LexNode(LexAction::Promote, Token::Xor, Token::XorDot),
	LexNode(LexAction::Promote, Token::DotDot, Token::DotDotDot),
	LexNode(LexAction::Promote, Token::Dot, Token::DotDot),
	LexNode(LexAction::DefaultNewToken, Token::Dot)
}},{';',';', {
	LexNode(LexAction::DefaultNewToken, Token::Semi),
}},{':',':', {
	LexNode(LexAction::DefaultNewToken, Token::Colon),
}},{',',',', {
	LexNode(LexAction::DefaultNewToken, Token::Comma),
}},{'=','=', {
	LexNode(LexAction::Promote, Token::DotDot, Token::RangeIncl),
	LexNode(LexAction::Promote, Token::DotOper, Token::DotOperEqual),
	LexNode(LexAction::Promote, Token::DotOperPlus, Token::DotDotPlusEqual),
	LexNode(LexAction::Promote, Token::Less, Token::LessEqual),
	LexNode(LexAction::Promote, Token::Greater, Token::GreaterEqual),
	LexNode(LexAction::Promote, Token::Xor, Token::O_XorEq),
	LexNode(LexAction::Promote, Token::Minus, Token::O_SubEq),
	LexNode(LexAction::Promote, Token::Slash, Token::OperEqual),
	LexNode(LexAction::Promote, Token::Oper, Token::OperEqual),
	LexNode(LexAction::DefaultNewToken, Token::Equal)
}},{'>','>', {
	LexNode(LexAction::Promote, Token::GreaterDot, Token::O_RightRot),
	LexNode(LexAction::Promote, Token::GreaterUnit, Token::O_RightRotIn),
	LexNode(LexAction::Promote, Token::LessEqual, Token::O_Spaceship),
	LexNode(LexAction::Promote, Token::Equal, Token::Func),
	LexNode(LexAction::Promote, Token::Minus, Token::Arrow),
	LexNode(LexAction::DefaultNewToken, Token::Greater)
}},{'<','<', {
	LexNode(LexAction::Promote, Token::LessDot, Token::O_LeftRot),
	LexNode(LexAction::Promote, Token::LessUnit, Token::O_LeftRotIn),
	LexNode(LexAction::DefaultNewToken, Token::Less)
}},{'+','+', {
	LexNode(LexAction::Promote, Token::DotDot, Token::DotOperPlus),
	LexNode(LexAction::Promote, Token::Dot, Token::Oper),
	LexNode(LexAction::DefaultNewToken, Token::Oper)
}},{'-','-', {
	LexNode(LexAction::Promote, Token::Dot, Token::DotOper),
	LexNode(LexAction::DefaultNewToken, Token::Minus)
}},{'&','&', { LexNode(LexAction::DefaultNewToken, Token::Oper)
}},{'|','|', { LexNode(LexAction::DefaultNewToken, Token::Oper)
}},{'^','^', {
	LexNode(LexAction::Promote, Token::XorDot, Token::O_Compose),
	LexNode(LexAction::DefaultNewToken, Token::Xor)
}},{'%','%', {
	LexNode(LexAction::Remain, Token::DotOper),
	LexNode(LexAction::Promote, Token::Slash, Token::Oper),
	LexNode(LexAction::DefaultNewToken, Token::Oper)
}},{'!','!', { LexNode(LexAction::DefaultNewToken, Token::Oper)
}},{'#','#', { LexNode(LexAction::DefaultNewToken, Token::Hash)
}},{'(','(', {
	LexNode(LexAction::Promote, Token::Dot, Token::DotBlock),
	LexNode(LexAction::DefaultNewToken, Token::Block)
}},{'[','[', {
	LexNode(LexAction::Promote, Token::Dot, Token::DotArray),
	LexNode(LexAction::DefaultNewToken, Token::Array)
}},{'{','{', {
	LexNode(LexAction::Promote, Token::Dot, Token::DotObject),
	LexNode(LexAction::DefaultNewToken, Token::Object)
}},{')',')', { LexNode(LexAction::DefaultNewToken, Token::EndDelim)
}},{']',']', { LexNode(LexAction::DefaultNewToken, Token::EndDelim)
}},{'}','}', { LexNode(LexAction::DefaultNewToken, Token::EndDelim)
}},{'"','"', { LexNode(LexAction::DefaultNewToken, Token::String, Token::String, LexState::DString)
}},{'\'','\'', { LexNode(LexAction::DefaultNewToken, Token::String, Token::String, LexState::SString)
}},{' ',' ', { LexNode(LexAction::DefaultNewToken, Token::White)
}},{'\t','\t', { LexNode(LexAction::DefaultNewToken, Token::White)
}},{'\r','\r', { LexNode(LexAction::DefaultNewToken, Token::White)
}},{'\n','\n', {
	LexNode(LexAction::Promote, Token::White, Token::Newline),
	LexNode(LexAction::DefaultNewToken, Token::Newline)
}}
};

#define DEF_EXPRESSIONS(f) \
	f(Undefined) \
	f(Delimiter) f(End) \
	f(Start) f(TypeStart) f(BlockExpr) \
	f(RawOper) f(FuncDecl) \
	f(OperExpr) f(FuncExpr) f(KeywExpr) \
	f(TypeDecl) f(TypeExpr)

MAKE_ENUM(Expr, DEF_EXPRESSIONS);

enum class KWA : uint8_t {
	None, Terminate,
	ObjIdent, ObjAssign,
};
struct ParserKeyword {
	std::string_view word;
	Token token;
	Expr parse_class;
	ASTSlots slots;
	KWA action;
	ParserKeyword(std::string_view w, Token t, Expr c, ASTSlots m) :
		word{w}, token{t}, parse_class{c}, slots{m}, action{KWA::None} {}
	ParserKeyword(std::string_view w, Token t, Expr c, ASTSlots m, KWA act) :
		word{w}, token{t}, parse_class{c}, slots{m}, action{act} {}
};
const ParserKeyword keyword_table[] = {
	{"if"sv, Token::K_If, Expr::KeywExpr, ASTSlots::SLOT2},
	{"else"sv, Token::K_Else, Expr::KeywExpr, ASTSlots::SLOT1},
	{"elif"sv, Token::K_ElseIf, Expr::KeywExpr, ASTSlots::SLOT2},
	{"elf"sv, Token::K_ElseIf, Expr::KeywExpr, ASTSlots::SLOT2},
	{"elsif"sv, Token::K_ElseIf, Expr::KeywExpr, ASTSlots::SLOT2},
	{"elseif"sv, Token::K_ElseIf, Expr::KeywExpr, ASTSlots::SLOT2},
	{"loop"sv, Token::K_Loop, Expr::KeywExpr, ASTSlots::SLOT1},
	{"while"sv, Token::K_While, Expr::KeywExpr, ASTSlots::SLOT2},
	{"until"sv, Token::K_Until, Expr::KeywExpr, ASTSlots::SLOT2},
	{"continue"sv, Token::K_Continue, Expr::KeywExpr, ASTSlots::NONE},
	{"end"sv, Token::K_End, Expr::KeywExpr, ASTSlots::SLOT1},
	{"break"sv, Token::K_Break, Expr::KeywExpr, ASTSlots::SLOT1},
	{"for"sv, Token::K_For, Expr::Undefined, ASTSlots::NONE},
	{"in"sv, Token::K_In, Expr::Undefined, ASTSlots::SLOT1},
	{"goto"sv, Token::K_Goto, Expr::KeywExpr, ASTSlots::SLOT1},
	{"let"sv, Token::K_Let, Expr::KeywExpr, ASTSlots::SLOT1},
	{"mut"sv, Token::K_Mut, Expr::KeywExpr, ASTSlots::SLOT1},
	{"true"sv, Token::K_True, Expr::Start, ASTSlots::NONE},
	{"false"sv, Token::K_False, Expr::Start, ASTSlots::NONE},
	{"import"sv, Token::K_Import, Expr::Undefined, ASTSlots::SLOT1},
	{"export"sv, Token::K_Export, Expr::Undefined, ASTSlots::SLOT1},
	{"type"sv, Token::K_Type, Expr::TypeDecl, ASTSlots::SLOT2, KWA::Terminate},
	{"enum"sv, Token::K_Enum, Expr::TypeDecl, ASTSlots::SLOT2, KWA::Terminate},
	{"#"sv ,Token::O_Length, Expr::OperExpr, ASTSlots::SLOT1},
	{"!"sv ,Token::O_Not, Expr::OperExpr, ASTSlots::SLOT1},
	{"&"sv ,Token::O_And, Expr::RawOper, ASTSlots::SLOT2},
	{"|"sv ,Token::O_Or, Expr::RawOper, ASTSlots::SLOT2},
	{"^"sv ,Token::O_Xor, Expr::RawOper, ASTSlots::SLOT2},
	{"+"sv ,Token::O_Add, Expr::RawOper, ASTSlots::SLOT2},
	{"-"sv ,Token::O_Sub, Expr::RawOper, ASTSlots::SLOT2},
	{"*"sv ,Token::O_Mul, Expr::RawOper, ASTSlots::SLOT2},
	{"**"sv ,Token::O_Power, Expr::RawOper, ASTSlots::SLOT2},
	{"/"sv ,Token::O_Div, Expr::RawOper, ASTSlots::SLOT2},
	{"%"sv ,Token::O_Mod, Expr::RawOper, ASTSlots::SLOT2},
	{".*"sv ,Token::O_DotP, Expr::RawOper, ASTSlots::SLOT2, KWA::ObjIdent},
	{".+"sv ,Token::O_Cross, Expr::RawOper, ASTSlots::SLOT2, KWA::ObjIdent},
	{">>"sv ,Token::O_Rsh, Expr::RawOper, ASTSlots::SLOT2},
	{">>>"sv ,Token::O_RAsh, Expr::RawOper, ASTSlots::SLOT2},
	{"<<"sv ,Token::O_Lsh, Expr::RawOper, ASTSlots::SLOT2},
	{"&="sv ,Token::O_AndEq, Expr::RawOper, ASTSlots::SLOT2},
	{"|="sv ,Token::O_OrEq, Expr::RawOper, ASTSlots::SLOT2},
	{"^="sv ,Token::O_XorEq, Expr::RawOper, ASTSlots::SLOT2},
	{"+="sv ,Token::O_AddEq, Expr::RawOper, ASTSlots::SLOT2},
	{"-="sv ,Token::O_SubEq, Expr::RawOper, ASTSlots::SLOT2},
	{"*="sv ,Token::O_MulEq, Expr::RawOper, ASTSlots::SLOT2},
	{"**="sv ,Token::O_PowerEq, Expr::RawOper, ASTSlots::SLOT2},
	{"/="sv ,Token::O_DivEq, Expr::RawOper, ASTSlots::SLOT2},
	{"%="sv ,Token::O_ModEq, Expr::RawOper, ASTSlots::SLOT2},
	{".+="sv ,Token::O_CrossEq, Expr::RawOper, ASTSlots::SLOT2, KWA::ObjIdent},
	{">>="sv ,Token::O_RshEq, Expr::RawOper, ASTSlots::SLOT2},
	{">>>="sv ,Token::O_RAshEq, Expr::RawOper, ASTSlots::SLOT2},
	{"<<="sv ,Token::O_LshEq, Expr::RawOper, ASTSlots::SLOT2},
	{"/%"sv ,Token::O_DivMod, Expr::RawOper, ASTSlots::SLOT2},
	{"<=>"sv ,Token::O_Spaceship, Expr::RawOper, ASTSlots::SLOT2},
	/*
	{"-"sv ,Token::O_Minus,
	*/
	{"<"sv ,Token::O_Less, Expr::RawOper, ASTSlots::SLOT2},
	{"="sv ,Token::O_Assign, Expr::RawOper, ASTSlots::SLOT2, KWA::ObjAssign},
	{">"sv ,Token::O_Greater, Expr::RawOper, ASTSlots::SLOT2},
	{"<="sv ,Token::O_LessEq, Expr::RawOper, ASTSlots::SLOT2},
	{">="sv ,Token::O_GreaterEq, Expr::RawOper, ASTSlots::SLOT2},
	{"=="sv ,Token::O_EqEq, Expr::RawOper, ASTSlots::SLOT2},
	{"!="sv ,Token::O_NotEq, Expr::RawOper, ASTSlots::SLOT2},
	{"."sv ,Token::O_Dot, Expr::RawOper, ASTSlots::SLOT2},
	{"->"sv ,Token::Arrow, Expr::RawOper, ASTSlots::SLOT2, KWA::ObjAssign},
	/*
	{".."sv ,Token::O_Range,
	{"..="sv ,Token::RangeIncl,
	{"./%"sv ,Token::Ident,
	*/
};
struct Precedence {
	uint8_t prec;
	Token token;
};
Precedence precedence_table[] = {
	{0, Token::O_Assign},
	{1, Token::Comma},

	{2, Token::O_Less},
	{2, Token::O_Greater},
	{2, Token::O_LessEq},
	{2, Token::O_GreaterEq},
	{2, Token::O_EqEq},
	{2, Token::O_NotEq},

	{3, Token::O_AndEq},
	{3, Token::O_OrEq},
	{3, Token::O_XorEq},
	{3, Token::O_AddEq},
	{3, Token::O_SubEq},
	{3, Token::O_MulEq},
	{3, Token::O_PowerEq},
	{3, Token::O_DivEq},
	{3, Token::O_ModEq},
	{3, Token::O_CrossEq},
	{3, Token::O_RshEq},
	{3, Token::O_RAshEq},
	{3, Token::O_LshEq},

	{4, Token::O_Or},
	{5, Token::O_And},
	{6, Token::O_Xor},
	{7, Token::O_Add},
	{7, Token::O_Sub},

	{8, Token::O_Rsh},
	{8, Token::O_RAsh},
	{9, Token::O_Lsh},

	{10, Token::O_Mul},

	{11, Token::O_Div},
	{11, Token::O_Mod},
	{11, Token::O_DivMod},

	{12, Token::O_Power},
	{12, Token::O_DotP},
	{12, Token::O_Cross},

	{13, Token::O_Range},

	{14, Token::O_Minus},
	{14, Token::O_Not},
	{14, Token::O_Length},

	{15, Token::O_CallExpr},
	{16, Token::O_Dot},
	{17, Token::O_Decl},
};
constexpr auto precedence_table_span = std::span{precedence_table};
static constexpr uint8_t get_precedence(Token token) {
	auto found = std::ranges::find(precedence_table_span, token, &Precedence::token);
	if(found != precedence_table_span.end()) {
		return found->prec;
	}
	return 0;
}
constexpr auto lex_tree_span = std::span{lex_tree};
constexpr auto keyword_table_span = std::span{keyword_table};

std::string_view LexTokenRange::as_string() const {
	return std::string_view(this->tk_begin, this->tk_end);
}
std::string_view LexTokenRange::as_string(size_t offset) const {
	return std::string_view(this->tk_begin + offset, this->tk_end);
}

ASTNode::ASTNode(Token token, Expr a, LexTokenRange lex_range)
	: ast_token{token}, asc{a}, block{lex_range},
	whitespace{WS::NONE}, precedence{get_precedence(token)},
	slots{ASTSlots::NONE} {}
ASTNode::ASTNode(Token token, Expr a, ASTSlots e, LexTokenRange lex_range)
	: ast_token{token}, asc{a}, block{lex_range},
	whitespace{WS::NONE}, precedence{get_precedence(token)},
	slots{e} {}
std::ostream& operator<<(std::ostream &os, const ASTSlots v) {
	switch(v) {
	case ASTSlots::NONE: os << "0"; break;
	case ASTSlots::VAR: os << "0+"; break;
	case ASTSlots::SLOT1: os << "1"; break;
	case ASTSlots::SLOT1VAR: os << "1+"; break;
	case ASTSlots::SLOT2: os << "2"; break;
	case ASTSlots::SLOT2VAR: os << "2+"; break;
	default:
	os << "NONE"; break;
	}
	return os;
}
std::ostream& operator<<(std::ostream &os, const WS v) {
	switch(v) {
	case WS::SP: os << "SP"; break;
	case WS::NL: os << "NL"; break;
	default:
	case WS::NONE: os << "NONE"; break;
	}
	return os;
}
static void show_node_open(std::ostream &os, const ASTNode &node) {
	os << "(" << name_of(node.ast_token)
		<< " " << name_of(node.asc);
	if(node.whitespace == WS::NL) os << " NL";
	else if(node.whitespace == WS::SP) os << " SP";
	if(node.asc == Expr::RawOper
		|| node.asc == Expr::OperExpr
		|| node.asc == Expr::KeywExpr
		|| node.asc == Expr::TypeExpr
		|| node.asc == Expr::TypeDecl
		|| node.asc == Expr::BlockExpr) {
		os << " " << static_cast<uint32_t>(node.precedence);
	}
	uint8_t count = node.slot_count();
	if(node.open()) {
		os << "+";
		if(count == 1 && !node.slot1) os << "S1";
		else if(count == 2 && !node.slot2) os << "S2";
		else os << "E";
	}
	if(node.asc == Expr::Start || node.asc == Expr::TypeStart) {
		os << " \"" << std::string_view(node.block.tk_begin, node.block.tk_end) << '\"';
	}
}
static void show_node(std::ostream &os, const ASTNode &node, int depth) {
	show_node_open(os, node);
	if(node.slot1) {
		os << "\n";
		for(int i = 0; i <= depth; i++) os << "  ";
		os << "A:";
		if(node.slot1) show_node(os, *node.slot1, depth + 1);
		else os << "nullnode";
	}
	if(node.slot2) {
		os << "\n";
		for(int i = 0; i <= depth; i++) os << "  ";
		os << "B:";
		if(node.slot2) show_node(os, *node.slot2, depth + 1);
		else os << "nullnode";
	}
	for(auto &expr : node.list) {
		os << '\n';
		for(int i = 0; i <= depth; i++) os << "  ";
		if(expr) show_node(os, *expr, depth + 1);
		else os << "nullnode";
	}
	if(node.slot1 || node.slot2 || node.list.size() > 0) {
		os << '\n';
		for(int i = 1; i <= depth; i++) os << "  ";
	}
	os << ")";
}
struct DiffList {
	bool fixed;
	size_t pos;
	const ASTNode *node;
	struct DiffList *next;
};
void show_recurse_list(std::ostream &out, DiffList *root) {
	DiffList *list = root;
	DiffList *prev = nullptr;
	while(list != nullptr) {
		if(prev) out << "[" << prev->pos << "]";
		else out << "[*]";
		show_node_open(out, *list->node);
		out << "\n";
		prev = list;
		list = list->next;
	}
}
bool show_node_diff_recurse(std::ostream &out, const ASTNode &lhs, const ASTNode &rhs, DiffList *root, DiffList *prev) {
	DiffList list { true, 0, &lhs, nullptr };
	if(root == nullptr) {
		root = &list;
	} else {
		prev->next = &list;
	}
	//out << "Down:" << prev << ',' << &list << '\n';
	if(lhs.ast_token != rhs.ast_token
		|| lhs.asc != rhs.asc
		|| lhs.list.size() != rhs.list.size()
		|| lhs.slots != rhs.slots
		|| lhs.whitespace != rhs.whitespace) {

		out << '\n';
		show_recurse_list(out, root);
		out << "at:"; show_node_open(out, rhs); out << '\n';

		if(lhs.ast_token != rhs.ast_token)
			out << "non-equal token:" << lhs.ast_token << "!=" << rhs.ast_token << "\n";
		if(lhs.asc != rhs.asc)
			out << "non-equal asc:" << lhs.asc << "!=" << rhs.asc << "\n";
		if(lhs.list.size() != rhs.list.size())
			out << "non-equal size:" << lhs.list.size() << "!=" << rhs.list.size() << "\n";
		if(lhs.slots != rhs.slots)
			out << "non-equal slots: " << lhs.slots << "!=" << rhs.slots << "\n";
		if(lhs.whitespace != rhs.whitespace) {
			out << "non-equal WS:" << lhs.whitespace << "!=" << rhs.whitespace << "\n";
		}
		return false;
	}
	if((lhs.asc == Expr::RawOper
		|| lhs.asc == Expr::OperExpr
		|| lhs.asc == Expr::KeywExpr
		|| lhs.asc == Expr::TypeExpr
		|| lhs.asc == Expr::TypeDecl
		|| lhs.asc == Expr::BlockExpr)
		&& (
		lhs.precedence != rhs.precedence
	)) {
		show_recurse_list(out, root);
		out << "at:"; show_node_open(out, rhs); out << '\n';
		if(lhs.precedence != rhs.precedence)
			out << "non-equal precedence: " << uint32_t{lhs.precedence} << "!=" << uint32_t{rhs.precedence} << "\n";
		return false;
	}
	if(
		(lhs.asc == Expr::Start || lhs.asc == Expr::TypeStart)
		&& lhs.ast_token == Token::Ident
		&& lhs.block.as_string() != rhs.block.as_string()
	) {
		show_recurse_list(out, root);
		out << "at:"; show_node_open(out, rhs); out << '\n';
		out << "string not equal\n";
		return false;
	}
	uint8_t count = static_cast<uint8_t>(lhs.slots) >> 1;
	list.pos = 0;
	if(count > 0 && (lhs.slot1 && !rhs.slot1) && (!lhs.slot1 && rhs.slot1)) {
		show_recurse_list(out, root);
		out << "at:"; show_node_open(out, rhs); out << '\n';
		if(!lhs.slot1) out << "< slot1 is nullptr\n";
		else {
			out << "< "; show_node_open(out, *lhs.slot1);
		}
		if(!rhs.slot1) out << "> slot1 is nullptr\n";
		else {
			out << "> "; show_node_open(out, *rhs.slot1);
		}
		return false;
	}
	if(count > 0 && lhs.slot1 && rhs.slot1 && !show_node_diff_recurse(out, *lhs.slot1, *rhs.slot1, root, &list)) {
		return false;
	}
	list.pos = 1;
	if(count > 1 && (lhs.slot2 && !rhs.slot2) && (!lhs.slot2 && rhs.slot2)) {
		show_recurse_list(out, root);
		out << "at:"; show_node_open(out, rhs); out << '\n';
		if(!lhs.slot2) out << "< slot2 is nullptr\n";
		else {
			out << "< "; show_node_open(out, *lhs.slot2);
		}
		if(!rhs.slot2) out << "> slot2 is nullptr\n";
		else {
			out << "> "; show_node_open(out, *rhs.slot2);
		}
		return false;
	}
	if(count > 1 && lhs.slot2 && rhs.slot2 && !show_node_diff_recurse(out, *lhs.slot2, *rhs.slot2, root, &list)) {
		return false;
	}
	list.fixed = false;
	auto left = lhs.list.cbegin();
	auto right = rhs.list.cbegin();
	while(left != lhs.list.cend()) {
		list.pos = left - lhs.list.cbegin();
		if(!show_node_diff_recurse(out, **left, **right, root, &list)) {
			return false;
		}
		left++; right++;
	}
	if(prev) prev->next = nullptr;
	return true;
}
bool show_node_diff(std::ostream &out, const ASTNode &lhs, const ASTNode &rhs) {
	return show_node_diff_recurse(out, lhs, rhs, nullptr, nullptr);
}

struct ExprStackItem {
	ASTNode *block;
	expr_list_t &expr_list;
	bool obj_ident;
	ExprStackItem(ASTNode *n, expr_list_t &e)
		: block{n}, expr_list{e}, obj_ident{false} {}
};

 module_source_ptr load_syntax_tree(string &&source) {
	auto module = std::make_unique<ModuleSource>();
	module->source = std::move(source);
	auto cursor = module->source.cbegin();
	auto source_end = module->source.cend();
	node_ptr *current_ptr = &module->root_tree;
	int state = 0;
	std::vector<node_ptr *> stack;
	Token current_token = Token::Invalid;
	Expr current_expr = Expr::End;
	WS current_ws = WS::NONE;
	ASTSlots meta = ASTSlots::NONE;
	uint8_t precedence = 0;
	bool error_tokens = false;
	std::string::const_iterator token_begin;
	// "(" token_name expr_name [WS::*] [prec]["+"] ["source string"]
	//     [[slotname":"] inner_expr ["," inner_expr]...] ")"
	auto begin_tree_node = [&]() {
		auto &up_node = *current_ptr;
		if(up_node) {
			// add an empty slot to insert into
			uint8_t count = up_node->slot_count();
			stack.push_back(current_ptr);
			if(count > 0 && !up_node->slot1) {
				current_ptr = &up_node->slot1;
			} else if(count > 1 && !up_node->slot2) {
				current_ptr = &up_node->slot2;
			} else {
				up_node->list.emplace_back(node_ptr{});
				current_ptr = &up_node->list.back();
			}
		}
		auto &node = *current_ptr;
		node = std::make_unique<ASTNode>(
			current_token, current_expr, meta, LexTokenRange{token_begin, cursor, current_token});
		node->precedence = precedence;
		node->slots = meta;
		node->whitespace = current_ws;
	};
	auto end_tree_node = [&]() {
		auto &node = *current_ptr;
		if(stack.empty()) {
			current_ptr = &module->root_tree;
			return;
		}
		current_ptr = stack.back();
		stack.pop_back();
		state = 11;
	};
	auto init_expr = [&]() {
		state = 1;
		current_ws = WS::NONE;
		meta = ASTSlots::NONE;
		precedence = 0;
	};
	for(;cursor != source_end; cursor++) {

		char c = *cursor;
		switch(state) {
		case 0: // initial
			if(c == '(') { init_expr(); }
			break;
		case 1:
			if(isalpha(c) || c == '_') { token_begin = cursor; state = 2; }
			break;
		case 2:
			if(c == ' ') {
				auto tk = std::string_view(token_begin, cursor);
				auto found = std::ranges::find(Token_table_span, tk);
				if(found != Token_table_span.end()) {
					current_token = static_cast<Token>(found - Token_table_span.begin());
				} else {
					current_token = Token::Invalid;
				}
				state = 3;
			}
			break;
		case 3:
			if(isalpha(c) || c == '_') { token_begin = cursor; state = 4; }
			break;
		case 4: // Expr
			if(!isalpha(c)) {
				auto tk = std::string_view(token_begin, cursor);
				auto found = std::ranges::find(Expr_table_span, tk);
				if(found != Expr_table_span.end()) {
					current_expr = static_cast<Expr>(found - Expr_table_span.begin());
				} else {
					current_expr = Expr::Undefined;
				}
				state = 5;
				if(c == ')') {
					precedence = get_precedence(current_token);
					begin_tree_node();
					end_tree_node();
					state = 11;
				}
			}
			break;
		case 5: // after Expr: prec or WS or string or slot
			if(c == '"') {
				token_begin = cursor + 1;
				state = 10; // string
			} else if(c == ',' || c == '/') {
				precedence = get_precedence(current_token);
				begin_tree_node();
				error_tokens = true;
				state = 11;
			} else if(c == ':') {
				precedence = get_precedence(current_token);
				meta = ASTSlots::SLOT1;
				begin_tree_node();
				state = 11;
			} else if(c == '(') {
				precedence = get_precedence(current_token);
				begin_tree_node();
				init_expr();
			} else if(c == '+') {
				precedence = get_precedence(current_token);
				meta = ASTNode::make_open(meta);
				begin_tree_node();
				state = 11;
			} else if(isdigit(c)) {
				precedence = c - '0';
				state = 7;
			} else if(isalpha(c)) {
				token_begin = cursor;
				state = 6;
			}
			break;
		case 6: // WS string
			if(!isalpha(c)) {
				auto tk = std::string_view(token_begin, cursor);
				if(tk == "SP") current_ws = WS::SP;
				if(tk == "NL") current_ws = WS::NL;
				state = 7;
				if(c == ')') {
					precedence = get_precedence(current_token);
					begin_tree_node();
					end_tree_node();
				}
			}
			break;
		case 7: // parameters (precedence ["+"]) or (auto precedence ["+"]) or slot or var_slot
			if(isdigit(c)) {
				state = 8;
			}
			// fallthrough
		case 8: // precedence continued
			if(c == '"') {
				token_begin = cursor + 1;
				state = 10; // string
			} else if(c == ',' || c == '/') {
				if(state == 7) precedence = get_precedence(current_token);
				begin_tree_node();
				error_tokens = true;
				state = 11;
			} else if(c == '+') {
				if(state == 7) precedence = get_precedence(current_token);
				meta = ASTNode::make_open(meta);
				begin_tree_node();
				state = 11;
			} else if(c == ':') {
				if(state == 7) precedence = get_precedence(current_token);
				meta = ASTSlots::SLOT1;
				begin_tree_node();
				state = 11;
			} else if(isdigit(c)) {
				precedence *= 10;
				precedence += c - '0';
			} else if(c == ')') {
				if(state == 7) precedence = get_precedence(current_token);
				begin_tree_node();
				end_tree_node();
			} else if(c == '(') {
				if(state == 7) precedence = get_precedence(current_token);
				begin_tree_node();
				init_expr();
			}
			break;
		case 10: // string
			if(c == '"') {
				begin_tree_node();
				state = 11;
			}
			break;
		case 11:
			if(c == '(') {
				// sub-expression
				init_expr();
			} else if(c == ':') {
				meta = (*current_ptr)->slots;
				if(meta == ASTSlots::VAR) meta = ASTSlots::SLOT1VAR;
				else if(meta == ASTSlots::SLOT1VAR) meta = ASTSlots::SLOT2VAR;
				if(meta == ASTSlots::NONE) meta = ASTSlots::SLOT1;
				else if(meta == ASTSlots::SLOT1) meta = ASTSlots::SLOT2;
				(*current_ptr)->slots = meta;
			} else if(c == ')') {
				// end of expression
				end_tree_node();
			}
			break;
		default:
			state = 0;
			break;
		}
	}
	if(error_tokens)
		std::cerr << "invalid tokens while loading syntax tree\n";
	return std::move(module);
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
node_ptr POP_EXPR(node_ptr &node) {
	uint8_t count = node->slot_count();
	if(count >= 2 && node->slot2) {
		if(node->slot1) node->whitespace = node->slot1->whitespace;
		return std::move(node->slot2);
	}
	if(count >= 1 && node->slot1) {
		return std::move(node->slot1);
	}
	if(node->list.empty()) return node_ptr{};
	auto b = std::move(node->list.back());
	node->list.pop_back();
	return std::move(b);
}
void push_into(node_ptr &node, node_ptr &&v) {
	if(node->block.tk_begin > v->block.tk_begin) {
		node->block.tk_begin = v->block.tk_begin;
	}
	if(node->block.tk_end < v->block.tk_end) {
		node->block.tk_end = v->block.tk_end;
	}
	node->whitespace = v->whitespace;
	uint8_t count = static_cast<uint8_t>(node->slots) >> 1;
	if(count > 0 && !node->slot1) node->slot1 = std::move(v);
	else if(count > 1 && !node->slot2) node->slot2 = std::move(v);
	else {
		node->mark_closed();
		node->list.emplace_back(std::move(v));
	}
}
void push_into_open(node_ptr &node, node_ptr &&v) {
	if(node->block.tk_begin > v->block.tk_begin) {
		node->block.tk_begin = v->block.tk_begin;
	}
	if(node->block.tk_end < v->block.tk_end) {
		node->block.tk_end = v->block.tk_end;
	}
	node->whitespace = v->whitespace;
	uint8_t count = static_cast<uint8_t>(node->slots) >> 1;
	if(count > 0 && !node->slot1) node->slot1 = std::move(v);
	else if(count > 1 && !node->slot2) node->slot2 = std::move(v);
	else {
		node->list.emplace_back(std::move(v));
	}
}
#define PUSH_INTO(d, s) push_into(d, std::move(s))
#define PUSH_INTO_OPEN(d, s) push_into_open(d, std::move(s))
static constexpr void convert_expr(auto &n, Expr t, ASTSlots num) noexcept {
	n->asc = t; n->slots = num;
}
#define CONV_N(n, t, num) convert_expr(n, Expr::t, ASTSlots::num)
#define CONV(n, t) n->asc = Expr::t
static constexpr void convert_token(auto &n, Token t) noexcept {
	n->ast_token = t;
	n->precedence = get_precedence(t);
}
#define CONV_TK(n, t) convert_token(n, Token::t)
#define EXTEND_TO(t, sl, sr) t->block.tk_begin = sl->block.tk_begin; \
	t->block.tk_end = sr->block.tk_end; \
	t->whitespace = sr->whitespace;
#define AT_EXPR_TARGET(id) !(id)->open()
#define IS_TOKEN(n, t) ((n)->ast_token == Token::t)
#define IS_CLASS(n, c) ((n)->asc == Expr::c)
#define IS_OPENTYPEEXPR(n) (((n)->asc == Expr::TypeExpr) && !AT_EXPR_TARGET(n))
#define IS_FULLTYPEEXPR(n) (((n)->asc == Expr::TypeExpr) && AT_EXPR_TARGET(n))
#define IS_FULLTYPEDECL(n) (((n)->asc == Expr::TypeDecl) && AT_EXPR_TARGET(n))
#define IS_OPENEXPR(n) (((n)->asc == Expr::OperExpr) && !AT_EXPR_TARGET(n))
#define IS_FULLEXPR(n) (((n)->asc == Expr::OperExpr) && AT_EXPR_TARGET(n))
#define IS2C(c1, c2) ((prev->asc == Expr::c1) && (head->asc == Expr::c2))
#define MAKE_NODE_FROM1(n, t, cl, s) n = std::make_unique<ASTNode>( \
Token::t, Expr::cl, LexTokenRange{s->block.tk_begin, s->block.tk_end, Token::t});
#define MAKE_NODE_N_FROM2(n, t, cl, num, s1, s2) n = std::make_unique<ASTNode>( \
Token::t, Expr::cl, ASTSlots::num, LexTokenRange{s1->block.tk_begin, s2->block.tk_end, Token::t});
#define MAKE_NODE_N_FROM1(n, t, cl, num, s) n = std::make_unique<ASTNode>( \
Token::t, Expr::cl, ASTSlots::num, LexTokenRange{s->block.tk_begin, s->block.tk_end, Token::t});

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
struct VMVar {
	virtual VarType get_type() const { return VarType::Unset; }
};
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
		<< ":";
	if(v.vtype == VarType::Function) {
		os << v.value;
	} else if(v.vtype == VarType::Bool) {
		os << (v.value == 0 ? 'F' : 'T');
	} else {
		os << static_cast<int64_t>(v.value);
	}
	os << "}";
	return os;
}

#define DEF_OPCODES(f) \
	f(LoadUnit) f(LoadConst) f(LoadBool) f(LoadString) f(LoadClosure) \
	f(LoadVariable) f(StoreVariable) f(LoadArg) f(StoreArg) \
	f(LoadLocal) f(StoreLocal) f(LoadStack) f(StoreStack) \
	f(NamedArgLookup) f(NamedLookup) f(AssignNamed) \
	f(PushRegister) f(PopRegister) f(PopStack) \
	f(CallExpression) f(ExitScope) f(ExitFunction) \
	f(Jump) f(JumpIf) f(JumpElse) \
	f(AddInteger) f(SubInteger) f(MulInteger) f(DivInteger) f(ModInteger) f(PowInteger) \
	f(Negate) f(Not) \
	f(AndInteger) f(OrInteger) f(XorInteger) \
	f(LSHInteger) f(RSHLInteger) f(RSHAInteger) \
	f(CompareLess) f(CompareGreater) f(CompareEqual) f(CompareNotEqual) \
	f(CompareLessEqual) f(CompareGreaterEqual) f(CompareSpaceship)
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
	os << "I(" << v.opcode << " " << v.param_a << "," << v.param_b << ")";
	return os;
}

struct FrameContext;

struct VariableDeclaration {
	size_t name_index;
	bool is_closed;
	bool is_arg;
	bool is_mut;
	VariableDeclaration(size_t n, bool arg) :
		name_index{n}, is_closed{false}, is_arg{arg}, is_mut{false} {}
	VariableDeclaration(size_t n, bool arg, bool m) :
		name_index{n}, is_closed{false}, is_arg{arg}, is_mut{m} {}
};
std::ostream &operator<<(std::ostream &os, const VariableDeclaration &v) {
	os << "Decl{" << (v.is_mut ? "mut " : "") << v.name_index << "}";
	return os;
}

const auto string_table_empty_string = ""sv;
struct LoopContext {
	size_t loop_pos;
	std::vector<size_t> exit_points;
	LoopContext(size_t p) : loop_pos{p} {}
};
struct FrameScopeContext {
	size_t depth;
	std::unique_ptr<LoopContext> loop;
};
struct FrameContext {
	std::shared_ptr<FrameContext> up;
	size_t frame_index;
	size_t current_closed;
	size_t current_depth;
	size_t current_var;
	std::vector<VariableDeclaration> closed_declarations;
	std::vector<VariableDeclaration> var_declarations;
	std::vector<VariableDeclaration> arg_declarations;
	std::vector<FrameScopeContext> scopes;
	std::vector<Instruction> instructions;
	FrameContext(size_t scope_id) : frame_index{scope_id}, current_closed{0}, current_depth{0}, current_var{0} {}
	FrameContext(size_t scope_id, std::shared_ptr<FrameContext> &up_ptr)
		: frame_index{scope_id}, up{up_ptr}, current_closed{0}, current_depth{0}, current_var{0} {}
};
typedef std::shared_ptr<FrameContext> framectx_ptr;
struct VariableExtern {
	string var_name;
	uint32_t pos;
};
struct ModuleContext {
	ModuleSource source;
	std::vector<size_t> line_positions;
	std::vector<std::unique_ptr<VariableExtern>> imports;
	std::vector<std::string_view> string_table;
	std::shared_ptr<FrameContext> root_context;
	std::vector<std::shared_ptr<FrameContext>> frames;
	ModuleContext(std::string &&source) : source{ModuleSource{std::move(source)}} {
		this->find_or_put_string(string_table_empty_string);
		this->root_context = std::make_shared<FrameContext>(FrameContext{0});
		this->frames.push_back(this->root_context);
	}
	size_t find_or_put_string(std::string_view s) {
		auto found = std::ranges::find(this->string_table, s);
		if(found != this->string_table.cend()) {
			return found - this->string_table.cbegin();
		}
		this->string_table.push_back(s);
		return this->string_table.size() - 1; // newly inserted string
	}
	void add_import(string import_name) {
		imports.emplace_back(std::make_unique<VariableExtern>(VariableExtern{import_name, 0}));
		auto import_ptr = imports.back().get();
		auto string_index = find_or_put_string(string_view(import_ptr->var_name));
		auto found = std::ranges::find_if(root_context->closed_declarations,
			[&](const auto &v) { return !v.is_arg && v.name_index == string_index; });
		if(found == root_context->closed_declarations.cend()) {
			root_context->closed_declarations.emplace_back(
				VariableDeclaration{ string_index, false, false });
		}
	}
};

struct devnull {};
template<typename T>
constexpr const devnull& operator<<(const devnull &n, const T &t) {
	return n;
}

#if defined(DEBUG_LEXPARSE) && (DEBUG_LEXPARSE) != 0
#define DEBUG_LEX
#define DEBUG_PARSE
#endif
#if defined(_DEBUG) && defined(DEBUG_LEX)
#define _DL(s) s
#else
#define _DL(s) devnull{}
#endif
#if defined(_DEBUG) && defined(DEBUG_PARSE)
#define _DP(s) s
#else
#define _DP(s) devnull{}
#endif
#if defined(_DEBUG) && defined(DEBUG_WALK)
#define _DW(s) s
#else
#define _DW(s) devnull{}
#endif

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

struct variable_pos {
	uint32_t up_count = 0;
	uint32_t decl_index = 0;
	VariableDeclaration &decl_ref;
};
struct WalkContext {
	std::ostream &dbg;
	std::ostream &err;
	ModuleContext &module_ctx;
	uint32_t current_frame_index;
	bool pass2;
	WalkContext(std::ostream &debug_stream, std::ostream &error_stream, ModuleContext &module)
		: dbg{debug_stream}, err{error_stream}, module_ctx{module},
			pass2{false}, current_frame_index{1}
	{}
	void pass_reset() {
		current_frame_index = 1;
		for(auto &frame : module_ctx.frames) {
			frame->current_var = 0;
		}
	}
	bool pass1() {
		return !pass2;
	}
	void show_position(std::ostream &out, const LexTokenRange &block) {
		auto file_start = module_ctx.source.source.cbegin();
		size_t offset = block.tk_begin - file_start;
		auto found = std::ranges::lower_bound(module_ctx.line_positions, offset);
		if(found == module_ctx.line_positions.cend()) {
			_DW(out) << "char:" << offset;
			return;
		}
		auto lines_start = module_ctx.line_positions.cbegin();
		size_t prev_pos = 0;
		if(found != lines_start) prev_pos = *(found - 1);
		size_t line = (found - lines_start) + 1;
		_DW(out) << line << ":" << (offset - prev_pos) << ": ";
	}
	void show_syn_error(const std::string_view what, const auto &node) {
		_DW(err) << '\n' << what << " syntax error: ";
		show_position(err, node->block);
		_DW(err) << node->block.as_string() << "\n";
	}
	auto get_var_ref(std::shared_ptr<FrameContext> &ctx, const size_t string_index) {
		uint32_t up_count = 0;
		uint32_t decl_index = 0;
		auto search_context = ctx.get();
		while(search_context != nullptr) {
			auto found = std::ranges::find(search_context->var_declarations, string_index, &VariableDeclaration::name_index);
			if(up_count > 0 && pass1() && found != search_context->var_declarations.cend()) {
				(*found).is_closed = true;
				search_context->closed_declarations.push_back(*found);
				search_context->var_declarations.erase(found);
			} else if(found != search_context->var_declarations.cend()) {
				// found variable case
				decl_index = static_cast<uint32_t>(found - search_context->var_declarations.cbegin());
				return std::optional(variable_pos{up_count, decl_index, *found});
			}
			found = std::ranges::find_if(search_context->closed_declarations, [&](const auto &v) { return !v.is_arg && v.name_index == string_index; });
			if(found != search_context->closed_declarations.cend()) {
				// found closed variable case
				decl_index = static_cast<uint32_t>(found - search_context->closed_declarations.cbegin());
				return std::optional(variable_pos{up_count, decl_index, *found});
			}
			search_context = search_context->up.get();
			up_count++;
		}
		_DW(err) << "variable not found: " << module_ctx.string_table[string_index] << "\n";
		return std::optional<variable_pos>();
	};
	auto get_arg_ref(std::shared_ptr<FrameContext> &ctx, const size_t string_index) {
		uint32_t up_count = 0;
		uint32_t decl_index = 0;
		auto search_context = ctx.get();
		while(search_context != nullptr) {
			_DW(dbg) << "Func: " << search_context->frame_index << '\n';
			for(auto &a : search_context->arg_declarations) {
				_DW(dbg) << "Arg: " << a << '\n';
			}
			auto found = std::ranges::find(search_context->arg_declarations, string_index, &VariableDeclaration::name_index);
			if(found != search_context->arg_declarations.cend() && !(*found).is_closed) {
				// found argument case
				if(up_count > 0 && pass1()) {
					found->is_closed = true;
					search_context->closed_declarations.push_back(*found);
					return std::optional(variable_pos{up_count, 0, *found});
				} else {
					decl_index = static_cast<uint32_t>(found - search_context->arg_declarations.cbegin());
					return std::optional(variable_pos{up_count, decl_index, *found});
				}
			}
			found = std::ranges::find_if(search_context->closed_declarations, [&](const auto &v) { return v.is_arg && v.name_index == string_index; });
			if(found != search_context->closed_declarations.cend()) {
				// found closed argument case
				decl_index = static_cast<uint32_t>(found - search_context->closed_declarations.cbegin());
				return std::optional(variable_pos{up_count, decl_index, *found});
			}
			search_context = search_context->up.get();
			up_count++;
		}
		_DW(err) << "argument not found: " << module_ctx.string_table[string_index] << "\n";
		return std::optional<variable_pos>();
	};
};
bool walk_expression(WalkContext &walk, std::shared_ptr<FrameContext> &ctx, const node_ptr &expr) {
	auto str_table = [&](size_t string_index) {
		return walk.module_ctx.string_table[string_index];
	};
	auto begin_scope = [&]() {
		ctx->scopes.push_back({ctx->current_depth});
	};
	auto begin_loop_scope = [&](size_t ins_pos_reloop) {
		ctx->scopes.push_back({
			ctx->current_depth,
			std::make_unique<LoopContext>( ins_pos_reloop )
		});
	};
	auto ins = [&](Instruction &&i) {
		if(walk.pass2) ctx->instructions.emplace_back(std::move(i));
	};
	auto end_scope = [&]() {
		if(ctx->scopes.empty()) return;
		if(walk.pass2) {
			auto &scope = ctx->scopes.back();
			if(scope.loop) {
				size_t end_pos = ctx->instructions.size();
				for(auto point : scope.loop->exit_points) {
					ctx->instructions[point].param = end_pos;
				}
			}
			if(scope.depth != ctx->current_depth)
				ins(Instruction{Opcode::ExitScope, scope.depth});
		}
		ctx->scopes.pop_back();
	};
	auto leave_to_scope = [&](auto scope_itr) {
		if(!walk.pass2) return;
		auto end_itr = ctx->scopes.end();
		auto search_itr = end_itr;
		size_t leave_depth = scope_itr->depth;
		while(search_itr != scope_itr) {
			search_itr--;
			if(search_itr->depth != leave_depth) {
				ins(Instruction{Opcode::ExitScope, search_itr->depth});
			}
		}
	};
	auto generate_oper_expr = [&](const node_ptr &ex) -> bool {
		switch(ex->ast_token) {
		case Token::O_Add:
			ins(Instruction{Opcode::AddInteger, 0});
			break;
		case Token::O_Sub:
			ins(Instruction{Opcode::SubInteger, 0});
			break;
		case Token::O_Mul:
			ins(Instruction{Opcode::MulInteger, 0});
			break;
		case Token::O_Div:
			ins(Instruction{Opcode::DivInteger, 0});
			break;
		case Token::O_Mod:
			ins(Instruction{Opcode::ModInteger, 0});
			break;
		case Token::O_Power:
			ins(Instruction{Opcode::PowInteger, 0});
			break;
		case Token::O_And:
			ins(Instruction{Opcode::AndInteger, 0});
			break;
		case Token::O_Or:
			ins(Instruction{Opcode::OrInteger, 0});
			break;
		case Token::O_Xor:
			ins(Instruction{Opcode::XorInteger, 0});
			break;
		case Token::O_RAsh:
			ins(Instruction{Opcode::RSHAInteger, 0});
			break;
		case Token::O_Rsh:
			ins(Instruction{Opcode::RSHLInteger, 0});
			break;
		case Token::O_Lsh:
			ins(Instruction{Opcode::LSHInteger, 0});
			break;
		case Token::O_Minus:
			ins(Instruction{Opcode::Negate, 0});
			break;
		case Token::O_Not:
			ins(Instruction{Opcode::Not, 0});
			break;
		case Token::O_Less:
			ins(Instruction{Opcode::CompareLess, 0});
			break;
		case Token::O_Greater:
			ins(Instruction{Opcode::CompareGreater, 0});
			break;
		case Token::O_EqEq:
			ins(Instruction{Opcode::CompareEqual, 0});
			break;
		case Token::O_NotEq:
			ins(Instruction{Opcode::CompareNotEqual, 0});
			break;
		case Token::O_LessEq:
			ins(Instruction{Opcode::CompareLessEqual, 0});
			break;
		case Token::O_GreaterEq:
			ins(Instruction{Opcode::CompareGreaterEqual, 0});
			break;
		case Token::O_Spaceship:
			ins(Instruction{Opcode::CompareSpaceship, 0});
			break;
		default:
			_DW(walk.err) << "unhandled operator: " << expr->ast_token << "\n";
			return false;
		}
		return true;
	};
	_DW(walk.dbg) << "Expr:" << expr->ast_token << " ";
	switch(expr->asc) {
	case Expr::BlockExpr: {
		if(IS_TOKEN(expr, Object) || IS_TOKEN(expr, Array)) {
			_DW(walk.err) << "unhandled block type: " << expr->ast_token << '\n';
			return false;
		}
		begin_scope();
		for(auto &walk_node : expr->list) {
			if(!walk_expression(walk, ctx, walk_node))
				return false;
		}
		end_scope();
		//for(auto &ins : block_context->instructions) _DW(dbg) << ins << '\n';
		if(IS_TOKEN(expr, Block)) {
			_DW(walk.dbg) << "block end\n";
			break;
		} else if(IS_TOKEN(expr, DotArray)) {
			_DW(walk.dbg) << "argument lookup end\n";
			ins(Instruction{Opcode::NamedArgLookup});
			break;
		}
		_DW(walk.err) << "unhandled block type: " << expr->ast_token << '\n';
		return false;
	}
	case Expr::KeywExpr:
		if(expr->open()) {
			walk.show_syn_error("Keyword expression", expr);
			return false;
		}
		switch(expr->ast_token) {
		case Token::K_Mut:
		case Token::K_Let: {
			auto item = expr->slot1.get();
			bool is_mutable = IS_TOKEN(expr, K_Mut);
			if(IS_TOKEN(item, K_Mut) && !item->empty()) {
				item = item->slot1.get();
				is_mutable = true;
			}
			std::string_view id;
			if(IS_TOKEN(item, O_Decl) && item->slot1 && IS_TOKEN(item->slot1, Ident)) {
				id = item->slot1->block.as_string();
			} else if(IS_TOKEN(item, Ident)) {
				id = item->block.as_string();
			} else {
				walk.show_syn_error("Let assignment expression", item);
				return false;
			}
			size_t string_index = walk.module_ctx.find_or_put_string(id);
			if(expr->slot2) {
				if(!walk_expression(walk, ctx, expr->slot2)) return false;
			}
			if(walk.pass1()) {
				ctx->var_declarations.emplace_back(VariableDeclaration{ string_index, false, is_mutable });
				ctx->current_var++;
				ctx->current_depth++;
			} else {
				auto var_ref = walk.get_var_ref(ctx, string_index);
				if(!var_ref.has_value()) return false;
				if(!var_ref->decl_ref.is_closed) {
					ctx->current_var++;
					ctx->current_depth++;
				}
				if(expr->slot2) {
					// assign the value
					_DW(walk.err) << "let decl " << var_ref->decl_ref << " " << str_table(string_index) << " = \n";
					ins(Instruction{var_ref->decl_ref.is_closed ? Opcode::StoreVariable : Opcode::StoreLocal, var_ref->up_count, var_ref->decl_index});
				} else {
					_DW(walk.err) << "TODO let fwd decl " << "\n";
				}
			}
			return true;
		}
		case Token::K_If: {
			if(!walk_expression(walk, ctx, expr->slot1)) return false;
			size_t skip_pos = ctx->instructions.size();
			ins(Instruction{Opcode::JumpElse, 0});
			if(!walk_expression(walk, ctx, expr->slot2)) return false;
			std::vector<size_t> exit_positions;
			bool hanging_elseif = false;
			auto walk_expr = expr->list.cbegin();
			if(walk_expr == expr->list.cend()) { // no else or elseif
				if(walk.pass2) ctx->instructions[skip_pos].param = ctx->instructions.size();
				//ins(Instruction{Opcode::LoadUnit, 0});
			} else while(walk_expr != expr->list.cend()) {
				if(IS_TOKEN((*walk_expr), K_ElseIf)) {
					if((*walk_expr)->open()) {
						walk.show_syn_error("ElseIf", *walk_expr);
						return false;
					}
					if(walk.pass2) {
						// cause the previous "if" to exit
						exit_positions.push_back(ctx->instructions.size());
						ins(Instruction{Opcode::Jump, 0});
						// fixup the previous test's "else" jump
						ctx->instructions[skip_pos].param = ctx->instructions.size();
					}
					// test expression
					if(!walk_expression(walk, ctx, (*walk_expr)->slot1)) return false;
					skip_pos = ctx->instructions.size();
					hanging_elseif = true; // "else" jump exits if no more branches
					ins(Instruction{Opcode::JumpElse, 0});
					if(!walk_expression(walk, ctx, (*walk_expr)->slot2)) return false;
				} else if(IS_TOKEN((*walk_expr), K_Else)) {
					if((*walk_expr)->open()) {
						walk.show_syn_error("Else", *walk_expr);
						return false;
					}
					hanging_elseif = false;
					if(walk.pass2) {
						// cause the previous "if" to exit
						exit_positions.push_back(ctx->instructions.size());
						ins(Instruction{Opcode::Jump, 0});
						// fixup the previous test's "else" jump
						ctx->instructions[skip_pos].param = ctx->instructions.size();
					}
					if(!walk_expression(walk, ctx, (*walk_expr)->slot1)) return false;
					break;
				} else {
					walk.show_syn_error("If-Else", *walk_expr);
					return false;
				}
				walk_expr++;
			}
			if(walk.pass2) {
				// fixup the exit points
				size_t exit_point = ctx->instructions.size();
				if(hanging_elseif) ctx->instructions[skip_pos].param = exit_point;
				for(auto pos : exit_positions) {
					ctx->instructions[pos].param = exit_point;
				}
			}
			return true;
		}
		case Token::K_End: {
			if(!walk_expression(walk, ctx, expr->slot1)) return false;
			ins(Instruction{Opcode::ExitFunction, 0});
			return true;
		}
		case Token::K_Loop: {
			auto &inner_expr = expr->slot1;
			size_t loop_point = ctx->instructions.size();
			begin_loop_scope(loop_point);
			if(!walk_expression(walk, ctx, inner_expr)) return false;
			ins(Instruction{Opcode::Jump, loop_point});
			end_scope();
			return true;
		}
		case Token::K_Break: {
			auto found = find_last_if(ctx->scopes, [](const auto &e) { return e.loop || false; });
			if(found == ctx->scopes.cend()) {
				walk.show_syn_error("Break without Loop", expr);
				return false;
			}
			if(!walk_expression(walk, ctx, expr->slot1)) return false;
			leave_to_scope(found);
			if(walk.pass2) {
				found->loop->exit_points.push_back(ctx->instructions.size());
				ins(Instruction{Opcode::Jump, 0});
			}
			return true;
		}
		case Token::K_Continue: {
			auto found = find_last_if(ctx->scopes, [](const auto &e) -> bool {
				return e.loop || false;
			});
			if(found == ctx->scopes.cend()) {
				walk.show_syn_error("Continue without Loop", expr);
				return false;
			}
			leave_to_scope(found);
			ins(Instruction{Opcode::Jump, found->loop->loop_pos });
			return true;
		}
		case Token::K_Until:
		case Token::K_While: {
			size_t loop_point = ctx->instructions.size();
			begin_loop_scope(loop_point);
			if(!walk_expression(walk, ctx, expr->slot1)) return false;
			size_t jump_ins = ctx->instructions.size();
			if(IS_TOKEN(expr, K_While)) // don't jump to exit
				ins(Instruction{Opcode::JumpElse, 0});
			else ins(Instruction{Opcode::JumpIf, 0});
			if(!walk_expression(walk, ctx, expr->slot2)) return false;
			if(walk.pass2) {
				_DW(walk.dbg) << "end " << expr->ast_token
					<< " jump_pos=" << ctx->instructions.size()
					<< " loop_point=" << loop_point << '\n';
				ins(Instruction{Opcode::Jump, loop_point});
				ctx->instructions[jump_ins].param = ctx->instructions.size();
			}
			end_scope();
			// for(auto &ins : ctx->instructions) _DW(walk.dbg) << ins << '\n';
			return true;
		}
		default:
			_DW(walk.err) << "unhandled keyword\n";
			return false;
		}
		return true;
	case Expr::TypeDecl:
		_DW(walk.err) << "Unhandled type " << expr;
		return true;
	case Expr::FuncDecl:
	case Expr::RawOper:
		walk.show_syn_error("Expression", expr);
		return false;
	case Expr::OperExpr: {
		if(!AT_EXPR_TARGET(expr)) {
			walk.show_syn_error("Expression", expr);
			return false;
		}
		if(IS_TOKEN(expr, O_CallExpr)) { // function calls
			if(expr->open()) {
				walk.show_syn_error("Function call", expr);
				return false;
			}
			_DW(walk.dbg) << "reference: ";
			if(!walk_expression(walk, ctx, expr->slot1)) return false;
			_DW(walk.dbg) << "\ncall with: ";
			// arguments
			auto &args = expr->slot2;
			bool func_save = false;
			size_t arg_count = 0;
			if(IS_TOKEN(args, Block) && args->list.empty()) {
				// we have no arguments at all
				// don't have to do anything
			} else if(IS_TOKEN(args, Block) && (args->list.size() == 1)) {
				func_save = true;
				auto &arg1 = args->list.front();
				ins(Instruction{Opcode::PushRegister, 0});
				arg_count = 1;
				if(IS_CLASS(arg1, OperExpr) && IS_TOKEN(arg1, Comma)) {
					_DW(walk.dbg) << "comma-sep: ";
					for(auto &comma_arg : arg1->list) {
						if(!walk_expression(walk, ctx, comma_arg)) return false;
						ins(Instruction{Opcode::PushRegister, 0});
					}
					arg_count = arg1->list.size();
					_DW(walk.dbg) << "\nargument count: " << arg_count << '\n';
				} else {
					if(!walk_expression(walk, ctx, arg1)) return false;
					ins(Instruction{Opcode::PushRegister, 0});
				}
				ins(Instruction{Opcode::LoadStack, arg_count});
			} else if(IS_TOKEN(args, Block) && (args->list.size() > 1)) {
				func_save = true;
				ins(Instruction{Opcode::PushRegister, 0});
				for(auto &arg : args->list) {
					if(!walk_expression(walk, ctx, arg)) return false;
					ins(Instruction{Opcode::PushRegister, 0});
				}
				arg_count = args->list.size();
				ins(Instruction{Opcode::LoadStack, arg_count});
			} else {
				_DW(walk.err) << "unhandled function call\n";
				return false;
			}
			ins(Instruction{Opcode::CallExpression, arg_count});
			if(func_save) ins(Instruction{Opcode::PopStack, 0});
			_DW(walk.dbg) << "\n";
			return true;
		} else if(IS_TOKEN(expr, O_Dot) && expr->slots == ASTSlots::NONE) {
			_DW(walk.dbg) << "Load primary argument(s) operator" << '\n';
			if(walk.pass1() && ctx->arg_declarations.empty()) {
				ctx->arg_declarations.emplace_back(VariableDeclaration{0, true});
			}
			ins(Instruction{Opcode::LoadLocal, 0, 0});
			return true;
		}
		_DW(walk.dbg) << "operator: " << expr->ast_token;
		if(IS_TOKEN(expr, O_Dot) && expr->slots == ASTSlots::SLOT1 && expr->slot1) {
			if(!IS_TOKEN(expr->slot1, Ident)) {
				walk.show_syn_error("Dot operator", expr);
				return false;
			}
			auto arg_string = expr->slot1->block.as_string();
			size_t string_index = walk.module_ctx.find_or_put_string(arg_string);
			_DW(walk.dbg) << "CTX:" << ctx << '\n';
			auto var_pos = walk.get_arg_ref(ctx, string_index);
			if(!var_pos.has_value()) return false;
			ins(Instruction{var_pos->decl_ref.is_closed ? Opcode::LoadVariable : Opcode::LoadArg, var_pos->up_count, var_pos->decl_index});
			_DW(walk.dbg) << "load named arg [" << string_index  << "]" << arg_string << "->" << var_pos->up_count << "," << var_pos->decl_index << '\n';
			return true;
		} else if(IS_TOKEN(expr, O_Assign) && !expr->open()) {
			_DW(walk.dbg) << "L ref: ";
			auto &left_ref = expr->slot1;
			auto &right_ref = expr->slot2;
			if(IS_TOKEN(left_ref, Ident)) {
				// ok! lookup variable
				auto string_index = walk.module_ctx.find_or_put_string(left_ref->block.as_string());
				auto var_pos = walk.get_var_ref(ctx, string_index);
				if(!var_pos.has_value()) return false;
				if(!var_pos->decl_ref.is_mut) {
					_DW(walk.err) << "write to immutable variable: " << str_table(string_index) << '\n';
					return false;
				}
				if(!walk_expression(walk, ctx, right_ref)) return false;
				ins(Instruction{
					var_pos->decl_ref.is_closed ? Opcode::StoreVariable : Opcode::StoreLocal,
					var_pos->up_count, var_pos->decl_index});
				_DW(walk.dbg) << "store variable [" << string_index << "]" <<
					str_table(string_index) << "->"
					<< var_pos->up_count << ","
					<< var_pos->decl_index << "\n";
				return true;
			} else {
				_DW(walk.err) << "unknown reference type: " << expr->slot1 << '\n';
				return false;
			}
		} else if(
				(
					IS_TOKEN(expr, O_AndEq)
					|| IS_TOKEN(expr, O_OrEq)
					|| IS_TOKEN(expr, O_XorEq)
					|| IS_TOKEN(expr, O_AddEq)
					|| IS_TOKEN(expr, O_SubEq)
					|| IS_TOKEN(expr, O_MulEq)
					|| IS_TOKEN(expr, O_DivEq)
					|| IS_TOKEN(expr, O_ModEq)
					|| IS_TOKEN(expr, O_LshEq)
					|| IS_TOKEN(expr, O_RAshEq)
					|| IS_TOKEN(expr, O_RshEq)
					|| IS_TOKEN(expr, O_Decl)
				)
				&& !expr->open()) {
			_DW(walk.dbg) << "L ref: ";
			auto &left_ref = expr->slot1;
			auto &right_ref = expr->slot2;
			if(IS_TOKEN(left_ref, Ident)) {
				// ok! lookup variable
				auto string_index = walk.module_ctx.find_or_put_string(left_ref->block.as_string());
				auto var_pos = walk.get_var_ref(ctx, string_index);
				if(!var_pos.has_value()) {
					if(walk.pass1()) {
						_DW(walk.err) << "pass1 variable not found: " << str_table(string_index) << "\n";
						return walk_expression(walk, ctx, right_ref);
					}
					_DW(walk.err) << "variable not found: " << str_table(string_index) << "\n";
					return false;
				}
				if(!var_pos->decl_ref.is_mut) {
					_DW(walk.err) << "write to immutable variable: " << str_table(string_index) << '\n';
					return false;
				}
				ins(Instruction{
					var_pos->decl_ref.is_closed ? Opcode::LoadVariable : Opcode::LoadLocal,
					var_pos->up_count, var_pos->decl_index});
				_DW(walk.dbg) << "load variable [" << string_index  << "]" <<
					str_table(string_index) << "->"
					<< var_pos->up_count << ","
					<< var_pos->decl_index << "\n";
				ins(Instruction{Opcode::PushRegister, 0});
				if(!walk_expression(walk, ctx, right_ref)) return false;
				switch(expr->ast_token) {
				case Token::O_AndEq:
					ins(Instruction{Opcode::AndInteger, 0});
					break;
				case Token::O_OrEq:
					ins(Instruction{Opcode::OrInteger, 0});
					break;
				case Token::O_XorEq:
					ins(Instruction{Opcode::XorInteger, 0});
					break;
				case Token::O_AddEq:
					ins(Instruction{Opcode::AddInteger, 0});
					break;
				case Token::O_SubEq:
					ins(Instruction{Opcode::SubInteger, 0});
					break;
				case Token::O_MulEq:
					ins(Instruction{Opcode::MulInteger, 0});
					break;
					//ins(Instruction{Opcode::PowInteger, 0});
				case Token::O_DivEq:
					ins(Instruction{Opcode::DivInteger, 0});
					break;
				case Token::O_ModEq:
					ins(Instruction{Opcode::ModInteger, 0});
					break;
				case Token::O_LshEq:
					ins(Instruction{Opcode::LSHInteger, 0});
					break;
				case Token::O_RAshEq:
					ins(Instruction{Opcode::RSHAInteger, 0});
					break;
				case Token::O_RshEq:
					ins(Instruction{Opcode::RSHLInteger, 0});
					break;
				default:
					_DW(walk.err) << "unhandled assign operator: " << expr->ast_token << '\n';
					return false;
				}
				ins(Instruction{Opcode::StoreVariable, var_pos.value().up_count, var_pos.value().decl_index});
				_DW(walk.dbg) << "store variable [" << string_index << "]" <<
					str_table(string_index) << "->"
					<< var_pos.value().up_count << ","
					<< var_pos.value().decl_index << "\n";
				return true;
			} else {
				_DW(walk.err) << "unknown reference type: " << expr->slot1 << '\n';
				return false;
			}
		} else if(expr->slot1 && expr->slot2) {
			_DW(walk.dbg) << "\nexpr L: ";
			if(!walk_expression(walk, ctx, expr->slot1)) return false;
			if(IS_TOKEN(expr, O_Dot)) {
				if(!IS_TOKEN(expr->slot2, Ident)) {
					walk.show_syn_error("Dot operator", expr);
					return false;
				}
				size_t string_index = walk.module_ctx.find_or_put_string(
					expr->slot2->block.as_string() );
				ins(Instruction{Opcode::NamedLookup, string_index});
				return true;
			}
			_DW(walk.dbg) << "\nexpr R: ";
			ins(Instruction{Opcode::PushRegister, 0});
			if(!walk_expression(walk, ctx, expr->slot2)) return false;
			if(!generate_oper_expr(expr)) return false;
			return true;
		} else if(expr->list.size() > 0) {
			// handle below
		} else {
			walk.show_syn_error("unknown operator", expr);
			return false;
		}
		auto expr_itr = expr->list.cbegin();
		auto expr_end = expr->list.cend();
		if(expr_itr != expr_end) {
			_DW(walk.dbg) << "\nexpr L: ";
			if(!walk_expression(walk, ctx, *expr_itr)) return false;
			expr_itr++;
		}
		while(expr_itr != expr_end) {
			_DW(walk.dbg) << "\nexpr I: ";
			ins(Instruction{Opcode::PushRegister, 0});
			if(!walk_expression(walk, ctx, *expr_itr)) return false;
			if(!generate_oper_expr(expr)) return false;
			expr_itr++;
		}
		return true;
	}
	case Expr::Start:
		switch(expr->ast_token) {
		case Token::Zero: 
			ins(Instruction{Opcode::LoadConst, 0});
			return true;
		case Token::Number: {
			uint64_t value = 0;
			if(!convert_number(expr->block.as_string(), value)) {
				_DW(walk.err) << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			_DW(walk.dbg) << "number value: " << value << "\n";
			ins(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case Token::NumberHex: {
			uint64_t value = 0;
			if(!convert_number_hex(expr->block.as_string(), value)) {
				_DW(walk.err) << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			_DW(walk.dbg) << "number value: " << value << " from " << expr->block.as_string() << "\n";
			ins(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case Token::NumberOct: {
			uint64_t value = 0;
			if(!convert_number_oct(expr->block.as_string(), value)) {
				_DW(walk.err) << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			_DW(walk.dbg) << "number value: " << value << " from " << expr->block.as_string() << "\n";
			ins(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case Token::NumberBin: {
			uint64_t value = 0;
			if(!convert_number_bin(expr->block.as_string(), value)) {
				_DW(walk.err) << "invalid number value: " << expr->block.as_string() << "\n";
				return false;
			}
			_DW(walk.dbg) << "number value: " << value << " from " << expr->block.as_string() << "\n";
			ins(Instruction{Opcode::LoadConst, value});
			return true;
		}
		case Token::K_True:
			ins(Instruction{Opcode::LoadBool, 1});
			return true;
		case Token::K_False:
			ins(Instruction{Opcode::LoadBool, 0});
			return true;
		case Token::Ident: {
			auto string_index = walk.module_ctx.find_or_put_string(expr->block.as_string());
			auto var_pos = walk.get_var_ref(ctx, string_index);
			if(walk.pass1() && !var_pos.has_value()) {
				_DW(walk.dbg) << "pass1 unfound variable\n";
				return true;
			}
			if(!var_pos.has_value()) {
				return false;
			}
			ins(Instruction{
				var_pos->decl_ref.is_closed ? Opcode::LoadVariable : Opcode::LoadLocal,
				var_pos->up_count, var_pos->decl_index});
			_DW(walk.dbg) << "load variable [" << string_index  << "]" <<
				expr->block.as_string() << "->"
				<< var_pos->up_count << ","
				<< var_pos->decl_index << "\n";
			return true;
		}
		case Token::String: {
			auto s = expr->block.as_string(1);
			for(auto c : s) {
				if(c == '\\') {
					_DW(walk.err) << "unhandled string: " << s << "\n";
					return false;
				}
			}
			size_t string_index = walk.module_ctx.find_or_put_string(s);
			ins(Instruction{Opcode::LoadString, string_index});
			return true;
		}
		default:
			_DW(walk.err) << "unhandled start token: " << expr->ast_token << "\n";
		}
		return false;
	case Expr::End: {
		//ins(Instruction{Opcode::LoadUnit, 0});
		return true;
	}
	case Expr::FuncExpr: {
		if(!AT_EXPR_TARGET(expr)) {
			walk.show_syn_error("Function expression", expr);
			return false;
		}
		_DW(walk.dbg) << "TODO function start[" << walk.current_frame_index << "] " << expr->block.as_string() << '\n';
		framectx_ptr function_context;
		if(walk.pass2) {
			function_context = walk.module_ctx.frames[walk.current_frame_index];
		} else {
			function_context = std::make_shared<FrameContext>(walk.current_frame_index, ctx);
			walk.module_ctx.frames.push_back(function_context);
		}
		walk.current_frame_index++;
		ins(Instruction{Opcode::LoadClosure, function_context->frame_index});
		// expr_args // TODO named argument list for the function declaration
		auto &expr_args = expr->slot1;
		if(IS_TOKEN(expr_args, Block)) {
			_DW(walk.err) << "unknown block function argument type: " << expr_args << '\n';
			return false;
		} else if((IS_CLASS(expr_args, OperExpr) && IS_TOKEN(expr_args, Comma))) {
			auto &comma_list = expr_args->list;
			if(walk.pass1()) {
				for(auto &arg : comma_list) {
					if(!IS_TOKEN(arg, Ident)) {
						_DW(walk.err) << "unknown list function argument type: " << arg << '\n';
						return false;
					}
					auto string_index =
						walk.module_ctx.find_or_put_string(arg->block.as_string());
					function_context->arg_declarations.emplace_back(
						VariableDeclaration{string_index, true});
				}
			}
		} else {
			_DW(walk.err) << "unknown function argument type: " << expr_args << '\n';
			return false;
		}
		// expr->slot2 // expression or block forming the function body
		if(IS_TOKEN(expr->slot2, Block)) {
			for(auto &walk_node : expr->slot2->list) {
				if(!walk_expression(walk, function_context, walk_node))
					return false;
			}
		} else {
			if(!walk_expression(walk, function_context, expr->slot2))
				return false;
		}
		_DW(walk.dbg) << "function end\n";
		break;
	}
	default:
		_DW(walk.err) << "Unhandled expression " << expr->ast_token << ":" << expr->asc << ": " << std::string_view(expr->block.tk_begin, expr->block.tk_end);
		return false;
	}
	return true;
};

struct CollapseContext {
	std::ostream &dbg;
	std::unique_ptr<ExprStackItem> &current_block;
	std::vector<node_ptr> &hold;
	bool terminating;
	Expr down_to;
	bool error;
	bool held;

	void terminate() {
		this->terminating = true;
		this->hold.emplace_back(std::move(current_block->expr_list.back()));
	}
};

static bool parse2t(CollapseContext &ctx, node_ptr &head, node_ptr &prev) {
	auto &expr_list = ctx.current_block->expr_list;
	if(
		!AT_EXPR_TARGET(prev)
		&& (IS_CLASS(prev, TypeExpr)
			|| IS_CLASS(prev, TypeDecl)
			)
		&& (IS_CLASS(head, TypeStart)
			|| IS_FULLTYPEEXPR(head)
			)
	) {
		auto merge = prev->ast_token;
		if(
			merge == head->ast_token
			&& (
				merge == Token::O_Sum
				|| merge == Token::O_Union
				|| merge == Token::Comma
				)
		) {
			_DP(ctx.dbg) << "<TypeMerge>";
			std::ranges::move(head->list, std::back_inserter(prev->list));
			prev->whitespace = head->whitespace;
			head.reset();
			return true;
		}
		_DP(ctx.dbg) << "<TypeCol>";
		PUSH_INTO(prev, head);
	} else if(!AT_EXPR_TARGET(prev)
		&& IS_CLASS(prev, OperExpr) && IS_TOKEN(prev, O_Decl)
		&& (IS_CLASS(head, TypeStart) || IS_FULLTYPEEXPR(head))
	) {
		PUSH_INTO(prev, head);
		_DP(ctx.dbg) << "<DeclMerge>";
		ctx.terminating = false;
	} else if(IS_CLASS(prev, FuncExpr) && prev->slot2_open()
		&& ctx.down_to != Expr::FuncExpr
		&& (
			IS_CLASS(head, BlockExpr)
			|| IS_CLASS(head, Start)
			|| IS_FULLEXPR(head)
			|| (IS_CLASS(head, FuncExpr) && AT_EXPR_TARGET(head))
			|| (IS_CLASS(head, KeywExpr) && AT_EXPR_TARGET(head)
				&& (IS_TOKEN(head, K_If)
					|| IS_TOKEN(head, K_While)
					|| IS_TOKEN(head, K_Until)
					|| IS_TOKEN(head, K_Loop)
		)))
	) {
		_DP(ctx.dbg) << "<FnPutEx>";
		EXTEND_TO(prev, prev, head);
		PUSH_INTO(prev, head);
	} else if(
		IS_CLASS(prev, KeywExpr)
		&& (IS_CLASS(head, Start)
			|| IS_FULLEXPR(head)
			|| IS_CLASS(head, BlockExpr)
			|| (AT_EXPR_TARGET(head)
				&& (IS_TOKEN(head, K_Break)
					|| IS_TOKEN(head, K_Continue)
					|| IS_TOKEN(head, K_If)
					|| IS_TOKEN(head, K_Loop)
					|| IS_TOKEN(head, K_While)
					|| IS_TOKEN(head, K_Until)
		)))
	) {
		_DP(ctx.dbg) << "<KeyEx>" << prev << ',' << head << '\n';
		if(prev->open()) {
			prev->whitespace = head->whitespace;
			if(IS_TOKEN(prev, K_Let) || IS_TOKEN(prev, K_Mut)) {
				if(IS_TOKEN(head, O_Assign) && prev->empty()) {
					EXTEND_TO(prev, prev, head);
					prev->slots = ASTSlots::SLOT2;
					prev->slot1 = std::move(head->slot1);
					prev->slot2 = std::move(head->slot2);
					head.reset();
					return true;
				} else if(IS_TOKEN(head, Ident)) {
					PUSH_INTO(prev, head);
				} else {
					_DP(ctx.dbg) << "\n<unhandled Let: " << head << '\n';
					return false;
				}
			} else {
				PUSH_INTO(prev, head);
			}
		} else {
			_DP(ctx.dbg) << "\n<unhandled KeywExpr *Expr>: " << prev << ',' << head << '\n';
			return false;
		}
	} else if(
		IS_OPENEXPR(prev) && IS_TOKEN(head, O_Dot)
		&& head->empty()
		&& head->slots != ASTSlots::NONE
	) {
		head->slots = ASTSlots::NONE;
	} else if(
		IS_OPENEXPR(prev)
		&& (IS_CLASS(head, Start)
			|| IS_CLASS(head, BlockExpr)
			|| (IS_CLASS(head, FuncExpr) && AT_EXPR_TARGET(head))
	)) {
		// before: (oper OperExpr (value Start)) (value Start)
		//  after: (oper OperExpr (value Start) (value Start))
		_DP(ctx.dbg) << "<OpPut>";
		prev->whitespace = head->whitespace;
		PUSH_INTO(prev, head);
	} else if(
		IS_CLASS(prev, RawOper)
		&& (IS_TOKEN(prev, O_Sub)
			|| (IS_TOKEN(prev, O_Dot)
				&& prev->whitespace == WS::NONE
				)
			)
		&& (IS_CLASS(head, Start) ||
			IS_FULLEXPR(head) ||
			IS_CLASS(head, BlockExpr)
			)
	) {
		if(IS_TOKEN(prev, O_Sub))
			CONV_TK(prev, O_Minus);
		CONV_N(prev, OperExpr, SLOT1);
	} else if(IS_OPENEXPR(prev) && IS_FULLEXPR(head)) {
		// collapse the tree one level
		_DP(ctx.dbg) << "<OpCol>";
		if(head->precedence <= prev->precedence) {
			// pop the prev item and put into the head item LHS
			auto merge = prev->ast_token;
			_DP(ctx.dbg) << "<LEP\nL:" << prev << "\nR:" << head << "\n>";
			// ex1front push_into ex2
			// ex2 insert_into ex1front
			auto decend_item = &head;
			node_ptr *upper_item = nullptr;
			while(
				IS_CLASS(*decend_item, OperExpr)
				&& !(
					IS_TOKEN(*decend_item, O_AndEq)
					|| IS_TOKEN(*decend_item, O_OrEq)
					|| IS_TOKEN(*decend_item, O_XorEq)
					|| IS_TOKEN(*decend_item, O_AddEq)
					|| IS_TOKEN(*decend_item, O_SubEq)
					|| IS_TOKEN(*decend_item, O_MulEq)
					|| IS_TOKEN(*decend_item, O_DivEq)
					|| IS_TOKEN(*decend_item, O_ModEq)
					|| IS_TOKEN(*decend_item, O_LshEq)
					|| IS_TOKEN(*decend_item, O_RAshEq)
					|| IS_TOKEN(*decend_item, O_RshEq)
					|| IS_TOKEN(*decend_item, O_Decl)
					)
				&& (*decend_item)->precedence <= prev->precedence) {
				_DP(ctx.dbg) << "\\";
				upper_item = decend_item;
				if((*decend_item)->slot_count() > 0) {
					decend_item = &(*decend_item)->slot1;
				} else {
					decend_item = &(*decend_item)->list.front();
				}
			}
			if(upper_item
				&& merge == (*upper_item)->ast_token
				&& IS_CLASS(*upper_item, OperExpr)
				&& (merge == Token::Comma
					|| merge == Token::O_Dot
					|| merge == Token::O_Assign
					|| merge == Token::O_Or
					|| merge == Token::O_And
					|| merge == Token::O_Xor
					|| merge == Token::O_Add
					|| merge == Token::O_Mul
					)
			) {
				auto &inner = *upper_item;
				_DP(ctx.dbg) << "<LEC " << inner << ">";
				if(prev->slots != ASTSlots::NONE) {
					if(prev->slot1) prev->list.emplace_back(std::move(prev->slot1));
					if(prev->slot2) prev->list.emplace_back(std::move(prev->slot2));
					prev->slots = ASTSlots::NONE;
				}
				if(inner->slot1) prev->list.emplace_back(std::move(inner->slot1));
				if(inner->slot2) prev->list.emplace_back(std::move(inner->slot2));
				std::ranges::move(inner->list, std::back_inserter(prev->list));
				prev->whitespace = inner->whitespace;
				inner = std::move(prev);
				return true;
			}
			_DP(ctx.dbg) << "<LED " << *decend_item << ">";
			PUSH_INTO(prev, *decend_item);
			*decend_item = std::move(prev);
		} else {
			_DP(ctx.dbg) << "<GP\nL:" << *prev << "\nR:" << *head << "\n>";
			PUSH_INTO(prev, head);
		}
	} else if(
		(IS_TOKEN(prev, K_Let) || IS_TOKEN(prev, K_Mut))
		&& !AT_EXPR_TARGET(prev)
		&& (IS_TOKEN(head, Ident)
			|| (IS_FULLEXPR(head) && IS_TOKEN(head, Comma))
			)
	) {
		PUSH_INTO(prev, head);
	} else if(IS2C(KeywExpr, KeywExpr)) {
		if(
			IS_TOKEN(prev, K_If) && AT_EXPR_TARGET(prev)
			&& (prev->list.empty() || !IS_TOKEN(prev->list.back(), K_Else))
			&& (IS_TOKEN(head, K_Else) || IS_TOKEN(head, K_ElseIf))
			&& AT_EXPR_TARGET(head)
		) {
			PUSH_INTO(prev, head);
		} else if(
			IS_TOKEN(prev, K_Let) && prev->empty()
			&& IS_TOKEN(head, K_Mut) && AT_EXPR_TARGET(head)
		) {
			PUSH_INTO(prev, head);
			auto &mutnode = prev->slot1;
			if(mutnode->slot2) {
				prev->slots = ASTSlots::SLOT2;
				prev->slot2 = std::move(POP_EXPR(mutnode));
				mutnode->slots = ASTSlots::SLOT1;
			}
		} else if(
			IS_TOKEN(prev, K_If) && AT_EXPR_TARGET(prev)
			&& (IS_TOKEN(head, K_Else) || IS_TOKEN(head, K_ElseIf))
			&& !AT_EXPR_TARGET(head)
		) {
			_DP(ctx.dbg) << "\n<if/else syntax error>";
			return false;
		} else {
			_DP(ctx.dbg) << "\n<unhandled Key Key\nL:" << prev << "\nR:" << head << ">";
			return false;
		}
	} else {
		return false;
	}
	return true;
};
static bool parse3nt(CollapseContext &ctx, node_ptr &head, node_ptr &prev, node_ptr &third) {
	if(!third) return false;
	if(
		IS_TOKEN(third, Ident)
		&& (IS_TOKEN(prev, Ident) || (IS_FULLEXPR(prev) && IS_TOKEN(prev, Comma)))
		&& IS_CLASS(head, FuncDecl)
	) {
		// check third for another ident or let
		// TODO operator names
		// TODO also need to check for newline
		// named function with one arg or comma arg list
		node_ptr comma_list;
		if(IS_TOKEN(prev, Comma)) {
			_DP(ctx.dbg) << "\n<name func comma args>";
			comma_list = std::move(prev);
		} else {
			_DP(ctx.dbg) << "\n<name func one arg>";
			MAKE_NODE_N_FROM1(comma_list, Comma, OperExpr, NONE, prev);
			PUSH_INTO(comma_list, prev);
		}
		auto MAKE_NODE_N_FROM1(let_assign, O_Assign, OperExpr, SLOT2, third);
		bool is_obj = IS_TOKEN(ctx.current_block->block, Object);
		if(!is_obj) {
			auto MAKE_NODE_N_FROM1(let_expr, K_Let, KeywExpr, SLOT1, third);
			PUSH_INTO(let_assign, third);
			third = std::move(let_expr);
		}
		prev = std::move(let_assign);
		CONV_N(head, FuncExpr, SLOT2);
		PUSH_INTO(head, comma_list);
	} else if(
		(IS_TOKEN(third, K_Let) && AT_EXPR_TARGET(third)
		&& IS_TOKEN(third->slot1, Ident) && third->slots == ASTSlots::SLOT1)
		&& IS_CLASS(prev, BlockExpr)
		&& IS_CLASS(head, FuncDecl)
	) {
		// TODO check let is well formed
		// let function with one argument or comma arg list
		node_ptr comma_list;
		if(prev->list.size() > 0) {
			auto &inner = prev->list.front();
			if(IS_TOKEN(inner, Comma)) {
				_DP(ctx.dbg) << "\n<let block func comma args>";
				comma_list = std::move(inner);
			} else {
				_DP(ctx.dbg) << "\n<let block func one arg>";
				MAKE_NODE_N_FROM1(comma_list, Comma, OperExpr, NONE, prev);
				PUSH_INTO(comma_list, inner);
			}
		} else {
			_DP(ctx.dbg) << "\n<let block func zero arg>";
			MAKE_NODE_N_FROM1(comma_list, Comma, OperExpr, NONE, prev);
		}
		auto MAKE_NODE_N_FROM1(let_assign, O_Assign, OperExpr, SLOT2, third);
		PUSH_INTO(let_assign, POP_EXPR(third));
		prev = std::move(let_assign);
		CONV_N(head, FuncExpr, SLOT2);
		PUSH_INTO(head, comma_list);
	} else if(
		(IS_TOKEN(third, K_Let) && AT_EXPR_TARGET(third)
		&& IS_TOKEN(third->slot1, Ident) && third->slots == ASTSlots::SLOT1)
		&& (IS_TOKEN(prev, Ident)
			|| (IS_FULLEXPR(prev) && IS_TOKEN(prev, Comma))
			)
		&& IS_CLASS(head, FuncDecl)
	) {
		// TODO check let is well formed
		// let function with one argument or comma arg list
		node_ptr comma_list;
		if(IS_TOKEN(prev, Comma)) {
			_DP(ctx.dbg) << "\n<let func comma args>";
			comma_list = std::move(prev);
		} else {
			_DP(ctx.dbg) << "\n<let func one arg>";
			MAKE_NODE_N_FROM1(comma_list, Comma, OperExpr, NONE, prev);
			PUSH_INTO(comma_list, prev);
		}
		auto MAKE_NODE_N_FROM1(let_assign, O_Assign, OperExpr, SLOT2, third);
		PUSH_INTO(let_assign, POP_EXPR(third));
		prev = std::move(let_assign);
		CONV_N(head, FuncExpr, SLOT2);
		PUSH_INTO(head, comma_list);
	} else if(IS2C(KeywExpr, KeywExpr)) {
		if(IS_TOKEN(third, K_If) && AT_EXPR_TARGET(third)
			&& IS_TOKEN(prev, K_ElseIf) && AT_EXPR_TARGET(prev)
		) {
			_DP(ctx.dbg) << "<IfElf*>";
			PUSH_INTO(third, prev);
			return true;
		} else if(IS_TOKEN(prev, K_Else) && prev->empty()
			&& IS_TOKEN(head, K_If)
		) {
			_DP(ctx.dbg) << "<Elf>";
			prev.reset();
			CONV_TK(head, K_ElseIf);
		} else if(IS_TOKEN(third, K_If) && !third->open()
			&& (third->list.empty() || !IS_TOKEN(third->list.back(), K_Else))
			&& IS_TOKEN(prev, K_Else) && AT_EXPR_TARGET(prev)
		) {
			_DP(ctx.dbg) << "<IfElse>";
			//PUSH_INTO(third, prev);
			ctx.terminate();
			return true;
		} else {
			_DP(ctx.dbg) << "\n<unhandled" << ctx.terminating << " Key Key\nT:"
				<< third->ast_token << " " << third->asc
				<< "\nL:" << prev
				<< "\nR:" << head << ">";
			ctx.error = true;
			return true;
		}
	} else return false;
	return true;
};
static bool parse2nt(CollapseContext &ctx, node_ptr &head, node_ptr &prev) {
	auto &expr_list = ctx.current_block->expr_list;
	if(
		IS_CLASS(prev, TypeDecl) && prev->slot1_open()
		&& IS_TOKEN(head, Ident)
	) {
		if(IS_CLASS(head, Start)) CONV(head, TypeStart);
		PUSH_INTO(prev, head);
		_DP(ctx.dbg) << "\n<TypeSet>";
	} else if(IS_CLASS(ctx.current_block->block, TypeExpr)
		&& IS_CLASS(head, Start)
		&& IS_TOKEN(head, Ident)
	) {
		CONV(head, TypeStart);
		return true;
	} else if(
		IS_CLASS(prev, TypeStart) && prev->whitespace == WS::NONE
		&& IS_CLASS(head, BlockExpr)
	) {
		auto MAKE_NODE_N_FROM2(tag, TypeTag, TypeExpr, SLOT2, prev, head);
		PUSH_INTO(tag, prev);
		PUSH_INTO(tag, head);
		prev = std::move(tag);
		_DP(ctx.dbg) << "<TypeTag>";
	} else if(
		IS_CLASS(prev, TypeDecl)
		&& prev->slot2_open()
		&& IS_CLASS(head, BlockExpr)
	) {
		if(IS_TOKEN(head, Object)) CONV_TK(head, TypeObject);
		else if(IS_TOKEN(head, Array)) CONV_TK(head, TypeArray);
		else if(IS_TOKEN(head, Block)) CONV_TK(head, TypeBlock);
		CONV(head, TypeExpr);
		PUSH_INTO(prev, head);
		_DP(ctx.dbg) << "\n<TypeBlock>";
	} else if(
		(IS_CLASS(prev, TypeExpr) || IS_CLASS(prev, TypeDecl))
		&& !AT_EXPR_TARGET(prev)
		&& IS_CLASS(head, Start)) {
		CONV(head, TypeStart);
	} else if(!ctx.terminating
			&& IS_FULLTYPEEXPR(prev) && IS_CLASS(head, RawOper)) {
		if(IS_TOKEN(head, O_Or)) {
			CONV_TK(head, O_Sum);
		} else if(IS_TOKEN(head, O_And)) {
			CONV_TK(head, O_Union);
		}
		auto link = head->ast_token;
		if(link == Token::O_Sum
			|| link == Token::O_Union
			|| link == Token::Comma
		) {
			if(link == prev->ast_token) {
				prev->mark_open();
				head.reset();
				_DP(ctx.dbg) << "<TypeOpInc>";
				return true;
			}
		} else {
			ctx.terminate();
			_DP(ctx.dbg) << "<TypeOpTerm>";
			return true;
		}
		CONV(head, TypeExpr);
		PUSH_INTO(head, prev);
		_DP(ctx.dbg) << "\n<TypeOp>";
	} else if(IS_CLASS(prev, TypeStart) && IS_CLASS(head, RawOper)
		&& (IS_TOKEN(head, O_Or) || IS_TOKEN(head, O_And))
	) {
		if(IS_TOKEN(head, O_Or)) { CONV_TK(head, O_Sum); }
		else if(IS_TOKEN(head, O_And)) { CONV_TK(head, O_Union); }
		CONV(head, TypeExpr);
		PUSH_INTO_OPEN(head, prev);
		_DP(ctx.dbg) << "\n<TypeOp>";
	} else if(IS_TOKEN(prev, O_Decl)
		&& IS_CLASS(head, Start) && IS_TOKEN(head, Ident)
	) {
		CONV(head, TypeStart);
		_DP(ctx.dbg) << "\n<DeclIdent>";
	} else if(IS_FULLTYPEDECL(prev) && IS_CLASS(head, TypeDecl)) {
		_DP(ctx.dbg) << "\n<TypeSep>";
	} else if(IS_CLASS(prev, Start) && !IS_TOKEN(prev, Ident)
		&& IS_CLASS(head, RawOper) && IS_TOKEN(head, O_Dot)
	) {
		CONV_N(head, OperExpr, SLOT1);
		_DP(ctx.dbg) << "\n<RawOpPfx>";
	} else if((IS_CLASS(prev, Start) || IS_CLASS(prev, BlockExpr))
		&& IS_CLASS(head, RawOper)
	) {
		CONV(head, OperExpr);
		PUSH_INTO_OPEN(head, prev);
		_DP(ctx.dbg) << "\n<RawOp>";
	} else if(
		IS_CLASS(prev, OperExpr)
		&& AT_EXPR_TARGET(prev)
		&& IS_TOKEN(prev, Comma)
		&& IS_CLASS(head, FuncDecl)
	) {
		CONV_N(head, FuncExpr, SLOT2);
		PUSH_INTO(head, prev);
		// function anon with comma arg list
		_DP(ctx.dbg) << "\n<anon func no args>";
	} else if(
		IS_TOKEN(prev, Ident) && IS_CLASS(head, FuncDecl)
	) {
		bool is_obj = IS_TOKEN(ctx.current_block->block, Object);
		auto prev_ptr = prev.get();
		auto MAKE_NODE_N_FROM1(let_assign, O_Assign, OperExpr, SLOT2, prev_ptr);
		PUSH_INTO(let_assign, prev);
		if(!is_obj) {
			auto MAKE_NODE_N_FROM1(let_expr, K_Let, KeywExpr, SLOT1, prev_ptr);
			prev = std::move(let_expr);
		}
		auto MAKE_NODE_N_FROM1(comma_list, Comma, OperExpr, NONE, head);
		CONV_N(head, FuncExpr, SLOT2);
		PUSH_INTO(head, comma_list);
		if(!is_obj) expr_list.insert(expr_list.cend() - 1, std::move(let_assign));
		else prev = std::move(let_assign);
		// named function with no args
		_DP(ctx.dbg) << "\n<func no args?>";
	} else if(
		(IS_TOKEN(prev, K_Let) || IS_TOKEN(prev, K_Mut))
		&& AT_EXPR_TARGET(prev)
		&& IS_CLASS(head, FuncDecl)
	) {
		CONV_N(head, FuncExpr, SLOT2);
		auto MAKE_NODE_N_FROM2(let_assign, O_Assign, OperExpr, SLOT2, prev, head);
		PUSH_INTO(let_assign, POP_EXPR(prev));
		auto MAKE_NODE_N_FROM1(comma_list, Comma, OperExpr, NONE, head);
		PUSH_INTO(head, comma_list);
		expr_list.insert(expr_list.cend() - 1, std::move(let_assign));
		_DP(ctx.dbg) << "\n<LetFn>";
	} else if(
		(IS_TOKEN(prev, Ident) || (IS_CLASS(prev, OperExpr) && IS_TOKEN(prev, Comma)))
		&& IS_CLASS(head, FuncDecl)
	) {
		_DP(ctx.dbg) << "\n<func no third>";
		CONV_N(head, FuncExpr, SLOT2);
		PUSH_INTO(head, prev);
	} else if(IS2C(BlockExpr, FuncDecl)) {
		// anon function with blocked arg list
		// or named function with blocked arg list?
		_DP(ctx.dbg) << "\n<func block>";
		CONV_N(head, FuncExpr, SLOT2);
		if(prev->list.size() == 1
				&& IS_TOKEN(prev->list.front(), Comma)
				&& AT_EXPR_TARGET(prev->list.front())
			) {
			PUSH_INTO(head, prev->list.front());
			prev.reset();
		} else {
			CONV_TK(prev, Comma);
			CONV_N(prev, OperExpr, NONE);
			PUSH_INTO(head, prev);
		}
	} else if(
			((IS_TOKEN(prev, Ident) && IS_CLASS(prev, Start))
			|| IS_TOKEN(prev, O_CallExpr)
			|| IS_TOKEN(prev, O_Dot)
			|| (IS_TOKEN(prev, Block) && IS_CLASS(prev, BlockExpr))
			)
		&& prev->whitespace == WS::NONE
		&& (IS_TOKEN(head, Array)
			|| IS_TOKEN(head, Block)
			|| IS_TOKEN(head, Object)
			)
	) {
		_DP(ctx.dbg) << "<MakeRef>";
		auto MAKE_NODE_N_FROM2(block_expr, O_CallExpr, OperExpr, SLOT2, prev, head);
		PUSH_INTO(block_expr, prev);
		PUSH_INTO(block_expr, head);
		prev = std::move(block_expr);
		return true;
	} else if(!ctx.terminating
		&& IS_CLASS(prev, OperExpr) && IS_TOKEN(prev, O_Dot)
		&& prev->slots == ASTSlots::SLOT1 && prev->empty()
		&& (prev->whitespace != WS::NONE)
		&& (IS_CLASS(head, Start)
			|| IS_CLASS(head, BlockExpr)
			|| IS_CLASS(head, RawOper)
			|| (IS_CLASS(head, FuncExpr) && AT_EXPR_TARGET(head))
	)) {
		_DP(ctx.dbg) << "<DotNLCv>";
		prev->slots = ASTSlots::NONE;
		//ctx.terminate();
		return true;
	} else if(IS_FULLEXPR(prev) && IS_CLASS(head, RawOper)) {
		_DP(ctx.dbg) << "<OpRaw>";
		// before: (oper1 OperExpr (value1 Start) (value2 Start)) (oper2 Raw)
		if(head->precedence <= prev->precedence) {
			// new is same precedence: (1+2)+ => ((1+2)+_)
			// new is lower precedence: (1*2)+ => ((1*2)+_)
			//  after: (oper2 OperOpen
			//             oper1 OperExpr (value1 Start) (value2 Start) )
			// put oper1 inside oper2
			//  ex2 push_into ex1
			CONV(head, OperExpr);
			PUSH_INTO_OPEN(head, prev);
		} else {
			// new is higher precedence: (1+2)* => (1+(2*_)) || (1+_) (2*_)
			//  after: (oper1 OperOpen (value1 Start)) (oper2 OperOpen (value2 Start))
			// convert both operators to open list
			// ex2back push_into ex1
			CONV(head, OperExpr);
			// pop value2 from oper1 push into oper2
			PUSH_INTO_OPEN(head, POP_EXPR(prev));
			// TODO have to decend or collapse the tree at some point
		}
	} else if(IS2C(KeywExpr, KeywExpr)) {
		if(
			IS_TOKEN(prev, K_Else) && prev->list.empty()
			&& IS_TOKEN(head, K_If)
		) {
			CONV_TK(head, K_ElseIf);
			EXTEND_TO(head, prev, head);
			return true;
		} else if(
			IS_TOKEN(prev, K_If) && AT_EXPR_TARGET(prev)
			&& (IS_TOKEN(head, K_Else) || IS_TOKEN(head, K_ElseIf))
			&& !AT_EXPR_TARGET(head)
		) {
			if(ctx.terminating) {
				_DP(ctx.dbg) << "\n<if/else syntax error>";
				ctx.error = true;
			}
		} else {
			_DP(ctx.dbg) << "\n<unhandled KeywExpr KeywExpr\nL:" << prev << "\nR:" << head << ">";
			ctx.error = true;
		}
	} else if(
		( (IS_CLASS(prev, Start)
			|| IS_CLASS(prev, BlockExpr)
			|| (IS_CLASS(prev, FuncExpr) && AT_EXPR_TARGET(prev))
			|| IS_FULLEXPR(prev)
			|| (IS_CLASS(prev, KeywExpr) && AT_EXPR_TARGET(prev)
				&& (IS_TOKEN(prev, K_If)
					|| IS_TOKEN(prev, K_While)
					|| IS_TOKEN(prev, K_Until)
					|| IS_TOKEN(prev, K_Loop)
				))
		) && (IS_CLASS(head, Start)
			|| IS_CLASS(head, TypeExpr)
			|| IS_CLASS(head, KeywExpr)
			|| IS_CLASS(head, BlockExpr)
			|| IS_FULLEXPR(head)
		) )
		|| (IS_CLASS(prev, TypeStart) && IS_CLASS(head, RawOper))
		) {
		_DP(ctx.dbg) << "<LAH>";
		if(!ctx.terminating) {
			//_DP(ctx.dbg) << "\n<collapse_expr before " << head << ">";
			if(IS_TOKEN(head, K_Else)) {
				ctx.down_to = Expr::FuncExpr;
			}
			if(IS_CLASS(head, RawOper)) {
				ctx.down_to = Expr::KeywExpr;
			}
			ctx.terminate();
			return true;
		}
		if(IS_FULLEXPR(head) && IS_TOKEN(head, O_Dot)) {
			ctx.terminate();
			return true;
		}
		_DP(ctx.dbg) << "\n<unhandled lookahead " << uint32_t{ctx.terminating} << ">" << prev->asc << "\nR:" << head->asc << '\n';
	} else if(!ctx.terminating && IS_TOKEN(head, Semi)) {
		_DP(ctx.dbg) << "\n<collapse_expr semi>";
		ctx.terminate();
		return true;
	} else if(
			(IS_OPENEXPR(prev)
			|| IS_CLASS(prev, Delimiter)
			|| IS_CLASS(prev, KeywExpr)
			|| (IS_CLASS(prev, FuncExpr) && !AT_EXPR_TARGET(prev))
			)
		&& IS_CLASS(head, RawOper)
		&& (IS_TOKEN(head, O_Sub) || IS_TOKEN(head, O_Dot))
	) {
		if(IS_TOKEN(head, O_Sub))
			CONV_TK(head, O_Minus);
		CONV_N(head, OperExpr, SLOT1);
		_DP(ctx.dbg) << "<DotPfx>";
		return true;
	} else if(IS_CLASS(prev, End)) {
		_DP(ctx.dbg) << "<End>";
		// no op
	} else if(
		(IS_CLASS(prev, Start)
			|| IS_CLASS(prev, TypeStart)
			|| IS_FULLEXPR(prev)
			|| IS_FULLTYPEEXPR(prev)
		)
		&& IS_TOKEN(head, Comma)
	) {
		auto MAKE_NODE_N_FROM2(comma_list, Comma, OperExpr, NONE, prev, head);
		PUSH_INTO(comma_list, prev);
		prev = std::move(comma_list); // comma_list -> prev
	} else if(IS_OPENEXPR(prev) && (
		IS_CLASS(head, Start) || IS_FULLEXPR(head) )) {
		_DP(ctx.dbg) << "<NOPS>";
	} else if(IS2C(Delimiter, Start)) {
		if(ctx.terminating) {
			_DP(ctx.dbg) << "<TODO Delimiter>";
			ctx.error = true;
		} else _DP(ctx.dbg) << "<NOP>";
	} else if((IS_TOKEN(prev, K_Let) || IS_TOKEN(prev, K_Mut)) && IS_TOKEN(head, O_Decl)) {
		_DP(ctx.dbg) << "<DeclEnd>";
	} else {
		_DP(ctx.dbg) << "\n<<collapse_expr " << ctx.terminating << " unhandled " << prev << ":" << head << ">>";
		ctx.error = true;
	}
	return false;
};

bool parse_source(std::ostream &dbg, std::shared_ptr<ModuleContext> root_module) {
	Token current_token = Token::White;
	const auto file_start = root_module->source.source.cbegin();
	source_itr token_start = file_start;
	const auto file_end = root_module->source.source.cend();
	auto cursor = token_start;
	LexState current_state = LexState::Free;

	std::vector<LexTokenRange> token_buffer;
	root_module->source.root_tree =
		std::make_unique<ASTNode>(Token::Block, Expr::BlockExpr,
			LexTokenRange{token_start, file_end, Token::Eof}
		);
	std::vector<std::unique_ptr<ExprStackItem>> block_stack;
	std::unique_ptr<ExprStackItem> current_block =
		std::make_unique<ExprStackItem>(ExprStackItem {
			root_module->source.root_tree.get(), root_module->source.root_tree->list
		});
	bool is_start_expr = false;
	node_ptr empty;
	std::vector<node_ptr> expr_hold;
	auto clean_list = [&]() {
		auto &expr_list = current_block->expr_list;
		for(size_t list_count = 0; list_count < expr_list.size() && list_count < 4;) {
			auto item = expr_list.cend() - (1 + list_count);
			if(!*item) {
				expr_list.erase(item);
				continue;
			}
			list_count++;
		}
	};
	auto collapse_expr = [&](bool terminating, Expr down_to = Expr::End) {
		auto &expr_list = current_block->expr_list;
		// try to collapse the expression list
		CollapseContext ctx { dbg, current_block, expr_hold, terminating, down_to, false, false };
		while(true) {
			if(expr_list.empty()) break;
			auto &head = expr_list.back();
			if( ctx.terminating ) {
				if(IS_CLASS(head, RawOper) && IS_TOKEN(head, O_Dot)) {
					CONV_N(head, OperExpr, NONE);
				} else if(IS_CLASS(head, OperExpr) && IS_TOKEN(head, O_Dot)
					&& head->empty()
					&& head->slots == ASTSlots::SLOT1
				) {
					head->slots = ASTSlots::NONE;
					continue;
				}
			}
			if(expr_list.size() == 1) {
				if(IS_CLASS(head, RawOper) && IS_TOKEN(head, O_Dot)) {
					CONV_N(head, OperExpr, SLOT1);
				} else if(IS_CLASS(current_block->block, TypeExpr) && IS_CLASS(head, Start)) {
					CONV(head, TypeStart);
				}
				break;
			}
			auto &prev = *(expr_list.end() - 2);
			if(!head || !prev) return;
			auto &third = expr_list.size() >= 3 ? *(expr_list.end() - 3) : empty;
			//auto &fourth = expr_list.size() >= 4 ? *(expr_list.end() - 4) : empty;
			//_DL(ctx.dbg) << "|";
			//_DL(ctx.dbg) << "<|" << prev << "," << head << "|>";
			bool was_terminating = ctx.terminating;
			if(!(
				(ctx.terminating && (
					parse2t(ctx, head, prev)
				))
				|| parse3nt(ctx, head, prev, third)
				|| parse2nt(ctx, head, prev)
			)) {
				clean_list();
				if(!ctx.held && expr_hold.size() > 0) {
					ctx.terminating = false;
					while(expr_hold.size() > 0) {
						//_DL(ctx.dbg) << "\n<END collapse_expr before " << expr_hold << ">";
						expr_list.emplace_back(std::move(expr_hold.back()));
						expr_hold.pop_back();
					}
					ctx.held = true;
				} else if(was_terminating || !ctx.terminating || ctx.error)
					break;
			} else clean_list();
			if(ctx.error) break;
		}
		while(expr_hold.size() > 0) {
			//_DL(ctx.dbg) << "\n<END collapse_expr before " << expr_hold << ">";
			expr_list.emplace_back(std::move(expr_hold.back()));
			expr_hold.pop_back();
		}
	};
	auto start_expr = [&](std::unique_ptr<ASTNode> new_node) {
		// push any existing start_expression
		auto &expr_list = current_block->expr_list;
		expr_list.emplace_back(std::move(new_node));
		collapse_expr(false);
	};
	auto push_token = [&](LexTokenRange &lex_range) {
		auto token_string = std::string_view(lex_range.tk_begin, lex_range.tk_end);
		bool show = true;
		auto terminal_expr = [&](Expr ty = Expr::End) {
			_DL(dbg) << "\n<TE " << lex_range.token << " " << token_string << ">";
			collapse_expr(true, ty);
		};
		switch(lex_range.token) {
		case Token::Block: // () [] {}
		case Token::Array:
		case Token::Object:
		case Token::DotBlock: // .() .[] .{}
		case Token::DotArray:
		case Token::DotObject: {
			auto block = std::make_unique<ASTNode>(lex_range.token, Expr::BlockExpr, lex_range);
			auto ptr_block = block.get();
			start_expr(std::move(block));
			block_stack.emplace_back(std::move(current_block));
			current_block = std::make_unique<ExprStackItem>(ExprStackItem{ptr_block, ptr_block->list});
			current_block->obj_ident = lex_range.token == Token::Object;
			_DL(dbg) << "(begin block '" << token_string << "' ";
			show = false;
			break;
		}
		case Token::EndDelim: {
			char block_char = '\\';
			terminal_expr();
			auto &expr_list = current_block->expr_list;
			//for(auto &expr : expr_list) _DL(ctx.dbg) << "expr: " << expr << "\n";
			//expr_list.clear(); // TODO use these
			switch(current_block->block->ast_token) {
			case Token::Block:
			case Token::TypeBlock:
			case Token::DotBlock: block_char = ')'; break;
			case Token::Array:
			case Token::TypeArray:
			case Token::DotArray: block_char = ']'; break;
			case Token::Object:
			case Token::TypeObject:
			case Token::DotObject: block_char = '}'; break;
			default: break;
			}
			if(token_string[0] != block_char) {
				_DL(dbg) << "\nInvalid closing for block: "
					<< current_block->block->ast_token << "!=" << token_string[0] << "\n";
				return true;
			}
			current_block->block->block.tk_end = lex_range.tk_end;
			show = false;
			if(!block_stack.empty()) {
				current_block.swap(block_stack.back());
				block_stack.pop_back();
				collapse_expr(false);
				_DL(dbg) << " end block)\n";
			} else {
				_DL(dbg) << "[empty stack!]\n";
				show = true;
			}
			break;
		}
		case Token::Newline:
			if(!current_block->expr_list.empty())
				current_block->expr_list.back()->whitespace = WS::NL;
			if(IS_TOKEN(current_block->block, Object))
				current_block->obj_ident = true;
			// fallthrough
		case Token::White:
			if(!current_block->expr_list.empty()) {
				auto &tk = current_block->expr_list.back();
				if(tk->whitespace == WS::NONE) tk->whitespace = WS::SP;
			}
			break;
		case Token::Comma: {
			//terminal_expr();
			start_expr(std::make_unique<ASTNode>(Token::Comma, Expr::RawOper, ASTSlots::VAR, lex_range));
			break;
		}
		case Token::Ident: {
			auto found = std::ranges::find(keyword_table_span, token_string, &ParserKeyword::word);
			if(found != keyword_table_span.end()) {
				lex_range.token = found->token;
				if(found->action == KWA::Terminate) {
					terminal_expr();
				}
				start_expr(std::make_unique<ASTNode>(
					lex_range.token, found->parse_class, found->slots, lex_range));
			} else {
				_DL(dbg) << "Ident token: \"" << token_string << "\"\n";
				// definitely not a keyword
				start_expr(std::make_unique<ASTNode>(Token::Ident, Expr::Start, lex_range));
			}
			break;
		}
		case Token::Number:
		case Token::NumberBin:
		case Token::NumberHex:
		case Token::NumberOct:
		case Token::NumberReal:
		case Token::Zero:
		case Token::K_True:
		case Token::K_False:
		case Token::DotIdent:
		case Token::String: {
			start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::Start, lex_range));
			_DL(dbg) << "<Num>";
			break;
		}
		case Token::DotDot:
		case Token::Oper:
		case Token::Minus:
		case Token::Arrow:
		case Token::RangeIncl:
		case Token::Dot:
		case Token::DotOper:
		case Token::DotOperEqual:
		case Token::DotOperPlus:
		case Token::DotDotPlusEqual:
		case Token::O_SubEq:
		case Token::Hash:
		case Token::OperEqual:
		case Token::Less:
		case Token::Xor:
		case Token::XorDot:
		case Token::O_XorEq:
		case Token::LessDot:
		case Token::LessUnit:
		case Token::O_Spaceship:
		case Token::Greater:
		case Token::GreaterEqual:
		case Token::GreaterUnit:
		case Token::GreaterDot:
		case Token::LessEqual:
		case Token::Equal:
		case Token::EqualEqual:
		case Token::Slash: {
			auto found = std::ranges::find(keyword_table_span, token_string, &ParserKeyword::word);
			if(found == keyword_table_span.end()) {
				_DL(dbg) << "\nInvalid operator\n";
				return true;
			}
			lex_range.token = found->token;
			if(found->action == KWA::ObjIdent && current_block->obj_ident) {
				current_block->obj_ident = false;
				start_expr(std::make_unique<ASTNode>(
					Token::Ident, Expr::Start, found->slots, lex_range));
			} else {
				if(found->action == KWA::ObjAssign
					&& IS_TOKEN(current_block->block, Object)) {
					if(current_block->obj_ident) {
						lex_range.token = Token::O_Assign;
					}
					current_block->obj_ident = false;
				}
				start_expr(std::make_unique<ASTNode>(
					lex_range.token, found->parse_class, found->slots, lex_range));
				_DL(dbg) << "<Op" << found->slots << ":" << lex_range.token << ">";
			}
			break;
		}
		case Token::Colon: {
			start_expr(std::make_unique<ASTNode>(Token::O_Decl, Expr::RawOper, ASTSlots::SLOT2, lex_range));
			break;
		}
		case Token::Semi: {
			terminal_expr();
			if(IS_TOKEN(current_block->block, Object))
				current_block->obj_ident = true;
			start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::End, lex_range));
			break;
		}
		case Token::Func: {
			terminal_expr(Expr::FuncExpr);
			start_expr(std::make_unique<ASTNode>(lex_range.token, Expr::FuncDecl, lex_range));
			break;
		}
		case Token::Eof: {
			terminal_expr();
			return true;
		}
		case Token::Invalid: {
			_DL(dbg) << "\nInvalid token\n";
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
		//_DL(dbg) << "TK:" << tk_start << "," << tk_end << ":" << current_token << "\n";
		if(push_token(range)) {
			//if(!token_buffer.empty()) parsed = token_buffer.back().token;
			if(current_token == Token::Newline) {
				_DL(dbg) << "\n";
			} else if(current_token != Token::White) {
				auto token_string = std::string_view(token_start, cursor);
				_DL(dbg) << "["sv << tk_start << ',' << tk_end << ':'
					<< original;
				if(range.token != original)
					_DL(dbg) << ':' << range.token;
				_DL(dbg) << " \""sv << token_string << "\"]";
			}
		}
		token_start = cursor;
	};

	for(;; cursor++) {
		Token next_token = current_token;
		char c = 0;
		if(cursor == file_end) {
			c = 0;
			next_token = Token::Eof;
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
								case Token::LessDot:
								case Token::LessUnit:
									current_token = Token::Less;
									break;
								case Token::GreaterDot:
								case Token::GreaterUnit:
									current_token = Token::Greater;
									break;
								case Token::XorDot:
									current_token = Token::Xor;
									break;
								default:
									current_token = Token::Invalid;
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
					next_token = Token::Invalid;
				}
				break;
			case LexState::Comment:
				if(contains("\r\n"sv, c)) {
					next_token = Token::Newline;
					current_state = LexState::Free;
				}
				break;
			case LexState::DString:
				if(c == '"') {
					next_token = Token::White;
					current_state = LexState::Free;
				}
				break;
			case LexState::SString:
				if(c == '\'') {
					next_token = Token::White;
					current_state = LexState::Free;
				}
				break;
			}
		}
		if((next_token != current_token)
			|| (current_token == Token::Block)
			|| (current_token == Token::Array)
			|| (current_token == Token::Object)
			|| (current_token == Token::EndDelim)) {
			emit_token(cursor);
			current_token = next_token;
		}
		if(cursor == file_end) {
			emit_token(cursor);
			break;
		}
	}
	_DL(dbg) << '\n';
	return true;
}

void show_string_table(std::ostream &out, const ModuleContext &module) {
	out << "\nString table:\n";
	for(auto itr = module.string_table.cbegin(); itr != module.string_table.cend(); itr++) {
		out << "[" << itr - module.string_table.cbegin() << "]" << *itr << '\n';
	}
}

void show_lines(std::ostream &out, const ModuleContext &module) {
	out << "\nLine table:\n";
	for(auto itr = module.line_positions.cbegin(); itr != module.line_positions.cend(); itr++) {
		out << "[" << itr - module.line_positions.cbegin() << "]" << *itr << '\n';
	}
}

void show_scopes(std::ostream &out, const ModuleContext &module) {
	out << "\nScopes:\n";
	for(auto scope = module.frames.cbegin(); scope != module.frames.cend(); scope++) {
		out << "Scope: " << scope - module.frames.cbegin() << "\n";
		auto ins_begin = (*scope)->instructions.cbegin();
		for(auto ins = ins_begin; ins != (*scope)->instructions.cend(); ins++)
			out << "[" << ins - ins_begin << "] " << *ins << '\n';
	}
}

void show_source_tree(std::ostream &out, const ModuleSource &source) {
	out << source.root_tree << '\n';
}
module_ptr test_parse_sourcefile(std::ostream &dbg, const string_view file_path) {
	std::string file_source;
	if(!LoadFileV(file_path, file_source)) return nullptr;
	module_ptr root_module = std::make_shared<ModuleContext>(std::move(file_source));
	parse_source(dbg, root_module);
	return std::move(root_module);
}
ModuleSource& get_source(const module_ptr &module) {
	return module->source;
}
module_ptr compile_sourcefile(std::ostream &out, string file_source) {
	module_ptr root_module = std::make_shared<ModuleContext>(std::move(file_source));
	parse_source(out, root_module);
	auto walk = WalkContext{out, out, *root_module.get()};
	root_module->add_import("sys");
	root_module->add_import("io");
	walk_expression(walk, root_module->root_context, root_module->source.root_tree);
	walk.pass_reset();
	walk.pass2 = true;
	walk_expression(walk, root_module->root_context, root_module->source.root_tree);
	return root_module;
}
module_ptr test_compile_sourcefile(std::ostream &dbg, const string_view file_path) {
	std::string file_source;
	if(!LoadFileV(file_path, file_source)) return nullptr;
	module_ptr root_module = std::make_shared<ModuleContext>(std::move(file_source));
	parse_source(dbg, root_module);
	show_source_tree(dbg, root_module->source);
	auto walk = WalkContext{dbg, dbg, *root_module.get()};
	dbg << "^ Complete:\n";
	root_module->add_import("sys");
	root_module->add_import("io");
	walk_expression(walk, root_module->root_context, root_module->source.root_tree);
	walk.pass_reset();
	walk.pass2 = true;
	dbg << "^ Pass 2:\n";
	walk_expression(walk, root_module->root_context, root_module->source.root_tree);
	dbg << "^ Result:\n";
	show_string_table(dbg, *root_module);
	show_scopes(dbg, *root_module);
	return root_module;
}

void ScriptContext::LoadScriptFile(const string_view file_path, const string_view into_name) {
	// load contents of script file at "file_path", put into namespace "into_name"
	auto dbg_file = std::fstream("./debug.txt", std::ios_base::out | std::ios_base::trunc);
	auto &dbg = dbg_file;
	std::string file_source;
	if(!LoadFileV(file_path, file_source)) return;
	module_ptr root_module = compile_sourcefile(dbg, std::move(file_source));
	show_string_table(dbg, *root_module);
	show_scopes(dbg, *root_module);
	this->script_map[string(into_name)] = root_module;
}

struct StackFrame;
struct UpScope {
	std::shared_ptr<UpScope> up;
	std::vector<Variable> vars;
};
struct VMFunction : VMVar {
	std::shared_ptr<UpScope> up;
	size_t scope_id;
	VMFunction(std::shared_ptr<UpScope> _up, size_t _id)
		: up{_up}, scope_id{_id} {}
	VarType get_type() const { return VarType::Function; }
};
struct StackFrame {
	std::shared_ptr<FrameContext> context;
	std::shared_ptr<UpScope> scope;
	std::vector<Instruction>::const_iterator current_instruction;
	size_t first_arg_pos;
	size_t first_var_pos;
	StackFrame(
		std::shared_ptr<FrameContext> ctx,
		std::shared_ptr<UpScope> up_ptr)
		: scope{std::make_shared<UpScope>(UpScope{up_ptr})}, context{ctx},
		first_arg_pos{0}, first_var_pos{0} { }
};

struct DisplayVar {
	const Variable &v;
	const std::vector<std::shared_ptr<VMVar>> &table;
};
std::ostream &operator<<(std::ostream &os, const DisplayVar &v) {
	size_t vtype = static_cast<size_t>(v.v.vtype);
	if(vtype >= variable_type_names_span.size()) vtype = 0;
	os << "{";
	switch(v.v.vtype) {
	case VarType::Bool:
		os << (v.v.value == 0 ? "False" : "True");
		break;
	case VarType::Integer:
		os << static_cast<int64_t>(v.v.value);
		break;
	case VarType::Function: {
		auto ptr = v.table[v.v.value].get();
		if(ptr->get_type() == VarType::Function) {
			auto f_ptr = static_cast<VMFunction*>(ptr);
			os << "Fn:" << f_ptr->scope_id;
		} else {
			os << "Fn!=" << variable_type_names[static_cast<size_t>(ptr->get_type())];
		}
		break;
	}
	case VarType::Unset:
	case VarType::Unit:
		os << variable_type_names[vtype];
		break;
	}
	os << "}";
	return os;
}

void ScriptContext::FunctionCall(const string_view func_name) {
	auto found = std::ranges::find_if(script_map.cbegin(), script_map.cend(), [&](auto &v) { return func_name == v.first; } );
	if(found == this->script_map.cend()) {
		std::cerr << "call to non-existant function: " << func_name << '\n';
		return;
	}
	auto dbg_file = std::fstream("./debug-run.txt", std::ios_base::out | std::ios_base::trunc);
	auto &dbg = dbg_file;
	auto number_of_instructions_run = 0;
	auto current_module = found->second;
	auto current_context = current_module->root_context;
	auto current_instruction = current_context->instructions.cbegin();
	auto end_of_instructions = current_context->instructions.cend();
	std::vector<std::shared_ptr<StackFrame>> scope_stack;
	std::vector<std::shared_ptr<VMVar>> var_table;
	std::vector<Variable> value_stack;
	auto current_frame = std::make_shared<StackFrame>(current_context, std::shared_ptr<UpScope>());
	current_frame->scope->vars.resize(current_context->closed_declarations.size());
	value_stack.resize(current_frame->first_arg_pos + current_context->arg_declarations.size());
	current_frame->first_var_pos = value_stack.size();
	value_stack.resize(current_frame->first_var_pos + current_context->var_declarations.size());
	Variable accumulator;
	auto show_accum = [&]() {
		dbg << "A: " << DisplayVar{accumulator, var_table} << '\n';
	};
	auto show_args = [&]() {
		dbg << "Args:";
		if(value_stack.empty() || current_frame->first_var_pos == current_frame->first_arg_pos) {
			dbg << "<E!>";
		} else {
			auto begin = value_stack.cbegin();
			auto end = begin + current_frame->first_var_pos;
			for(auto v = begin + current_frame->first_arg_pos; v != end; v++) {
				dbg << " " << DisplayVar{*v, var_table};
			}
		}
		dbg << '\n';
	};
	auto show_vars = [&]() {
		dbg << " A: " << DisplayVar{accumulator, var_table};
		auto local_size = current_frame->context->var_declarations.size();
		auto close_size = current_frame->context->closed_declarations.size();
		dbg << "\n Vars:";
		if(value_stack.empty() || close_size == 0) {
			dbg << "<E!>";
		} else {
			for(auto &v : current_frame->scope->vars) {
				dbg << " " << DisplayVar{v, var_table};
			}
		}
		dbg << "\n Locals:";
		auto stack_itr = value_stack.cbegin() + current_frame->first_var_pos;
		if(current_frame->first_var_pos >= value_stack.size()) {
			stack_itr = value_stack.cend();
		}
		if(value_stack.empty() || local_size == 0) {
			dbg << "<E!>";
		} else {
			auto locals_end = current_frame->first_var_pos + local_size >= value_stack.size() ?
				value_stack.cend() : stack_itr + local_size;
			for(; stack_itr != locals_end; stack_itr++) {
				dbg << " " << DisplayVar{*stack_itr, var_table};
			}
		}
		dbg << "\n Stack:";
		if(stack_itr == value_stack.cend()) dbg << "<E!>";
		else for(;stack_itr != value_stack.cend(); stack_itr++) { dbg << " " << DisplayVar{*stack_itr, var_table}; }
		dbg << '\n';
	};
	auto pop_value = [&]() {
		auto value = value_stack.back();
		value_stack.pop_back();
		return std::move(value);
	};
	for(;;) {
		if(current_instruction == end_of_instructions) {
			if(!scope_stack.empty()) {
				show_vars();
				value_stack.resize(current_frame->first_arg_pos);
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
		dbg << "EXEC: [" << current_instruction - first_instruction << "]" << *current_instruction << '\n';
		switch(current_instruction->opcode) {
		case Opcode::ExitFunction: {
			current_instruction = end_of_instructions;
			continue;
		}
		case Opcode::CallExpression: {
			size_t var_index = accumulator.value >> 32;
			if(accumulator.vtype != VarType::Function || var_index >= var_table.size()) {
				dbg << "call to non-function value\n";
				show_vars();
				return;
			}
			auto func_var = var_table[var_index];
			if(func_var->get_type() != VarType::Function) {
				dbg << "broken function reference\n";
				show_vars();
				return;
			}
			auto func_ptr = static_cast<VMFunction*>(func_var.get());
			size_t frame_index = func_ptr->scope_id;
			if(frame_index >= current_module->frames.size()) {
				dbg << "call to non-existant function value\n";
				show_vars();
				return;
			}
			auto closure_frame_ptr = func_ptr->up;
			auto next_context = current_module->frames[frame_index];
			current_frame->current_instruction = current_instruction;
			auto prev_frame = current_frame.get();
			scope_stack.push_back(current_frame);
			current_frame = std::make_shared<StackFrame>(next_context, closure_frame_ptr);
			current_frame->scope->vars.resize(next_context->closed_declarations.size());
			// take the argument's values from the stack
			// and actually give them to the function
			size_t end_prev_frame_vars =
				prev_frame->first_var_pos + prev_frame->context->var_declarations.size();
			size_t stack_size = value_stack.size();
			if(param > stack_size) {
				dbg << "stack underflow!\n";
				return;
			}
			if(end_prev_frame_vars > stack_size) {
				dbg << "the value stack is broken!\n";
				return;
			}
			if(param > stack_size - end_prev_frame_vars) {
				dbg << "stack underflow!\n";
				return;
			}
			current_frame->first_arg_pos = stack_size - param;
			current_frame->first_var_pos = stack_size;
			value_stack.resize(stack_size + next_context->var_declarations.size());
			current_instruction = next_context->instructions.cbegin();
			end_of_instructions = next_context->instructions.cend();
			current_context = next_context;
			show_args();
			show_vars();
			continue;
		}
		case Opcode::LoadClosure: {
			dbg << "load closure: " << param << " capture scope variables\n";
			// param is scope_id
			size_t current_var = var_table.size();
			var_table.emplace_back(std::make_shared<VMFunction>(current_frame->scope, param));
			accumulator = Variable{current_var, VarType::Function};
			show_accum();
			break;
		}
		case Opcode::NamedLookup: {
			dbg << "named lookup: \"" << current_module->string_table[ins->param_a] << "\" not implemented\n";
			return;
			break;
		}
		case Opcode::NamedArgLookup: {
			if(accumulator.vtype != VarType::Integer
				|| accumulator.value >= current_frame->first_var_pos - current_frame->first_arg_pos) {
				dbg << "arg ref: bad time or number";
				return;
			}
			accumulator = value_stack[current_frame->first_arg_pos + accumulator.value];
			show_accum();
			break;
		}
		case Opcode::LoadConst:
			accumulator = Variable{param, VarType::Integer};
			break;
		case Opcode::LoadVariable: {
			uint32_t up_count = ins->param_a;
			dbg << "load variable: " << up_count << "," << ins->param_b << ": ";
			auto search_context = current_frame->scope.get();
			for(;up_count != 0; up_count--) {
				search_context = search_context->up.get();
				if(search_context == nullptr) {
					dbg << "frame depth error\n";
					return;
				}
			}
			if(ins->param_b >= search_context->vars.size()) {
				dbg << "reference out of bounds\n";
				return;
			}
			// found variable case
			accumulator = search_context->vars[ins->param_b];
			show_accum();
			break;
		}
		case Opcode::StoreVariable: {
			uint32_t up_count = ins->param_a;
			dbg << "store variable: " << up_count << "," << ins->param_b << ": ";
			auto search_context = current_frame->scope.get();
			for(;up_count != 0; up_count--) {
				search_context = search_context->up.get();
				if(search_context == nullptr) {
					dbg << "frame depth error\n";
					return;
				}
			}
			if(ins->param_b >= search_context->vars.size()) {
				dbg << "reference out of bounds\n";
				return;
			}
			// found variable case
			search_context->vars[ins->param_b] = accumulator;
			show_vars();
			break;
		}
		case Opcode::LoadLocal: {
			size_t pos = current_frame->first_var_pos + ins->param_b;
			dbg << "load local: " << pos << "," << ins->param_b << ": ";
			if(pos >= value_stack.size()) {
				dbg << "reference out of bounds\n";
				return;
			}
			accumulator = value_stack[pos];
			show_args();
			show_accum();
			break;
		}
		case Opcode::StoreLocal: {
			uint32_t pos = current_frame->first_var_pos + ins->param_b;
			dbg << "assign local variable: [" << pos << "]" << '\n';
			if(pos >= value_stack.size()) {
				value_stack.resize(pos + 1);
			}
			value_stack.at(pos) = accumulator;
			show_vars();
			break;
		}
		case Opcode::LoadArg: {
			uint32_t pos = current_frame->first_arg_pos + ins->param_b;
			dbg << "load arg: " << current_frame->first_arg_pos << "+" << ins->param_b << ": ";
			if(pos >= value_stack.size()) {
				accumulator = Variable{};
			} else {
				accumulator = value_stack[pos];
			}
			show_args();
			show_accum();
			break;
		}
		case Opcode::PushRegister:
			value_stack.push_back(accumulator);
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
			if(param >= value_stack.size()) {
				accumulator = Variable{};
			} else {
				accumulator = *(value_stack.crbegin() + param);
			}
			break;
		case Opcode::StoreStack:
			if(param < value_stack.size()) {
				*(value_stack.rbegin() + param) = accumulator;
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
		case Opcode::CompareNotEqual: {
			auto lh = pop_value();
			accumulator = Variable{
				(lh.vtype != accumulator.vtype
				&& lh.value != accumulator.value
				) ? uint64_t{1} : uint64_t{0}
				, VarType::Bool};
			show_vars();
			break;
		}
		case Opcode::CompareEqual: {
			auto lh = pop_value();
			accumulator = Variable{
				(lh.vtype == accumulator.vtype
				&& lh.value == accumulator.value
				) ? uint64_t{1} : uint64_t{0}
				, VarType::Bool};
			show_vars();
			break;
		}
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
		case Opcode::JumpElse:
			if(param > current_context->instructions.size()) {
				param = current_context->instructions.size();
			}
			if(accumulator.value != 0) break;
			current_instruction = first_instruction + param;
			continue;
		case Opcode::LoadBool:
			accumulator = Variable{param, VarType::Bool};
			break;
		case Opcode::LoadUnit:
			accumulator = Variable{0, VarType::Unit};
			break;
		case Opcode::ExitScope: {
			dbg << "TODO ExitScope\n";
			show_vars();
			break;
		}
		default:
			dbg << "not implemented: " << current_instruction->opcode << '\n';
			return;
		}
		current_instruction++;
		if(++number_of_instructions_run > 1000) {
			dbg << "instruction limit reached, script terminated.\n";
			return;
		}
	}
}

}
