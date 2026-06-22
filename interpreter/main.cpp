#include <iostream>
#include <iomanip>
#include <fstream>
#include <cctype>
#include <cstring>

#include <vector>
#include <string>
#include <stdexcept>

using namespace std;

class InterpretError : public exception {
    string msg_;
public:
    explicit InterpretError(string msg) : msg_(move(msg)) {}

    InterpretError(string msg, int line)
            : msg_("line " + to_string(line) + ": " + move(msg)) {}

    const char *what() const noexcept override { return msg_.c_str(); }
};

enum class types_lex {
    LEX_NULL,
    LEX_PROGRAM,
    LEX_INT, LEX_BOOL, LEX_STRING, LEX_STRING_VAL,
    LEX_READ, LEX_WRITE,
    LEX_IF, LEX_ELSE,
    LEX_DO, LEX_WHILE,
    LEX_TRUE, LEX_FALSE,
    LEX_AND, LEX_NOT, LEX_OR,
    LEX_PLUS, LEX_MINUS,
    LEX_SLASH, LEX_STAR,
    LEX_GRT, LEX_LSS, LEX_GOE, LEX_LOE,
    LEX_EQ, LEX_NEQ,
    LEX_ASSIGN,
    LEX_COMMA, LEX_SEMICOLON, LEX_LPAREN, LEX_RPAREN,
    LEX_STBLOCK, LEX_FNBLOCK, LEX_QOTE,

    LEX_NUM,
    LEX_ID,

    POLIZ_LABEL, POLIZ_ADDRESS, POLIZ_GO, POLIZ_FGO
};

enum class ValType { NONE, INT, STR };

ostream &operator<<(ostream &os, types_lex t) {
    return os << static_cast<int>(t);
}

const vector<string> TW = {"", "program", "int", "string", "read", "write", "if", "else",
                           "do", "while", "true", "false", "or", "and", "not"};
const vector<types_lex> wtypes = {
        types_lex::LEX_NULL,
        types_lex::LEX_PROGRAM,
        types_lex::LEX_INT, types_lex::LEX_STRING,
        types_lex::LEX_READ, types_lex::LEX_WRITE,
        types_lex::LEX_IF, types_lex::LEX_ELSE,
        types_lex::LEX_DO, types_lex::LEX_WHILE,
        types_lex::LEX_TRUE, types_lex::LEX_FALSE,
        types_lex::LEX_OR, types_lex::LEX_AND, types_lex::LEX_NOT
};
const vector<string> TD = {"", "+", "-", "/", "*", ">", "<", ">=", "<=", "==",
                           "!=", "=", ",", ";", "(", ")", "{", "}", "\"", "!"};
const vector<types_lex> dtypes = {
        types_lex::LEX_NULL,
        types_lex::LEX_PLUS, types_lex::LEX_MINUS, types_lex::LEX_SLASH, types_lex::LEX_STAR,
        types_lex::LEX_GRT, types_lex::LEX_LSS, types_lex::LEX_GOE, types_lex::LEX_LOE,
        types_lex::LEX_EQ, types_lex::LEX_NEQ,
        types_lex::LEX_ASSIGN,
        types_lex::LEX_COMMA, types_lex::LEX_SEMICOLON, types_lex::LEX_LPAREN, types_lex::LEX_RPAREN,
        types_lex::LEX_STBLOCK, types_lex::LEX_FNBLOCK, types_lex::LEX_QOTE,
        types_lex::LEX_NOT
};

/* ------------------------- Lex ------------------------- */
class Lex {
    types_lex type_;
    int val_ = 0;
    string str_;
    bool bval_ = false;
public:
    Lex(types_lex new_t = types_lex::LEX_NULL, int new_val = 0) : type_(new_t), val_(new_val) {}

    Lex(types_lex new_t, bool new_b) : type_(new_t), bval_(new_b) {}

    Lex(types_lex new_t, string new_str) : type_(new_t), str_(move(new_str)) {}

    types_lex get_type() const { return type_; }

    int get_val() const { return val_; }

    const string &get_str() const { return str_; }

    bool get_bool() const { return bval_; }

    friend ostream &operator<<(ostream &os, const Lex &out_lex) {
        return os << "numb of type = " << out_lex.get_type() << ", value = " << out_lex.get_val() << endl;
    }
};

