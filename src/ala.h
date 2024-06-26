/* date = June 9th 2024 6:13 pm */
#ifndef ALA_H

#include <stdint.h>

typedef enum {
    NO_JMP_LIMIT = 1,
    PRINT_NUMBERS = 2,
    ALA_EXTRA = 4,
    ALA_DEBUG = 8,
} ala_flags;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef int32_t b32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define SV_IMPLEMENTATION
#include "sv.h"

#define ARENA_MEMORY_SIZE 1024*1024*1024 + sizeof(memory_arena)

typedef struct _memory_arena {
    size_t Size;
    u8 *Base;
    size_t Used;
    struct _memory_arena *Next;
} memory_arena;

void InitializeArena(memory_arena *Arena, size_t Size, void *Base)
{
    Arena->Size = Size - sizeof(memory_arena);
    Arena->Base = (u8 *)Base;
    Arena->Used = 0;
    Arena->Next = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size_)
void *PushSize_(memory_arena **Arena, size_t SizeInit)
{
    size_t Size = SizeInit;
    
    if(((*Arena)->Used + Size) > (*Arena)->Size) {
        void *Memory = malloc(ARENA_MEMORY_SIZE);
        memset(Memory, 0, ARENA_MEMORY_SIZE);
        (*Arena)->Next = (memory_arena *)(*Arena)->Base;
        InitializeArena((*Arena)->Next, ARENA_MEMORY_SIZE, Memory);
        *Arena = (*Arena)->Next;
    }
    
    void *Result = (*Arena)->Base + (*Arena)->Used;
    (*Arena)->Used += Size;
    
    assert(Size >= SizeInit);
    
    return(Result);
}

typedef enum {
    IOP_LDM = 0,
    IOP_LDD = 1,
    IOP_LDI = 2,
    IOP_LDX = 3,
    IOP_LDR = 4,
    IOP_STO = 5,
    IOP_STX = 6,
    IOP_STI = 7,
    IOP_ADD = 8,
    IOP_JMP = 9,
    IOP_CMP = 10, // immediate + direct
    IOP_JPE = 11,
    IOP_JPN = 12,
    IOP_INP = 13,
    IOP_OUT = 14,
    IOP_AND = 15, // immediate + direct
    IOP_XOR = 16, // immediate + direct
    IOP_OR  = 17, // immediate + direct
    IOP_LSL = 18,
    IOP_LSR = 19,
    IOP_END = 20,
    
    IOP_ACCINC = 21,
    IOP_ACCDEC = 22,
    // NOTE(vic): ORDER MATTERS!!
    IOP_IXINC = 23,
    IOP_IXDEC = 24,
    
    IOP_CALL = 25, // This is an extra -> doesn't exist in A level assembly
    IOP_RETURN = 26, // This is an extra -> doesn't exist in A level assembly
    
    // IOP_JPI // indirect jump
    
    IOP_COUNT,
} instruction_code;

typedef struct {
    String_View Name;
    int Evaluated;
    size_t LineRef;
    int FileIndex;
} symbol;

typedef struct _symbol_table {
    symbol Symbols[1024];
    int Used;
    struct _symbol_table *Next;
} symbol_table;

typedef struct {
    size_t LineInFile;
    instruction_code Opcode;
    long int Operand;
    symbol *Label;
    int Immediate;
} line_of_code;

// TODO(vic): Test only data here!
typedef struct {
    size_t LOC;
    symbol *Symbol;
    int nJumps;
    long int *Data;
} line_map;

typedef struct {
    size_t Capacity;
    char *Cstr;
} tmp_cstr;

typedef struct {
    memory_arena **Arena;
    tmp_cstr *tc;
    String_View *File;
    String_View **ProgramLines;
    int FileIndex;
    line_of_code *Program;
    
    symbol *StartSymbol;
    size_t StartLOC;
    
    symbol_table *SymbolTable;
    symbol_table **CurrentSymbolTable;
} lexer;

