#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/utsname.h>

#define MAX_REGISTERS 128
#define MAX_CONSTANTS 128
#define MAX_FUNCTIONS 255
#define MAX_FRAMES 8192
#define MAX_STACK_VALUES 1000000
#define MAX_ARRAY_LENGTH 10000000

typedef enum { TY_INT, TY_ARRAY } Type;
typedef enum {
    TK_EOF, TK_IDENT, TK_NUMBER,
    TK_FN, TK_LET, TK_IF, TK_ELSE, TK_WHILE, TK_RETURN, TK_INT,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
    TK_COLON, TK_SEMI, TK_COMMA, TK_ARROW,
    TK_ASSIGN, TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_BANG, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *start;
    size_t len;
    int64_t number;
    unsigned line;
} Token;

static const char *error_path = "<input>";
static unsigned error_line = 1;

static void fail(const char *message) {
    fprintf(stderr, "%s:%u: %s\n", error_path, error_line, message);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) fail("out of memory");
    return p;
}

static void *xcalloc(size_t n, size_t size) {
    void *p = calloc(n ? n : 1, size ? size : 1);
    if (!p) fail("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) fail("out of memory");
    return q;
}

static char *copy_span(const char *s, size_t n) {
    char *r = xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static bool token_is(Token t, const char *word) {
    return strlen(word) == t.len && memcmp(t.start, word, t.len) == 0;
}

typedef struct {
    const char *source;
    size_t length;
    size_t pos;
    unsigned line;
} Lexer;

static char peek_char(Lexer *l) {
    return l->pos < l->length ? l->source[l->pos] : '\0';
}

static char take_char(Lexer *l) {
    char c = peek_char(l);
    if (c) {
        l->pos++;
        if (c == '\n') l->line++;
    }
    return c;
}

static void skip_space(Lexer *l) {
    for (;;) {
        char c = peek_char(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            take_char(l);
        } else if (c == '/' && l->pos + 1 < l->length && l->source[l->pos + 1] == '/') {
            while (peek_char(l) && peek_char(l) != '\n') take_char(l);
        } else {
            return;
        }
    }
}

static Token make_token(TokenKind kind, const char *start, size_t len, unsigned line) {
    Token t = {0};
    t.kind = kind;
    t.start = start;
    t.len = len;
    t.line = line;
    return t;
}

static Token lex_next(Lexer *l) {
    skip_space(l);
    const char *start = l->source + l->pos;
    unsigned line = l->line;
    char c = take_char(l);
    if (!c) return make_token(TK_EOF, start, 0, line);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        while ((peek_char(l) >= 'a' && peek_char(l) <= 'z') ||
               (peek_char(l) >= 'A' && peek_char(l) <= 'Z') ||
               (peek_char(l) >= '0' && peek_char(l) <= '9') || peek_char(l) == '_') take_char(l);
        Token t = make_token(TK_IDENT, start, (size_t)((l->source + l->pos) - start), line);
        if (token_is(t, "fn")) t.kind = TK_FN;
        else if (token_is(t, "let")) t.kind = TK_LET;
        else if (token_is(t, "if")) t.kind = TK_IF;
        else if (token_is(t, "else")) t.kind = TK_ELSE;
        else if (token_is(t, "while")) t.kind = TK_WHILE;
        else if (token_is(t, "return")) t.kind = TK_RETURN;
        else if (token_is(t, "int")) t.kind = TK_INT;
        return t;
    }
    if (c >= '0' && c <= '9') {
        while (peek_char(l) >= '0' && peek_char(l) <= '9') take_char(l);
        Token t = make_token(TK_NUMBER, start, (size_t)((l->source + l->pos) - start), line);
        char *digits = copy_span(t.start, t.len);
        errno = 0;
        char *end = NULL;
        long long n = strtoll(digits, &end, 10);
        if (errno == ERANGE || !end || *end) fail("integer literal is outside the signed 64-bit range");
        free(digits);
        t.number = (int64_t)n;
        return t;
    }
#define TWO(a, b, yes, no) do { if (peek_char(l) == (b)) { take_char(l); return make_token((yes), start, 2, line); } return make_token((no), start, 1, line); } while (0)
    switch (c) {
        case '(': return make_token(TK_LPAREN, start, 1, line);
        case ')': return make_token(TK_RPAREN, start, 1, line);
        case '{': return make_token(TK_LBRACE, start, 1, line);
        case '}': return make_token(TK_RBRACE, start, 1, line);
        case '[': return make_token(TK_LBRACKET, start, 1, line);
        case ']': return make_token(TK_RBRACKET, start, 1, line);
        case ':': return make_token(TK_COLON, start, 1, line);
        case ';': return make_token(TK_SEMI, start, 1, line);
        case ',': return make_token(TK_COMMA, start, 1, line);
        case '+': return make_token(TK_PLUS, start, 1, line);
        case '*': return make_token(TK_STAR, start, 1, line);
        case '/': return make_token(TK_SLASH, start, 1, line);
        case '%': return make_token(TK_PERCENT, start, 1, line);
        case '-': if (peek_char(l) == '>') { take_char(l); return make_token(TK_ARROW, start, 2, line); }
                  return make_token(TK_MINUS, start, 1, line);
        case '=': TWO('=', '=', TK_EQ, TK_ASSIGN);
        case '!': TWO('!', '=', TK_NE, TK_BANG);
        case '<': TWO('<', '=', TK_LE, TK_LT);
        case '>': TWO('>', '=', TK_GE, TK_GT);
        default: fail("unexpected character");
    }
#undef TWO
    return make_token(TK_EOF, start, 0, line);
}

typedef enum { EX_INT, EX_VAR, EX_CALL, EX_NEW_ARRAY, EX_INDEX, EX_BINARY, EX_UNARY } ExprKind;
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_NEG, OP_NOT } Operator;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

struct Expr {
    ExprKind kind;
    Type type;
    unsigned line;
    union {
        int64_t integer;
        struct { char *name; int local; } var;
        struct { char *name; Expr **args; size_t count; int function; } call;
        struct { Expr *length; } new_array;
        struct { Expr *array; Expr *index; } index;
        struct { Operator op; Expr *left; Expr *right; } binary;
        struct { Operator op; Expr *value; } unary;
    } as;
};

typedef enum { ST_BLOCK, ST_LET, ST_ASSIGN, ST_IF, ST_WHILE, ST_RETURN, ST_EXPR } StmtKind;
struct Stmt {
    StmtKind kind;
    unsigned line;
    union {
        struct { Stmt **items; size_t count; } block;
        struct { char *name; Type type; int local; Expr *value; } let;
        struct { Expr *target; Expr *value; } assign;
        struct { Expr *condition; Stmt *yes; Stmt *no; } branch;
        struct { Expr *condition; Stmt *body; } loop;
        struct { Expr *value; } ret;
        struct { Expr *value; } expression;
    } as;
};

typedef struct { char *name; Type type; } Param;
typedef struct { char *name; Type type; bool parameter; unsigned line; } Local;
typedef struct {
    char *name;
    Param *params;
    size_t param_count;
    Stmt *body;
    Local *locals;
    size_t local_count;
    unsigned line;
} Function;
typedef struct { Function *functions; size_t count; } Program;

typedef struct { Lexer lexer; Token token; } Parser;

static void parser_advance(Parser *p) { p->token = lex_next(&p->lexer); error_line = p->token.line; }
static bool parser_accept(Parser *p, TokenKind kind) { if (p->token.kind != kind) return false; parser_advance(p); return true; }
static void parser_expect(Parser *p, TokenKind kind, const char *message) { if (!parser_accept(p, kind)) fail(message); }

static char *expect_identifier(Parser *p) {
    if (p->token.kind != TK_IDENT) fail("expected an identifier");
    char *name = copy_span(p->token.start, p->token.len);
    parser_advance(p);
    return name;
}

static Expr *new_expr(ExprKind kind, unsigned line) {
    Expr *e = xcalloc(1, sizeof(*e));
    e->kind = kind;
    e->line = line;
    return e;
}

static Stmt *new_stmt(StmtKind kind, unsigned line) {
    Stmt *s = xcalloc(1, sizeof(*s));
    s->kind = kind;
    s->line = line;
    return s;
}

#define APPEND_PTR(items, count, item) do { \
    (items) = xrealloc((items), (*(count) + 1) * sizeof(*(items))); \
    (items)[(*(count))++] = (item); \
} while (0)