/* ------------------------- Ident ------------------------- */
class Ident {
    string name_;
    types_lex type_ = types_lex::LEX_NULL;
    int val_ = 0;
    string str_;
    bool declaration_ = false;
    bool assign_ = false;
public:
    Ident() = default;

    const string &get_name() const { return name_; }

    void set_name(const string &new_name) { name_ = new_name; }

    const string &get_str() const { return str_; }

    void set_str(const string &new_str) { str_ = new_str; }

    types_lex get_type() const { return type_; }

    void set_type(types_lex new_type) { type_ = new_type; }

    int get_val() const { return val_; }

    void set_val(int new_val) { val_ = new_val; }

    bool get_declaration() const { return declaration_; }

    void set_declaration() { declaration_ = true; }

    bool get_assign() const { return assign_; }

    void set_assign() { assign_ = true; }
};

class TableIdent {
    vector<Ident> table_;
public:
    TableIdent() { table_.resize(1); }

    Ident &operator[](int k) {
        if (k < 0 || k >= static_cast<int>(table_.size()))
            throw InterpretError("identifier table: index out of range");
        return table_[k];
    }

    int put(const string &buf) {
        for (size_t i = 1; i < table_.size(); ++i) {
            if (buf == table_[i].get_name())
                return static_cast<int>(i);
        }
        table_.emplace_back();
        table_.back().set_name(buf);
        return static_cast<int>(table_.size()) - 1;
    }
};

/* ------------------------- Lexer ------------------------- */
class Lexer {
    ifstream fd;
    int c;
    int line_ = 1;

    enum state {
        H, IDENT, NUMB, COM, COM2, QOUT, COMPR, NOT, DELIM, FIN_OF_FILE, SLASH
    } G;

    void get_symb() {
        if (c == '\n') line_++;
        c = fd.get();
    }

    static int define_type(const string &buf, const vector<string> &table) {
        for (size_t i = 0; i < table.size(); ++i)
            if (buf == table[i]) return static_cast<int>(i);
        return 0;
    }

    TableIdent &tid;
    bool not_eof = true;

public:
    Lexer(const string &path, TableIdent &t) : tid(t) {
        fd.open(path);
        if (!fd.is_open())
            throw InterpretError("file didn't open: " + path);
        get_symb();
    }

    bool is_eof() const { return not_eof; }

    int get_line() const { return line_; }

    Lex get_lex();
};

Lex Lexer::get_lex() {
    int digit = 0;
    int num;
    string buf;
    string str;

    G = H;
    while (true) {
        switch (G) {
            case H:
                if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
                    get_symb();
                else if (c == '#') {
                    get_symb();
                    G = COM2;
                } else if (isalpha(c)) {
                    buf.clear();
                    buf.push_back(static_cast<char>(c));
                    get_symb();
                    G = IDENT;
                } else if (isdigit(c)) {
                    digit = c - '0';
                    get_symb();
                    G = NUMB;
                } else if (c == '/') {
                    buf = "/";
                    get_symb();
                    if (c == '*') {
                        get_symb();
                        G = COM;
                    } else
                        G = SLASH;
                } else if (c == '"') {
                    get_symb();
                    G = QOUT;
                } else if (c == '<' || c == '>' || c == '=') {
                    buf.clear();
                    buf.push_back(static_cast<char>(c));
                    get_symb();
                    G = COMPR;
                } else if (c == '!') {
                    buf.clear();
                    buf.push_back(static_cast<char>(c));
                    get_symb();
                    G = NOT;
                } else if (c != EOF) {
                    buf.clear();
                    buf.push_back(static_cast<char>(c));
                    G = DELIM;
                } else
                    G = FIN_OF_FILE;
                break;

            case IDENT:
                if (isalpha(c) || isdigit(c)) {
                    buf.push_back(static_cast<char>(c));
                    get_symb();
                } else if ((num = define_type(buf, TW)))
                    return Lex(wtypes[num], num);
                else {
                    num = tid.put(buf);
                    return Lex(types_lex::LEX_ID, num);
                }
                break;

            case NUMB:
                if (isdigit(c)) {
                    digit = digit * 10 + c - '0';
                    get_symb();
                } else
                    return Lex(types_lex::LEX_NUM, digit);
                break;

            case COM:
                if (c == '*') {
                    get_symb();
                    if (c == '/') {
                        get_symb();
                        G = H;
                    }
                } else if (c != EOF)
                    get_symb();
                else
                    G = FIN_OF_FILE;
                break;

            case COM2:
                if (c == '\n') {
                    get_symb();
                    G = H;
                } else if (c == EOF)
                    G = FIN_OF_FILE;
                else
                    get_symb();
                break;

            case SLASH:
                num = define_type(buf, TD);
                return Lex(dtypes[num], num);

            case QOUT:
                if (c == '"') {
                    get_symb();
                    return Lex(types_lex::LEX_STRING_VAL, str);
                } else if (c != EOF) {
                    str.push_back(static_cast<char>(c));
                    get_symb();
                } else
                    G = FIN_OF_FILE;
                break;

            case COMPR:
                if (c == '=') {
                    buf.push_back(static_cast<char>(c));
                    get_symb();
                }
                num = define_type(buf, TD);
                return Lex(dtypes[num], num);

            case NOT:
                if (c == '=') {
                    buf.push_back(static_cast<char>(c));
                    get_symb();
                }
                num = define_type(buf, TD);
                return Lex(dtypes[num], num);

            case DELIM:
                if ((num = define_type(buf, TD))) {
                    get_symb();
                    return Lex(dtypes[num], num);
                } else
                    throw InterpretError(
                            string("unexpected symbol '") + static_cast<char>(c) + "'", line_);

            case FIN_OF_FILE:
                not_eof = false;
                return Lex();
        }
    }
}

