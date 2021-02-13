#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include "endian.h"

using u16 = std::uint16_t;

enum {
    OP_BR,
    OP_ADD,
    OP_LD,
    OP_ST,
    OP_JSR,
    OP_AND,
    OP_LDR,
    OP_STR,
    OP_RTI,
    OP_NOT,
    OP_LDI,
    OP_STI,
    OP_JMP,
    OP_INVALID,
    OP_LEA,
    OP_TRAP,
};

enum {
    TRAP_GETC = 0x20,
    TRAP_OUT  = 0x21,
    TRAP_PUTS = 0x22,
    TRAP_IN   = 0x23,
    TRAP_HALT = 0x25,
};

static u16 registers[8];

static inline
u16& get_register(u16 x, int n)
{ return registers[(x >> n) & 0x7]; }

static u16 condition_register;
static u16 memory[std::numeric_limits<u16>::max()];

u16 sign_extend(u16 x, int n)
{
    if ((x >> (n - 1)) & 1) x |= (0xFFFF << n);
    return x;
}

u16 sign_extend_mask(u16 x, int n)
{
    u16 mask = 0xFFFF >> (16 - n);
    return sign_extend(x & mask, n);
}

enum {
    FLAG_POSITIVE = 1 << 0,
    FLAG_ZERO = 1 << 1,
    FLAG_NEGATIVE = 1 << 2,
};

void set_condition_codes(u16 x)
{
    if (x == 0) condition_register = FLAG_ZERO;
    else if (x >> 15) condition_register = FLAG_NEGATIVE;
    else condition_register = FLAG_POSITIVE;
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fputs("Usage: lc3 objectfile", stderr);
        return EXIT_FAILURE;
    }

    const char* object_filename = argv[1];
    std::ifstream object_file(object_filename, std::ios::binary);
    if (!object_file) {
        fprintf(stderr, "lc3: error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    std::istream_iterator<unsigned char> f_i(object_file);
    std::istream_iterator<unsigned char> l_i{};
    u16 program_counter;
    f_i = rks::load_big_endian(program_counter, f_i);
    u16* memory_cursor = memory + program_counter;
    while (f_i != l_i) {
        f_i = rks::load_big_endian(*memory_cursor, f_i);
        ++memory_cursor;
    }

    bool running = true;
    while (running) {
        u16 instruction = memory[program_counter++];
        u16 opcode = instruction >> 12;
        switch (opcode) {
        case OP_BR: {
            if (((instruction >> 9) & 0x7) & condition_register)
                program_counter += sign_extend_mask(instruction, 9);
        } break;

        case OP_ADD: {
            u16& destination = get_register(instruction, 9);
            u16 source1 = get_register(instruction, 6);
            if ((instruction >> 5u) & 0x1)
                destination = source1 + sign_extend_mask(instruction, 5);
            else
                destination = source1 + get_register(instruction, 0);
            set_condition_codes(destination);
        } break;

        case OP_LD: {
            u16& destination = get_register(instruction, 9);
            destination = memory[program_counter + sign_extend_mask(instruction, 9)];
            set_condition_codes(destination);
        } break;

        case OP_ST: {
            memory[program_counter + sign_extend_mask(instruction, 9)] = get_register(instruction, 9);
        } break;

        case OP_JSR: {
            registers[7] = program_counter;
            if ((instruction >> 11) & 0x1)
                program_counter += sign_extend_mask(instruction, 11);
            else
                program_counter = get_register(instruction, 6);
        } break;

        case OP_AND: {
            u16& destination = get_register(instruction, 9);
            u16 source1 = get_register(instruction, 6);
            if ((instruction >> 5u) & 0x1)
                destination = source1 & sign_extend_mask(instruction, 5);
            else
                destination = source1 & get_register(instruction, 0);
            set_condition_codes(destination);
        } break;

        case OP_LDR: {
            u16& destination = get_register(instruction, 9);
            destination = memory[get_register(instruction, 6) + sign_extend_mask(instruction, 6)];
            set_condition_codes(destination);
        } break;

        case OP_STR: {
            memory[get_register(instruction, 6) + sign_extend_mask(instruction, 6)] = get_register(instruction, 9);
        } break;

        case OP_NOT: {
            u16& destination = get_register(instruction, 9);
            destination = ~get_register(instruction, 6);
            set_condition_codes(destination);
        } break;

        case OP_LDI: {
            u16& destination = get_register(instruction, 9);
            destination = memory[memory[program_counter + sign_extend_mask(instruction, 9)]];
            set_condition_codes(destination);
        } break;

        case OP_STI: {
            memory[memory[program_counter + sign_extend_mask(instruction, 9)]] = get_register(instruction, 9);
        } break;

        case OP_JMP: {
            program_counter = get_register(instruction, 6);
        } break;

        case OP_LEA: {
            u16& destination = get_register(instruction, 9);
            destination = program_counter + sign_extend_mask(instruction, 9);
            set_condition_codes(destination);
        } break;

        case OP_TRAP: {
            switch (instruction & 0xFF) {
            case TRAP_GETC: {
                int c = getchar();
                registers[0] = static_cast<u16>(c);
            } break;
            case TRAP_OUT: {
                putchar(registers[0]);
            } break;
            case TRAP_PUTS: {
                u16* s = memory + registers[0];
                while (*s) {
                    putchar(*s);
                    ++s;
                }
            } break;
            case TRAP_IN: {
                puts("Enter the character: ");
                unsigned char c = getchar();
                putchar(c);
                registers[0] = static_cast<u16>(c);
            } break;
            case TRAP_HALT: {
                puts("\nprogram finished");
                fflush(stdout);
                running = false;
            } break;
            }
        } break;

        case OP_RTI:
        case OP_INVALID:
            fputs("invalid operation: terminating", stderr);
            std::exit(EXIT_FAILURE);
        }
    }
}