static Type parse_type(Parser *p) {
    parser_expect(p, TK_INT, "expected type 'int'");
    if (parser_accept(p, TK_LBRACKET)) {
        parser_expect(p, TK_RBRACKET, "expected ']' in array type");
        return TY_ARRAY;
    }
    return TY_INT;
}

static Expr *parse_expression(Parser *p);
static Stmt *parse_statement(Parser *p);

static Expr *parse_primary(Parser *p) {
    unsigned line = p->token.line;
    if (p->token.kind == TK_NUMBER) {
        Expr *e = new_expr(EX_INT, line);
        e->as.integer = p->token.number;
        parser_advance(p);
        return e;
    }
    if (parser_accept(p, TK_LPAREN)) {
        Expr *e = parse_expression(p);
        parser_expect(p, TK_RPAREN, "expected ')' after expression");
        return e;
    }
    if (p->token.kind == TK_IDENT) {
        char *name = expect_identifier(p);
        if (parser_accept(p, TK_LPAREN)) {
            Expr *e = new_expr(EX_CALL, line);
            e->as.call.name = name;
            while (p->token.kind != TK_RPAREN) {
                Expr *arg = parse_expression(p);
                APPEND_PTR(e->as.call.args, &e->as.call.count, arg);
                if (!parser_accept(p, TK_COMMA)) break;
            }
            parser_expect(p, TK_RPAREN, "expected ')' after arguments");
            return e;
        }
        Expr *e = new_expr(EX_VAR, line);
        e->as.var.name = name;
        e->as.var.local = -1;
        return e;
    }
    fail("expected an expression");
    return NULL;
}

static Expr *parse_postfix(Parser *p) {
    Expr *e = parse_primary(p);
    while (parser_accept(p, TK_LBRACKET)) {
        Expr *index = parse_expression(p);
        parser_expect(p, TK_RBRACKET, "expected ']' after index");
        Expr *next = new_expr(EX_INDEX, e->line);
        next->as.index.array = e;
        next->as.index.index = index;
        e = next;
    }
    return e;
}

static Expr *parse_unary(Parser *p) {
    if (p->token.kind == TK_MINUS || p->token.kind == TK_BANG) {
        Token t = p->token;
        parser_advance(p);
        Expr *e = new_expr(EX_UNARY, t.line);
        e->as.unary.op = t.kind == TK_MINUS ? OP_NEG : OP_NOT;
        e->as.unary.value = parse_unary(p);
        return e;
    }
    return parse_postfix(p);
}

static Expr *parse_binary_left(Parser *p, Expr *(*next)(Parser *), const TokenKind *tokens, const Operator *ops, size_t count) {
    Expr *left = next(p);
    for (;;) {
        size_t i;
        for (i = 0; i < count && p->token.kind != tokens[i]; i++) {}
        if (i == count) return left;
        Token t = p->token;
        parser_advance(p);
        Expr *right = next(p);
        Expr *e = new_expr(EX_BINARY, t.line);
        e->as.binary.op = ops[i];
        e->as.binary.left = left;
        e->as.binary.right = right;
        left = e;
    }
}

static Expr *parse_factor(Parser *p) {
    static const TokenKind tokens[] = {TK_STAR, TK_SLASH, TK_PERCENT};
    static const Operator ops[] = {OP_MUL, OP_DIV, OP_MOD};
    return parse_binary_left(p, parse_unary, tokens, ops, 3);
}
static Expr *parse_term(Parser *p) {
    static const TokenKind tokens[] = {TK_PLUS, TK_MINUS};
    static const Operator ops[] = {OP_ADD, OP_SUB};
    return parse_binary_left(p, parse_factor, tokens, ops, 2);
}
static Expr *parse_compare(Parser *p) {
    static const TokenKind tokens[] = {TK_LT, TK_LE, TK_GT, TK_GE};
    static const Operator ops[] = {OP_LT, OP_LE, OP_GT, OP_GE};
    return parse_binary_left(p, parse_term, tokens, ops, 4);
}
static Expr *parse_equality(Parser *p) {
    static const TokenKind tokens[] = {TK_EQ, TK_NE};
    static const Operator ops[] = {OP_EQ, OP_NE};
    return parse_binary_left(p, parse_compare, tokens, ops, 2);
}
static Expr *parse_expression(Parser *p) { return parse_equality(p); }

static Stmt *parse_block(Parser *p) {
    unsigned line = p->token.line;
    parser_expect(p, TK_LBRACE, "expected '{'");
    Stmt *s = new_stmt(ST_BLOCK, line);
    while (p->token.kind != TK_RBRACE && p->token.kind != TK_EOF) {
        Stmt *item = parse_statement(p);
        APPEND_PTR(s->as.block.items, &s->as.block.count, item);
    }
    parser_expect(p, TK_RBRACE, "expected '}'");
    return s;
}

static Stmt *parse_statement(Parser *p) {
    unsigned line = p->token.line;
    if (p->token.kind == TK_LBRACE) return parse_block(p);
    if (parser_accept(p, TK_LET)) {
        Stmt *s = new_stmt(ST_LET, line);
        s->as.let.name = expect_identifier(p);
        parser_expect(p, TK_COLON, "expected ':' after local name");
        s->as.let.type = parse_type(p);
        parser_expect(p, TK_ASSIGN, "expected '=' in local declaration");
        s->as.let.value = parse_expression(p);
        parser_expect(p, TK_SEMI, "expected ';' after local declaration");
        return s;
    }
    if (parser_accept(p, TK_IF)) {
        Stmt *s = new_stmt(ST_IF, line);
        parser_expect(p, TK_LPAREN, "expected '(' after if");
        s->as.branch.condition = parse_expression(p);
        parser_expect(p, TK_RPAREN, "expected ')' after condition");
        s->as.branch.yes = parse_statement(p);
        if (parser_accept(p, TK_ELSE)) s->as.branch.no = parse_statement(p);
        return s;
    }
    if (parser_accept(p, TK_WHILE)) {
        Stmt *s = new_stmt(ST_WHILE, line);
        parser_expect(p, TK_LPAREN, "expected '(' after while");
        s->as.loop.condition = parse_expression(p);
        parser_expect(p, TK_RPAREN, "expected ')' after condition");
        s->as.loop.body = parse_statement(p);
        return s;
    }
    if (parser_accept(p, TK_RETURN)) {
        Stmt *s = new_stmt(ST_RETURN, line);
        s->as.ret.value = parse_expression(p);
        parser_expect(p, TK_SEMI, "expected ';' after return");
        return s;
    }
    Expr *left = parse_expression(p);
    if (parser_accept(p, TK_ASSIGN)) {
        Stmt *s = new_stmt(ST_ASSIGN, line);
        s->as.assign.target = left;
        s->as.assign.value = parse_expression(p);
        parser_expect(p, TK_SEMI, "expected ';' after assignment");
        return s;
    }
    Stmt *s = new_stmt(ST_EXPR, line);
    s->as.expression.value = left;
    parser_expect(p, TK_SEMI, "expected ';' after expression");
    return s;
}

static Function parse_function(Parser *p) {
    parser_expect(p, TK_FN, "expected function definition");
    Function f = {0};
    f.line = p->token.line;
    f.name = expect_identifier(p);
    parser_expect(p, TK_LPAREN, "expected '(' after function name");
    while (p->token.kind != TK_RPAREN) {
        Param param = {0};
        param.name = expect_identifier(p);
        parser_expect(p, TK_COLON, "expected ':' after parameter name");
        param.type = parse_type(p);
        f.params = xrealloc(f.params, (f.param_count + 1) * sizeof(*f.params));
        f.params[f.param_count++] = param;
        if (!parser_accept(p, TK_COMMA)) break;
    }
    parser_expect(p, TK_RPAREN, "expected ')' after parameters");
    parser_expect(p, TK_ARROW, "expected '->' before return type");
    parser_expect(p, TK_INT, "functions must return int");
    f.body = parse_block(p);
    return f;
}

static Program parse_program(const char *source, size_t length) {
    Parser p = {0};
    p.lexer.source = source;
    p.lexer.length = length;
    p.lexer.line = 1;
    parser_advance(&p);
    Program program = {0};
    while (p.token.kind != TK_EOF) {
        Function f = parse_function(&p);
        program.functions = xrealloc(program.functions, (program.count + 1) * sizeof(*program.functions));
        program.functions[program.count++] = f;
    }
    if (program.count == 0) fail("program has no functions");
    if (program.count > MAX_FUNCTIONS) fail("too many functions (limit is 255)");
    return program;
}

static int find_function(Program *program, const char *name) {
    for (size_t i = 0; i < program->count; i++)
        if (strcmp(program->functions[i].name, name) == 0) return (int)i;
    return -1;
}