/* ------------------------- Stack ------------------------- */
template<typename T, int max_size>
class Stack {
    T s[max_size];
    int top;

public:
    Stack() : top(0) {}

    void push(T t) {
        if (!is_full())
            s[top++] = t;
        else
            throw InterpretError("stack overflow");
    }

    T pop() {
        if (!is_empty())
            return s[--top];
        else
            throw InterpretError("stack underflow");
    }

    void reset() { top = 0; }

    bool is_empty() const { return top == 0; }

    bool is_full() const { return top == max_size; }
};

/* ------------------------- Function ------------------------- */
class Function {
    string name;
    int poliz_num = 0;
    int num_str = 0;
    int num_int = 0;
    vector<types_lex> array;

public:
    Function() = default;

    const string &get_name() const { return name; }

    void set_name(const string &n) { name = n; }

    void add_int() { num_int++; }

    void add_str() { num_str++; }

    void push(types_lex l) { array.push_back(l); }

    types_lex get_el(int i) const {
        if (i < 0 || i >= static_cast<int>(array.size()))
            throw InterpretError("function args: index out of range");
        return array[i];
    }

    void set_num_str(int i) { num_str = i; }

    int get_num_str() const { return num_str; }

    void set_num_int(int i) { num_int = i; }

    int get_num_int() const { return num_int; }

    void set_poliz_num(int i) { poliz_num = i; }

    int get_poliz_num() const { return poliz_num; }

    void add_var(types_lex l) {
        if (l == types_lex::LEX_INT)
            add_int();
        else if (l == types_lex::LEX_STRING)
            add_str();
        else
            throw l;
        push(l);
    }
};

class FTable {
    vector<Function> table;

public:
    FTable() { table.resize(1); }

    int put(const string &buf) {
        table.emplace_back();
        table.back().set_name(buf);
        return static_cast<int>(table.size()) - 1;
    }

    Function &operator[](int i) {
        if (i < 0 || i >= static_cast<int>(table.size()))
            throw InterpretError("function table: index out of range");
        return table[i];
    }

    bool check_fn(const string &buf) const {
        for (size_t i = 1; i < table.size(); ++i)
            if (buf == table[i].get_name())
                return true;
        return false;
    }

    int get_top() const { return static_cast<int>(table.size()); }
};

/* ------------------------- Poliz ------------------------- */
class Poliz {
    vector<Lex> p;
    int free_ = 0;

public:
    Poliz(int max_size) { p.resize(max_size); }

    void put_lex(Lex l) {
        p[free_] = move(l);
        free_++;
    }

    types_lex get_c_type() { return p[free_ - 1].get_type(); }

    void put_lex(Lex l, int place) { p[place] = move(l); }

    void blank() { free_++; }

    int get_free() const { return free_; }

    Lex &operator[](int ident) {
        if (ident < 0 || ident >= static_cast<int>(p.size()))
            throw InterpretError("Poliz: out of array");
        if (ident >= free_)
            throw InterpretError("Poliz: indefinite element of array");
        return p[ident];
    }

