#include <iostream>
#include<iomanip>
#include <fstream>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

#include <vector>
#include <string>

#define SIZE_ID 128
#define SIZE_BUF 128
#define SIZE_FN 100

using namespace std;

enum types_lex {
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

const char *TW[] = {"", "program", "int", "string", "read", "write", "if", "else",
                    "do", "while", "true", "false", "or", "and", NULL};
const types_lex wtypes[] = {
        LEX_NULL,
        LEX_PROGRAM,
        LEX_INT, LEX_STRING,
        LEX_READ, LEX_WRITE,
        LEX_IF, LEX_ELSE,
        LEX_DO, LEX_WHILE,
        LEX_TRUE, LEX_FALSE,
        LEX_OR, LEX_AND,
        LEX_NULL
};
const char *TD[] = {"", "+", "-", "/", "*", ">", "<", ">=", "<=", "==",
                    "!=", "=", ",", ";", "(", ")", "{", "}", "\"", "!", NULL};
static types_lex dtypes[] = {
        LEX_NULL,
        LEX_PLUS, LEX_MINUS, LEX_SLASH, LEX_STAR,
        LEX_GRT, LEX_LSS, LEX_GOE, LEX_LOE, LEX_EQ, LEX_NEQ,
        LEX_ASSIGN,
        LEX_COMMA, LEX_SEMICOLON, LEX_LPAREN, LEX_RPAREN, LEX_STBLOCK, LEX_FNBLOCK, LEX_QOTE,
        LEX_NOT,
        LEX_NULL
};

/* ------------------------- Lex ------------------------- */
class Lex {
    types_lex type_;
    int val_;
    string str_;
    bool bval_;
public:
    Lex(types_lex new_t = LEX_NULL, int new_val = 0) : type_(new_t), val_(new_val) {}

    Lex(types_lex new_t, bool new_b) : type_(new_t), bval_(new_b) {}

    Lex(types_lex new_t, string new_str) : type_(new_t), str_(new_str) {}

    types_lex get_type() const { return type_; }

    int get_val() const { return val_; }

    string get_str() const { return str_; }

    bool get_bool() const { return bval_; }

    friend ostream &operator<<(ostream &os, const Lex out_lex) {
        return os << "numb of type = " << out_lex.get_type() << ", value = " << out_lex.get_val() << endl;
    }
};

/* ------------------------- Ident ------------------------- */
class Ident {
    char *name_;
    types_lex type_;
    int val_;
    string str_;
    bool declaration_;
    bool assign_;
public:
    Ident() : name_(nullptr), declaration_(false), assign_(false) {}

    ~Ident() {
        if (name_ != nullptr) delete[] name_;
    }

    char *get_name() const { return name_; }

    void set_name(const char *new_name) {
        name_ = new char[strlen(new_name) + 1];
        strcpy(name_, new_name);
    }

    string get_str() const { return str_; }

    void set_str(const string new_str) { str_ = new_str; }

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
    Ident *table_ = nullptr;
    int size_;
    int top_;
public:
    TableIdent(int const new_size) : size_(new_size) {
        table_ = new Ident[new_size];
        top_ = 1;
    }

    ~TableIdent() {
        if (table_ != nullptr) delete[] table_;
    }

    void set_table(int const new_size) {
        if (table_ == nullptr) {
            table_ = new Ident[new_size];
            top_ = 1;
        }
    }

    Ident &operator[](const int k) { return table_[k]; }

    int get_size() const { return size_; }

    int put(const char *buf) {
        for (int i = 1; i < top_; i++) {
            if (!strcmp(buf, table_[i].get_name()))
                return i;
        }
        table_[top_].set_name(buf);
        top_++;
        return top_ - 1;
    }
} TID(SIZE_ID);

/* ------------------------- Lexer ------------------------- */
class Lexer {
    FILE *fd;
    char c;
    char buf[SIZE_BUF];
    int buf_top;
    bool not_eof = true;

    enum state {
        H, IDENT, NUMB, COM, COM2, QOUT, COMPR, NOT, DELIM, FIN_OF_FILE, SLASH
    } G;