static int find_local(Function *function, const char *name) {
    for (size_t i = 0; i < function->local_count; i++)
        if (strcmp(function->locals[i].name, name) == 0) return (int)i;
    return -1;
}

static int add_local(Function *function, const char *name, Type type, bool parameter, unsigned line) {
    if (find_local(function, name) >= 0) {
        error_line = line;
        fail("local names must be unique within a function");
    }
    function->locals = xrealloc(function->locals, (function->local_count + 1) * sizeof(*function->locals));
    int index = (int)function->local_count;
    function->locals[function->local_count++] = (Local){copy_span(name, strlen(name)), type, parameter, line};
    if (function->local_count > 255) {
        error_line = line;
        fail("too many locals in function (limit is 255)");
    }
    return index;
}

static Type check_expr(Program *program, Function *function, Expr *e) {
    error_line = e->line;
    switch (e->kind) {
        case EX_INT:
            e->type = TY_INT;
            return e->type;
        case EX_VAR: {
            int local = find_local(function, e->as.var.name);
            if (local < 0) fail("unknown local variable");
            e->as.var.local = local;
            e->type = function->locals[local].type;
            return e->type;
        }
        case EX_CALL: {
            if (strcmp(e->as.call.name, "array") == 0) {
                if (e->as.call.count != 1) fail("array() expects one length argument");
                if (check_expr(program, function, e->as.call.args[0]) != TY_INT) fail("array length must be int");
                Expr *length = e->as.call.args[0];
                free(e->as.call.args);
                free(e->as.call.name);
                e->kind = EX_NEW_ARRAY;
                e->as.new_array.length = length;
                e->type = TY_ARRAY;
                return e->type;
            }
            int target = find_function(program, e->as.call.name);
            if (target < 0) fail("call to unknown function");
            Function *callee = &program->functions[target];
            if (e->as.call.count != callee->param_count) fail("wrong number of function arguments");
            for (size_t i = 0; i < e->as.call.count; i++) {
                if (check_expr(program, function, e->as.call.args[i]) != callee->params[i].type)
                    fail("function argument type does not match parameter type");
            }
            e->as.call.function = target;
            e->type = TY_INT;
            return e->type;
        }
        case EX_NEW_ARRAY:
            e->type = TY_ARRAY;
            return e->type;
        case EX_INDEX:
            if (check_expr(program, function, e->as.index.array) != TY_ARRAY) fail("indexing requires an int[] value");
            if (check_expr(program, function, e->as.index.index) != TY_INT) fail("array index must be int");
            e->type = TY_INT;
            return e->type;
        case EX_BINARY: {
            Type left = check_expr(program, function, e->as.binary.left);
            Type right = check_expr(program, function, e->as.binary.right);
            if (left != TY_INT || right != TY_INT) fail("operators require int operands");
            e->type = TY_INT;
            return e->type;
        }
        case EX_UNARY:
            if (check_expr(program, function, e->as.unary.value) != TY_INT) fail("unary operators require int operands");
            e->type = TY_INT;
            return e->type;
    }
    fail("internal error: unknown expression");
    return TY_INT;
}

static void check_stmt(Program *program, Function *function, Stmt *s) {
    error_line = s->line;
    switch (s->kind) {
        case ST_BLOCK:
            for (size_t i = 0; i < s->as.block.count; i++) check_stmt(program, function, s->as.block.items[i]);
            return;
        case ST_LET: {
            if (check_expr(program, function, s->as.let.value) != s->as.let.type) fail("initializer type does not match local type");
            s->as.let.local = add_local(function, s->as.let.name, s->as.let.type, false, s->line);
            return;
        }
        case ST_ASSIGN: {
            Type value_type = check_expr(program, function, s->as.assign.value);
            Expr *target = s->as.assign.target;
            if (target->kind == EX_VAR) {
                int local = find_local(function, target->as.var.name);
                if (local < 0) { error_line = target->line; fail("assignment to unknown local variable"); }
                target->as.var.local = local;
                target->type = function->locals[local].type;
                if (target->type != value_type) fail("assigned value has wrong type");
            } else if (target->kind == EX_INDEX) {
                if (check_expr(program, function, target->as.index.array) != TY_ARRAY) fail("indexed assignment requires an int[] value");
                if (check_expr(program, function, target->as.index.index) != TY_INT) fail("array index must be int");
                if (value_type != TY_INT) fail("integer arrays only accept int values");
                target->type = TY_INT;
            } else {
                error_line = target->line;
                fail("assignment target must be a local or array element");
            }
            return;
        }
        case ST_IF:
            if (check_expr(program, function, s->as.branch.condition) != TY_INT) fail("if condition must be int");
            check_stmt(program, function, s->as.branch.yes);
            if (s->as.branch.no) check_stmt(program, function, s->as.branch.no);
            return;
        case ST_WHILE:
            if (check_expr(program, function, s->as.loop.condition) != TY_INT) fail("while condition must be int");
            check_stmt(program, function, s->as.loop.body);
            return;
        case ST_RETURN:
            if (check_expr(program, function, s->as.ret.value) != TY_INT) fail("function must return int");
            return;
        case ST_EXPR:
            if (check_expr(program, function, s->as.expression.value) == TY_ARRAY) fail("array value cannot be discarded as an expression statement");
            if (s->as.expression.value->kind != EX_CALL) fail("only function calls may be expression statements");
            return;
    }
}

static void type_check(Program *program) {
    for (size_t i = 0; i < program->count; i++) {
        Function *f = &program->functions[i];
        error_line = f->line;
        if (f->param_count > 127) fail("too many parameters (limit is 127)");
        for (size_t p = 0; p < f->param_count; p++)
            add_local(f, f->params[p].name, f->params[p].type, true, f->line);
    }
    for (size_t i = 0; i < program->count; i++) {
        Function *f = &program->functions[i];
        if (find_function(program, f->name) != (int)i) { error_line = f->line; fail("duplicate function name"); }
        check_stmt(program, f, f->body);
    }
    if (find_function(program, "main") < 0) fail("program must define main() -> int");
    if (program->functions[find_function(program, "main")].param_count != 0) fail("main() must have no parameters");
}

typedef struct { int64_t values[MAX_CONSTANTS]; size_t count; } ConstantPool;
typedef struct {
    uint32_t start;
    uint32_t end;
    uint16_t local_count;
    uint16_t reg_count;
    uint8_t param_count;
} FunctionCode;
typedef struct {
    bool is_register;
    unsigned char *bytes;
    size_t byte_count;
    size_t byte_capacity;
    uint32_t *words;
    size_t word_count;
    size_t word_capacity;
    uint64_t static_instructions;
    FunctionCode *functions;
    size_t function_count;
    ConstantPool *constants;
    uint8_t main_function;
} Bytecode;

static size_t intern_constant(ConstantPool *pool, int64_t value) {
    for (size_t i = 0; i < pool->count; i++) if (pool->values[i] == value) return i;
    if (pool->count >= MAX_CONSTANTS) fail("too many distinct integer constants (limit is 128)");
    pool->values[pool->count] = value;
    return pool->count++;
}

static void collect_expr_constants(ConstantPool *pool, Expr *e) {
    switch (e->kind) {
        case EX_INT: intern_constant(pool, e->as.integer); return;
        case EX_VAR: return;
        case EX_CALL:
            for (size_t i = 0; i < e->as.call.count; i++) collect_expr_constants(pool, e->as.call.args[i]);
            return;
        case EX_NEW_ARRAY: collect_expr_constants(pool, e->as.new_array.length); return;
        case EX_INDEX:
            collect_expr_constants(pool, e->as.index.array);
            collect_expr_constants(pool, e->as.index.index);
            return;
        case EX_BINARY:
            collect_expr_constants(pool, e->as.binary.left);
            collect_expr_constants(pool, e->as.binary.right);
            return;
        case EX_UNARY: collect_expr_constants(pool, e->as.unary.value); return;
    }
}

static void collect_stmt_constants(ConstantPool *pool, Stmt *s) {
    switch (s->kind) {
        case ST_BLOCK:
            for (size_t i = 0; i < s->as.block.count; i++) collect_stmt_constants(pool, s->as.block.items[i]);
            return;
        case ST_LET: collect_expr_constants(pool, s->as.let.value); return;
        case ST_ASSIGN:
            collect_expr_constants(pool, s->as.assign.target);
            collect_expr_constants(pool, s->as.assign.value);
            return;
        case ST_IF:
            collect_expr_constants(pool, s->as.branch.condition);
            collect_stmt_constants(pool, s->as.branch.yes);
            if (s->as.branch.no) collect_stmt_constants(pool, s->as.branch.no);
            return;
        case ST_WHILE:
            collect_expr_constants(pool, s->as.loop.condition);
            collect_stmt_constants(pool, s->as.loop.body);
            return;
        case ST_RETURN: collect_expr_constants(pool, s->as.ret.value); return;
        case ST_EXPR: collect_expr_constants(pool, s->as.expression.value); return;
    }
}