    void print() {
        for (int i = 0; i < free_; i++) cout << p[i];
    }
};

/* ------------------------- Parser ------------------------- */
class Parser {
    Lex curr_lex;
    types_lex c_type = types_lex::LEX_NULL;
    int c_IntVal = 0;
    string c_StrVal;

    Stack<types_lex, 100> st_lex;

    types_lex tmp = types_lex::LEX_NULL;

    TableIdent &TID;
    FTable &FN;
    Lexer scan;

    InterpretError err(const string &what) {
        return InterpretError(what, scan.get_line());
    }

    void check_id() {
        if (TID[c_IntVal].get_declaration())
            st_lex.push(TID[c_IntVal].get_type());
        else
            throw err("identifier is not declared");
    }

    void check_semicolon() {
        if (c_type != types_lex::LEX_SEMICOLON)
            throw err("';' expected");
        get_lex();
    }

    void get_lex() {
        curr_lex = scan.get_lex();
        c_type = curr_lex.get_type();
        c_IntVal = curr_lex.get_val();
        c_StrVal = curr_lex.get_str();
    }

    void check_not() {
        if (st_lex.pop() != types_lex::LEX_BOOL)
            throw err("boolean operand expected for '!'");
        else {
            st_lex.push(types_lex::LEX_BOOL);
            prog.put_lex(Lex(types_lex::LEX_NOT, 0));
        }
    }

    void check_op() {
        types_lex t1, t2, op, r, t;
        t2 = st_lex.pop();
        op = st_lex.pop();
        t1 = st_lex.pop();
        t = t1;
        if (t1 == t2) {
            if (t == types_lex::LEX_BOOL) {
                if (op == types_lex::LEX_AND || op == types_lex::LEX_OR)
                    r = types_lex::LEX_BOOL;
                else
                    throw err("invalid operator for boolean operands");
            } else if (t == types_lex::LEX_INT) {
                if (op == types_lex::LEX_EQ || op == types_lex::LEX_NEQ || op == types_lex::LEX_GRT ||
                    op == types_lex::LEX_LSS || op == types_lex::LEX_GOE || op == types_lex::LEX_LOE)
                    r = types_lex::LEX_BOOL;
                else if (op == types_lex::LEX_MINUS || op == types_lex::LEX_PLUS || op == types_lex::LEX_SLASH ||
                         op == types_lex::LEX_STAR)
                    r = types_lex::LEX_INT;
                else
                    throw err("invalid operator for int operands");
            } else if (t == types_lex::LEX_STRING) {
                if (op == types_lex::LEX_EQ || op == types_lex::LEX_NEQ || op == types_lex::LEX_GRT ||
                    op == types_lex::LEX_LSS || op == types_lex::LEX_GOE || op == types_lex::LEX_LOE)
                    r = types_lex::LEX_BOOL;
                else if (op == types_lex::LEX_PLUS)
                    r = types_lex::LEX_STRING;
                else
                    throw err("invalid operator for string operands");
            } else
                throw err("invalid operand type in expression");
        } else
            throw err("operand types do not match");
        st_lex.push(r);
        prog.put_lex(Lex(op, 0));
    }

    void eq_bool() {
        if (st_lex.pop() != types_lex::LEX_BOOL)
            throw err("expression is not boolean");
    }

    void eq_type() {
        if (st_lex.pop() != st_lex.pop())
            throw err("type mismatch in assignment");
    }

    void check_Lparen() {
        if (c_type != types_lex::LEX_LPAREN)
            throw err("'(' expected");
        get_lex();
    }

    void check_Rparen() {
        if (c_type != types_lex::LEX_RPAREN)
            throw err("')' expected");
        get_lex();
    }

    void check_decl() {
        if (!TID[c_IntVal].get_declaration())
            throw err("identifier is not declared");
    }

    void P();

    void B();

    void S();

    void DE();

    void PE();

    void E();

    void E1();

    void T();

    void F();

    void STR();

    void I();

public:
    Poliz prog;

    Parser(const string &program, TableIdent &tid, FTable &fn)
            : TID(tid), FN(fn), scan(program, tid), prog(1000) {}

    void analyze();
};

void Parser::analyze() {
    get_lex();
    P();
    cout << "Poliz's output:\n";
    prog.print();
    cout << "Program's output:\n";
    cout << "Success!\n";
}