static const String_View InstructionList[IOP_COUNT] = {
    [IOP_LDM] = SV_STATIC("LDM"),
    [IOP_LDD] = SV_STATIC("LDD"),
    [IOP_LDI] = SV_STATIC("LDI"),
    [IOP_LDX] = SV_STATIC("LDX"),
    [IOP_LDR] = SV_STATIC("LDR"),
    [IOP_STO] = SV_STATIC("STO"),
    [IOP_STX] = SV_STATIC("STX"),
    [IOP_STI] = SV_STATIC("STI"),
    [IOP_ADD] = SV_STATIC("ADD"),
    [IOP_JMP] = SV_STATIC("JMP"),
    [IOP_CMP] = SV_STATIC("CMP"),
    [IOP_JPE] = SV_STATIC("JPE"),
    [IOP_JPN] = SV_STATIC("JPN"),
    [IOP_INP] = SV_STATIC("INP"),
    [IOP_OUT] = SV_STATIC("OUT"),
    [IOP_AND] = SV_STATIC("AND"),
    [IOP_XOR] = SV_STATIC("XOR"),
    [IOP_OR] = SV_STATIC("OR"),
    [IOP_LSL] = SV_STATIC("LSL"),
    [IOP_LSR] = SV_STATIC("LSR"),
    [IOP_END] = SV_STATIC("END"),
    
    [IOP_ACCINC] = SV_STATIC("INC"),
    [IOP_ACCDEC] = SV_STATIC("DEC"),
    [IOP_IXINC] = SV_STATIC("INC"),
    [IOP_IXDEC] = SV_STATIC("DEC"),
    
    [IOP_CALL] = SV_STATIC("CALL"),
    [IOP_RETURN] = SV_STATIC("RETURN"),
};

static const char *InstructionListInfo[IOP_COUNT] = {
    [IOP_LDM] = "LDM #n: Immediate addressing. Load the number n to ACC",
    [IOP_LDD] = "LDD <address>: Direct Addressing. Load the contents of the location at the given address to ACC",
    [IOP_LDI] = "LDI <address>: Indirect Addressing. The address to be used is the given address.\n"
        "Load the contents of this second address to ACC",
    [IOP_LDX] = "LDX <address>: Indexed Addressing. Form the address from <address> + the contents of the index register.\n"
        "Copy the contents of this calculated address to ACC",
    [IOP_LDR] = "LDR #n: Immediate Addressing. Load the number n to IX",
    [IOP_STO] = "STO <address>: Store the contents of ACC at the given address",
    [IOP_STX] = "STX <address>: Indexed Address. Form the address from <address> + the contents of the index register.\n"
        "Copy the contents from ACC to this calculated address",
    [IOP_STI] = "STI <address>: Indirect Addressing. The address to be used is at the given address.\n"
        "Store the contents of ACC at this second address",
    [IOP_ADD] = "ADD <address>: Add the contents of the given address to the ACC",
    [IOP_JMP] = "JMP <address>: Jump to the given address",
    [IOP_CMP] = "CMP <address>: Compare the contents of ACC with the contents of <address>\n"
        "CMP #n: Compare the contents of ACC with number n",
    [IOP_JPE] = "JPE <address>: Following a compare instruction, jump to <address> if the compare was True",
    [IOP_JPN] = "JPN <address>: Following a compare instruction, jump to <address> if the compare was False",
    [IOP_INP] = "INP: Key in a character and store its ASCII value in ACC",
    [IOP_OUT] = "OUT: Output to the screen the ASCII value in ACC",
    [IOP_AND] = "AND #n: Bitwise AND operation of the contents of ACC with the operand\n"
        "AND <address>: Bitwise AND operation of the content of ACC with the contents of <address>",
    [IOP_XOR] = "XOR #n: Bitwise XOR operation of the contents of ACC with the operand\n"
        "XOR <address>: Bitwise XOR operation of the content of ACC with the contents of <address>",
    [IOP_OR] = "OR #n: Bitwise OR operation of the contents of ACC with the operand\n"
        "OR <address>: Bitwise OR operation of the content of ACC with the contents of <address>",
    [IOP_LSL] = "LSL #n: Shift the bits in ACC n places to the left. Zeros are introduced on the right-hand end",
    [IOP_LSR] = "LSR #n: Shift the bits in ACC n places to the right. Zeros are introduced on the left-hand end",
    [IOP_END] = "END: Return control to the operating system",
    
    [IOP_ACCINC] = "INC <register>: Add 1 to the contents of the register (ACC or IX)",
    [IOP_ACCDEC] = "DEC <register>: Substract 1 to the contents of the register (ACC or IX)",
    [IOP_IXINC] = "INC <register>: Add 1 to the contents of the register (ACC or IX)",
    [IOP_IXDEC] = "DEC <register>: Substract 1 to the contents of the register (ACC or IX)",
    
    [IOP_CALL] = "CALL <label>: Records the current address and jumps to label",
    [IOP_RETURN] = "RETURN: Returns to the last recorded address (by a CALL instruction)",
};

int IsSet(int A, int Flag)
{
    return (A & Flag);
}

void ClearFlags(int *A, int Flag)
{
    *A &= ~Flag;
}

#define ALA_H
#endif //ALA_H