static ConstantPool collect_constants(Program *program) {
    ConstantPool pool = {0};
    intern_constant(&pool, 0);
    for (size_t i = 0; i < program->count; i++) collect_stmt_constants(&pool, program->functions[i].body);
    return pool;
}

enum {
    S_PUSH_CONST = 1, S_LOAD_LOCAL, S_STORE_LOCAL, S_NEW_ARRAY, S_GET_ELEMENT, S_SET_ELEMENT,
    S_ADD, S_SUB, S_MUL, S_DIV, S_MOD, S_EQ, S_NE, S_LT, S_LE, S_GT, S_GE,
    S_NEG, S_NOT, S_JUMP, S_JUMP_FALSE, S_CALL, S_RETURN, S_POP
};

enum {
    R_LOADK = 1, R_MOVE, R_NEW_ARRAY, R_GET_ELEMENT, R_SET_ELEMENT,
    R_ADD, R_SUB, R_MUL, R_DIV, R_MOD, R_EQ, R_NE, R_LT, R_LE, R_GT, R_GE,
    R_NEG, R_NOT, R_JUMP, R_JUMP_ZERO, R_CALL, R_RETURN
};

static void stack_emit_byte(Bytecode *bc, uint8_t value) {
    if (bc->byte_count == bc->byte_capacity) {
        bc->byte_capacity = bc->byte_capacity ? bc->byte_capacity * 2 : 256;
        bc->bytes = xrealloc(bc->bytes, bc->byte_capacity);
    }
    bc->bytes[bc->byte_count++] = value;
}

static void stack_emit_u32(Bytecode *bc, uint32_t value) {
    stack_emit_byte(bc, (uint8_t)value);
    stack_emit_byte(bc, (uint8_t)(value >> 8));
    stack_emit_byte(bc, (uint8_t)(value >> 16));
    stack_emit_byte(bc, (uint8_t)(value >> 24));
}

static void stack_emit_op(Bytecode *bc, uint8_t op) {
    stack_emit_byte(bc, op);
    bc->static_instructions++;
}

static void stack_patch_i32(Bytecode *bc, size_t operand_at, size_t base, size_t target) {
    int64_t delta = (int64_t)target - (int64_t)base;
    if (delta < INT32_MIN || delta > INT32_MAX) fail("stack bytecode branch is out of range");
    uint32_t value = (uint32_t)(int32_t)delta;
    bc->bytes[operand_at] = (uint8_t)value;
    bc->bytes[operand_at + 1] = (uint8_t)(value >> 8);
    bc->bytes[operand_at + 2] = (uint8_t)(value >> 16);
    bc->bytes[operand_at + 3] = (uint8_t)(value >> 24);
}

static size_t constant_index(ConstantPool *pool, int64_t value) {
    for (size_t i = 0; i < pool->count; i++) if (pool->values[i] == value) return i;
    fail("internal error: missing literal in constant pool");
    return 0;
}

static uint8_t stack_opcode_for(Operator op) {
    switch (op) {
        case OP_ADD: return S_ADD; case OP_SUB: return S_SUB; case OP_MUL: return S_MUL;
        case OP_DIV: return S_DIV; case OP_MOD: return S_MOD; case OP_EQ: return S_EQ;
        case OP_NE: return S_NE; case OP_LT: return S_LT; case OP_LE: return S_LE;
        case OP_GT: return S_GT; case OP_GE: return S_GE; case OP_NEG: return S_NEG;
        case OP_NOT: return S_NOT;
    }
    return 0;
}

static void compile_stack_expr(Bytecode *bc, Program *ast, Expr *e);
static void compile_stack_stmt(Bytecode *bc, Program *ast, Stmt *s);

static void compile_stack_expr(Bytecode *bc, Program *ast, Expr *e) {
    switch (e->kind) {
        case EX_INT:
            stack_emit_op(bc, S_PUSH_CONST);
            stack_emit_byte(bc, (uint8_t)constant_index(bc->constants, e->as.integer));
            return;
        case EX_VAR:
            stack_emit_op(bc, S_LOAD_LOCAL);
            stack_emit_byte(bc, (uint8_t)e->as.var.local);
            return;
        case EX_CALL:
            for (size_t i = 0; i < e->as.call.count; i++) compile_stack_expr(bc, ast, e->as.call.args[i]);
            stack_emit_op(bc, S_CALL);
            stack_emit_byte(bc, (uint8_t)e->as.call.function);
            stack_emit_byte(bc, (uint8_t)e->as.call.count);
            return;
        case EX_NEW_ARRAY:
            compile_stack_expr(bc, ast, e->as.new_array.length);
            stack_emit_op(bc, S_NEW_ARRAY);
            return;
        case EX_INDEX:
            compile_stack_expr(bc, ast, e->as.index.array);
            compile_stack_expr(bc, ast, e->as.index.index);
            stack_emit_op(bc, S_GET_ELEMENT);
            return;
        case EX_BINARY:
            compile_stack_expr(bc, ast, e->as.binary.left);
            compile_stack_expr(bc, ast, e->as.binary.right);
            stack_emit_op(bc, stack_opcode_for(e->as.binary.op));
            return;
        case EX_UNARY:
            compile_stack_expr(bc, ast, e->as.unary.value);
            stack_emit_op(bc, stack_opcode_for(e->as.unary.op));
            return;
    }
}

static void compile_stack_stmt(Bytecode *bc, Program *ast, Stmt *s) {
    switch (s->kind) {
        case ST_BLOCK:
            for (size_t i = 0; i < s->as.block.count; i++) compile_stack_stmt(bc, ast, s->as.block.items[i]);
            return;
        case ST_LET:
            compile_stack_expr(bc, ast, s->as.let.value);
            stack_emit_op(bc, S_STORE_LOCAL);
            stack_emit_byte(bc, (uint8_t)s->as.let.local);
            return;
        case ST_ASSIGN:
            if (s->as.assign.target->kind == EX_VAR) {
                compile_stack_expr(bc, ast, s->as.assign.value);
                stack_emit_op(bc, S_STORE_LOCAL);
                stack_emit_byte(bc, (uint8_t)s->as.assign.target->as.var.local);
            } else {
                Expr *target = s->as.assign.target;
                compile_stack_expr(bc, ast, target->as.index.array);
                compile_stack_expr(bc, ast, target->as.index.index);
                compile_stack_expr(bc, ast, s->as.assign.value);
                stack_emit_op(bc, S_SET_ELEMENT);
            }
            return;
        case ST_IF: {
            compile_stack_expr(bc, ast, s->as.branch.condition);
            stack_emit_op(bc, S_JUMP_FALSE);
            size_t false_operand = bc->byte_count;
            stack_emit_u32(bc, 0);
            size_t false_base = bc->byte_count;
            compile_stack_stmt(bc, ast, s->as.branch.yes);
            if (s->as.branch.no) {
                stack_emit_op(bc, S_JUMP);
                size_t end_operand = bc->byte_count;
                stack_emit_u32(bc, 0);
                size_t end_base = bc->byte_count;
                stack_patch_i32(bc, false_operand, false_base, bc->byte_count);
                compile_stack_stmt(bc, ast, s->as.branch.no);
                stack_patch_i32(bc, end_operand, end_base, bc->byte_count);
            } else {
                stack_patch_i32(bc, false_operand, false_base, bc->byte_count);
            }
            return;
        }
        case ST_WHILE: {
            size_t start = bc->byte_count;
            compile_stack_expr(bc, ast, s->as.loop.condition);
            stack_emit_op(bc, S_JUMP_FALSE);
            size_t end_operand = bc->byte_count;
            stack_emit_u32(bc, 0);
            size_t end_base = bc->byte_count;
            compile_stack_stmt(bc, ast, s->as.loop.body);
            stack_emit_op(bc, S_JUMP);
            size_t back_operand = bc->byte_count;
            stack_emit_u32(bc, 0);
            size_t back_base = bc->byte_count;
            stack_patch_i32(bc, back_operand, back_base, start);
            stack_patch_i32(bc, end_operand, end_base, bc->byte_count);
            return;
        }
        case ST_RETURN:
            compile_stack_expr(bc, ast, s->as.ret.value);
            stack_emit_op(bc, S_RETURN);
            return;
        case ST_EXPR:
            compile_stack_expr(bc, ast, s->as.expression.value);
            stack_emit_op(bc, S_POP);
            return;
    }
}