void Parser::P() {
    if (c_type != types_lex::LEX_PROGRAM)
        throw err("'program' expected");
    get_lex();
    B();
}

void Parser::B() {
    if (c_type == types_lex::LEX_STBLOCK) {
        get_lex();
        while (c_type != types_lex::LEX_FNBLOCK) {
            S();
        }
        get_lex();
    } else
        throw err("'{' expected");
}

void Parser::S() {
    int pl0, pl1, pl2, pl3;

    if (c_type == types_lex::LEX_STRING) {
        get_lex();
        STR();
    } else if (c_type == types_lex::LEX_INT) {
        get_lex();
        I();
    } else if (c_type == types_lex::LEX_IF) {
        get_lex();
        check_Lparen();
        PE();
        eq_bool();
        check_Rparen();
        pl2 = prog.get_free();
        prog.blank();
        prog.put_lex(Lex(types_lex::POLIZ_FGO, 0));

        if (c_type == types_lex::LEX_STBLOCK)
            B();
        else
            S();

        if (c_type == types_lex::LEX_ELSE) {
            pl3 = prog.get_free();
            prog.blank();
            prog.put_lex(Lex(types_lex::POLIZ_GO, 0));
            prog.put_lex(Lex(types_lex::POLIZ_LABEL, prog.get_free()), pl2);
            get_lex();
            if (c_type == types_lex::LEX_STBLOCK) {
                B();
                prog.put_lex(Lex(types_lex::POLIZ_LABEL, prog.get_free()), pl3);
            } else {
                S();
                prog.put_lex(Lex(types_lex::POLIZ_LABEL, prog.get_free()), pl3);
            }
        } else
            prog.put_lex(Lex(types_lex::POLIZ_LABEL, prog.get_free()), pl2);
    } else if (c_type == types_lex::LEX_WHILE) {
        pl0 = prog.get_free();
        get_lex();
        check_Lparen();
        PE();
        eq_bool();
        check_Rparen();
        pl1 = prog.get_free();
        prog.blank();
        prog.put_lex(Lex(types_lex::POLIZ_FGO, 0));

        if (c_type == types_lex::LEX_STBLOCK)
            B();
        else
            S();

        prog.put_lex(Lex(types_lex::POLIZ_LABEL, pl0));
        prog.put_lex(Lex(types_lex::POLIZ_GO, 0));
        prog.put_lex(Lex(types_lex::POLIZ_LABEL, prog.get_free()), pl1);
    } else if (c_type == types_lex::LEX_READ) {
        int i;
        i = FN.put("read");
        get_lex();
        check_Lparen();
        while (c_type == types_lex::LEX_ID) {
            check_decl();
            FN[i].add_var(TID[c_IntVal].get_type());
            prog.put_lex(Lex(types_lex::POLIZ_ADDRESS, c_IntVal));
            get_lex();
            if (c_type == types_lex::LEX_COMMA)
                get_lex();
            else break;
        }
        check_Rparen();
        check_semicolon();
        FN[i].set_poliz_num(prog.get_free());
        prog.put_lex(Lex(types_lex::LEX_READ, 0));
    } else if (c_type == types_lex::LEX_WRITE) {
        int i;
        i = FN.put("write");
        get_lex();
        check_Lparen();
        E();
        FN[i].add_var(tmp);
        while (c_type == types_lex::LEX_COMMA) {
            get_lex();
            E();
            FN[i].add_var(tmp);
        }
        check_Rparen();
        check_semicolon();
        FN[i].set_poliz_num(prog.get_free());
        prog.put_lex(Lex(types_lex::LEX_WRITE, 0));
    } else if (c_type == types_lex::LEX_ID) {
        check_id();
        prog.put_lex(Lex(types_lex::POLIZ_ADDRESS, c_IntVal));
        get_lex();
        if (c_type == types_lex::LEX_ASSIGN) {
            get_lex();
            DE();
            eq_type();
            prog.put_lex(Lex(types_lex::LEX_ASSIGN, 0));
        } else
            throw err("'=' expected");
        check_semicolon();
        prog.put_lex(Lex(types_lex::LEX_SEMICOLON, 0));
    } else
        B();
}