    void clear() {
        buf_top = 0;
        for (int i = 0; i < SIZE_BUF; i++) buf[i] = 0;
    }

    void add() { buf[buf_top++] = c; }

    void get_symb() { c = fgetc(fd); }

    int define_type(const char *buf, const char **table) { // table = TW | TD
        int i = 0;
        while (table[i]) {
            if (!strcmp(buf, table[i])) return i;
            ++i;
        }
        return 0;
    }

public:
    Lexer(
            const char *path) {
        if ((fd = fopen(path, "r")) == NULL) {
            cout << "Error: file didn't open" << endl;
            exit(1);
        }
        clear();
        get_symb();
    }

    ~Lexer() { fclose(fd); }

    bool is_eof() const { return not_eof; }

    Lex get_lex();
};

Lex Lexer::get_lex() {
    int digit;
    int num;
    string str;

    G = H;
    while (true) {
        switch (G) {
            case H:
                if (c == '\n' || c == '\r' || c == ' ' || c == '\t')
                    get_symb();
                else if (isalpha(c)) {
                    clear();
                    add();
                    get_symb();
                    G = IDENT;
                } else if (isdigit(c)) {
                    digit = c - '0';
                    get_symb();
                    G = NUMB;
                } else if (c == '/') {
                    clear();
                    add();
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
                    clear();
                    add();
                    get_symb();
                    G = COMPR;
                } else if (c == '!') {
                    clear();
                    add();
                    get_symb();
                    G = NOT;
                } else if (c != EOF) {
                    clear();
                    add();
                    G = DELIM;
                } else if (c == '#') {
                    G = COM2;
                } else
                    G = FIN_OF_FILE;
                break;

            case IDENT:
                if (isalpha(c) || isdigit(c)) {
                    add();
                    get_symb();
                } else if ((num = define_type(buf, TW)))
                    return Lex(wtypes[num], num);
                else // variable
                {
                    num = TID.put(buf); // new or old
                    return Lex(LEX_ID, num);
                }
                break;

            case NUMB:
                if (isdigit(c)) {
                    digit = digit * 10 + c - '0';
                    get_symb();
                } else
                    return Lex(LEX_NUM, digit);
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
                else if (c == EOF)
                    G = FIN_OF_FILE;
                break;
            case COM2:
                if (c == '\n')
                    G = H;
                else if (cin.eof()){
                    G = H;
                    break;
                }
                break;
            case SLASH:
                num = define_type(buf, TD);
                return Lex(dtypes[num], num);

            case QOUT:
                if (c == '"') {
                    get_symb();
                    return Lex(LEX_STRING_VAL, str);
                } else if (c != EOF) {
                    str.push_back(c);
                    get_symb();
                } else if (c == EOF)
                    G = FIN_OF_FILE;
                break;

            case COMPR:
                if (c == '=') {
                    add();
                    get_symb();
                }
                num = define_type(buf, TD);
                return Lex(dtypes[num], num);
                return Lex(dtypes[num], num);

            case NOT:
                if (c == '=') {
                    add();
                    get_symb();
                }
                num = define_type(buf, TD);
                return Lex(dtypes[num], num);

            case DELIM:
                if ((num = define_type(buf, TD))) {
                    get_symb();
                    return Lex(dtypes[num], num);
                } else if (c != '!')
                    throw c;

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
            throw "Stack is full";
    }

    T pop() {
        if (!is_empty())
            return s[--top];
        else
            throw "Stack is empty";
    }

    void reset() { top = 0; }

    bool is_empty() const { return top == 0; }

    bool is_full() const { return top == max_size - 1; }
};

/* ------------------------- Function ------------------------- */
class Function {
    char *name = nullptr;
    int poliz_num;
    int num_str = 0;
    int num_int = 0;
    types_lex array[30];
    int index = 0;

public:
    Function() {}

    char *get_name() const { return name; }

    void set_name(const char *n) {
        name = new char[strlen(n) + 1];
        strcpy(name, n);
    }

    void add_int() { num_int++; }

    void add_str() { num_str++; }

    void push(types_lex l) { array[index++] = l; }

    types_lex get_el(int i) const { return array[i]; }

    void set_num_str(int i) { num_str = i; }

    int get_num_str() const { return num_str; }

    void set_num_int(int i) { num_int = i; }

    int get_num_int() const { return num_int; }

    void set_poliz_num(int i) { poliz_num = i; }

    int get_poliz_num() const { return poliz_num; }

    void add_var(types_lex l) {
        if (l == LEX_INT)
            add_int();
        else if (l == LEX_STRING)
            add_str();
        else
            throw l;
        push(l);
    }
};

class FTable {
    Function *table = nullptr;
    int top;
    int size;

public:
    FTable(int const size) : size(size) {
        table = new Function[size];
        top = 1;
    }

    ~FTable() {
        if (table != nullptr)
            delete[] table;
    }

    int put(const char *buf) {
        table[top].set_name(buf);
        top++;
        return top - 1;
    }

    Function &operator[](int i) { return table[i]; }

    bool check_fn(const char *buf) const {
        for (int i = 1; i < top; i++)
            if (!strcmp(buf, table[i].get_name()))
                return true;
        return false;
    }

    int get_top() const { return top; }
} FN(SIZE_FN);

/* ------------------------- Poliz ------------------------- */
class Poliz {
    Lex *p = nullptr;
    int size; // фактический размер
    int free; // текущая позиция куда можно закинуть лексему

public:
    Poliz(int max_size) {
        p = new Lex[max_size];
        size = max_size;
        free = 0;
    }

    ~Poliz() { delete[] p; }

    void put_lex(Lex l) {
        p[free] = l;
        free++;
    }

    types_lex get_c_type() { return p[free - 1].get_type(); }

    void put_lex(Lex l, int place) { p[place] = l; } // для цикла for

    void blank() { free++; }

    int get_free() const { return free; }

    Lex &operator[](int ident) {
        if (ident > size)
            throw "Poliz: out of array";
        if (ident > free)
            throw "Poliz: indefine element of array";
        return p[ident];
    }

    void print() {
        for (int i = 0; i < free; i++) cout << p[i];
    }
};

/* ------------------------- Parser ------------------------- */
class Parser {
    Lex curr_lex;
    types_lex c_type;
    int c_IntVal;
    string c_StrVal;
    int IntVal;
    string StrVal;

    Stack<int, 100> st_int;
    Stack<string, 100> st_str;
    Stack<types_lex, 100> st_lex;

    types_lex tmp;

    Lexer scan;

    void check_id() {
        if (TID[c_IntVal].get_declaration())
            st_lex.push(TID[c_IntVal].get_type());
        else
            throw "not declarated";
    }

    void check_semicolon() {
        if (c_type != LEX_SEMICOLON)
            throw curr_lex;
        get_lex();
    }

    void get_lex() {
        curr_lex = scan.get_lex();
        c_type = curr_lex.get_type();
        c_IntVal = curr_lex.get_val();
        c_StrVal = curr_lex.get_str();
    }

    void check_not() {
        if (st_lex.pop() != LEX_BOOL)
            throw "wrong type is in not";
        else {
            st_lex.push(LEX_BOOL);
            prog.put_lex(Lex(LEX_NOT, 0));
        }
    }

    void check_op() {
        types_lex t1, t2, op, r, t;
        t2 = st_lex.pop();
        op = st_lex.pop();
        t1 = st_lex.pop();
        t = t1;
        if (t1 == t2) {
            if (t == LEX_BOOL) {
                if (op == LEX_AND || op == LEX_OR)
                    r = LEX_BOOL;
                else
                    throw op;
            } else if (t == LEX_INT) {
                if (op == LEX_EQ || op == LEX_NEQ || op == LEX_GRT || op == LEX_LSS || op == LEX_GOE || op == LEX_LOE)
                    r = LEX_BOOL;
                else if (op == LEX_MINUS || op == LEX_PLUS || op == LEX_SLASH || op == LEX_STAR)
                    r = LEX_INT;
                else
                    throw op;
            } else if (t == LEX_STRING) {
                if (op == LEX_EQ || op == LEX_NEQ || op == LEX_GRT || op == LEX_LSS || op == LEX_GOE || op == LEX_LOE)
                    r = LEX_BOOL;
                else if (op == LEX_PLUS)
                    r = LEX_STRING;
                else
                    throw op;
            } else
                throw t;
        } else
            throw t2;
        st_lex.push(r);
        prog.put_lex(Lex(op, 0));
    }

    void eq_bool() {
        if (st_lex.pop() != LEX_BOOL)
            throw "expression is not boolean";
    }

    void eq_type() {
        if (st_lex.pop() != st_lex.pop())
            throw "wrong types are in assignment";
    }

    void check_Lparen() {
        if (c_type != LEX_LPAREN)
            throw curr_lex;
        get_lex();
    }

    void check_Rparen() {
        if (c_type != LEX_RPAREN)
            throw curr_lex;
        get_lex();
    }

    void check_decl() {
        if (!TID[c_IntVal].get_declaration())
            throw curr_lex;
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

    Parser(char *program) : scan(program), prog(1000) {}

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
    if (c_type != LEX_PROGRAM)
        throw curr_lex;
    get_lex();
    B();
}

void Parser::B() {
    if (c_type == LEX_STBLOCK) {
        get_lex();
        while (c_type != LEX_FNBLOCK) {
            S();
        }
        get_lex();
    } else
        throw curr_lex;
}

void Parser::S() {
    int pl0, pl1, pl2, pl3;

    if (c_type == LEX_STRING) {
        get_lex();
        STR();
    } else if (c_type == LEX_INT) {
        get_lex();
        I();
    } else if (c_type == LEX_IF) {
        get_lex();
        check_Lparen();
        PE();
        eq_bool();
        check_Rparen();
        pl2 = prog.get_free();
        prog.blank();
        prog.put_lex(Lex(POLIZ_FGO, 0));

        if (c_type == LEX_STBLOCK)
            B();
        else
            S();

        if (c_type == LEX_ELSE) {
            pl3 = prog.get_free();
            prog.blank();
            prog.put_lex(Lex(POLIZ_GO, 0));
            prog.put_lex(Lex(POLIZ_LABEL, prog.get_free()), pl2);
            get_lex();
            if (c_type == LEX_STBLOCK) {
                B();
                prog.put_lex(Lex(POLIZ_LABEL, prog.get_free()), pl3);
            } else {
                S();
                prog.put_lex(Lex(POLIZ_LABEL, prog.get_free()), pl3);
            }
        } else
            prog.put_lex(Lex(POLIZ_LABEL, prog.get_free()), pl2);
    } else if (c_type == LEX_WHILE) {
        pl0 = prog.get_free();
        get_lex();
        check_Lparen();
        PE();
        eq_bool();
        check_Rparen();
        pl1 = prog.get_free();
        prog.blank();
        prog.put_lex(Lex(POLIZ_FGO, 0));

        if (c_type == LEX_STBLOCK)
            B();
        else
            S();

        prog.put_lex(Lex(POLIZ_LABEL, pl0));
        prog.put_lex(Lex(POLIZ_GO, 0));
        prog.put_lex(Lex(POLIZ_LABEL, prog.get_free()), pl1);
    } else if (c_type == LEX_READ) {
        int i;
        i = FN.put("read");
        get_lex();
        check_Lparen();
        while (c_type == LEX_ID) {
            if (c_type != LEX_ID)
                throw curr_lex;
            check_decl();
            FN[i].add_var(TID[c_IntVal].get_type());
            prog.put_lex(Lex(POLIZ_ADDRESS, c_IntVal));
            get_lex();
            if (c_type == LEX_COMMA)
                get_lex();
            else break;
        }
        check_Rparen();
        check_semicolon();
        FN[i].set_poliz_num(prog.get_free());
        prog.put_lex(Lex(LEX_READ, 0));
    } else if (c_type == LEX_WRITE) {
        int i;
        i = FN.put("write");
        get_lex();
        check_Lparen();
        E();
        FN[i].add_var(tmp);
        while (c_type == LEX_COMMA) {
            get_lex();
            E();
            FN[i].add_var(tmp);
        }
        check_Rparen();
        check_semicolon();
        FN[i].set_poliz_num(prog.get_free());
        prog.put_lex(Lex(LEX_WRITE, 0));
    } else if (c_type == LEX_ID) {
        check_id();
        prog.put_lex(Lex(POLIZ_ADDRESS, c_IntVal));
        get_lex();
        if (c_type == LEX_ASSIGN) {
            get_lex();
            DE();
            eq_type();
            prog.put_lex(Lex(LEX_ASSIGN, 0));
        } else
            throw curr_lex;
        check_semicolon();
        prog.put_lex(Lex(LEX_SEMICOLON, 0));
    } else
        B();
}

void Parser::STR() {
    if (c_type != LEX_ID)
        throw curr_lex;

    int var_num;
    while (c_type != LEX_SEMICOLON) {
        if (c_type != LEX_ID)
            throw curr_lex;

        var_num = c_IntVal;
        if (TID[c_IntVal].get_declaration() == true)
            throw "double declaration";

        TID[var_num].set_declaration();
        TID[var_num].set_type(LEX_STRING);

        get_lex(); // comma || assign
        if (c_type == LEX_ASSIGN) {
            prog.put_lex(Lex(POLIZ_ADDRESS, var_num));
            get_lex();
            if (c_type == LEX_STRING_VAL) {
                TID[var_num].set_assign();
                prog.put_lex(Lex(LEX_STRING_VAL, c_StrVal));
                prog.put_lex(Lex(LEX_ASSIGN, 0));
                prog.put_lex(Lex(LEX_SEMICOLON, 0));
            } else
                throw curr_lex;
            get_lex(); // comma || semicolon
        }
        if (c_type == LEX_COMMA)
            get_lex();
    }
    check_semicolon();
}

void Parser::I() {
    if (c_type != LEX_ID)
        throw curr_lex;

    int var_num;
    while (c_type != LEX_SEMICOLON) {
        if (c_type != LEX_ID)
            throw curr_lex;

        var_num = c_IntVal;
        if (TID[c_IntVal].get_declaration() == true)
            throw "double declaration";

        TID[var_num].set_declaration();
        TID[var_num].set_type(LEX_INT);

        get_lex(); // comma || assign
        if (c_type == LEX_ASSIGN) {
            prog.put_lex(Lex(POLIZ_ADDRESS, var_num));
            get_lex();
            if (c_type == LEX_NUM) {
                TID[var_num].set_assign();
                prog.put_lex(Lex(LEX_NUM, c_IntVal));
                prog.put_lex(Lex(LEX_ASSIGN, 0));
                prog.put_lex(Lex(LEX_SEMICOLON, 0));
            } else
                throw curr_lex;
            get_lex(); // comma || semicolon
        }
        if (c_type == LEX_COMMA)
            get_lex();
    }
    check_semicolon();
}

void Parser::DE() {
    int count = 0;
    E();
    while (prog.get_c_type() == POLIZ_ADDRESS) {
        count++;
        get_lex();
        if (c_type == LEX_ID) {
            check_id();
            int i = c_IntVal;
            get_lex();
            if (c_type == LEX_ASSIGN)
                prog.put_lex(Lex(POLIZ_ADDRESS, i));
            else
                prog.put_lex(Lex(LEX_ID, i));
        }
        if (c_type == LEX_NUM) {
            st_lex.push(LEX_INT);
            prog.put_lex(curr_lex);
            get_lex();
        } else if (c_type == LEX_STRING_VAL) {
            st_lex.push(LEX_STRING);
            prog.put_lex(curr_lex);
            get_lex();
        }
    }
    for (int i = 0; i < count; i++)
        prog.put_lex(Lex(LEX_ASSIGN, 0));
}

void Parser::PE() {
    E();
    if (c_type == LEX_AND || c_type == LEX_OR) {
        st_lex.push(c_type);
        get_lex();
        PE();
        check_op();
    }
}

void Parser::E() {
    E1();
    if (c_type == LEX_EQ || c_type == LEX_LSS || c_type == LEX_GRT || c_type == LEX_LOE || c_type == LEX_GOE ||
        c_type == LEX_NEQ) {
        st_lex.push(c_type);
        get_lex();
        E1();
        check_op();
    }
}

void Parser::E1() {
    T();
    while (c_type == LEX_PLUS || c_type == LEX_MINUS) {
        st_lex.push(c_type);
        get_lex();
        T();
        check_op();
    }
}

void Parser::T() {
    F();
    while (c_type == LEX_STAR || c_type == LEX_SLASH) {
        st_lex.push(c_type);
        get_lex();
        F();
        check_op();
    }
}

void Parser::F() {
    if (c_type == LEX_ID) {
        check_id();
        int i = c_IntVal;
        tmp = TID[i].get_type();
        get_lex();
        if (c_type == LEX_ASSIGN)
            prog.put_lex(Lex(POLIZ_ADDRESS, i));
        else
            prog.put_lex(Lex(LEX_ID, i));
    } else if (c_type == LEX_NUM) {
        tmp = LEX_INT;
        st_lex.push(LEX_INT);
        prog.put_lex(curr_lex);
        get_lex();
    } else if (c_type == LEX_STRING_VAL) {
        tmp = LEX_STRING;
        st_lex.push(LEX_STRING);
        prog.put_lex(curr_lex);
        get_lex();
    } else if (c_type == LEX_TRUE) {
        st_lex.push(LEX_BOOL);
        prog.put_lex(Lex(LEX_TRUE, true));
        get_lex();
    } else if (c_type == LEX_FALSE) {
        st_lex.push(LEX_BOOL);
        prog.put_lex(Lex(LEX_FALSE, false));
        get_lex();
    } else if (c_type == LEX_NOT) {
        get_lex();
        F();
        check_not();
    } else if (c_type == LEX_LPAREN) {
        get_lex();
        E();
        if (c_type == LEX_RPAREN)
            get_lex();
        else
            throw curr_lex;
    } else
        throw curr_lex;
}

/* ------------------------- Executer ------------------------- */
class Executer {
    Lex pc_el;
    int flag = 0;

public:
    int get_flag() const { return flag; }

    void rezero_flag() { flag = 0; }

    void set_flag_int() { flag = 100; }

    void set_flag_string() { flag = 1000; }

    bool flag_is_int() const { return flag == 100; }

    bool flag_is_string() const { return flag == 1000; }

    void execute(Poliz &prog);
};

void Executer::execute(Poliz &prog) {
    Stack<int, 100> st_var;         //переменные
    Stack<int, 100> st_move;        //переход
    Stack<bool, 100> st_bool;       //bool
    Stack<int, 100> st_num;         //числа
    Stack<int, 100> st_wr_num;
    Stack<string, 100> st_str;      //строки
    Stack<string, 100> st_wr_str;
    int i, j, index = 0, f_index = 100, size = prog.get_free();
    string str;
    bool t;
    int k1, k2, k;
    while (index < size) {
        pc_el = prog[index];
        switch (pc_el.get_type()) {
            case LEX_TRUE:
            case LEX_FALSE:
                st_bool.push(pc_el.get_bool());
                break;
            case LEX_NUM:
                set_flag_int();
                st_num.push(pc_el.get_val());
                break;
            case LEX_STRING_VAL:
                set_flag_string();
                st_str.push(pc_el.get_str());
                break;
            case POLIZ_LABEL:
                st_move.push(pc_el.get_val());
                break;
            case POLIZ_ADDRESS:
                st_var.push(pc_el.get_val());
                break;
            case LEX_ID:
                i = pc_el.get_val();
                if (TID[i].get_assign()) {
                    if (TID[i].get_type() == LEX_STRING) {
                        set_flag_string();
                        st_str.push(TID[i].get_name());
                    } else {
                        set_flag_int();
                        st_num.push(TID[i].get_val());
                    }
                    break;
                } else
                    throw "POLIZ: indefinite identifier";
            case LEX_NOT:
                st_bool.push(!st_bool.pop());
                break;
            case LEX_OR:
                t = st_bool.pop();
                st_bool.push(st_bool.pop() || t);
                break;
            case LEX_AND:
                t = st_bool.pop();
                st_bool.push(st_bool.pop() && t);
                break;
            case POLIZ_GO:
                index = st_move.pop() - 1;
                break;
            case POLIZ_FGO:
                i = st_move.pop() - 1;//т. к. потом index++;
                if (!st_bool.pop())
                    index = i;
                break;
            case LEX_WRITE:
                for (int i = 0; i < FN.get_top(); i++)
                    if (FN[i].get_poliz_num() == index)
                        f_index = i;
                k1 = FN[f_index].get_num_int();
                k2 = FN[f_index].get_num_str();

                for (int i = 0; i < k1; i++)
                    st_wr_num.push(st_num.pop());

                for (int i = 0; i < k2; i++)
                    st_wr_str.push(st_str.pop());

                for (int i = 0; i < k1 + k2; i++) {
                    if (FN[f_index].get_el(i) == LEX_INT)
                        cout << st_wr_num.pop() << endl;
                    else
                        cout << st_wr_str.pop() << endl;
                }
                f_index++;
                break;
            case LEX_READ:
                for (int i = 0; i < FN.get_top(); i++)
                    if (FN[i].get_poliz_num() == index)
                        f_index = i;
                k1 = FN[f_index].get_num_int();
                k2 = FN[f_index].get_num_str();

                for (int i = 0; i < k1 + k2; i++)
                    st_wr_num.push(st_var.pop());

                for (int i = 0; i < k1 + k2; i++) {
                    j = st_wr_num.pop();
                    if (FN[f_index].get_el(i) == LEX_INT) {
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
                f_index++;
                break;
            case LEX_PLUS:
                if (flag_is_string())
                    st_str.push(st_str.pop() + st_str.pop());
                else
                    st_num.push(st_num.pop() + st_num.pop());
                break;
            case LEX_STAR:
                st_num.push(st_num.pop() * st_num.pop());
                break;
            case LEX_MINUS:
                i = st_num.pop();
                st_num.push(st_num.pop() - i);
                break;
            case LEX_SLASH:
                i = st_num.pop();
                if (!i)
                    st_num.push(st_num.pop() / i);
                else
                    throw "POLIZ:divide by zero";
                break;
            case LEX_EQ:
                if (flag_is_string())
                    st_bool.push(st_str.pop() == st_str.pop());
                else
                    st_bool.push(st_num.pop() == st_num.pop());
                break;
            case LEX_LSS:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() < str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() < i);
                }
                break;
            case LEX_GRT:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() > str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() > i);
                }
                break;
            case LEX_LOE:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() <= str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() <= i);
                }
                break;
            case LEX_GOE:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() >= str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() >= i);
                }
                break;
            case LEX_NEQ:
                if (flag_is_string()) {
                    str = st_str.pop();
                    st_bool.push(st_str.pop() != str);
                } else {
                    i = st_num.pop();
                    st_bool.push(st_num.pop() != i);
                }
                break;
            case LEX_ASSIGN:
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
            case LEX_SEMICOLON:
                if (flag_is_int())
                    st_num.pop();
                else
                    st_str.pop();
                rezero_flag();
                break;
            default:
                throw "POLIZ: unexpected elem";
        }
        ++index;
    }
    cout << "Finish of executing." << endl;
}

/* ------------------------- Interpretator ------------------------- */
class Interpretator {
    Parser pars;
    Executer E;

public:
    Interpretator(char *program) : pars(program) {};

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
        } else
            cout << "There is no program\n";
        return 0;
    }
    catch (types_lex t) {
        cout << "Error: lex type == " << t << endl;
    }
    catch (Lex l) {
        cout << "Error: lex type == " << l.get_type() << " " << l.get_val() << " " << l.get_str() << endl;
    }
    catch (const char *str) {
        cout << "Error: " << str << endl;
    }
    catch (...) {
        cout << "Error with logic" << endl;
    }
    return 0;
}