static Bytecode compile_stack(Program *ast, ConstantPool *constants) {
    Bytecode bc = {0};
    bc.constants = constants;
    bc.main_function = (uint8_t)find_function(ast, "main");
    bc.function_count = ast->count;
    bc.functions = xcalloc(bc.function_count, sizeof(*bc.functions));
    for (size_t i = 0; i < ast->count; i++) {
        Function *f = &ast->functions[i];
        if (f->local_count > 255 || f->param_count > 255) fail("stack VM local or parameter limit exceeded");
        bc.functions[i].start = (uint32_t)bc.byte_count;
        bc.functions[i].local_count = (uint16_t)f->local_count;
        bc.functions[i].param_count = (uint8_t)f->param_count;
        compile_stack_stmt(&bc, ast, f->body);
        stack_emit_op(&bc, S_PUSH_CONST);
        stack_emit_byte(&bc, (uint8_t)constant_index(constants, 0));
        stack_emit_op(&bc, S_RETURN);
        bc.functions[i].end = (uint32_t)bc.byte_count;
    }
    return bc;
}

typedef struct { bool constant; uint8_t index; } Operand;
typedef struct { Bytecode *bc; Program *ast; Function *function; unsigned next_reg; } RegBuilder;

static uint32_t reg_word(uint8_t op, uint8_t a, uint8_t b, uint8_t c) {
    return ((uint32_t)op << 24) | ((uint32_t)a << 16) | ((uint32_t)b << 8) | c;
}

static void reg_emit(Bytecode *bc, uint32_t word) {
    if (bc->word_count == bc->word_capacity) {
        bc->word_capacity = bc->word_capacity ? bc->word_capacity * 2 : 128;
        bc->words = xrealloc(bc->words, bc->word_capacity * sizeof(*bc->words));
    }
    bc->words[bc->word_count++] = word;
    bc->static_instructions++;
}

static uint8_t encode_operand(Operand operand) {
    if (operand.constant) {
        if (operand.index >= 128) fail("register VM constant operand exceeds 7-bit limit");
        return (uint8_t)(0x80 | operand.index);
    }
    if (operand.index >= 128) fail("register VM source register exceeds 7-bit limit");
    return operand.index;
}

static unsigned new_register(RegBuilder *b) {
    if (b->next_reg >= MAX_REGISTERS) fail("register VM function exceeds 128 register slots");
    return b->next_reg++;
}

static unsigned reserve_registers(RegBuilder *b, size_t count) {
    if (count > MAX_REGISTERS || b->next_reg + count > MAX_REGISTERS) fail("register VM call arguments exceed register limit");
    unsigned start = b->next_reg;
    b->next_reg += (unsigned)count;
    return start;
}

static unsigned require_register(RegBuilder *b, Operand operand) {
    if (!operand.constant) return operand.index;
    unsigned reg = new_register(b);
    reg_emit(b->bc, reg_word(R_LOADK, (uint8_t)reg, encode_operand(operand), 0));
    return reg;
}

static uint8_t register_opcode_for(Operator op) {
    switch (op) {
        case OP_ADD: return R_ADD; case OP_SUB: return R_SUB; case OP_MUL: return R_MUL;
        case OP_DIV: return R_DIV; case OP_MOD: return R_MOD; case OP_EQ: return R_EQ;
        case OP_NE: return R_NE; case OP_LT: return R_LT; case OP_LE: return R_LE;
        case OP_GT: return R_GT; case OP_GE: return R_GE; case OP_NEG: return R_NEG;
        case OP_NOT: return R_NOT;
    }
    return 0;
}

static Operand compile_reg_expr(RegBuilder *b, Expr *e);

static void compile_reg_expr_into(RegBuilder *b, Expr *e, unsigned destination) {
    switch (e->kind) {
        case EX_INT: {
            size_t ci = constant_index(b->bc->constants, e->as.integer);
            reg_emit(b->bc, reg_word(R_MOVE, (uint8_t)destination, (uint8_t)(0x80 | ci), 0));
            return;
        }
        case EX_VAR:
            if (destination != (unsigned)e->as.var.local)
                reg_emit(b->bc, reg_word(R_MOVE, (uint8_t)destination, (uint8_t)e->as.var.local, 0));
            return;
        case EX_CALL: {
            unsigned first = reserve_registers(b, e->as.call.count);
            for (size_t i = 0; i < e->as.call.count; i++) {
                Operand arg = compile_reg_expr(b, e->as.call.args[i]);
                reg_emit(b->bc, reg_word(R_MOVE, (uint8_t)(first + i), encode_operand(arg), 0));
            }
            reg_emit(b->bc, reg_word(R_CALL, (uint8_t)destination, (uint8_t)e->as.call.function, (uint8_t)first));
            return;
        }
        case EX_NEW_ARRAY: {
            Operand length = compile_reg_expr(b, e->as.new_array.length);
            reg_emit(b->bc, reg_word(R_NEW_ARRAY, (uint8_t)destination, encode_operand(length), 0));
            return;
        }
        case EX_INDEX: {
            Operand array = compile_reg_expr(b, e->as.index.array);
            Operand index = compile_reg_expr(b, e->as.index.index);
            reg_emit(b->bc, reg_word(R_GET_ELEMENT, (uint8_t)destination, encode_operand(array), encode_operand(index)));
            return;
        }
        case EX_BINARY: {
            Operand left = compile_reg_expr(b, e->as.binary.left);
            Operand right = compile_reg_expr(b, e->as.binary.right);
            reg_emit(b->bc, reg_word(register_opcode_for(e->as.binary.op), (uint8_t)destination,
                                     encode_operand(left), encode_operand(right)));
            return;
        }
        case EX_UNARY: {
            Operand value = compile_reg_expr(b, e->as.unary.value);
            reg_emit(b->bc, reg_word(register_opcode_for(e->as.unary.op), (uint8_t)destination,
                                     encode_operand(value), 0));
            return;
        }
    }
}

static Operand compile_reg_expr(RegBuilder *b, Expr *e) {
    if (e->kind == EX_INT) {
        return (Operand){true, (uint8_t)constant_index(b->bc->constants, e->as.integer)};
    }
    if (e->kind == EX_VAR) return (Operand){false, (uint8_t)e->as.var.local};
    unsigned result = new_register(b);
    compile_reg_expr_into(b, e, result);
    return (Operand){false, (uint8_t)result};
}

static void reg_patch_jump(Bytecode *bc, size_t instruction_at, size_t target) {
    int64_t delta = (int64_t)target - (int64_t)(instruction_at + 1);
    if (delta < -8388608 || delta > 8388607) fail("register VM branch is out of range");
    uint32_t word = bc->words[instruction_at];
    bc->words[instruction_at] = (word & 0xff000000u) | ((uint32_t)delta & 0x00ffffffu);
}

static void reg_patch_conditional(Bytecode *bc, size_t instruction_at, size_t target) {
    int64_t delta = (int64_t)target - (int64_t)(instruction_at + 1);
    if (delta < INT16_MIN || delta > INT16_MAX) fail("register VM conditional branch is out of range");
    uint32_t word = bc->words[instruction_at];
    bc->words[instruction_at] = (word & 0xffff0000u) | ((uint16_t)(int16_t)delta);
}