void Parser::STR() {
    if (c_type != types_lex::LEX_ID)
        throw err("identifier expected in declaration");

    int var_num;
    while (c_type != types_lex::LEX_SEMICOLON) {
        if (c_type != types_lex::LEX_ID)
            throw err("identifier expected in declaration");

        var_num = c_IntVal;
        if (TID[c_IntVal].get_declaration() == true)
            throw err("double declaration");

        TID[var_num].set_declaration();
        TID[var_num].set_type(types_lex::LEX_STRING);

        get_lex();
        if (c_type == types_lex::LEX_ASSIGN) {
            prog.put_lex(Lex(types_lex::POLIZ_ADDRESS, var_num));
            get_lex();
            if (c_type == types_lex::LEX_STRING_VAL) {
                TID[var_num].set_assign();
                prog.put_lex(Lex(types_lex::LEX_STRING_VAL, c_StrVal));
                prog.put_lex(Lex(types_lex::LEX_ASSIGN, 0));
                prog.put_lex(Lex(types_lex::LEX_SEMICOLON, 0));
            } else
                throw err("string literal expected");
            get_lex();
        }
        if (c_type == types_lex::LEX_COMMA)
            get_lex();
    }
    check_semicolon();
}

void Parser::I() {
    if (c_type != types_lex::LEX_ID)
        throw err("identifier expected in declaration");

    int var_num;
    while (c_type != types_lex::LEX_SEMICOLON) {
        if (c_type != types_lex::LEX_ID)
            throw err("identifier expected in declaration");

        var_num = c_IntVal;
        if (TID[c_IntVal].get_declaration() == true)
            throw err("double declaration");

        TID[var_num].set_declaration();
        TID[var_num].set_type(types_lex::LEX_INT);

        get_lex();
        if (c_type == types_lex::LEX_ASSIGN) {
            prog.put_lex(Lex(types_lex::POLIZ_ADDRESS, var_num));
            get_lex();
            int sign = 1;
            if (c_type == types_lex::LEX_PLUS) {
                get_lex();
            } else if (c_type == types_lex::LEX_MINUS) {
                sign = -1;
                get_lex();
            }
            if (c_type == types_lex::LEX_NUM) {
                TID[var_num].set_assign();
                prog.put_lex(Lex(types_lex::LEX_NUM, sign * c_IntVal));
                prog.put_lex(Lex(types_lex::LEX_ASSIGN, 0));
                prog.put_lex(Lex(types_lex::LEX_SEMICOLON, 0));
            } else
                throw err("number expected");
            get_lex();
        }
        if (c_type == types_lex::LEX_COMMA)
            get_lex();
    }
    check_semicolon();
}

void Parser::DE() {
    int count = 0;
    E();
    while (prog.get_c_type() == types_lex::POLIZ_ADDRESS) {
        count++;
        get_lex();
        if (c_type == types_lex::LEX_ID) {
            check_id();
            int i = c_IntVal;
            get_lex();
            if (c_type == types_lex::LEX_ASSIGN)
                prog.put_lex(Lex(types_lex::POLIZ_ADDRESS, i));
            else
                prog.put_lex(Lex(types_lex::LEX_ID, i));
        }
        if (c_type == types_lex::LEX_NUM) {
            st_lex.push(types_lex::LEX_INT);
            prog.put_lex(curr_lex);
            get_lex();
        } else if (c_type == types_lex::LEX_STRING_VAL) {
            st_lex.push(types_lex::LEX_STRING);
            prog.put_lex(curr_lex);
            get_lex();
        }
    }
    for (int i = 0; i < count; i++)
        prog.put_lex(Lex(types_lex::LEX_ASSIGN, 0));
}

void Parser::PE() {
    E();
    if (c_type == types_lex::LEX_AND || c_type == types_lex::LEX_OR) {
        st_lex.push(c_type);
        get_lex();
        PE();
        check_op();
    }
}

void Parser::E() {
    E1();
    if (c_type == types_lex::LEX_EQ || c_type == types_lex::LEX_LSS || c_type == types_lex::LEX_GRT ||
        c_type == types_lex::LEX_LOE || c_type == types_lex::LEX_GOE || c_type == types_lex::LEX_NEQ) {
        st_lex.push(c_type);
        get_lex();
        E1();
        check_op();
    }
}

void Parser::E1() {
    T();
    while (c_type == types_lex::LEX_PLUS || c_type == types_lex::LEX_MINUS) {
        st_lex.push(c_type);
        get_lex();
        T();
        check_op();
    }
}

void Parser::T() {
    F();
    while (c_type == types_lex::LEX_STAR || c_type == types_lex::LEX_SLASH) {
        st_lex.push(c_type);
        get_lex();
        F();
        check_op();
    }
}

