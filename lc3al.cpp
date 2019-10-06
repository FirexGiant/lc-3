#include <cassert>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>
#include "rks/list_pool.h"
#include "rks/endian.h"

using u16 = std::uint16_t;

static list_pool<u16, u16> pool;
using list_type = typename list_pool<u16, u16>::list_type;
std::vector<u16> object;
static const char* program_name = "lc3al";
static int error_count = 0;
static char line[512];
static int line_number = 0;
static const char* source_filename;
static std::ifstream source_file;

static char object_filename[FILENAME_MAX];
static char listing_filename[FILENAME_MAX];

enum token_kind {
    TOKEN_NONE,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_NAME,
    TOKEN_INTEGER,
    TOKEN_STRING,
    TOKEN_EOL,
};

struct token_t {
    token_kind kind;
    const char* f;
    const char* l;
    int base;
};

static
void error(int status, const char* format, ...)
{
    fprintf(stderr, "%s:%d: error: ", source_filename, line_number);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    ++error_count;
    if (status) {
        fprintf(stderr, "program terminated\n");
        std::exit(status);
    }
}

#define fatal_error(...) error(EXIT_FAILURE, __VA_ARGS__)

static inline
void warn(const char* message)
{
    fprintf(stderr, "%s:%d: warning: %s\n", source_filename, line_number, message);
}

static inline
bool iswordstart(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.';
}