static void compile_reg_stmt(RegBuilder *b, Stmt *s) {
    switch (s->kind) {
        case ST_BLOCK:
            for (size_t i = 0; i < s->as.block.count; i++) compile_reg_stmt(b, s->as.block.items[i]);
            return;
        case ST_LET:
            compile_reg_expr_into(b, s->as.let.value, (unsigned)s->as.let.local);
            return;
        case ST_ASSIGN:
            if (s->as.assign.target->kind == EX_VAR) {
                compile_reg_expr_into(b, s->as.assign.value, (unsigned)s->as.assign.target->as.var.local);
            } else {
                Expr *target = s->as.assign.target;
                Operand array = compile_reg_expr(b, target->as.index.array);
                Operand index = compile_reg_expr(b, target->as.index.index);
                Operand value = compile_reg_expr(b, s->as.assign.value);
                reg_emit(b->bc, reg_word(R_SET_ELEMENT, require_register(b, array), encode_operand(index), encode_operand(value)));
            }
            return;
        case ST_IF: {
            Operand condition = compile_reg_expr(b, s->as.branch.condition);
            unsigned condition_reg = require_register(b, condition);
            size_t false_jump = b->bc->word_count;
            reg_emit(b->bc, reg_word(R_JUMP_ZERO, (uint8_t)condition_reg, 0, 0));
            compile_reg_stmt(b, s->as.branch.yes);
            if (s->as.branch.no) {
                size_t end_jump = b->bc->word_count;
                reg_emit(b->bc, reg_word(R_JUMP, 0, 0, 0));
                reg_patch_conditional(b->bc, false_jump, b->bc->word_count);
                compile_reg_stmt(b, s->as.branch.no);
                reg_patch_jump(b->bc, end_jump, b->bc->word_count);
            } else {
                reg_patch_conditional(b->bc, false_jump, b->bc->word_count);
            }
            return;
        }
        case ST_WHILE: {
            size_t start = b->bc->word_count;
            Operand condition = compile_reg_expr(b, s->as.loop.condition);
            unsigned condition_reg = require_register(b, condition);
            size_t end_jump = b->bc->word_count;
            reg_emit(b->bc, reg_word(R_JUMP_ZERO, (uint8_t)condition_reg, 0, 0));
            compile_reg_stmt(b, s->as.loop.body);
            size_t back_jump = b->bc->word_count;
            reg_emit(b->bc, reg_word(R_JUMP, 0, 0, 0));
            reg_patch_jump(b->bc, back_jump, start);
            reg_patch_conditional(b->bc, end_jump, b->bc->word_count);
            return;
        }
        case ST_RETURN: {
            Operand value = compile_reg_expr(b, s->as.ret.value);
            reg_emit(b->bc, reg_word(R_RETURN, 0, encode_operand(value), 0));
            return;
        }
        case ST_EXPR: {
            unsigned ignored = new_register(b);
            compile_reg_expr_into(b, s->as.expression.value, ignored);
            return;
        }
    }
}

static Bytecode compile_register(Program *ast, ConstantPool *constants) {
    Bytecode bc = {0};
    bc.is_register = true;
    bc.constants = constants;
    bc.main_function = (uint8_t)find_function(ast, "main");
    bc.function_count = ast->count;
    bc.functions = xcalloc(bc.function_count, sizeof(*bc.functions));
    for (size_t i = 0; i < ast->count; i++) {
        Function *f = &ast->functions[i];
        if (f->local_count > MAX_REGISTERS) fail("register VM local limit exceeded");
        bc.functions[i].start = (uint32_t)bc.word_count;
        bc.functions[i].local_count = (uint16_t)f->local_count;
        bc.functions[i].param_count = (uint8_t)f->param_count;
        RegBuilder builder = {&bc, ast, f, (unsigned)f->local_count};
        compile_reg_stmt(&builder, f->body);
        size_t zero = constant_index(constants, 0);
        reg_emit(&bc, reg_word(R_RETURN, 0, (uint8_t)(0x80 | zero), 0));
        if (builder.next_reg > MAX_REGISTERS) fail("register VM function exceeds register limit");
        bc.functions[i].reg_count = (uint16_t)builder.next_reg;
        bc.functions[i].end = (uint32_t)bc.word_count;
    }
    return bc;
}

typedef struct Array {
    size_t length;
    int64_t *items;
} Array;
typedef enum { VALUE_EMPTY, VALUE_INT, VALUE_ARRAY } ValueTag;
typedef struct { ValueTag tag; union { int64_t integer; Array *array; } as; } Value;
typedef struct { uint16_t function; uint32_t pc; size_t base; } StackFrame;
typedef struct {
    uint16_t function;
    uint32_t pc;
    uint8_t return_register;
    Value registers[MAX_REGISTERS];
} RegisterFrame;
typedef struct {
    Bytecode *bytecode;
    bool is_register;
    Value *stack;
    size_t stack_pointer;
    StackFrame *stack_frames;
    size_t stack_depth;
    RegisterFrame *register_frames;
    size_t register_depth;
    Array **arrays;
    size_t array_count;
    size_t array_capacity;
    uint64_t dynamic_instructions;
} VM;

static Value value_int(int64_t n) {
    Value v = {0};
    v.tag = VALUE_INT;
    v.as.integer = n;
    return v;
}

static Value value_array(Array *array) {
    Value v = {0};
    v.tag = VALUE_ARRAY;
    v.as.array = array;
    return v;
}

static int64_t as_integer(Value v) {
    if (v.tag != VALUE_INT) fail("runtime type error: expected int");
    return v.as.integer;
}

static Array *as_array(Value v) {
    if (v.tag != VALUE_ARRAY || !v.as.array) fail("runtime type error: expected int[]");
    return v.as.array;
}

static int64_t from_bits(uint64_t bits) {
    int64_t n;
    memcpy(&n, &bits, sizeof(n));
    return n;
}

static int64_t wrap_add(int64_t a, int64_t b) { return from_bits((uint64_t)a + (uint64_t)b); }
static int64_t wrap_sub(int64_t a, int64_t b) { return from_bits((uint64_t)a - (uint64_t)b); }
static int64_t wrap_mul(int64_t a, int64_t b) { return from_bits((uint64_t)a * (uint64_t)b); }
static int64_t wrap_neg(int64_t a) { return from_bits(0u - (uint64_t)a); }

static void vm_init(VM *vm, Bytecode *bc) {
    memset(vm, 0, sizeof(*vm));
    vm->bytecode = bc;
    vm->is_register = bc->is_register;
    vm->array_capacity = 4096;
    vm->arrays = xcalloc(vm->array_capacity, sizeof(*vm->arrays));
    if (vm->is_register) {
        vm->register_frames = xcalloc(MAX_FRAMES, sizeof(*vm->register_frames));
    } else {
        vm->stack = xcalloc(MAX_STACK_VALUES, sizeof(*vm->stack));
        vm->stack_frames = xcalloc(MAX_FRAMES, sizeof(*vm->stack_frames));
    }
}

static void vm_clear_arrays(VM *vm) {
    for (size_t i = 0; i < vm->array_count; i++) {
        free(vm->arrays[i]->items);
        free(vm->arrays[i]);
        vm->arrays[i] = NULL;
    }
    vm->array_count = 0;
}

static void vm_destroy(VM *vm) {
    vm_clear_arrays(vm);
    free(vm->arrays);
    free(vm->stack);
    free(vm->stack_frames);
    free(vm->register_frames);
    memset(vm, 0, sizeof(*vm));
}

static void vm_prepare_run(VM *vm) {
    vm_clear_arrays(vm);
    vm->stack_pointer = 0;
    vm->stack_depth = 0;
    vm->register_depth = 0;
    vm->dynamic_instructions = 0;
}

static void stack_push(VM *vm, Value value) {
    if (vm->stack_pointer >= MAX_STACK_VALUES) fail("stack VM value stack overflow");
    vm->stack[vm->stack_pointer++] = value;
}

static Value stack_pop(VM *vm) {
    if (vm->stack_pointer == 0) fail("stack VM value stack underflow");
    return vm->stack[--vm->stack_pointer];
}

static Array *vm_new_array(VM *vm, int64_t length) {
    if (length < 0 || (uint64_t)length > MAX_ARRAY_LENGTH) fail("array length must be between 0 and 10,000,000");
    if (vm->array_count == vm->array_capacity) fail("too many live arrays");
    Array *array = xcalloc(1, sizeof(*array));
    array->length = (size_t)length;
    array->items = xcalloc(array->length, sizeof(*array->items));
    vm->arrays[vm->array_count++] = array;
    return array;
}

static int64_t checked_array_index(Array *array, int64_t index) {
    if (index < 0 || (uint64_t)index >= array->length) fail("array index out of bounds");
    return index;
}

static int32_t read_i32(const unsigned char *p) {
    uint32_t value = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return (int32_t)value;
}

static Value stack_binary_value(VM *vm, uint8_t op) {
    int64_t right = as_integer(stack_pop(vm));
    int64_t left = as_integer(stack_pop(vm));
    switch (op) {
        case S_ADD: return value_int(wrap_add(left, right));
        case S_SUB: return value_int(wrap_sub(left, right));
        case S_MUL: return value_int(wrap_mul(left, right));
        case S_DIV:
            if (right == 0) fail("division by zero");
            if (left == INT64_MIN && right == -1) return value_int(INT64_MIN);
            return value_int(left / right);
        case S_MOD:
            if (right == 0) fail("division by zero");
            if (left == INT64_MIN && right == -1) return value_int(0);
            return value_int(left % right);
        case S_EQ: return value_int(left == right);
        case S_NE: return value_int(left != right);
        case S_LT: return value_int(left < right);
        case S_LE: return value_int(left <= right);
        case S_GT: return value_int(left > right);
        case S_GE: return value_int(left >= right);
        default: fail("internal error: invalid binary stack opcode");
    }
    return value_int(0);
}