void Parser::F() {
    if (c_type == types_lex::LEX_ID) {
        check_id();
        int i = c_IntVal;
        tmp = TID[i].get_type();
        get_lex();
        if (c_type == types_lex::LEX_ASSIGN)
            prog.put_lex(Lex(types_lex::POLIZ_ADDRESS, i));
        else
            prog.put_lex(Lex(types_lex::LEX_ID, i));
    } else if (c_type == types_lex::LEX_NUM) {
        tmp = types_lex::LEX_INT;
        st_lex.push(types_lex::LEX_INT);
        prog.put_lex(curr_lex);
        get_lex();
    } else if (c_type == types_lex::LEX_STRING_VAL) {
        tmp = types_lex::LEX_STRING;
        st_lex.push(types_lex::LEX_STRING);
        prog.put_lex(curr_lex);
        get_lex();
    } else if (c_type == types_lex::LEX_TRUE) {
        st_lex.push(types_lex::LEX_BOOL);
        prog.put_lex(Lex(types_lex::LEX_TRUE, true));
        get_lex();
    } else if (c_type == types_lex::LEX_FALSE) {
        st_lex.push(types_lex::LEX_BOOL);
        prog.put_lex(Lex(types_lex::LEX_FALSE, false));
        get_lex();
    } else if (c_type == types_lex::LEX_NOT) {
        get_lex();
        F();
        check_not();
    } else if (c_type == types_lex::LEX_LPAREN) {
        get_lex();
        E();
        if (c_type == types_lex::LEX_RPAREN)
            get_lex();
        else
            throw err("')' expected");
    } else
        throw err("operand expected in expression");
}

/* ------------------------- Executer ------------------------- */
class Executer {
    Lex pc_el;
    ValType flag = ValType::NONE;

    TableIdent &TID;
    FTable &FN;

public:
    Executer(TableIdent &tid, FTable &fn) : TID(tid), FN(fn) {}

    ValType get_flag() const { return flag; }

    void rezero_flag() { flag = ValType::NONE; }

    void set_flag_int() { flag = ValType::INT; }

    void set_flag_string() { flag = ValType::STR; }

    bool flag_is_int() const { return flag == ValType::INT; }

    bool flag_is_string() const { return flag == ValType::STR; }

    void execute(Poliz &prog);
};

