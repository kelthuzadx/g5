//===---------------------------------------------------------------------------------------===//
// g5 : golang compiler and runtime in 5 named functions
// Copyright (C) 2018 racaljk<1948638989@qq.com>.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the 
// GNU General Public License as published by the Free Software Foundation, either version 3 of 
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
// General Public License for more details. You should have received a copy of the GNU General 
// Public License along with this program.  If not, see <https://www.gnu.org/licenses/>.
//===---------------------------------------------------------------------------------------===//
#include <cstdio>
#include <exception>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <optional>
#define LAMBDA_FUN(X) function<Ast##X*(Token&)> parse##X;
#define REPORT_ERR(PRE,STR) \
fprintf(stderr,"%s:%s at line %d, col %d\n",#PRE,#STR,line,column);\
exit(EXIT_FAILURE);
#define _ND :public AstNode
using namespace std;

//===---------------------------------------------------------------------------------------===//
// global data
//===---------------------------------------------------------------------------------------===//
string keywords[] = { "break",    "default",     "func",   "interface", "select",
                     "case",     "defer",       "go",     "map",       "struct",
                     "chan",     "else",        "goto",   "package",   "switch",
                     "const",    "fallthrough", "if",     "range",     "type",
                     "continue", "for",         "import", "return",    "var" };
static int line = 1, column = 1, lastToken = 0, shouldEof = 0, nestLev = 0;
static struct goruntime {} grt;
auto anyone = [](auto&& k, auto&&... args) ->bool { return ((args == k) || ...); };
//===---------------------------------------------------------------------------------------===//
// various declarations 
//===---------------------------------------------------------------------------------------===//
#pragma region GlobalDecl
enum TokenType : signed int {
    INVALID = 0, KW_break, KW_default, KW_func, KW_interface, KW_select, KW_case, KW_defer, KW_go, 
    KW_map, KW_struct, KW_chan, KW_else, KW_goto, KW_package, KW_switch, KW_const, KW_fallthrough, 
    KW_if, KW_range, KW_type, KW_continue, KW_for, KW_import, KW_return, KW_var, OP_ADD, OP_BITAND,
    OP_ADDAGN, OP_SHORTAGN, OP_AND, OP_EQ, OP_NE, OP_LPAREN, OP_RPAREN, OP_SUB, OP_BITOR, OP_SUBAGN,
    OP_BITORAGN, OP_OR, OP_LT, OP_LE, OP_LBRACKET, OP_RBRACKET, OP_MUL, OP_XOR, OP_MULAGN, OP_BITXORAGN,
    OP_CHAN, OP_GT, OP_GE, OP_LBRACE, OP_RBRACE, OP_DIV, OP_LSHIFT, OP_DIVAGN, OP_LSFTAGN, OP_INC, 
    OP_AGN, OP_BITANDAGN, OP_COMMA, OP_SEMI, OP_MOD, OP_RSHIFT, OP_MODAGN, OP_RSFTAGN, OP_DEC, OP_NOT,
    OP_VARIADIC, OP_DOT, OP_COLON, OP_ANDXOR, OP_ANDXORAGN, TK_ID, LIT_INT, LIT_FLOAT, LIT_IMG, 
    LIT_RUNE, LIT_STR, TK_EOF = -1,
};
//todo: add destructor for these structures
// Common
struct AstExpr;
struct AstNode { virtual ~AstNode() = default; };
struct AstIdentList _ND { vector<string> identList; };
struct AstExprList _ND { vector<AstExpr*> exprList; };
struct AstStmtList _ND { vector<AstNode*> stmtList; };

// Declaration
struct AstImportDecl _ND { map<string, string> imports; };
struct AstConstDecl _ND {
    vector<AstIdentList*> identList;
    vector<AstNode*> type;
    vector<AstExprList*> exprList;
};
struct AstTypeSpec _ND { string ident; AstNode* type; };
struct AstTypeDecl _ND { vector<AstTypeSpec*> typeSpec; };
struct AstVarDecl _ND { vector<AstNode*> varSpec; };
struct AstVarSpec _ND {
    AstNode* identList{};
    AstExprList* exprList{};
    AstNode* type{};
};
struct AstFuncDecl _ND {
    string funcName;
    AstNode* receiver{};
    AstNode* signature{};
    AstStmtList* funcBody{};
};
struct AstCompilationUnit _ND {
    string package;
    vector<AstImportDecl*> importDecl;
    vector<AstConstDecl*> constDecl;
    vector<AstTypeDecl*> typeDecl;
    vector<AstFuncDecl*> funcDecl;
    vector<AstVarDecl*> varDecl;
};
// Type
struct AstType _ND { AstNode* type{}; };
struct AstName _ND { string name; };
struct AstArrayType _ND {
    AstNode* length{};
    AstNode* elemType{};
    bool automaticLen;
};
struct AstStructType _ND {
    vector<tuple<AstNode*, AstNode*, string, bool>> fields;// <IdentList/Name,Type,Tag,isEmbeded>
};
struct AstPtrType _ND { AstNode* baseType{}; };
struct AstSignature _ND {
    AstNode* param{};
    AstNode* resultParam{};
    AstNode* resultType{};
};
struct AstFuncType _ND { AstSignature * signature{}; };
struct AstParam _ND { vector<AstNode*> paramList; };
struct AstParamDecl _ND {
    bool isVariadic = false;
    bool hasName = false;
    AstNode* type{};
    string name;
};
struct AstInterfaceType _ND { vector<tuple<AstName*,AstSignature*>> method; };
struct AstSliceType _ND { AstNode* elemType{}; };
struct AstMapType _ND { AstNode* keyType{}; AstNode* elemType{}; };
struct AstChanType _ND { AstNode* elemType{}; };
// Statement
struct AstGoStmt _ND { AstExpr* expr{}; AstGoStmt(AstExpr* expr) :expr(expr) {} };
struct AstReturnStmt _ND { AstExprList* exprList{}; AstReturnStmt(AstExprList* el) :exprList(el) {} };
struct AstBreakStmt _ND { string label; AstBreakStmt(const string&s) :label(s) {} };
struct AstDeferStmt _ND { AstExpr* expr{}; AstDeferStmt(AstExpr* expr) :expr(expr) {} };
struct AstContinueStmt _ND { string label; AstContinueStmt(const string&s) :label(s) {} };
struct AstGotoStmt _ND { string label; AstGotoStmt(const string&s) :label(s) {} };
struct AstFallthroughStmt _ND {};
struct AstLabeledStmt _ND {
    string label;
    AstNode* stmt{};
};
struct AstIfStmt _ND {
    AstNode* init{};
    AstExpr* cond{};
    AstNode* ifBlock{}, *elseBlock{};
};
struct AstSwitchCase _ND {
    AstNode* exprList{};
    AstNode* stmtList{};
};
struct AstSwitchStmt _ND {
    AstNode* init{};
    AstNode* cond{};
    vector<AstSwitchCase*> caseList{};
};
struct AstSelectCase _ND {
    AstNode* cond{};
    AstStmtList* stmtList{};
};
struct AstSelectStmt _ND {
    vector<AstSelectCase*> caseList;
};
struct AstForStmt _ND {
    AstNode* init{}, *cond{}, *post{};
    AstNode* block{};
};
struct AstSRangeClause _ND {
    vector<string> lhs;
    AstExpr* rhs{};
};
struct AstRangeClause _ND {
    AstExprList* lhs{};
    TokenType op;
    AstExpr* rhs{};
};
struct AstExprStmt _ND { AstExpr* expr{}; };
struct AstSendStmt _ND { AstExpr* receiver{}, *sender{}; };
struct AstIncDecStmt _ND {
    AstExpr* expr{};
    bool isInc{};
};
struct AstAssignStmt _ND {
    AstExprList* lhs{}, *rhs{};
    TokenType op{};
};
struct AstSAssignStmt _ND {//short assign
    vector<string> lhs{};
    AstExprList* rhs{};
};
// Expression
struct AstPrimaryExpr _ND {
    AstNode* expr{};//one of SelectorExpr, TypeSwitchExpr,TypeAssertionExpr,IndexExpr,SliceExpr,CallExpr,Operand
};
struct AstUnaryExpr _ND {
    AstNode*expr{};
    TokenType op{};
};
struct AstExpr _ND {
    AstUnaryExpr* lhs{};
    TokenType op{};
    AstExpr* rhs{};
};
struct AstSelectorExpr _ND {
    AstNode* operand{};
    string selector;
};
struct AstTypeSwitchExpr _ND {
    AstNode* operand{};
};
struct AstTypeAssertionExpr _ND {
    AstNode* operand{};
    AstNode* type{};
};
struct AstIndexExpr _ND {
    AstNode* operand{};
    AstNode* index{};
};
struct AstSliceExpr _ND {
    AstNode* operand{};
    AstNode* begin{};
    AstNode* end{};
    AstNode* step{};
};
struct AstCallExpr _ND {
    AstNode* operand{};
    AstNode* arguments{};
    AstNode* type{};
    bool isVariadic{};
};
struct AstKeyedElement _ND {
    AstNode*key{};
    AstNode*element{};
};
struct AstLitValue _ND { vector<AstKeyedElement*> keyedElement; };
struct AstBasicLit _ND { 
    TokenType type{}; string value; 
    AstBasicLit(TokenType t, const string&s) :type(t), value(s) {} 
};
struct AstCompositeLit _ND { AstNode* litName{}; AstLitValue* litValue{}; };
struct Token { 
    TokenType type{}; string lexeme;
    Token(TokenType t, string e) :type(t), lexeme(e) { lastToken = t; }
};
#pragma endregion
//===---------------------------------------------------------------------------------------===//
// Implementation of golang compiler and runtime within 5 explicit functions
//===---------------------------------------------------------------------------------------===//
Token next(fstream& f) {
    auto consumePeek = [&](char& c) {
        f.get();
        column++;
        char oc = c;
        c = static_cast<char>(f.peek());
        return oc;
    };
    auto c = static_cast<char>(f.peek());
skip_comment_and_find_next:
    for (; anyone(c, ' ', '\r', '\t', '\n'); column++) {
        if (c == '\n') {
            line++;
            column = 1;
            if (anyone(lastToken, TK_ID, LIT_INT, LIT_FLOAT, LIT_IMG, LIT_RUNE, LIT_STR, KW_fallthrough,
                KW_continue, KW_return, KW_break, OP_INC, OP_DEC, OP_RPAREN, OP_RBRACKET, OP_RBRACE)) {
                consumePeek(c);
                return Token(OP_SEMI, ";");
            }
        }
        consumePeek(c);
    }
    if (f.eof()) {
        if (shouldEof) {
            return Token(TK_EOF, "");
        }
        shouldEof = 1;
        return Token(OP_SEMI, ";");
    }

    string lexeme;
    // identifier = letter { letter | unicode_digit } .
    if (c >= 'a'&&c <= 'z' || c >= 'A'&&c <= 'Z' || c == '_') {
        while (c >= 'a'&&c <= 'z' || c >= 'A'&&c <= 'Z' || c >= '0'&&c <= '9' || c == '_') {
            lexeme += consumePeek(c);
        }

        for (int i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
            if (keywords[i] == lexeme) {
                return Token(static_cast<TokenType>(i + 1), lexeme);
            }
        return Token(TK_ID, lexeme);
    }

    // int_lit     = decimal_lit | octal_lit | hex_lit .
    // decimal_lit = ( "1" … "9" ) { decimal_digit } .
    // octal_lit   = "0" { octal_digit } .
    // hex_lit     = "0" ( "x" | "X" ) hex_digit { hex_digit } .

    // float_lit = decimals "." [ decimals ] [ exponent ] |
    //         decimals exponent |
    //         "." decimals [ exponent ] .
    // decimals  = decimal_digit { decimal_digit } .
    // exponent  = ( "e" | "E" ) [ "+" | "-" ] decimals .

    // imaginary_lit = (decimals | float_lit) "i" .
    if (c >= '0'&&c <= '9' || c == '.') {
        if (c == '0') {
            lexeme += consumePeek(c);
            if (c == 'x' || c == 'X') {
                do {
                    lexeme += consumePeek(c);
                } while (c >= '0'&&c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F');
                return Token(LIT_INT, lexeme);
            } else if ((c >= '0' && c <= '9') || anyone(c, '.', 'e', 'E', 'i')) {
                while ((c >= '0' && c <= '9') || anyone(c, '.', 'e', 'E', 'i')) {
                    if (c >= '0' && c <= '7') {
                        lexeme += consumePeek(c);
                    } else {
                        goto shall_float;
                    }
                }
                return Token(LIT_INT, lexeme);
            }
            goto may_float;
        } else {  // 1-9 or . or just a single 0
        may_float:
            TokenType type = LIT_INT;
            if (c == '.') {
                lexeme += consumePeek(c);
                if (c == '.') {
                    lexeme += consumePeek(c);
                    if (c == '.') {
                        lexeme += consumePeek(c);
                        return Token(OP_VARIADIC, lexeme);
                    } else { REPORT_ERR("lex error", "expect variadic notation(...)"); }
                } else if (c >= '0'&&c <= '9') {
                    type = LIT_FLOAT;
                } else {
                    return Token(OP_DOT, lexeme);
                }
                goto shall_float;
            } else if (c >= '1'&&c <= '9') {
                lexeme += consumePeek(c);
            shall_float:  // skip char consuming and appending since we did that before jumping here;
                bool hasDot = false, hasExponent = false;
                while ((c >= '0' && c <= '9') || anyone(c, '.', 'e', 'E', 'i')) {
                    if (c >= '0' && c <= '9') {
                        lexeme += consumePeek(c);
                    } else if (c == '.' && !hasDot) {
                        lexeme += consumePeek(c);
                        type = LIT_FLOAT;
                    } else if ((c == 'e' && !hasExponent) ||
                        (c == 'E' && !hasExponent)) {
                        hasExponent = true;
                        type = LIT_FLOAT;
                        lexeme += consumePeek(c);
                        if (c == '+' || c == '-') {
                            lexeme += consumePeek(c);
                        }
                    } else {
                        f.get();
                        column++;
                        lexeme += c;
                        return Token(LIT_IMG, lexeme);
                    }
                }
                return Token(type, lexeme);
            } else {
                return Token(type, lexeme);
            }
        }
    }
    // rune_lit         = "'" ( unicode_value | byte_value ) "'" .
    // unicode_value    = unicode_char | little_u_value | big_u_value |
    // escaped_char . byte_value       = octal_byte_value | hex_byte_value .
    // octal_byte_value = `\` octal_digit octal_digit octal_digit .
    // hex_byte_value   = `\` "x" hex_digit hex_digit .
    // little_u_value   = `\` "u" hex_digit hex_digit hex_digit hex_digit .
    // big_u_value      = `\` "U" hex_digit hex_digit hex_digit hex_digit
    //                            hex_digit hex_digit hex_digit hex_digit .
    // escaped_char     = `\` ( "a" | "b" | "f" | "n" | "r" | "t" | "v" | `\` |
    // "'" | `"` ) .
    if (c == '\'') {
        lexeme += consumePeek(c);
        if (c == '\\') {
            lexeme += consumePeek(c);
            if (anyone(c, 'U', 'u', 'X', 'x'))
                do lexeme += consumePeek(c); while (c >= '0'&&c <= '9' || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
            else if (c >= '0' && c <= '7')
                do lexeme += consumePeek(c); while (c >= '0' && c <= '7');
            else if (anyone(c, 'a', 'b', 'f', 'n', 'r', 't', 'v', '\\', '\'', '"'))
                lexeme += consumePeek(c);
            else { REPORT_ERR("lex error", "illegal rune"); }
        } else lexeme += consumePeek(c);

        if (c != '\'') {
            REPORT_ERR("lex error", "illegal rune at least in current implementation of g8");
        }
        lexeme += consumePeek(c);
        return Token(LIT_RUNE, lexeme);
    }

    // string_lit             = raw_string_lit | interpreted_string_lit .
    // raw_string_lit         = "`" { unicode_char | newline } "`" .
    // interpreted_string_lit = `"` { unicode_value | byte_value } `"` .
    if (c == '`') {
        do {
            lexeme += consumePeek(c);
            if (c == '\n') line++;
        } while (f.good() && c != '`');
        if (c != '`') {
            REPORT_ERR("lex error", "raw string literal does not have a closed symbol \"`\"");
        }
        lexeme += consumePeek(c);
        return Token(LIT_STR, lexeme);
    } else if (c == '"') {
        do {
            lexeme += consumePeek(c);
            if (c == '\\') {
                lexeme += consumePeek(c);
                lexeme += consumePeek(c);
            }
        } while (f.good() && (c != '\n' && c != '\r' && c != '"'));
        if (c != '"') {
            REPORT_ERR("lex error", "string literal does not have a closed symbol");
        }
        lexeme += consumePeek(c);
        return Token(LIT_STR, lexeme);
    }

    auto match = [&](
        initializer_list<
        tuple<
        pair<char, TokenType>,
        initializer_list<pair<string_view, TokenType>>,
        pair<string_view, TokenType>>> big) ->Token {
        for (const auto&[v1, v2, v3] : big) {
            if (c == v1.first) {
                lexeme += consumePeek(c);
                for (const auto &[v2str, v2type] : v2) {
                    if (c == v2str[1]) {
                        lexeme += consumePeek(c);
                        if (const auto&[v3str, v3type] = v3; v3type != INVALID) {
                            if (c == v3str[2]) {
                                lexeme += consumePeek(c);
                                return Token(v3type, lexeme);
                            }
                        }
                        return Token(v2type, lexeme);
                    }
                }
                return Token(v1.second, lexeme);
            }
        }

        return Token(INVALID, "");
    };

    // operators
    if(c=='/') { // special case for /  /= // /*...*/
        char pending = consumePeek(c);
        if (c == '=') {
            lexeme += pending;
            lexeme += consumePeek(c);

            return Token(OP_DIVAGN, lexeme);
        } else if (c == '/') {
            do {
                consumePeek(c);
            } while (f.good() && (c != '\n' && c != '\r'));
            goto skip_comment_and_find_next;
        } else if (c == '*') {
            do {
                consumePeek(c);
                if (c == '\n') line++;
                if (c == '*') {
                    consumePeek(c);
                    if (c == '/') {
                        consumePeek(c);
                        goto skip_comment_and_find_next;
                    }
                }
            } while (f.good());
        }
        lexeme += pending;
        return Token(OP_DIV, lexeme);
    }
    auto result = match({
        {{ '+',OP_ADD },    {{"+=",OP_ADDAGN} ,{"++",OP_INC}},                      {}},
        {{'&',OP_BITAND},   {{"&=",OP_BITANDAGN},{"&&",OP_AND},{"&^",OP_ANDXOR}},   {"&^=",OP_ANDXORAGN}},
        {{'=',OP_AGN},      {{"==",OP_EQ}},                                         {}},
        {{'!',OP_NOT},      {{"!=",OP_NE}},                                         {}},
        {{'(',OP_LPAREN},   {},                                                     {}},
        {{')',OP_RPAREN},   {},                                                     {}},
        {{'-',OP_SUB},      {{"-=",OP_SUBAGN},{"--",OP_DEC}},                       {}},
        {{'|',OP_BITOR},    {{"|=",OP_BITORAGN},{"||",OP_OR}},                      {}},
        {{'<',OP_LT},       {{"<=",OP_LE},{"<-",OP_CHAN},{"<<",OP_LSHIFT}},         {"<<=",OP_LSFTAGN}},
        {{'[',OP_LBRACKET}, {},                                                     {}},
        {{']',OP_RBRACKET}, {},                                                     {}},
        {{'*',OP_MUL},      {{"*=",OP_MULAGN}},                                     {}},
        {{'^',OP_XOR},      {{"^=",OP_BITXORAGN}},                                  {}},
        {{'>',OP_GT},       {{">=",OP_GE},{">>",OP_RSHIFT}},                        {">>=",OP_RSFTAGN}},
        {{'{',OP_LBRACE},   {},                                                     {}},
        {{'}',OP_RBRACE},   {},                                                     {}},
        {{':',OP_COLON},    {{":=",OP_SHORTAGN}},                                   {}},
        {{',',OP_COMMA},    {},                                                     {}},
        {{';',OP_SEMI},     {},                                                     {}},
        {{'%',OP_MOD},      {{"%=",OP_MODAGN}},                                     {}},
    });

    if (result.type != INVALID) { return result; }
    else { REPORT_ERR("lex error", "illegal token in source file"); }
}

const AstNode* parse(const string & filename) {
    fstream f(filename, ios::binary | ios::in);
    auto t = next(f);

    auto eat = [&f, &t](TokenType tk, const string&msg) {
        if (t.type != tk) throw runtime_error(msg);
        t = next(f);
    };
    auto eatOptionalSemi = [&]() { if (t.type == OP_SEMI) t = next(f); };

    LAMBDA_FUN(TypeDecl); LAMBDA_FUN(VarDecl); LAMBDA_FUN(ConstDecl); LAMBDA_FUN(LitValue);
    LAMBDA_FUN(ImportDecl); LAMBDA_FUN(Expr); LAMBDA_FUN(Signature); LAMBDA_FUN(UnaryExpr);
    LAMBDA_FUN(PrimaryExpr); LAMBDA_FUN(KeyedElement); LAMBDA_FUN(TypeSpec); LAMBDA_FUN(SwitchStmt);
    LAMBDA_FUN(SelectCase); LAMBDA_FUN(SwitchCase);
    function<AstFuncDecl*(bool, Token&)> parseFuncDecl;
    function<AstNode*(AstExprList *, Token&)> parseSimpleStmt;
    function<AstNode*(Token&)> parseTypeAssertion, parseType, parseArrayOrSliceType, parseStructType, parsePtrType,
        parseFuncType, parseParam, parseParamDecl, parseInterfaceType, parseMapType, parseChanType,
        parseVarSpec, parseStmt, parseIfStmt, parseSelectStmt, parseForStmt, parseOperand, parseKey;

#pragma region Common
    auto parseName = [&](bool couldFullName, Token&t)->AstName* {
        AstName * node{};
        if (t.type == TK_ID) {
            node = new AstName;
            string name;
            name += t.lexeme;
            t = next(f);
            if (couldFullName && t.type == OP_DOT) {
                t = next(f);
                name.operator+=(".").operator+=(t.lexeme);
                t = next(f);
            }
            node->name = name;
        }
        return node;
    };
    auto parseIdentList = [&](Token&t)->AstIdentList* {
        AstIdentList* node{};
        if (t.type == TK_ID) {
            node = new  AstIdentList;
            node->identList.emplace_back(t.lexeme);
            t = next(f);
            while (t.type == OP_COMMA) {
                t = next(f);
                node->identList.emplace_back(t.lexeme);
                t = next(f);
            }
        }
        return node;
    };
    auto parseExprList = [&](Token&t)->AstExprList* {
        AstExprList* node{};
        if (auto* tmp = parseExpr(t); tmp != nullptr) {
            node = new  AstExprList;
            node->exprList.emplace_back(tmp);
            while (t.type == OP_COMMA) {
                t = next(f);
                node->exprList.emplace_back(parseExpr(t));
            }
        }
        return node;
    };
    auto parseStmtList = [&](Token&t)->AstStmtList* {
        AstStmtList * node{};
        AstNode* tmp = nullptr;
        while ((tmp = parseStmt(t))) {
            if (node == nullptr) {
                node = new AstStmtList;
            }
            node->stmtList.push_back(tmp);
            eatOptionalSemi();
        }
        return node;
    };
    auto parseBlock = [&](Token&t)->AstStmtList* {
        AstStmtList * node{};
        if (t.type == OP_LBRACE) {
            t = next(f);
            if (t.type != OP_RBRACE) {
                node = parseStmtList(t);
                eat(OP_RBRACE, "expect } around code block");
            } else {
                t = next(f);
            }
        }
        return node;
    };
#pragma endregion
#pragma region Declaration
    parseImportDecl = [&](Token&t)->AstImportDecl* {
        auto node = new AstImportDecl;
        eat(KW_import, "it should be import declaration");
        if (t.type == OP_LPAREN) {
            t = next(f);
            do {
                string importName, alias;
                if (t.type == OP_DOT || t.type == TK_ID) {
                    alias = t.lexeme;
                    t = next(f);
                    importName = t.lexeme;
                }
                else {
                    importName = t.lexeme;
                }
                importName = importName.substr(1, importName.length() - 2);
                node->imports[importName] = alias;
                t = next(f);
                eatOptionalSemi();
            } while (t.type != OP_RPAREN);
            eat(OP_RPAREN, "expect )");
        }
        else {
            string importName, alias;
            if (t.type == OP_DOT || t.type == TK_ID) {
                alias = t.lexeme;
                t = next(f);
                importName = t.lexeme;
                t = next(f);
            }
            else {
                importName = t.lexeme;
                t = next(f);
            }
            importName = importName.substr(1, importName.length() - 2);
            node->imports[importName] = alias;
        }
        return node;
    };
    parseConstDecl = [&](Token&t)->AstConstDecl* {
        auto * node = new AstConstDecl;
        eat(KW_const, "it should be const declaration");
        if (t.type == OP_LPAREN) {
            t = next(f);
            do {
                node->identList.push_back(parseIdentList(t));
                if (auto*tmp = parseType(t); tmp != nullptr) {
                    node->type.push_back(tmp);
                }
                else {
                    node->type.push_back(nullptr);
                }
                if (t.type == OP_AGN) {
                    t = next(f);
                    node->exprList.push_back(parseExprList(t));
                }
                else {
                    node->exprList.push_back(nullptr);
                }
                eatOptionalSemi();
            } while (t.type != OP_RPAREN);
            eat(OP_RPAREN, "eat right parenthesis");
        }
        else {
            node->identList.push_back(parseIdentList(t));
            if (auto*tmp = parseType(t); tmp != nullptr) {
                node->type.push_back(tmp);
            }
            else {
                node->type.push_back(nullptr);
            }
            if (t.type == OP_AGN) {
                t = next(f);
                node->exprList.push_back(parseExprList(t));
            }
            else {
                node->exprList.push_back(nullptr);
            }
            if (t.type != OP_SEMI) throw runtime_error("expect an explicit semicolon");
        }

        return node;
    };
    parseTypeDecl = [&](Token&t)->AstTypeDecl* {
        auto * node = new AstTypeDecl;
        eat(KW_type, "it should be type declaration");
        if (t.type == OP_LPAREN) {
            t = next(f);
            do {
                node->typeSpec.push_back(parseTypeSpec(t));
                eatOptionalSemi();
            } while (t.type != OP_RPAREN);
            eat(OP_RPAREN, "expect )");
        }
        else {
            node->typeSpec.push_back(parseTypeSpec(t));
        }
        return node;
    };
    parseTypeSpec = [&](Token&t)->AstTypeSpec* {
        AstTypeSpec* node{};
        if (t.type == TK_ID) {
            node = new AstTypeSpec;
            node->ident = t.lexeme;
            t = next(f);
            if (t.type == OP_AGN) {
                t = next(f);
            }
            node->type = parseType(t);
        }
        return node;
    };
    parseVarDecl = [&](Token&t)->AstVarDecl* {
        auto * node = new AstVarDecl;
        eat(KW_var, "it should be var declaration");
        if (t.type == OP_LPAREN) {
            do {
                node->varSpec.push_back(parseVarSpec(t));
                t = next(f);
            } while (t.type != OP_RPAREN);
            eat(OP_RPAREN, "expect )");
        }
        else {
            node->varSpec.push_back(parseVarSpec(t));
        }

        return node;
    };
    parseVarSpec = [&](Token&t)->AstNode* {
        AstVarSpec* node{};
        if (auto*tmp = parseIdentList(t); tmp != nullptr) {
            node = new AstVarSpec;
            node->identList = tmp;
            if (t.type == OP_AGN) {
                t = next(f);
                node->exprList = parseExprList(t);
            }
            else {
                node->type = parseType(t);
                if (t.type == OP_AGN) {
                    t = next(f);
                    node->exprList = parseExprList(t);
                }
            }
        }
        return node;
    };
    parseFuncDecl = [&](bool anonymous, Token&t)->AstFuncDecl* {
        auto * node = new AstFuncDecl;
        eat(KW_func, "it should be function declaration");
        if (!anonymous) {
            if (t.type == OP_LPAREN) {
                node->receiver = parseParam(t);
            }
            node->funcName = t.lexeme;
            t = next(f);
        }
        node->signature = parseSignature(t);
        nestLev++;
        node->funcBody = parseBlock(t);
        nestLev--;
        return node;
    };
    parseFuncType = [&](Token&t)->AstNode* {
        AstFuncType* node{};
        node = new AstFuncType;
        t = next(f);
        node->signature = parseSignature(t);
        return node;
    };
    parseSignature = [&](Token&t)->AstSignature* {
        AstSignature* node{};
        if (t.type == OP_LPAREN) {
            node = new AstSignature;
            node->param = parseParam(t);
            if (auto*result = parseParam(t); result != nullptr) {
                node->resultParam = result;
            } else  if (auto*result = parseType(t); result != nullptr) {
                node->resultType = result;
            } 
        }
        return node;
    };
    parseParam = [&](Token&t)->AstNode* {
        AstParam* node{};
        if (t.type == OP_LPAREN) {
            node = new AstParam;
            t = next(f);
            do {
                if (auto * tmp = parseParamDecl(t); tmp != nullptr) {
                    node->paramList.push_back(tmp);
                }
                if (t.type == OP_COMMA) {
                    t = next(f);
                }
            } while (t.type != OP_RPAREN);
            t = next(f);

            for (int i = 0, rewriteStart = 0; i < node->paramList.size(); i++) {
                if (dynamic_cast<AstParamDecl*>(node->paramList[i])->hasName) {
                    for (int k = rewriteStart; k < i; k++) {
                        string name = dynamic_cast<AstName*>(
                            dynamic_cast<AstParamDecl*>(node->paramList[k])->type)->name;
                        dynamic_cast<AstParamDecl*>(node->paramList[k])->type = dynamic_cast<AstParamDecl*>(node->paramList[i])->type;
                        dynamic_cast<AstParamDecl*>(node->paramList[k])->name = name;
                        dynamic_cast<AstParamDecl*>(node->paramList[k])->hasName = true; //It's not necessary
                    }
                    rewriteStart = i + 1;
                }
            }
        }
        return node;
    };
    parseParamDecl = [&](Token&t)->AstNode* {
        AstParamDecl* node{};
        if (t.type == OP_VARIADIC) {
            node = new AstParamDecl;
            node->isVariadic = true;
            t = next(f);
            node->type = parseType(t);
        }
        else if (t.type != OP_RPAREN) {
            node = new AstParamDecl;
            auto*mayIdentOrType = parseType(t);
            if (t.type != OP_COMMA && t.type != OP_RPAREN) {
                node->hasName = true;
                if (t.type == OP_VARIADIC) {
                    node->isVariadic = true;
                    t = next(f);
                }
                node->name = dynamic_cast<AstName*>(mayIdentOrType)->name;
                node->type = parseType(t);
            }
            else {
                node->type = mayIdentOrType;
            }
        }
        return node;
    };
#pragma endregion
#pragma region Type
    parseType = [&](Token&t)->AstNode* {
        switch (t.type) {
        case TK_ID:       return parseName(true, t);
        case OP_LBRACKET: return parseArrayOrSliceType(t);
        case KW_struct:   return parseStructType(t);
        case OP_MUL:      return parsePtrType(t);
        case KW_func:     return parseFuncType(t);
        case KW_interface:return parseInterfaceType(t);
        case KW_map:      return parseMapType(t);
        case KW_chan:     return parseChanType(t);
        case OP_LPAREN: {t = next(f); auto*tmp = parseType(t); t = next(f); return tmp; }
        default:return nullptr;
        }
    };
    parseArrayOrSliceType = [&](Token&t)->AstNode* {
        AstNode* node{};
        eat(OP_LBRACKET, "array/slice type requires [ to denote that");
        nestLev++;
        if (t.type != OP_RBRACKET) {
            node = new AstArrayType;
            if (t.type == OP_VARIADIC) {
                dynamic_cast<AstArrayType*>(node)->automaticLen = true;
                t = next(f);
            }
            else {
                dynamic_cast<AstArrayType*>(node)->length = parseExpr(t);
            }
            nestLev--;
            t = next(f);
            dynamic_cast<AstArrayType*>(node)->elemType = parseType(t);
        }
        else {
            node = new AstSliceType;
            nestLev--;
            t = next(f);
            dynamic_cast<AstSliceType*>(node)->elemType = parseType(t);
        }
        return node;
    };
    parseStructType = [&](Token&t)->AstNode* {
        auto * node = new  AstStructType;
        eat(KW_struct, "structure type requires struct keyword");
        eat(OP_LBRACE, "a { is required after struct");
        do {
            tuple<AstNode*, AstNode*, string, bool> field;
            if (auto * tmp = parseIdentList(t); tmp != nullptr) {
                get<0>(field) = tmp;
                get<1>(field) = parseType(t);
                get<3>(field) = false;
            }
            else {
                if (t.type == OP_MUL) {
                    get<3>(field) = true;
                    t = next(f);
                }
                get<0>(field) = parseName(true, t);
            }
            if (t.type == LIT_STR) {
                get<2>(field) = t.lexeme;
            }
            node->fields.push_back(field);
            eatOptionalSemi();
        } while (t.type != OP_RBRACE);
        eat(OP_RBRACE, "expect }");
        eatOptionalSemi();
        return node;
    };
    parsePtrType = [&](Token&t)->AstNode* {
        auto * node = new AstPtrType;
        eat(OP_MUL, "pointer type requires * to denote that");
        node->baseType = parseType(t);
        return node;
    };
    parseInterfaceType = [&](Token&t)->AstNode* {
        auto * node = new AstInterfaceType;
        eat(KW_interface, "interface type requires keyword interface");
        eat(OP_LBRACE, "{ is required after interface");
        while (t.type != OP_RBRACE) {
            if (auto* tmp = parseName(true, t); tmp != nullptr && tmp->name.find('.') == string::npos) {
                node->method.emplace_back(tmp, parseSignature(t));
            }
            else {
                node->method.emplace_back(tmp, nullptr);
            }
            eatOptionalSemi();
        }
        t = next(f);
        return node;
    };
    parseMapType = [&](Token&t)->AstNode* {
        auto * node = new AstMapType;
        eat(KW_map, "map type requires keyword map to denote that");
        eat(OP_LBRACKET, "[ is required after map");
        node->keyType = parseType(t);
        eat(OP_RBRACKET, "] is required after map[Key");
        node->elemType = parseType(t);
        return node;
    };
    parseChanType = [&](Token&t)->AstNode* {
        AstChanType* node{};
        if (t.type == KW_chan) {
            node = new AstChanType;
            t = next(f);
            if (t.type == OP_CHAN) {
                t = next(f);
                node->elemType = parseType(t);
            }
            else {
                node->elemType = parseType(t);
            }
        }
        else if (t.type == OP_CHAN) {
            node = new AstChanType;
            t = next(f);
            if (t.type == KW_chan) {
                node->elemType = parseType(t);
            }
        }
        return node;
    };
#pragma endregion
#pragma region Statement
    parseStmt = [&](Token&t)->AstNode* {
        switch (t.type) {
        case KW_type:       { return parseTypeDecl(t);  }
        case KW_const:      { return parseConstDecl(t);  }
        case KW_var:        { return parseVarDecl(t);  }
        case KW_fallthrough:{t = next(f);  return new AstFallthroughStmt();  }
        case KW_go:         {t = next(f);  return new AstGoStmt(parseExpr(t));  }
        case KW_return:     {t = next(f);  return new AstReturnStmt(parseExprList(t));  }
        case KW_break:      {t = next(f);  return new AstBreakStmt(t.type == TK_ID ? t.lexeme : "");  }
        case KW_continue:   {t = next(f);  return new AstContinueStmt(t.type == TK_ID ? t.lexeme : "");  }
        case KW_goto:       {t = next(f);  return new AstGotoStmt(t.lexeme);  }
        case KW_defer:      {t = next(f);  return new AstDeferStmt(parseExpr(t));  }
        case KW_if:         return parseIfStmt(t);
        case KW_switch:     return parseSwitchStmt(t);
        case KW_select:     return parseSelectStmt(t);
        case KW_for:        return parseForStmt(t);
        case OP_LBRACE:     return parseBlock(t);
        case OP_SEMI:       return nullptr;
        case OP_ADD:case OP_SUB:case OP_NOT:case OP_XOR:case OP_MUL:case OP_CHAN:
        case LIT_STR:case LIT_INT:case LIT_IMG:case LIT_FLOAT:case LIT_RUNE:
        case KW_func:case KW_struct:case KW_map:case OP_LBRACKET:case TK_ID: case OP_LPAREN:
        {
            auto* exprList = parseExprList(t);
            if (t.type == OP_COLON) {
                //it shall a labeled statement(not part of simple stmt so we handle it here)
                t = next(f);
                auto* labeledStmt = new AstLabeledStmt;
                labeledStmt->label = dynamic_cast<AstName*>(
                    dynamic_cast<AstPrimaryExpr*>(
                        exprList->exprList[0]->lhs->expr)->expr)->name;
                labeledStmt->stmt = parseStmt(t);
                return labeledStmt;
            }
            else return parseSimpleStmt(exprList, t);
        }
        }
        return nullptr;
    };
    parseSimpleStmt = [&](AstExprList* lhs, Token&t)->AstNode* {
        if (t.type == KW_range) {    //special case for ForStmt
            auto*stmt = new AstSRangeClause;
            t = next(f);
            stmt->rhs = parseExpr(t);
            return stmt;
        }
        if (lhs == nullptr) lhs = parseExprList(t);

        switch (t.type) {
        case OP_CHAN: {
            if (lhs->exprList.size() != 1) throw runtime_error("one expr required");
            t = next(f);
            auto* stmt = new AstSendStmt;
            stmt->receiver = lhs->exprList[0];
            stmt->sender = parseExpr(t);
            return stmt;
        }
        case OP_INC:case OP_DEC: {
            if (lhs->exprList.size() != 1) throw runtime_error("one expr required");
            auto* stmt = new AstIncDecStmt;
            stmt->isInc = t.type == OP_INC;
            t = next(f);
            stmt->expr = lhs->exprList[0];
            return stmt;
        }
        case OP_SHORTAGN: {
            if (lhs->exprList.empty()) throw runtime_error("one expr required");

            vector<string> identList;
            for (auto* e : lhs->exprList) {
                string identName =
                    dynamic_cast<AstName*>(
                        dynamic_cast<AstPrimaryExpr*>(
                            e->lhs->expr)->expr)->name;

                identList.push_back(identName);
            }
            t = next(f);
            if (t.type == KW_range) {
                t = next(f);
                auto* stmt = new AstSRangeClause;
                stmt->lhs = move(identList);
                stmt->rhs = parseExpr(t);
                return stmt;
            }
            else {
                auto*stmt = new AstSAssignStmt;
                stmt->lhs = move(identList);
                stmt->rhs = parseExprList(t);
                return stmt;
            }
        }
        case OP_ADDAGN:case OP_SUBAGN:case OP_BITORAGN:case OP_BITXORAGN:case OP_MULAGN:case OP_DIVAGN:
        case OP_MODAGN:case OP_LSFTAGN:case OP_RSFTAGN:case OP_BITANDAGN:case OP_ANDXORAGN:case OP_AGN: {
            if (lhs->exprList.empty()) throw runtime_error("one expr required");
            auto op = t.type;
            t = next(f);
            if (t.type == KW_range) {
                t = next(f);
                auto* stmt = new AstRangeClause;
                stmt->lhs = lhs;
                stmt->op = op;
                stmt->rhs = parseExpr(t);
                return stmt;
            }
            else {
                auto* stmt = new AstAssignStmt;
                stmt->lhs = lhs;
                stmt->op = op;
                stmt->rhs = parseExprList(t);
                return stmt;
            }

        }
        default: {//ExprStmt
            if (lhs->exprList.size() != 1) throw runtime_error("one expr required");
            auto* stmt = new AstExprStmt;
            stmt->expr = lhs->exprList[0];
            return stmt;
        }
        }
    };
    parseIfStmt = [&](Token&t)->AstIfStmt* {
        const int outLev = nestLev;
        nestLev = -1;
        eat(KW_if, "expect keyword if");
        auto * node = new AstIfStmt;
        if (t.type == OP_LBRACE) throw runtime_error("if statement requires a condition");
        auto* tmp = parseSimpleStmt(nullptr, t);
        if (t.type == OP_SEMI) {
            node->init = tmp;
            t = next(f);
            node->cond = parseExpr(t);
        }
        else {
            node->cond = dynamic_cast<AstExprStmt*>(tmp)->expr;
        }
        node->ifBlock = parseBlock(t);
        if (t.type == KW_else) {
            t = next(f);
            if (t.type == KW_if) {
                node->elseBlock = parseIfStmt(t);
            }
            else if (t.type == OP_LBRACE) {
                node->elseBlock = parseBlock(t);
            }
            else throw runtime_error("only else-if or else could place here");
        }
        nestLev = outLev;
        return node;
    };
    parseSwitchStmt = [&](Token&t)->AstSwitchStmt* {
        const int outLev = nestLev;
        nestLev = -1;
        eat(KW_switch, "expect keyword switch");
        auto * node = new AstSwitchStmt;
        if (t.type != OP_LBRACE) {
            node->init = parseSimpleStmt(nullptr, t);
            if (t.type == OP_SEMI) t = next(f);
            if (t.type != OP_LBRACE) node->cond = parseSimpleStmt(nullptr, t);
        }
        eat(OP_LBRACE, "expec { after switch header");
        do {
            if (auto*tmp = parseSwitchCase(t); tmp != nullptr) {
                node->caseList.push_back(tmp);
            }
        } while (t.type != OP_RBRACE);
        t = next(f);
        nestLev = outLev;
        return node;
    };
    parseSwitchCase = [&](Token&t)->AstSwitchCase* {
        AstSwitchCase* node{};
        if (t.type == KW_case) {
            node = new AstSwitchCase;
            t = next(f);
            node->exprList = parseExprList(t);
            eat(OP_COLON, "statements in each case requires colon to separate");
            node->stmtList = parseStmtList(t);
        }
        else if (t.type == KW_default) {
            node = new AstSwitchCase;
            t = next(f);
            eat(OP_COLON, "expect : after default label");
            node->stmtList = parseStmtList(t);
        }
        return node;
    };
    parseSelectStmt = [&](Token&t)->AstNode* {
        const int outLev = nestLev;
        nestLev = -1;
        eat(KW_select, "expect keyword select");
        eat(OP_LBRACE, "expect { after select keyword");
        auto* node = new AstSelectStmt;
        do {
            if (auto*tmp = parseSelectCase(t); tmp != nullptr) {
                node->caseList.push_back(tmp);
            }
        } while (t.type != OP_RBRACE);
        t = next(f);
        nestLev = outLev;
        return node;
    };
    parseSelectCase = [&](Token&t)->AstSelectCase* {
        AstSelectCase* node{};
        if (t.type == KW_case) {
            node = new AstSelectCase;
            t = next(f);
            auto*tmp = parseSimpleStmt(nullptr, t);
            eat(OP_COLON, "statements in each case requires colon to separate");
            node->stmtList = parseStmtList(t);
        }
        else if (t.type == KW_default) {
            node = new AstSelectCase;
            t = next(f);
            eat(OP_COLON, "expect : after default label");
            node->stmtList = parseStmtList(t);
        }
        return node;
    };
    parseForStmt = [&](Token&t)->AstNode* {
        const int outLev = nestLev;
        nestLev = -1;
        eat(KW_for, "expect keyword for");
        auto* node = new AstForStmt;
        if (t.type != OP_LBRACE) {
            if (t.type != OP_SEMI) {
                auto*tmp = parseSimpleStmt(nullptr, t);
                switch (t.type) {
                case OP_LBRACE:
                    node->cond = tmp;
                    if (typeid(*tmp) == typeid(AstSRangeClause) || typeid(*tmp) == typeid(AstRangeClause))
                        nestLev = outLev;
                    break;
                case OP_SEMI:
                    node->init = tmp;
                    eat(OP_SEMI, "for syntax are as follows: [init];[cond];[post]{...}");
                    node->cond = parseExpr(t);
                    eat(OP_SEMI, "for syntax are as follows: [init];[cond];[post]{...}");
                    if (t.type != OP_LBRACE)node->post = parseSimpleStmt(nullptr, t);
                    break;
                default:throw runtime_error("expect {/;/range/:=/=");
                }
            }
            else {
                // for ;cond;post{}
                t = next(f);
                node->init = nullptr;
                node->cond = parseExpr(t);
                eat(OP_SEMI, "for syntax are as follows: [init];[cond];[post]{...}");
                if (t.type != OP_LBRACE)node->post = parseSimpleStmt(nullptr, t);
            }
        }
        node->block = parseBlock(t);
        nestLev = outLev;
        return node;
    };
#pragma endregion
#pragma region Expression
    parseExpr = [&](Token&t)->AstExpr* {
        AstExpr* node{};
        if (auto*tmp = parseUnaryExpr(t); tmp != nullptr) {
            node = new  AstExpr;
            node->lhs = tmp;
            if (anyone(t.type, OP_OR, OP_AND, OP_EQ, OP_NE, OP_LT, OP_LE, OP_XOR, OP_GT, OP_GE, OP_ADD,
                OP_SUB, OP_BITOR, OP_XOR, OP_ANDXOR, OP_MUL, OP_DIV, OP_MOD, OP_LSHIFT, OP_RSHIFT, OP_BITAND)) {
                node->op = t.type;
                t = next(f);
                node->rhs = parseExpr(t);
            }
        }
        return node;
    };
    parseUnaryExpr = [&](Token&t)->AstUnaryExpr* {
        AstUnaryExpr* node{};
        if (anyone(t.type, OP_ADD, OP_SUB, OP_NOT, OP_XOR, OP_MUL, OP_BITAND, OP_CHAN)) {
            node = new AstUnaryExpr;
            node->op = t.type;
            t = next(f);
            node->expr = parseUnaryExpr(t);
        }
        else if (anyone(t.type, TK_ID, LIT_INT, LIT_FLOAT, LIT_IMG, LIT_RUNE, LIT_STR, 
            KW_struct, KW_map, OP_LBRACKET, KW_chan, KW_interface, KW_func, OP_LPAREN)) {
            node = new AstUnaryExpr;
            node->expr = parsePrimaryExpr(t);
        }
        return node;
    };
    parsePrimaryExpr = [&](Token&t)->AstPrimaryExpr* {
        AstPrimaryExpr*node{};
        if (auto*tmp = parseOperand(t); tmp != nullptr) {
            node = new AstPrimaryExpr;
            // eliminate left-recursion by infinite loop; these code referenced golang official impl
            // since this work requires somewhat familiarity of golang syntax
            while (true) {
                if (t.type == OP_DOT) {
                    t = next(f);
                    if (t.type == TK_ID) {
                        auto* e = new AstSelectorExpr;
                        e->operand = tmp;
                        e->selector = t.lexeme;
                        tmp = e;
                        t = next(f);
                    }
                    else if (t.type == OP_LPAREN) {
                        t = next(f);
                        if (t.type == KW_type) {
                            auto* e = new AstTypeSwitchExpr;
                            e->operand = tmp;
                            tmp = e;
                            t = next(f);
                        }
                        else {
                            auto* e = new AstTypeAssertionExpr;
                            e->operand = tmp;
                            e->type = parseType(t);
                            tmp = e;
                        }
                        eat(OP_RPAREN, "expect )");
                    }
                }
                else if (t.type == OP_LBRACKET) {
                    nestLev++;
                    t = next(f);
                    AstNode* start = nullptr;//Ignore start if next token is :(syntax of operand[:xxx])
                    if (t.type != OP_COLON) {
                        start = parseExpr(t);
                        if (t.type == OP_RBRACKET) {
                            auto* e = new AstIndexExpr;
                            e->operand = tmp;
                            e->index = start;
                            tmp = e;
                            t = next(f);
                            nestLev--;
                            continue;
                        }
                    }
                    auto* e = new AstSliceExpr;
                    e->operand = tmp;
                    e->begin = start;
                    eat(OP_COLON, "expect : in slice expression");
                    e->end = parseExpr(t);//may nullptr
                    if (t.type == OP_COLON) {
                        t = next(f);
                        e->step = parseExpr(t);
                        eat(OP_RBRACKET, "expect ] at the end of slice expression");
                    }
                    else if (t.type == OP_RBRACKET) {
                        t = next(f);
                    }
                    tmp = e;
                    nestLev--;
                }
                else if (t.type == OP_LPAREN) {
                    t = next(f);
                    auto* e = new AstCallExpr;
                    e->operand = tmp;
                    nestLev++;
                    if (auto*tmp1 = parseExprList(t); tmp1 != nullptr) {
                        e->arguments = tmp1;
                    }
                    if (t.type == OP_VARIADIC) {
                        e->isVariadic = true;
                        t = next(f);
                    }
                    nestLev--;
                    eat(OP_RPAREN, "() must match in call expr");
                    tmp = e;
                }
                else if (t.type == OP_LBRACE) {
                    // only operand has literal value, otherwise, treats it as a block
                    if (anyone(typeid(*tmp), typeid(AstArrayType), typeid(AstSliceType), typeid(AstStructType), typeid(AstMapType))
                        || ((anyone(typeid(*tmp), typeid(AstName), typeid(AstSelectorExpr))) && nestLev >= 0)) {
                        // it's somewhat curious since official implementation treats literal type and literal value as separate parts
                        auto* e = new AstCompositeLit;
                        e->litName = tmp;
                        e->litValue = parseLitValue(t);
                        tmp = e;
                    }
                    else break;
                }
                else break;
            }
            node->expr = tmp;
        }
        return node;
    };
    parseOperand = [&](Token&t)->AstNode* {
        if (t.type == TK_ID) { 
            return parseName(false, t); 
        } else if (t.type == KW_func) {
            return parseFuncDecl(true, t); 
        } else if (t.type == OP_LPAREN) {
            t = next(f); nestLev++; auto* e = parseExpr(t); nestLev--; eat(OP_RPAREN, "expect )"); return e;
        } else if (anyone(t.type, LIT_INT, LIT_FLOAT, LIT_IMG, LIT_RUNE, LIT_STR)) {
            auto*tmp = new AstBasicLit(t.type, t.lexeme); t = next(f); return tmp; 
        } else if (anyone(t.type, KW_struct, KW_map, OP_LBRACKET, KW_chan, KW_interface)) {
            return parseType(t);
        } else return nullptr;
        
    };
    parseLitValue = [&](Token&t)->AstLitValue* {
        AstLitValue*node{};
        if (t.type == OP_LBRACE) {
            nestLev++;
            node = new AstLitValue;
            do {
                t = next(f);
                if (t.type == OP_RBRACE) break; // it's necessary since both {a,b} or {a,b,} are legal form
                node->keyedElement.push_back(parseKeyedElement(t));
            } while (t.type != OP_RBRACE);
            eat(OP_RBRACE, "brace {} must match");
            nestLev--;
        }
        return node;
    };
    parseKeyedElement = [&](Token&t)->AstKeyedElement* {
        AstKeyedElement*node{};
        if (auto*tmp = parseKey(t); tmp != nullptr) {
            node = new AstKeyedElement;
            node->element = tmp;
            if (t.type == OP_COLON) {
                node->key = tmp;
                t = next(f);
                if (auto*tmp1 = parseExpr(t); tmp1 != nullptr)          node->element = tmp1;
                else if (auto*tmp1 = parseLitValue(t); tmp1 != nullptr) node->element = tmp1;
                else node->element = nullptr;
            }
        }
        return node;
    };
    parseKey = [&](Token&t)->AstNode* {
        if (t.type == TK_ID) return parseName(false, t);
        else if (auto*tmp = parseLitValue(t); tmp != nullptr) return tmp;
        else if (auto*tmp = parseExpr(t); tmp != nullptr) return tmp;
        return nullptr;
    };
#pragma endregion
    // parsing startup
    auto * node = new AstCompilationUnit;
    eat(KW_package, "a go source file must start with package declaration");
    node->package = t.lexeme;
    eat(TK_ID, "name required at the package declaration");
    eat(OP_SEMI, "expect ; at the end of package declaration");
    while (t.type != TK_EOF) {
        switch (t.type) {
        case KW_import: node->importDecl.push_back(parseImportDecl(t));     break;
        case KW_const:  node->constDecl.push_back(parseConstDecl(t));       break;
        case KW_type:   node->typeDecl.push_back(parseTypeDecl(t));         break;
        case KW_var:    node->varDecl.push_back(parseVarDecl(t));           break;
        case KW_func:   node->funcDecl.push_back(parseFuncDecl(false, t));  break;
        case OP_SEMI:   t = next(f); break;
        default:        {REPORT_ERR("syntax error","unknown top level declaration"); }
        }
    }
    return node;
}

void emitStub() {}

void runtimeStub() {}

//===---------------------------------------------------------------------------------------===//
// debug auxiliary functions, they are not part of 5 functions
//===---------------------------------------------------------------------------------------===//
void printLex(const string & filename) {
    fstream f(filename, ios::binary | ios::in);
    while (lastToken != TK_EOF) {
        auto[token, lexeme] = next(f);
        fprintf(stdout, "<%d,%s,%d,%d>\n", token, lexeme.c_str(), line, column);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argv[1] == nullptr) {
        fprintf(stderr, "specify your go source file\n");
        return 1;
    }
    //printLex(argv[1]);
    const AstNode* ast = parse(argv[1]);
    fprintf(stdout, "parsing passed\n");
    return 0;
}