static int64_t register_binary_value(uint8_t op, int64_t left, int64_t right) {
    switch (op) {
        case R_ADD: return wrap_add(left, right);
        case R_SUB: return wrap_sub(left, right);
        case R_MUL: return wrap_mul(left, right);
        case R_DIV:
            if (right == 0) fail("division by zero");
            if (left == INT64_MIN && right == -1) return INT64_MIN;
            return left / right;
        case R_MOD:
            if (right == 0) fail("division by zero");
            if (left == INT64_MIN && right == -1) return 0;
            return left % right;
        case R_EQ: return left == right;
        case R_NE: return left != right;
        case R_LT: return left < right;
        case R_LE: return left <= right;
        case R_GT: return left > right;
        case R_GE: return left >= right;
        default: fail("internal error: invalid binary register opcode");
    }
    return 0;
}

static Value run_stack_vm(VM *vm, uint64_t *dynamic_count) {
    Bytecode *bc = vm->bytecode;
    int main_id = bc->main_function;
    FunctionCode *main_code = &bc->functions[main_id];
    vm->stack_frames[0] = (StackFrame){(uint16_t)main_id, main_code->start, 0};
    vm->stack_depth = 1;
    for (size_t i = 0; i < main_code->local_count; i++) vm->stack[i] = value_int(0);
    vm->stack_pointer = main_code->local_count;

    for (;;) {
        if (!vm->stack_depth) fail("stack VM returned without a result");
        StackFrame *frame = &vm->stack_frames[vm->stack_depth - 1];
        if (frame->pc >= bc->byte_count) fail("stack VM program counter out of range");
        uint8_t op = bc->bytes[frame->pc++];
        vm->dynamic_instructions++;
        switch (op) {
            case S_PUSH_CONST: {
                uint8_t index = bc->bytes[frame->pc++];
                stack_push(vm, value_int(bc->constants->values[index]));
                break;
            }
            case S_LOAD_LOCAL: {
                uint8_t index = bc->bytes[frame->pc++];
                stack_push(vm, vm->stack[frame->base + index]);
                break;
            }
            case S_STORE_LOCAL: {
                uint8_t index = bc->bytes[frame->pc++];
                vm->stack[frame->base + index] = stack_pop(vm);
                break;
            }
            case S_NEW_ARRAY: {
                int64_t length = as_integer(stack_pop(vm));
                stack_push(vm, value_array(vm_new_array(vm, length)));
                break;
            }
            case S_GET_ELEMENT: {
                int64_t index = as_integer(stack_pop(vm));
                Array *array = as_array(stack_pop(vm));
                stack_push(vm, value_int(array->items[checked_array_index(array, index)]));
                break;
            }
            case S_SET_ELEMENT: {
                int64_t value = as_integer(stack_pop(vm));
                int64_t index = as_integer(stack_pop(vm));
                Array *array = as_array(stack_pop(vm));
                array->items[checked_array_index(array, index)] = value;
                break;
            }
            case S_ADD: case S_SUB: case S_MUL: case S_DIV: case S_MOD:
            case S_EQ: case S_NE: case S_LT: case S_LE: case S_GT: case S_GE:
                stack_push(vm, stack_binary_value(vm, op));
                break;
            case S_NEG: stack_push(vm, value_int(wrap_neg(as_integer(stack_pop(vm))))); break;
            case S_NOT: stack_push(vm, value_int(as_integer(stack_pop(vm)) == 0)); break;
            case S_JUMP: {
                int32_t delta = read_i32(bc->bytes + frame->pc);
                frame->pc += 4;
                frame->pc = (uint32_t)((int64_t)frame->pc + delta);
                break;
            }
            case S_JUMP_FALSE: {
                int32_t delta = read_i32(bc->bytes + frame->pc);
                frame->pc += 4;
                int64_t condition = as_integer(stack_pop(vm));
                if (condition == 0) frame->pc = (uint32_t)((int64_t)frame->pc + delta);
                break;
            }
            case S_CALL: {
                uint8_t function_id = bc->bytes[frame->pc++];
                uint8_t arg_count = bc->bytes[frame->pc++];
                if (vm->stack_depth >= MAX_FRAMES) fail("stack VM call depth exceeded");
                FunctionCode *callee = &bc->functions[function_id];
                if (arg_count != callee->param_count || vm->stack_pointer < arg_count) fail("invalid stack VM call");
                size_t base = vm->stack_pointer - arg_count;
                if (base + callee->local_count >= MAX_STACK_VALUES) fail("stack VM value stack overflow");
                for (size_t i = arg_count; i < callee->local_count; i++) vm->stack[base + i] = value_int(0);
                vm->stack_pointer = base + callee->local_count;
                vm->stack_frames[vm->stack_depth++] = (StackFrame){function_id, callee->start, base};
                break;
            }
            case S_RETURN: {
                Value result = stack_pop(vm);
                size_t base = frame->base;
                if (vm->stack_depth == 1) {
                    *dynamic_count = vm->dynamic_instructions;
                    return result;
                }
                vm->stack_depth--;
                vm->stack_pointer = base;
                stack_push(vm, result);
                break;
            }
            case S_POP: (void)stack_pop(vm); break;
            default: fail("unknown stack VM opcode");
        }
    }
}

static Value register_source(VM *vm, RegisterFrame *frame, uint8_t encoded) {
    if (encoded & 0x80) return value_int(vm->bytecode->constants->values[encoded & 0x7f]);
    return frame->registers[encoded];
}

static Value run_register_vm(VM *vm, uint64_t *dynamic_count) {
    Bytecode *bc = vm->bytecode;
    if (bc->function_count == 0) fail("missing main function");
    uint8_t main_id = bc->main_function;
    FunctionCode *main_code = &bc->functions[main_id];
    RegisterFrame *main = &vm->register_frames[0];
    memset(main, 0, sizeof(*main));
    main->function = main_id;
    main->pc = main_code->start;
    vm->register_depth = 1;

    for (;;) {
        if (!vm->register_depth) fail("register VM returned without a result");
        RegisterFrame *frame = &vm->register_frames[vm->register_depth - 1];
        if (frame->pc >= bc->word_count) fail("register VM program counter out of range");
        uint32_t word = bc->words[frame->pc++];
        uint8_t op = (uint8_t)(word >> 24);
        uint8_t a = (uint8_t)(word >> 16);
        uint8_t b = (uint8_t)(word >> 8);
        uint8_t c = (uint8_t)word;
        vm->dynamic_instructions++;
        switch (op) {
            case R_LOADK:
            case R_MOVE:
                frame->registers[a] = register_source(vm, frame, b);
                break;
            case R_NEW_ARRAY: {
                int64_t length = as_integer(register_source(vm, frame, b));
                frame->registers[a] = value_array(vm_new_array(vm, length));
                break;
            }
            case R_GET_ELEMENT: {
                Array *array = as_array(register_source(vm, frame, b));
                int64_t index = as_integer(register_source(vm, frame, c));
                frame->registers[a] = value_int(array->items[checked_array_index(array, index)]);
                break;
            }
            case R_SET_ELEMENT: {
                Array *array = as_array(frame->registers[a]);
                int64_t index = as_integer(register_source(vm, frame, b));
                int64_t value = as_integer(register_source(vm, frame, c));
                array->items[checked_array_index(array, index)] = value;
                break;
            }
            case R_ADD: case R_SUB: case R_MUL: case R_DIV: case R_MOD:
            case R_EQ: case R_NE: case R_LT: case R_LE: case R_GT: case R_GE: {
                int64_t left = as_integer(register_source(vm, frame, b));
                int64_t right = as_integer(register_source(vm, frame, c));
                frame->registers[a] = value_int(register_binary_value(op, left, right));
                break;
            }
            case R_NEG: frame->registers[a] = value_int(wrap_neg(as_integer(register_source(vm, frame, b)))); break;
            case R_NOT: frame->registers[a] = value_int(as_integer(register_source(vm, frame, b)) == 0); break;
            case R_JUMP: {
                int32_t delta = (int32_t)(word & 0x00ffffffu);
                if (delta & 0x00800000) delta |= (int32_t)0xff000000u;
                frame->pc = (uint32_t)((int64_t)frame->pc + delta);
                break;
            }
            case R_JUMP_ZERO: {
                int16_t delta = (int16_t)(word & 0xffffu);
                if (as_integer(frame->registers[a]) == 0)
                    frame->pc = (uint32_t)((int64_t)frame->pc + delta);
                break;
            }
            case R_CALL: {
                uint8_t destination = a;
                uint8_t function_id = b;
                uint8_t first_argument = c;
                if (vm->register_depth >= MAX_FRAMES) fail("register VM call depth exceeded");
                FunctionCode *callee = &bc->functions[function_id];
                if (callee->param_count > MAX_REGISTERS || first_argument + callee->param_count > MAX_REGISTERS)
                    fail("invalid register VM call arguments");
                RegisterFrame *next = &vm->register_frames[vm->register_depth++];
                next->function = function_id;
                next->pc = callee->start;
                next->return_register = destination;
                memset(next->registers, 0, callee->reg_count * sizeof(Value));
                for (size_t i = 0; i < callee->param_count; i++)
                    next->registers[i] = frame->registers[first_argument + i];
                break;
            }
            case R_RETURN: {
                Value result = register_source(vm, frame, b);
                if (vm->register_depth == 1) {
                    *dynamic_count = vm->dynamic_instructions;
                    return result;
                }
                uint8_t destination = frame->return_register;
                vm->register_depth--;
                vm->register_frames[vm->register_depth - 1].registers[destination] = result;
                break;
            }
            default: fail("unknown register VM opcode");
        }
    }
}