void Executer::execute(Poliz &prog) {
    Stack<int, 100> st_var;
    Stack<int, 100> st_move;
    Stack<bool, 100> st_bool;
    Stack<int, 100> st_num;
    Stack<int, 100> st_wr_num;
    Stack<string, 100> st_str;
    Stack<string, 100> st_wr_str;
    int i, j, index = 0, f_index = 100, size = prog.get_free();
    string str;
    bool t;
    int k1, k2, k;
    while (index < size) {
        pc_el = prog[index];
        switch (pc_el.get_type()) {
            case types_lex::LEX_TRUE:
            case types_lex::LEX_FALSE:
                st_bool.push(pc_el.get_bool());
                break;
            case types_lex::LEX_NUM:
                set_flag_int();
                st_num.push(pc_el.get_val());
                break;
            case types_lex::LEX_STRING_VAL:
                set_flag_string();
                st_str.push(pc_el.get_str());
                break;
            case types_lex::POLIZ_LABEL:
                st_move.push(pc_el.get_val());
                break;
            case types_lex::POLIZ_ADDRESS:
                st_var.push(pc_el.get_val());
                break;
            case types_lex::LEX_ID:
                i = pc_el.get_val();
                if (TID[i].get_assign()) {
                    if (TID[i].get_type() == types_lex::LEX_STRING) {
                        set_flag_string();
                        st_str.push(TID[i].get_str());
                    } else {
                        set_flag_int();
                        st_num.push(TID[i].get_val());
                    }
                    break;
                } else
                    throw InterpretError("runtime: identifier used before assignment");
            case types_lex::LEX_NOT:
                st_bool.push(!st_bool.pop());
                break;
            case types_lex::LEX_OR:
                t = st_bool.pop();
                st_bool.push(st_bool.pop() || t);
                break;
            case types_lex::LEX_AND:
                t = st_bool.pop();
                st_bool.push(st_bool.pop() && t);
                break;
            case types_lex::POLIZ_GO:
                index = st_move.pop() - 1;
                break;
            case types_lex::POLIZ_FGO:
                i = st_move.pop() - 1;
                if (!st_bool.pop())
                    index = i;
                break;
            case types_lex::LEX_WRITE:
                for (int n = 0; n < FN.get_top(); n++)
                    if (FN[n].get_poliz_num() == index)
                        f_index = n;
                k1 = FN[f_index].get_num_int();
                k2 = FN[f_index].get_num_str();

                for (int n = 0; n < k1; n++)
                    st_wr_num.push(st_num.pop());

                for (int n = 0; n < k2; n++)
                    st_wr_str.push(st_str.pop());

                for (int n = 0; n < k1 + k2; n++) {
                    if (FN[f_index].get_el(n) == types_lex::LEX_INT)
                        cout << st_wr_num.pop() << endl;
                    else
                        cout << st_wr_str.pop() << endl;
                }
                break;
            case types_lex::LEX_READ:
                for (int n = 0; n < FN.get_top(); n++)
                    if (FN[n].get_poliz_num() == index)
                        f_index = n;
                k1 = FN[f_index].get_num_int();
                k2 = FN[f_index].get_num_str();

                for (int n = 0; n < k1 + k2; n++)
                    st_wr_num.push(st_var.pop());

                for (int n = 0; n < k1 + k2; n++) {
                    j = st_wr_num.pop();
                    if (FN[f_index].get_el(n) == types_lex::LEX_INT) {
                        cout << "Input int value for ";
                        cout << TID[j].get_name() << endl;
                        cin >> k;
                        TID[j].set_val(k);
                    } else {
                        cout << "Input string value for ";
                        cout << TID[j].get_name() << endl;
                        cin >> str;
                        TID[j].set_str(str);
                    }
                    TID[j].set_assign();
                }
                break;
            case types_lex::LEX_PLUS:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_str.push(st_str.pop() + str);
                } else {
                    i = st_num.pop();
                    st_num.push(st_num.pop() + i);
                }
                break;
            case types_lex::LEX_STAR:
                i = st_num.pop();
                st_num.push(st_num.pop() * i);
                break;
            case types_lex::LEX_MINUS:
                i = st_num.pop();
                st_num.push(st_num.pop() - i);
                break;
            case types_lex::LEX_SLASH:
                i = st_num.pop();
                if (i)
                    st_num.push(st_num.pop() / i);
                else
                    throw InterpretError("runtime: division by zero");
                break;
            case types_lex::LEX_EQ:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() == str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() == i);
                }
                break;
            case types_lex::LEX_LSS:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() < str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() < i);
                }
                break;
            case types_lex::LEX_GRT:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() > str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() > i);
                }
                break;
            case types_lex::LEX_LOE:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() <= str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() <= i);
                }
                break;
            case types_lex::LEX_GOE:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() >= str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() >= i);
                }
                break;
            case types_lex::LEX_NEQ:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() != str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() != i);
                }
                break;
            case types_lex::LEX_ASSIGN:
                j = st_var.pop();
                if (flag_is_int()) {
                    i = st_num.pop();
                    st_num.push(i);
                    TID[j].set_val(i);
                } else {
                    str = st_str.pop();
                    st_str.push(str);
                    TID[j].set_str(str);
                }
                TID[j].set_assign();
                break;
            case types_lex::LEX_SEMICOLON:
                if (flag_is_int())
                    st_num.pop();
                else
                    st_str.pop();
                rezero_flag();
                break;
            default:
                throw InterpretError("runtime: unexpected poliz element");
        }
        ++index;
    }
    cout << "Finish of executing." << endl;
}

/* ------------------------- Interpretator ------------------------- */
class Interpretator {
    TableIdent TID;
    FTable FN;
    Parser pars;
    Executer E;

public:
    Interpretator(const string &program)
            : pars(program, TID, FN), E(TID, FN) {}

    void interpretation() {
        pars.analyze();
        E.execute(pars.prog);
    }
};

int main(int argc, char **argv) {
    try {
        if (argc >= 2) {
            Interpretator I(argv[1]);
            I.interpretation();
        } else {
            cout << "Usage: " << argv[0] << " <program-file>\n";
            return 1;
        }
        return 0;
    }
    catch (const exception &e) {
        cout << "Error: " << e.what() << endl;
        return 1;
    }
    catch (...) {
        cout << "Error: unknown failure" << endl;
        return 1;
    }
}