static inline
bool isletter(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static token_t token;
static const char* line_cursor;

static
void next_token()
{
start:
    while (isspace(*line_cursor)) ++line_cursor;
    token.f = line_cursor;

    switch (*line_cursor) {
    case '\0':
        token.kind = TOKEN_EOL;
        break;
    case ';':
        do ++line_cursor; while (*line_cursor);
        token.kind = TOKEN_EOL;
        break;
    case ':':
        ++line_cursor;
        token.kind = TOKEN_COLON;
        break;
    case ',':
        ++line_cursor;
        token.kind = TOKEN_COMMA;
        break;
    case '$':
        ++line_cursor;
        token.kind = TOKEN_INTEGER;
        token.base = 16;
        if (*line_cursor == '-') ++line_cursor;
        if (!std::isxdigit(*line_cursor)) {
            token.kind = TOKEN_NONE;
            break;
        }
        do ++line_cursor; while (std::isxdigit(*line_cursor));
        break;
    case '#':
        ++line_cursor;
        token.kind = TOKEN_INTEGER;
        token.base = 10;
        if (*line_cursor == '-') ++line_cursor;
        if (!std::isdigit(*line_cursor)) {
            token.kind = TOKEN_NONE;
            break;
        }
        do ++line_cursor; while (std::isdigit(*line_cursor));
        break;
    case '"':
        token.kind = TOKEN_STRING;
        ++line_cursor;
        while (*line_cursor) {
            if (line_cursor[0] == '\\' && line_cursor[1] == '"')
                ++line_cursor;
            else if (line_cursor[0] == '"') break;
            ++line_cursor;
        }
        if (*line_cursor != '"')
            fatal_error("The string literal was not terminated");
        ++line_cursor;
        break;
    default:
        if (iswordstart(*line_cursor)) {
            token.kind = TOKEN_NAME;
            do ++line_cursor; while (isletter(*line_cursor));
        } else {
            if (std::isprint(*line_cursor))
                error(0, "stray '%c' in program", *line_cursor);
            else
                error(0, "stray 'x%x' in program", static_cast<int>(*line_cursor));
            ++line_cursor;
            goto start;
        }
        break;
    }
    token.l = line_cursor;
}

static inline
bool peek(token_kind x)
{
    return token.kind == x;
}

static inline
bool match(token_kind x)
{
    if (peek(x)) {
        next_token();
        return true;
    }
    return false;
}

static inline
token_t expect()
{
    token_t tmp = token;
    next_token();
    return tmp;
}

static inline
token_t expect(token_kind x)
{
    static const char* token_kind_names[] = {
        "nothing", "a label", "a name", "an integer", "a register", "a string", "the end of the line",
    };

    if (!peek(x)) {
        fatal_error("I was expecting %s but got '%.*s' instead",
              token_kind_names[x], static_cast<int>(token.l - token.f), token.f);
    }
    return expect();
}

static inline
bool peek_register()
{
    return peek(TOKEN_NAME) && (token.l - token.f == 2) &&
        (token.f[0] == 'r' || token.f[0] == 'R') &&
        (token.f[1] >= '0' && token.f[1] <= '7');
}

static inline
u16 expect_register()
{
    u16 reg;
    if (peek_register()) {
        reg = token.f[1] - '0';
    } else {
        reg = 0;
        error(0, "I was expecting a register but got '%.*s' instead",
              static_cast<int>(token.l - token.f), token.f);
    }
    next_token();
    return reg;
}

static inline
u16 location_counter()
{
    return object[0] + static_cast<u16>(object.size() - 1);
}

struct symbol_t {
    std::string name;
    int line_number;
    u16 location;

    symbol_t() = default;

    symbol_t(std::string name, int line_number, u16 location)
        : name(std::move(name)), line_number(line_number), location(location)
    { }
};

static std::vector<symbol_t> symbols;

static inline
symbol_t& get_symbol(const char* f, const char* l)
{
    auto iter = std::find_if(
        symbols.begin(), symbols.end(),
        [f, l](const symbol_t& symbol) {
            return std::equal(symbol.name.begin(), symbol.name.end(), f, l);
        });
    if (iter != symbols.end()) return *iter;
    return symbols.emplace_back(std::string(f, l), 0, 0);
}

static inline
int ordinal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

template <typename I, typename N>
std::pair<I, N> parse_integer_nonnegative(I f, I l, N x, int base)
{
    while (f != l) {
        N digit = ordinal(*f);
        if (x > (std::numeric_limits<N>::max() - digit) / base)
            break;
        x = x * base + digit;
        ++f;
    }
    return std::make_pair(f, x);
}

template <typename I, typename N>
std::pair<I, N> parse_integer_negative(I f, I l, N x, int base)
{
    while (f != l) {
        N digit = ordinal(*f);
        if (x < (std::numeric_limits<N>::min() + digit) / base)
            break;
        x = x * base - digit;
        ++f;
    }
    return std::make_pair(f, x);
}

template <typename I, typename N>
std::pair<I, N> parse_integer_unsigned(I f, I l, N x, int base)
{
    bool negate;
    if (*f == '-') {
        negate = true;
        ++f;
    } else negate = false;

    std::pair<I, N> p = parse_integer_nonnegative(f, l, x, base);
    if (negate) p.second = ~(p.second) + 1;
    return p;
}

template <typename I, typename N>
inline
std::pair<I, N> parse_integer_signed(I f, I l, N x, int base)
{
    if (*f == '-') return parse_integer_negative(++f, l, x, base);
    return parse_integer_nonnegative(f, l, x, base);
}

template <typename I, typename N>
inline
std::pair<I, N> parse_integer(I f, I l, N x, int base, std::true_type)
{
    return parse_integer_signed(f, l, x, base);
}

template <typename I, typename N>
inline
std::pair<I, N> parse_integer(I f, I l, N x, int base, std::false_type)
{
    return parse_integer_unsigned(f, l, x, base);
}

template <typename I, typename N>
inline
std::pair<I, N> parse_integer(I f, I l, N x, int base)
{
    return parse_integer(f, l, x, base, std::is_signed<N>());
}

enum {
    OP_ADD,
    OP_AND,
    OP_BRn, OP_BRz, OP_BRp, OP_BR, OP_BRzp, OP_BRnp, OP_BRnz, OP_BRnzp,
    OP_JMP, OP_RET,
    OP_JSR, OP_JSRR,
    OP_LD, OP_LDI, OP_LDR, OP_LEA,
    OP_NOT,
    OP_RTI,
    OP_ST, OP_STI, OP_STR,
    OP_TRAP,
    OP_GETC, OP_OUT, OP_PUTS, OP_IN, OP_PUTSP, OP_HALT,
    OP_ORIG, OP_END, OP_BLKW, OP_FILL, OP_STRINGZ,
};

struct opcode_t {
    std::string name;
    u16 base_code;
    void (*assemble)(const opcode_t*);
    void (*assemble_fn)(const opcode_t*);
};

static std::ofstream listing_file;

static
void print_listing(u16 x)
{
    static int last_line_number = 0;
    listing_file << std::setfill('0') << std::setw(4) << std::hex << location_counter() << ' ';
    listing_file << std::setfill('0') << std::setw(4) << std::hex << x;
    if (line_number != last_line_number) {
        listing_file << " (" << std::setw(4) << std::dec << line_number << ")\t" << line;
        last_line_number = line_number;
    }
    listing_file << '\n';
}

static
void write_instruction(u16 x)
{
    if (object.size() > 65536)
        fatal_error("exceeded memory capacity");
    print_listing(x);
    object.push_back(x);
}

static
void assemble_add_and(const opcode_t* op)
{
    u16 base_code = op->base_code | (expect_register() << 9);
    match(TOKEN_COMMA);

    base_code |= (expect_register() << 6);
    match(TOKEN_COMMA);

    if (peek_register()) {
        base_code |= expect_register();
    } else if (peek(TOKEN_INTEGER)) {
        token_t imm5 = expect();
        auto pair = parse_integer(imm5.f + 1, imm5.l, std::int16_t(0), imm5.base);
        if (pair.first != imm5.l || pair.second < -16 || pair.second > 16)
            error(0, "%.*s cannot be represented as a signed 5-bit integer",
                  static_cast<int>(imm5.l - imm5.f), imm5.f);
        base_code |= (1 << 5);
        base_code |= static_cast<u16>(pair.second) & 0x1F;
    } else {
        fatal_error("I was expecting a register or an integer but got '%.*s' instead",
              static_cast<int>(token.l - token.f), token.f);
    }
    write_instruction(base_code);
}

static
void assemble_label(symbol_t& symbol, u16 base_code, int n)
{
    if (symbol.line_number) {
        int offset = symbol.location - (location_counter() + 1);
        if (offset < -(1 << (n - 1))) error(0, "offset too large");
        base_code |= offset & ((1 << n) - 1);
    } else {
        symbol.location = pool.allocate(location_counter(), symbol.location);
    }
    write_instruction(base_code);
}

static
void assemble_branch(const opcode_t* op)
{
    token_t name = expect(TOKEN_NAME);
    symbol_t& symbol = get_symbol(name.f, name.l);
    assemble_label(symbol, op->base_code, 9);
}

static
void assemble_jump(const opcode_t* op)
{
    write_instruction(op->base_code | (expect_register() << 6));
}

static
void assemble_jump_subroutine(const opcode_t* op)
{
    token_t name = expect(TOKEN_NAME);
    symbol_t& symbol = get_symbol(name.f, name.l);
    assemble_label(symbol, op->base_code, 11);
}

static
void assemble_load_store(const opcode_t* op)
{
    u16 base_code = op->base_code | (expect_register() << 9);
    match(TOKEN_COMMA);
    token_t label = expect(TOKEN_NAME);
    symbol_t& symbol = get_symbol(label.f, label.l);
    assemble_label(symbol, base_code, 9);
}

static
void assemble_load_store_relative(const opcode_t* op)
{
    u16 base_code = op->base_code | (expect_register() << 9);
    match(TOKEN_COMMA);
    base_code |= (expect_register() << 6);
    match(TOKEN_COMMA);

    token_t integer = expect(TOKEN_INTEGER);
    auto pair = parse_integer(integer.f + 1, integer.l, u16(0), integer.base);
    if (pair.first != integer.l)
        fatal_error("cannot represent '%.*s' as a 16-bit unsigned integer",
              static_cast<int>(integer.l - integer.f), integer.f);
    write_instruction(base_code | pair.second);
}

static
void assemble_not(const opcode_t* op)
{
    u16 base_code = op->base_code | (expect_register() << 9);
    match(TOKEN_COMMA);
    write_instruction(base_code | (expect_register() << 6));
}

static
void assemble_trap(const opcode_t* op)
{
    token_t integer = expect(TOKEN_INTEGER);
    auto pair = parse_integer(integer.f + 1, integer.l, u16(0), integer.base);
    if (pair.first != integer.l || pair.second > 255)
        fatal_error("cannot represent '%.*s' as 8-bit unsigned integer",
              static_cast<int>(integer.l - integer.f), integer.f);
    write_instruction(op->base_code | (pair.second & 0xFF));
}

static
void assemble_base_code(const opcode_t* op)
{
    write_instruction(op->base_code);
}

static
void directive_end(const opcode_t*)
{
    source_file.setstate(std::ios_base::eofbit);
}

static
void directive_blkw(const opcode_t*)
{
    token_t integer = expect(TOKEN_INTEGER);
    auto pair = parse_integer(integer.f + 1, integer.l, u16(0), integer.base);
    if (pair.first != integer.l)
        error(0, "cannot represent '%.*s' as a 16-bit unsigned integer",
              static_cast<int>(integer.l - integer.f), integer.f);
    if ((65536 - location_counter()) < pair.second)
        fatal_error("unable to reserve %d words, insufficient space",
              static_cast<int>(pair.second));
    while (pair.second--) write_instruction(0);
}

static
void directive_fill(const opcode_t*)
{
    token_t integer = expect(TOKEN_INTEGER);
    auto pair = parse_integer(integer.f + 1, integer.l, u16(0), integer.base);
    if (pair.first != integer.l)
        fatal_error("cannot represent '%.*s' as a 16-bit unsigned integer",
              static_cast<int>(integer.l - integer.f), integer.f);
    write_instruction(pair.second);
}

static
void directive_stringz(const opcode_t*)
{
    token_t string = expect(TOKEN_STRING);
    const char* f = string.f + 1;
    const char* l = string.l - 1;
    if ((65536 - location_counter()) < (l - f + 1))
        fatal_error("The string is too large to fit in the available space");
    write_instruction(static_cast<unsigned char>(*f++));
    while (f != l) write_instruction(static_cast<unsigned char>(*f++));
    write_instruction(0);
}

static
void directive_orig(const opcode_t*);

static opcode_t opcodes[] = {
    { "ADD",   0x1000, directive_orig, assemble_add_and },
    { "AND",   0x5000, directive_orig, assemble_add_and },
    { "BRn",   0x0800, directive_orig, assemble_branch },
    { "BRz",   0x0400, directive_orig, assemble_branch },
    { "BRp",   0x0200, directive_orig, assemble_branch },
    { "BR",    0x0E00, directive_orig, assemble_branch },
    { "BRzp",  0x0600, directive_orig, assemble_branch },
    { "BRnp",  0x0A00, directive_orig, assemble_branch },
    { "BRnz",  0x0C00, directive_orig, assemble_branch },
    { "BRnzp", 0x0E00, directive_orig, assemble_branch },
    { "JMP",   0xC000, directive_orig, assemble_jump },
    { "RET",   0xC1C0, directive_orig, assemble_base_code },
    { "JSR",   0x4800, directive_orig, assemble_jump_subroutine },
    { "JSRR",  0x4000, directive_orig, assemble_jump },
    { "LD",    0x2000, directive_orig, assemble_load_store },
    { "LDI",   0xA000, directive_orig, assemble_load_store },
    { "LDR",   0x6000, directive_orig, assemble_load_store_relative },
    { "LEA",   0xE000, directive_orig, assemble_load_store },
    { "NOT",   0x903F, directive_orig, assemble_not },
    { "RTI",   0x8000, directive_orig, assemble_base_code },
    { "ST",    0x3000, directive_orig, assemble_load_store },
    { "STI",   0xB000, directive_orig, assemble_load_store },
    { "STR",   0x7000, directive_orig, assemble_load_store_relative },
    { "TRAP",  0xF000, directive_orig, assemble_trap },
    { "GETC",  0xF020, directive_orig, assemble_base_code },
    { "OUT",   0xF021, directive_orig, assemble_base_code },
    { "PUTS",  0xF022, directive_orig, assemble_base_code },
    { "IN",    0xF023, directive_orig, assemble_base_code },
    { "PUTSP", 0xF024, directive_orig, assemble_base_code },
    { "HALT",  0xF025, directive_orig, assemble_base_code },
    { ".ORIG", 0x0000, directive_orig, directive_orig },
    { ".END",  0x0000, directive_orig, directive_end },
    { ".BLKW", 0x0000, directive_orig, directive_blkw },
    { ".FILL", 0x0000, directive_orig, directive_fill },
    { ".STRINGZ", 0x0000, directive_orig, directive_stringz },
};

static inline
const opcode_t* get_opcode(const char* f, const char* l)
{
    const opcode_t* iter = std::find_if(
        std::begin(opcodes), std::end(opcodes),
        [f, l](const opcode_t& op) {
            return std::equal(op.name.begin(), op.name.end(), f, l,
                              [](const char x, const char y) {
                                  return x == std::toupper(y);
                              });
        });
    return iter;
}

static
void directive_orig(const opcode_t* op)
{
    static bool initialized = false;
    if (initialized) {
        error(0, ".ORIG can only be called once");
        return;
    }

    for (std::size_t i(0); i < sizeof(opcodes) / sizeof(opcodes[0]); ++i)
        opcodes[i].assemble = opcodes[i].assemble_fn;

    assert(object.size() == 0);
    object.push_back(0);
    if (op->name != ".ORIG") {
        error(0, "expected .ORIG as first instruction");
        op->assemble(op);
    }

    initialized = true;

    token_t integer = expect(TOKEN_INTEGER);
    auto pair = parse_integer(integer.f + 1, integer.l, u16(0), integer.base);
    if (pair.first != integer.l)
        error(0, "integer overflow: '%.*s'", static_cast<int>(integer.l - integer.f), integer.f);
    print_listing(pair.second);
    object[0] = pair.second;
}

static
void fix_forward_references(u16 position)
{
    u16& instruction = object[std::size_t(position + 1 - object[0])];
    int offset = location_counter() - position;
    switch (instruction >> 12) {
    case 0: case 2: case 3: case 10: case 11: case 14:
        if (offset - 1 > 255) error(0, "offset too large");
        instruction |= static_cast<u16>(offset - 1) & 0x1FF;
        break;
    case 4:
        if (offset - 1 > 1023) error(0, "offset too large");
        instruction |= static_cast<u16>(offset - 1) & 0x7FF;
        break;
    }
}

template <typename I, typename P>
I find_if_backward(I f, I l, P p)
{
    while (true) {
        if (l == f) return f;
        --l;
        if (p(*l)) return ++l;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s sourcefile", program_name);
        return EXIT_FAILURE;
    }

    source_filename = argv[1];
    source_file.open(source_filename);
    if (!source_file.is_open()) {
        fprintf(stderr, "%s: error: %s: %s",
                program_name, source_filename, strerror(errno));
        return EXIT_FAILURE;
    }

    const char* end = source_filename + strlen(source_filename);
    const char* tmp = find_if_backward(
        source_filename, end, [](char c) {
            return c == '.' || c == '\\' || c == '/';
        });
    if (tmp != source_filename && *(tmp - 1) == '.') {
        char* temp = std::copy(source_filename, tmp, listing_filename);
        strcpy(temp, "lst");
        temp = std::copy(source_filename, tmp, object_filename);
        strcpy(temp, "obj");
    } else {
        char* temp = std::copy(source_filename, end, listing_filename);
        strcpy(temp, ".lst");
        temp = std::copy(source_filename, end, object_filename);
        strcpy(temp, ".obj");
    }

    listing_file.open(listing_filename);
    if (!listing_file) {
        fprintf(stderr, "%s: error: %s: %s", program_name, listing_filename, strerror(errno));
        return EXIT_FAILURE;
    }

    while (!source_file.getline(line, sizeof(line)).eof()) {
        ++line_number;
        if (source_file.fail()) {
            warn("line length too long, ignoring characters");
            source_file.clear();
            source_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        line_cursor = line;
        next_token();
        if (peek(TOKEN_EOL)) continue;

        token_t name = expect(TOKEN_NAME);
        if (match(TOKEN_COLON)) {
            symbol_t& symbol = get_symbol(name.f, name.l);
            if (symbol.line_number) {
                error(0, "label '%.*s' already defined, see line %d",
                      static_cast<int>(name.l - name.f), name.f,
                      symbol.line_number);
            } else {
                list_type list = symbol.location;
                while (!pool.is_end(list)) {
                    fix_forward_references(pool.value(list));
                    list = pool.free(list);
                }
                symbol.line_number = line_number;
                symbol.location = location_counter();
            }
            name = expect(TOKEN_NAME);
        }

        const opcode_t* op = get_opcode(name.f, name.l);
        if (op == std::end(opcodes)) {
            error(0, "unrecognized instruction '%.*s'",
                  static_cast<int>(name.l - name.f), name.f);
            continue;
        }
        op->assemble(op);
        expect(TOKEN_EOL);
    }

    listing_file << "\nSymbol Table\n------------\n";
    for (const auto& symbol : symbols) {
        if (!symbol.line_number) {
            error(0, "undefined reference '%s'", symbol.name.c_str());
        } else {
            listing_file << '(' << std::setfill('0') << std::setw(4) << std::dec <<  symbol.line_number << ") ";
            listing_file << std::hex << symbol.location << ' ' << symbol.name << std::endl;
        }
    }

    if (error_count != 0) {
        if (error_count == 1) fputs("one error found", stderr);
        else fprintf(stderr, "%d errors found", error_count);
        return EXIT_FAILURE;
    }

    std::ofstream object_file(object_filename, std::ios::binary);
    if (!object_file) {
        fprintf(stderr, "%s: error: %s: %s",
                program_name, object_filename, strerror(errno));
        return EXIT_FAILURE;
    }

    std::ostream_iterator<unsigned char> f_o(object_file);
    for (u16 x : object) f_o = store_big_endian(x, f_o);
}