static Value run_vm(VM *vm, uint64_t *dynamic_count) {
    return vm->is_register ? run_register_vm(vm, dynamic_count) : run_stack_vm(vm, dynamic_count);
}

typedef struct {
    const char *name;
    const char *input_description;
} Workload;

static const Workload workloads[] = {
    {"sum", "20,000,000 additions"},
    {"fibonacci", "recursive fib(30)"},
    {"ackermann", "ack(3, 8)"},
    {"sieve", "100 passes over 8,192 values"},
    {"heapsort", "50,000 values"},
    {"matrix", "50 x 50 matrix multiplication"}
};
static const size_t workload_count = sizeof(workloads) / sizeof(workloads[0]);

static char *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    if (fseek(file, 0, SEEK_END) != 0) fail("cannot seek source file");
    long size = ftell(file);
    if (size < 0) fail("cannot read source file size");
    if (fseek(file, 0, SEEK_SET) != 0) fail("cannot rewind source file");
    char *source = xmalloc((size_t)size + 1);
    size_t n = fread(source, 1, (size_t)size, file);
    if (n != (size_t)size || ferror(file)) fail("cannot read source file");
    fclose(file);
    source[n] = '\0';
    *length = n;
    return source;
}

static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) fail("clock_gettime(CLOCK_MONOTONIC) failed");
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static double measure_run(VM *vm, int64_t expected, uint64_t *dynamic_count) {
    vm_prepare_run(vm);
    double begin = now_seconds();
    Value result = run_vm(vm, dynamic_count);
    double end = now_seconds();
    if (as_integer(result) != expected) fail("VM result changed between repetitions");
    return end - begin;
}

static int compare_double(const void *a, const void *b) {
    double left = *(const double *)a;
    double right = *(const double *)b;
    return (left > right) - (left < right);
}

static void run_workload(const Workload *workload) {
    char path[256];
    if (snprintf(path, sizeof(path), "workloads/%s.sv", workload->name) >= (int)sizeof(path)) fail("workload path too long");
    error_path = path;
    error_line = 1;
    size_t source_length = 0;
    char *source = read_file(path, &source_length);
    Program ast = parse_program(source, source_length);
    type_check(&ast);
    ConstantPool constants = collect_constants(&ast);
    Bytecode stack_code = compile_stack(&ast, &constants);
    Bytecode register_code = compile_register(&ast, &constants);
    VM stack_vm, register_vm;
    vm_init(&stack_vm, &stack_code);
    vm_init(&register_vm, &register_code);

    uint64_t stack_dynamic = 0, register_dynamic = 0;
    vm_prepare_run(&stack_vm);
    Value stack_warmup = run_vm(&stack_vm, &stack_dynamic);
    vm_prepare_run(&register_vm);
    Value register_warmup = run_vm(&register_vm, &register_dynamic);
    int64_t result = as_integer(stack_warmup);
    if (result != as_integer(register_warmup)) fail("VMs produced different results during warm-up");

    double stack_times[9], register_times[9];
    for (size_t i = 0; i < 9; i++) {
        uint64_t count = 0;
        if ((i & 1) == 0) {
            stack_times[i] = measure_run(&stack_vm, result, &count);
            if (i == 0) stack_dynamic = count;
            register_times[i] = measure_run(&register_vm, result, &count);
            if (i == 0) register_dynamic = count;
        } else {
            register_times[i] = measure_run(&register_vm, result, &count);
            if (i == 1) register_dynamic = count;
            stack_times[i] = measure_run(&stack_vm, result, &count);
            if (i == 1) stack_dynamic = count;
        }
    }
    qsort(stack_times, 9, sizeof(double), compare_double);
    qsort(register_times, 9, sizeof(double), compare_double);

    size_t stack_stream = stack_code.byte_count;
    size_t register_stream = register_code.word_count * sizeof(uint32_t);
    size_t pool_bytes = constants.count * sizeof(int64_t);
    double stack_ms = stack_times[4] * 1000.0;
    double register_ms = register_times[4] * 1000.0;
    double speedup = register_ms > 0.0 ? stack_ms / register_ms : 0.0;
    printf("| %s (%s) | %" PRId64 " | %" PRIu64 " / %" PRIu64 " | %" PRIu64 " / %" PRIu64 " | %zu / %zu | %zu / %zu | %.3f (%.3f-%.3f) | %.3f (%.3f-%.3f) | %.3fx |\n",
           workload->name, workload->input_description, result,
           stack_code.static_instructions, register_code.static_instructions,
           stack_dynamic, register_dynamic,
           stack_stream, stack_stream + pool_bytes,
           register_stream, register_stream + pool_bytes,
           stack_ms, stack_times[0] * 1000.0, stack_times[8] * 1000.0,
           register_ms, register_times[0] * 1000.0, register_times[8] * 1000.0,
           speedup);

    vm_destroy(&stack_vm);
    vm_destroy(&register_vm);
    free(stack_code.bytes);
    free(register_code.words);
    free(stack_code.functions);
    free(register_code.functions);
    free(source);
    (void)ast;
}

static void print_system_info(void) {
    struct utsname info;
    if (uname(&info) == 0) printf("System: %s %s %s\n", info.sysname, info.release, info.machine);
#if defined(__clang__)
    printf("Compiler: clang %s; flags: -O2 -std=c11 -Wall -Wextra -Wpedantic\n", __clang_version__);
#elif defined(__GNUC__)
    printf("Compiler: GCC %s; flags: -O2 -std=c11 -Wall -Wextra -Wpedantic\n", __VERSION__);
#else
    printf("Compiler: C11 compiler; flags: -O2 -std=c11 -Wall -Wextra -Wpedantic\n");
#endif
    printf("Timing: CLOCK_MONOTONIC; one warm-up and nine measured runs per VM/workload; alternating order\n\n");
}

static void print_usage(const char *argv0) {
    fprintf(stderr, "usage: %s [all|sum|fibonacci|ackermann|sieve|heapsort|matrix|--list]\n", argv0);
}

int main(int argc, char **argv) {
    if (argc > 2) {
        print_usage(argv[0]);
        return 2;
    }
    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        for (size_t i = 0; i < workload_count; i++) printf("%s\t%s\n", workloads[i].name, workloads[i].input_description);
        return 0;
    }
    print_system_info();
    printf("| Workload | Result | Static instructions (stack / register) | Dynamic instructions (stack / register) | Stream / total bytes (stack) | Stream / total bytes (register) | Stack time ms (median, min-max) | Register time ms (median, min-max) | Register speedup |\n");
    printf("|---|---:|---:|---:|---:|---:|---:|---:|---:|\n");
    if (argc == 1 || strcmp(argv[1], "all") == 0) {
        for (size_t i = 0; i < workload_count; i++) run_workload(&workloads[i]);
        return 0;
    }
    for (size_t i = 0; i < workload_count; i++) {
        if (strcmp(argv[1], workloads[i].name) == 0) {
            run_workload(&workloads[i]);
            return 0;
        }
    }
    print_usage(argv[0]);
    return 2;
}
