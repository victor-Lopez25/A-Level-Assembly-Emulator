/* 
Creator: Víctor López Cortés

This is a program that is supposed to translate/execute 
A level assembly into C or x64 assembly

Name: ALA (A Level Assembler)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ala.h"
#include "file.c"

#define PROGRAM_NAME "ala.exe"

void PrintAscii(void)
{
    for(unsigned char c = 32; c != 0; c++)
    {
        if(c == 127) {
            printf("DEL = %d\n", (int)c);
        }
        else {
            printf("%c = %d\n", (char)c, (int)c);
        }
    }
}

int GetInstructionCode(String_View Token)
{
    int Code = -1;
    for(int i = 0; i < IOP_COUNT; i++) {
        if(sv_eq_ignorecase(Token, InstructionList[i])) {
            Code = i;
            break;
        }
    }
    
    return Code;
}

String_View sv_get_str(char *_In, int n)
{
    char c = (char)fgetc(stdin);
    int i = 0;
    for(; (i < (n - 1)) && c != '\0' && c != '\n' && c != EOF; 
        i++)
    {
        _In[i] = c;
        c = (char)fgetc(stdin);
    }
    return sv_from_parts(_In, i);
}

void Usage(void)
{
    printf("To start using ALA (A Level Assembler), execute "PROGRAM_NAME" <file>\n"
           "To show the printable characters in the ascii table, type 'ascii'\n"
           "To show the instruction list, type IL\n"
           "To show info on a specific instruction, type I followed by the instruction name (with a space)\n"
           "To show availiable flags, type F\n"
           "To learn more assembly, type A\n"
           "Or quit by pressing enter\n");
    
    char _In[30];
    String_View Option = sv_trim(sv_get_str(_In, 30));
    
    if(sv_eq_ignorecase(Option, SV("ascii"))) {
        PrintAscii();
    }
    else if(sv_eq_ignorecase(Option, SV("IL"))) {
        for(int Instruction = 0; Instruction < IOP_COUNT; Instruction++)
        {
            printf("\n%s\n", InstructionListInfo[Instruction]);
        }
    }
    else if(sv_eq_ignorecase(Option, SV("A"))) {
        printf("If you want to learn more assembly, I suggest you look at MIPS R2000 assembly.\n"
               "To execute MIPS R2000 assembly, you'll need the PCSpim emulator you can find online.\n"
               "It is a more 'complete' but still simple assembly.\n"
               "The difference is, MIPS R2000 is an actual assembly language.\n\n"
               "If you want to learn programming, it is still essential to understand assembly,\n"
               "however, it isn't necessary to be able to program in assembly, only to read it.\n"
               "Simpler assembly instruction sets allow students to understand assembly more easily.");
    }
    else if(sv_eq_ignorecase(Option, SV("F"))) {
        printf("Flags:\n"
               "no-jmp-limits: Removes the jump limits, in case you want infinite loops\n"
               "print-numbers: OUT instruction will print integers instead of characters\n"
               "extra: Adds in a couple extra instructions to make using this assembly easier\n\n"
               "Extra instructions:\n"
               "CALL <label>: Records the current address and jumps to label\n"
               "RETURN: Returns to the last recorded address (by a CALL instruction)");
    }
    else if(*Option.data == 'I') {
        sv_chop_by_delim(&Option, ' ');
        String_View SubOption = sv_trim(Option);
        if(SubOption.count == 0) {
            printf("Show info on a specific instruction by typing the instruction opcode\n");
            SubOption = sv_trim(sv_get_str(_In, 30));
        }
        int Opcode = GetInstructionCode(SubOption);
        while(Opcode == -1 && SubOption.count != 0)
        {
            printf(SV_Fmt" is not a supported instruction.\n"
                   "Show info on a specific instruction by typing the instruction opcode\n"
                   "If you don't want to get info on an instruction press enter.\n",
                   SV_Arg(SubOption));
            SubOption = sv_trim(sv_get_str(_In, 30));
            Opcode = GetInstructionCode(SubOption);
        }
        
        if(SubOption.count != 0) {
            printf("%s", InstructionListInfo[Opcode]);
        }
    }
}

symbol *AddSymbol(memory_arena **Arena, symbol_table **SymbolTable, symbol Symbol)
{
    symbol_table *Table = *SymbolTable;
    if(Table->Used == 1024) {
        Table->Next = PushStruct(Arena, symbol_table);
        SymbolTable = &Table->Next;
    }
    
    symbol *NewSymbol = &(*SymbolTable)->Symbols[(*SymbolTable)->Used++];
    *NewSymbol = Symbol;
    return NewSymbol;
}

char *TmpCstrFill(tmp_cstr *tc, const char *Data, size_t DataSize)
{
    if(DataSize + 1 >= tc->Capacity) {
        tc->Capacity = DataSize + 1;
        tc->Cstr = realloc(tc->Cstr, tc->Capacity);
    }
    
    memcpy(tc->Cstr, Data, DataSize);
    tc->Cstr[DataSize] = '\0';
    return tc->Cstr;
}

bool sv_strtol(String_View sv, tmp_cstr *tc, long int *out)
{
    int Base = 10;
    String_View temp = sv;
    if(*temp.data == 'B') {
        sv_chop_left(&temp, 1);
        Base = 2;
    }
    else if(*temp.data == '&') {
        sv_chop_left(&temp, 1);
        Base = 16;
    }
    
    char *ptr = TmpCstrFill(tc, temp.data, temp.count);
    char *endptr = NULL;
    long int result;
    if(Base == 10) {
        result = strtol(ptr, &endptr, Base);
    }
    else {
        result = (long int)strtoul(ptr, &endptr, Base);
    }
    
    if(out) *out = result;
    return endptr != ptr && *endptr == '\0';
}

symbol *IsInSymbolTable(symbol_table *SymbolTable, String_View Symbol)
{
    for(symbol_table *Table = SymbolTable;
        Table;
        Table = Table->Next)
    {
        for(int i = 0; i < Table->Used; i++)
        {
            if(sv_eq(Table->Symbols[i].Name, Symbol)) {
                return &Table->Symbols[i];
            }
        }
    }
    
    return 0;
}

void ParseGeneralOperand(lexer *Lexer, size_t CurrentLine, String_View OperandToken, line_of_code *LOC)
{
    if(!sv_strtol(OperandToken, Lexer->tc, &LOC->Operand)) {
        // It's a label
        if(GetInstructionCode(OperandToken) != -1) {
            fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid operand using reserved keyword\n",
                    SV_Arg(*Lexer->File), CurrentLine);
            exit(1);
        }
        
        // Add an unevaluated symbol
        symbol *TempSymbol = IsInSymbolTable(Lexer->SymbolTable, OperandToken);
        if(TempSymbol) {
            LOC->Label = TempSymbol;
        }
        else {
            symbol Symbol = {0};
            Symbol.Name = OperandToken;
            Symbol.LineRef = CurrentLine;
            Symbol.FileIndex = Lexer->FileIndex;
            LOC->Label = AddSymbol(Lexer->Arena, Lexer->CurrentSymbolTable, Symbol);
        }
    }
}

line_of_code ParseInstruction(lexer *Lexer, int Flags, size_t CurrentLine, String_View Line, int Opcode)
{
    line_of_code LOC = {0};
    LOC.LineInFile = CurrentLine;
    LOC.Opcode = Opcode;
    
    if(Opcode == IOP_INP || Opcode == IOP_OUT || Opcode == IOP_END || Opcode == IOP_RETURN) {
        if(Opcode == IOP_RETURN && !IsSet(Flags, ALA_EXTRA)) {
            fprintf(stderr,
                    "\n"SV_Fmt"(%zu): ERROR: This instruction doesn't exist in A level assembly\n"
                    "NOTE: To use this instruction, use the flag '-extra' to use this instruction",
                    SV_Arg(*Lexer->File), CurrentLine);
            exit(1);
        }
        
        if(Line.count > 0 && (*Line.data != '/' || *(Line.data + 1) != '/'))
        {
            fprintf(stderr, SV_Fmt"(%zu): ERROR: The operand '"SV_Fmt"' doesn't take an opcode\n", 
                    SV_Arg(*Lexer->File), CurrentLine, SV_Arg(InstructionList[Opcode]));
            exit(1);
        }
    }
    else
    {
        if(Line.count == 0) {
            fprintf(stderr, SV_Fmt"(%zu): ERROR: The operand '"SV_Fmt"'Is missing an opcode\n", 
                    SV_Arg(*Lexer->File), CurrentLine, SV_Arg(InstructionList[Opcode]));
            exit(1);
        }
        
        String_View OperandToken = sv_chop_by_delim(&Line, ' ');
        if(Line.count > 0) {
            if(Line.count == 1 || 
               (*Line.data != '/' && *(Line.data + 1) != '/'))
            {
                fprintf(stderr, 
                        SV_Fmt"(%zu): ERROR: Unkown token(s) '"SV_Fmt"' after operand '"SV_Fmt"'\n",
                        SV_Arg(*Lexer->File), CurrentLine, SV_Arg(Line), SV_Arg(OperandToken));
                exit(1);
            }
        }
        
        switch(Opcode)
        {
            // registers
            case IOP_ACCINC:
            {
                if(sv_eq(OperandToken, SV("IX"))) {
                    LOC.Opcode = IOP_IXINC;
                }
                else if(!sv_eq(OperandToken, SV("ACC"))) {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: The operand '"SV_Fmt"' doesn't take a register\n", 
                            SV_Arg(*Lexer->File), CurrentLine, SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
            } break;
            // registers
            case IOP_ACCDEC:
            {
                if(sv_eq(OperandToken, SV("IX"))) {
                    LOC.Opcode = IOP_IXDEC;
                }
                else if(!sv_eq(OperandToken, SV("ACC"))) {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: The operand '"SV_Fmt"' doesn't take a register\n", 
                            SV_Arg(*Lexer->File), CurrentLine, SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
            } break;
            
            // only immediate
            case IOP_LDM:
            case IOP_LDR:
            case IOP_LSL:
            case IOP_LSR:
            {
                if(*OperandToken.data == '#') {
                    sv_chop_left(&OperandToken, 1);
                }
                else {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid operand for "SV_Fmt".\n"
                            SV_Fmt" operands start with a '#'.\n", SV_Arg(*Lexer->File), CurrentLine, 
                            SV_Arg(InstructionList[Opcode]), SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
                
                if(!sv_strtol(OperandToken, Lexer->tc, &LOC.Operand)) {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid operand for "SV_Fmt".\n"
                            "Immediate addressing has opcodes that only contain numbers starting with '#':.\n"
                            "#<number>\n"
                            "#5\n", SV_Arg(*Lexer->File), 
                            CurrentLine, SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
            } break;
            
            // not immediate
            case IOP_LDD:
            case IOP_LDI:
            case IOP_LDX:
            case IOP_STO:
            case IOP_STX:
            case IOP_STI:
            case IOP_ADD:
            case IOP_JMP:
            case IOP_JPE:
            case IOP_JPN:
            {
                if(*OperandToken.data == '#') {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid operand for "SV_Fmt"\n"
                            SV_Fmt" doesn't have immediate addressing\n", 
                            SV_Arg(*Lexer->File), CurrentLine,
                            SV_Arg(InstructionList[Opcode]), SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
                
                ParseGeneralOperand(Lexer, CurrentLine, OperandToken, &LOC);
            } break;
            
            case IOP_CALL:
            {
                if(!IsSet(Flags, ALA_EXTRA)) {
                    fprintf(stderr,
                            "\n"SV_Fmt"(%zu): ERROR: This instruction doesn't exist in A level assembly\n"
                            "NOTE: To use this instruction, use the flag '-extra' to use this instruction",
                            SV_Arg(*Lexer->File), CurrentLine);
                    exit(1);
                }
                
                // only takes labels
                if(*OperandToken.data == '#') {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid operand for "SV_Fmt"\n"
                            SV_Fmt" Only takes labels\n", SV_Arg(*Lexer->File), CurrentLine,
                            SV_Arg(InstructionList[Opcode]), SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
                
                if(sv_strtol(OperandToken, Lexer->tc, &LOC.Operand)) {
                    fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid operand for "SV_Fmt".\n"
                            SV_Fmt" Only takes labels\n", SV_Arg(*Lexer->File), CurrentLine, SV_Arg(InstructionList[Opcode]), SV_Arg(InstructionList[Opcode]));
                    exit(1);
                }
                
                symbol *TempSymbol = IsInSymbolTable(Lexer->SymbolTable, OperandToken);
                if(TempSymbol) {
                    LOC.Label = TempSymbol;
                }
                else {
                    symbol Symbol = {0};
                    Symbol.Name = OperandToken;
                    Symbol.LineRef = CurrentLine;
                    Symbol.FileIndex = Lexer->FileIndex;
                    LOC.Label = AddSymbol(Lexer->Arena, Lexer->CurrentSymbolTable, Symbol);
                }
            } break;
            
            // any
            default:
            {
                if(*OperandToken.data == '#') {
                    sv_chop_left(&OperandToken, 1);
                    LOC.Immediate = 1;
                }
                
                ParseGeneralOperand(Lexer, CurrentLine, OperandToken, &LOC);
            } break;
        }
    }
    
    return LOC;
}

size_t ParseCode(String_View Content, lexer *Lexer, int Flags,
                 line_map *LineMappings, size_t CurrentLOC)
{
    for(size_t CurrentLine = 0; Content.count > 0; CurrentLine++)
    {
        String_View Line = sv_trim(sv_chop_by_delim(&Content, '\n'));
        LineMappings[CurrentLine].LOC = CurrentLOC;
        if(Line.count == 0) {
            LineMappings[CurrentLine].Data = PushStruct(Lexer->Arena, long int);
            *LineMappings[CurrentLine].Data = 0;
            continue;
        }
        if(Line.count > 1 && *Line.data == '/' && *(Line.data + 1) == '/') {
            continue;
        }
        
        String_View OpcodeToken = sv_trim(sv_chop_by_delim(&Line, ' '));
        long int DataValue = 0;
        if(sv_strtol(OpcodeToken, Lexer->tc, &DataValue)) {
            LineMappings[CurrentLine].Data = PushStruct(Lexer->Arena, long int);
            *LineMappings[CurrentLine].Data = DataValue;
        }
        else {
            int Opcode = GetInstructionCode(OpcodeToken);
            if(Opcode == -1) {
                if(OpcodeToken.data[OpcodeToken.count - 1] != ':') {
                    fprintf(stderr, 
                            SV_Fmt"(%zu): ERROR: Unknown token '"SV_Fmt"'.\n"
                            "Make labels with an identifier followed by a colon:\n"
                            SV_Fmt": \n"
                            "Make sure to add a space after the colon.\n", 
                            SV_Arg(*Lexer->File), CurrentLine, 
                            SV_Arg(OpcodeToken), SV_Arg(OpcodeToken));
                    exit(1);
                }
                
                int ShouldIncLOC = 0;
                // (FR == for real)
                String_View OpcodeTokenFR = sv_chop_by_delim(&Line, ' ');
                Opcode = GetInstructionCode(OpcodeTokenFR);
                long int SymbolValue = 0;
                if(Opcode == -1) {
                    if(OpcodeTokenFR.count > 0 &&
                       (*OpcodeTokenFR.data != '/' ||
                        *(OpcodeTokenFR.data + 1) != '/'))
                    {
                        // NOTE(vic): Try to parse symbol value
                        if(sv_strtol(OpcodeTokenFR, Lexer->tc, &SymbolValue)) {
                            LineMappings[CurrentLine].Data = PushStruct(Lexer->Arena, long int);
                            *LineMappings[CurrentLine].Data = SymbolValue;
                        } else {
                            fprintf(stderr, SV_Fmt"(%zu): ERROR: Invalid value for label "SV_Fmt"\n", 
                                    SV_Arg(*Lexer->File), CurrentLine, SV_Arg(OpcodeToken));
                            exit(1);
                        }
                    }
                }
                else {
                    line_of_code LOC = 
                        ParseInstruction(Lexer, Flags, CurrentLine, Line, Opcode);
                    
                    Lexer->Program[CurrentLOC] = LOC;
                    ShouldIncLOC = 1;
                }
                
                OpcodeToken.count--; // delete ':'
                symbol *Symbol = IsInSymbolTable(Lexer->SymbolTable, OpcodeToken);
                if(Symbol) {
                    if(Symbol->Evaluated) {
                        fprintf(stderr, SV_Fmt"(%zu): ERROR: Label already declared.\n"
                                SV_Fmt"(%zu): NOTE: See initial declaration of label\n",
                                SV_Arg(*Lexer->File), CurrentLine, SV_Arg(*Lexer->File), Symbol->LineRef);
                        exit(1);
                    }
                }
                else {
                    symbol NewSymbol = {
                        .Name = OpcodeToken,
                    };
                    Symbol = AddSymbol(Lexer->Arena, Lexer->CurrentSymbolTable, NewSymbol);
                    
                    if(Lexer->StartSymbol && sv_eq(Symbol->Name, SV("START"))) {
                        fprintf(stderr, SV_Fmt"(%zu): ERROR: There can only be one START label\n"
                                SV_Fmt"(%zu): NOTE: See first definition of the START label\n",
                                SV_Arg(*Lexer->File), CurrentLine, SV_Arg(*Lexer->File), Symbol->LineRef);
                    }
                }
                Symbol->LineRef = CurrentLine;
                Symbol->Evaluated = 1;
                Symbol->FileIndex = Lexer->FileIndex;
                LineMappings[CurrentLine].Symbol = Symbol;
                
                if(sv_eq(Symbol->Name, SV("START"))) {
                    Lexer->StartSymbol = Symbol;
                    Lexer->StartLOC = CurrentLOC;
                }
                
                if(ShouldIncLOC) {
                    CurrentLOC++;
                }
            }
            else {
                line_of_code LOC = 
                    ParseInstruction(Lexer, Flags, CurrentLine, Line, Opcode);
                
                Lexer->Program[CurrentLOC++] = LOC;
            }
        }
    }
    
    return CurrentLOC;
}

#define CheckAddress(Line, Address, Message, ...) \
if(Address > LineCounts[AddressFileIndex] || Address < 0) { \
fprintf(stderr, \
"\n"SV_Fmt"(%zu): ERROR: Incorrect address for operand, not in program" \
Message, SV_Arg(FileNames[CurrentFileIndex]), Line, ##__VA_ARGS__); \
exit(1); \
}

#define CheckDataInAddress(Line, Address, Message, ...) \
if(!LineMappings[AddressFileIndex][Address].Data) { \
fprintf(stderr, "\n"SV_Fmt"(%zu): ERROR: No data in address %zd in file "SV_Fmt Message, \
SV_Arg(FileNames[CurrentFileIndex]), Line, Address, \
SV_Arg(FileNames[AddressFileIndex]), ##__VA_ARGS__); \
exit(1); \
}

#define CheckStoreDataInAddress(Line, Address, Message, ...) \
if(!LineMappings[AddressFileIndex][Address].Data) { \
fprintf(stderr, "\n"SV_Fmt"(%zu): ERROR: Invalid address %zd in file "SV_Fmt Message, \
SV_Arg(FileNames[CurrentFileIndex]), Line, Address, \
SV_Arg(FileNames[AddressFileIndex]), ##__VA_ARGS__); \
exit(1); \
}

#define CheckJumpToAddress(Line, Address, Message, ...) \
if(LineMappings[AddressFileIndex][Address].Data) { \
fprintf(stderr, \
"\n"SV_Fmt"(%zu): ERROR: Invalid jump address %zd in file "SV_Fmt", it contains data"Message, \
SV_Arg(FileNames[CurrentFileIndex]), Line, Address, \
SV_Arg(FileNames[AddressFileIndex]), ##__VA_ARGS__); \
exit(1); \
}

#define JMP_LIMIT 100000
#define CheckJumpLimit(Address) \
LineMappings[AddressFileIndex][Address].nJumps++; \
if(LineMappings[AddressFileIndex][Address].nJumps > JMP_LIMIT && !IsSet(Flags, NO_JMP_LIMIT)) { \
fprintf(stderr, "\n"SV_Fmt"(%zu): ERROR: Maximum jump limit reached\n" \
"NOTE: If you want to disable this error use the '-no-jmp-limits' flag", \
SV_Arg(FileNames[CurrentFileIndex]), Address); \
exit(1); \
}

#define JumpToLine() \
if(LOC->Label) { \
AddressFileIndex = LOC->Label->FileIndex; \
CheckJumpToAddress(LOC->LineInFile, LOC->Label->LineRef, ""); \
line = LineMappings[AddressFileIndex][LOC->Label->LineRef].LOC - 1; \
} else { \
CheckAddress(LOC->LineInFile, LOC->Operand, ""); \
CheckJumpToAddress(LOC->LineInFile, (size_t)LOC->Operand, ""); \
line = LineMappings[AddressFileIndex][LOC->Operand].LOC - 1; \
} \
CheckJumpLimit(Lexer->Program[line].LineInFile); \
CurrentFileIndex = AddressFileIndex;

void Evaluate(String_View *FileNames, lexer *Lexer, 
              line_map **LineMappings, size_t *LineCounts, size_t ProgramLength, int Flags) {
    int ACC = 0; // accumulator
    int IX = 0; // index register
    int LastCompareResult = 0;
    size_t ReturnAddress = 0;
    int CurrentFileIndex = Lexer->StartSymbol->FileIndex;
    int PreviousFileIndex = Lexer->StartSymbol->FileIndex;
    
    for(size_t line = Lexer->StartLOC;
        line < ProgramLength;
        line++)
    {
        line_of_code *LOC = Lexer->Program + line;
        int AddressFileIndex = CurrentFileIndex;
        
        switch(LOC->Opcode)
        {
            case IOP_LDM:
            {
                ACC = LOC->Operand;
            } break;
            
            case IOP_LDD:
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    ACC = *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    ACC = *LineMappings[AddressFileIndex][LOC->Operand].Data;
                }
            } break;
            
            case IOP_LDI:
            {
                size_t AddressToAddress;
                if(LOC->Label) {
                    AddressToAddress = LOC->Label->LineRef;
                    AddressFileIndex = LOC->Label->FileIndex;
                }
                else {
                    AddressToAddress = LOC->Operand;
                    CheckAddress(LOC->LineInFile, AddressToAddress, "");
                }
                CheckDataInAddress(LOC->LineInFile, AddressToAddress, "");
                
                size_t Address = *LineMappings[AddressFileIndex][AddressToAddress].Data;
                CheckAddress(LOC->LineInFile, Address, 
                             "\nAddress %zd (from data in address %zd in file "SV_Fmt") not in program",
                             Address, AddressToAddress, SV_Arg(FileNames[AddressFileIndex]));
                CheckDataInAddress(LOC->LineInFile, Address,
                                   "\nNOTE: Remember LDI is for indirect addressing");
                
                ACC = *LineMappings[AddressFileIndex][Address].Data;
            } break;
            
            case IOP_LDX:
            {
                size_t Address = IX;
                if(LOC->Label) {
                    Address += LOC->Label->LineRef;
                    AddressFileIndex = LOC->Label->FileIndex;
                }
                else {
                    Address += LOC->Operand;
                }
                
                CheckAddress(LOC->LineInFile, Address, 
                             "\nNOTE: Address is %zd (%zd + IX) in file "SV_Fmt, Address, Address - IX,
                             SV_Arg(FileNames[AddressFileIndex]));
                CheckDataInAddress(LOC->LineInFile, Address, 
                                   "\nNOTE: Remember LDX is for indexed addressing.\n"
                                   "So the address is %zd + IX", Address - IX);
                
                ACC = *LineMappings[AddressFileIndex][Address].Data;
            } break;
            
            case IOP_LDR:
            {
                IX = LOC->Operand;
            } break;
            
            case IOP_STO:
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckStoreDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data = ACC;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckStoreDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    *LineMappings[AddressFileIndex][LOC->Operand].Data = ACC;
                }
            } break;
            
            case IOP_STX:
            {
                size_t Address = IX;
                if(LOC->Label) {
                    Address += LOC->Label->LineRef;
                    AddressFileIndex = LOC->Label->FileIndex;
                }
                else {
                    Address += LOC->Operand;
                }
                
                CheckAddress(LOC->LineInFile, Address, 
                             "\nNOTE: Address is %zd (%zd + IX) in file "SV_Fmt, 
                             Address, Address - IX, SV_Arg(FileNames[AddressFileIndex]));
                CheckStoreDataInAddress(LOC->LineInFile, Address, 
                                        "\nNOTE: Remember STX is for indexed addressing.\n"
                                        "So the address is %zd + IX", Address - IX);
                
                *LineMappings[AddressFileIndex][Address].Data = ACC;
            } break;
            
            case IOP_STI:
            {
                size_t AddressToAddress;
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    AddressToAddress = LOC->Label->LineRef;
                }
                else {
                    AddressToAddress = LOC->Operand;
                }
                CheckAddress(LOC->LineInFile, AddressToAddress, "");
                CheckDataInAddress(LOC->LineInFile, AddressToAddress, "");
                
                size_t Address = *LineMappings[AddressFileIndex][AddressToAddress].Data;
                CheckAddress(LOC->LineInFile, Address, 
                             "\nAddress %zd (from data in address %zd) not in program",
                             Address, AddressToAddress);
                CheckStoreDataInAddress(LOC->LineInFile, Address,
                                        "\nNOTE: Remember LDI is for indirect addressing");
                
                *LineMappings[AddressFileIndex][AddressToAddress].Data = ACC;
            } break;
            
            case IOP_ADD:
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    ACC += *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    ACC += *LineMappings[AddressFileIndex][LOC->Operand].Data;
                }
            } break;
            
            case IOP_JMP:
            {
                JumpToLine();
            } break;
            
            case IOP_CMP: // immediate + direct
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    
                    LastCompareResult = ACC == *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data;
                }
                else if(LOC->Immediate) {
                    LastCompareResult = ACC == LOC->Operand;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    LastCompareResult = ACC == *LineMappings[AddressFileIndex][LOC->Operand].Data;
                }
            } break;
            
            case IOP_JPE:
            {
                if(LastCompareResult) JumpToLine();
            } break;
            
            case IOP_JPN:
            {
                if(!LastCompareResult) JumpToLine();
            } break;
            
            case IOP_INP:
            {
                ACC = (int)getchar();
            } break;
            
            case IOP_OUT:
            {
                if(IsSet(Flags, PRINT_NUMBERS)) {
                    printf("%d\n", ACC);
                }
                else {
                    putchar((char)ACC);
                }
            } break;
            
            case IOP_AND: // immediate + direct
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    
                    ACC = ACC & *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data;
                }
                else if(LOC->Immediate) {
                    ACC = ACC & LOC->Operand;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    ACC = ACC & *LineMappings[AddressFileIndex][LOC->Operand].Data;
                }
            } break;
            
            case IOP_XOR: // immediate + direct
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    
                    ACC = ACC ^ *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data;
                }
                else if(LOC->Immediate) {
                    ACC = ACC ^ LOC->Operand;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    ACC = ACC ^ *LineMappings[AddressFileIndex][LOC->Operand].Data;
                }
            } break;
            
            case IOP_OR: // immediate + direct
            {
                if(LOC->Label) {
                    AddressFileIndex = LOC->Label->FileIndex;
                    CheckDataInAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                    
                    ACC = ACC | *LineMappings[AddressFileIndex][LOC->Label->LineRef].Data;
                }
                else if(LOC->Immediate) {
                    ACC = ACC | LOC->Operand;
                }
                else {
                    CheckAddress(LOC->LineInFile, LOC->Operand, "");
                    CheckDataInAddress(LOC->LineInFile, (size_t)LOC->Operand, "");
                    
                    ACC = ACC | *LineMappings[AddressFileIndex][LOC->Operand].Data;
                }
            } break;
            
            case IOP_LSL:
            {
                ACC = ACC << LOC->Operand;
            } break;
            
            case IOP_LSR:
            {
                ACC = ACC >> LOC->Operand;
            } break;
            
            // NOTE(vic): Exit loop
            case IOP_END: line = ProgramLength; break;
            
            case IOP_ACCINC: ACC++; break;
            case IOP_ACCDEC: ACC--; break;
            case IOP_IXINC: IX++; break;
            case IOP_IXDEC: IX--; break;
            
            case IOP_CALL:
            {
                AddressFileIndex = LOC->Label->FileIndex;
                
                CheckJumpToAddress(LOC->LineInFile, LOC->Label->LineRef, "");
                ReturnAddress = line;
                line = LineMappings[AddressFileIndex][LOC->Label->LineRef].LOC - 1;
                PreviousFileIndex = CurrentFileIndex;
                CurrentFileIndex = AddressFileIndex;
                
                CheckJumpLimit(Lexer->Program[line].LineInFile);
            } break;
            
            case IOP_RETURN:
            {
                line = ReturnAddress;
                CurrentFileIndex = PreviousFileIndex;
            } break;
            
            // case IOP_JMI: // indirect jump
            
            case IOP_COUNT:
            default:
            {
                assert(0 && "This shouldn't happen");
            } break;
        }
    }
}

int main(int argc, char **args)
{
    if(argc == 1) {
        Usage();
        return 0;
    }
    
    // NOTE(vic): Parse command line options
    char **Files = malloc(argc*sizeof(char *));
    memset(Files, 0, argc*sizeof(char *));
    int FileCount = 0;
    int Flags = 0;
    for(int i = 1; i < argc; i++)
    {
        if(args[i][0] == '-') {
            // flags here!
            String_View flag = sv_from_cstr(&args[i][1]);
            if(sv_eq_ignorecase(flag, SV("no-jmp-limits"))) {
                Flags |= NO_JMP_LIMIT;
            }
            else if(sv_eq_ignorecase(flag, SV("print-numbers"))) {
                Flags |= PRINT_NUMBERS;
            }
            else if(sv_eq_ignorecase(flag, SV("extra"))) {
                Flags |= ALA_EXTRA;
            }
            else {
                fprintf(stderr, "WARNING: Unknown flag '%s' ignored\n", args[i] + 1);
            }
        }
        else if(access(args[i], F_OK) == 0) {
            Files[FileCount++] = args[i];
        }
        else {
            fprintf(stderr, "ERROR: Could not open file '%s'\n", args[i]);
        }
    }
    
    // TODO(vic): Handle no accessable files
    if(FileCount == 0) {
        fprintf(stderr, "ERROR: No input files found\n");
        exit(1);
    }
    
#if 0
    for(int i = 0; i < argc; i++) printf("%s\n", args[i]);
    for(int i = 0; i < FileCount; i++) printf("%s\n", Files[i]);
    printf("%d", FileCount);
#endif
    
    memory_arena _Arena;
    memory_arena *Arena = &_Arena;
    size_t MemorySize = ARENA_MEMORY_SIZE;
    void *Memory = malloc(MemorySize);
    memset(Memory, 0, MemorySize);
    InitializeArena(Arena, MemorySize, (u8 *)Memory);
    
    String_View *InputData = PushArray(&Arena, FileCount, String_View);
    int InputDataCount = 0;
    String_View *ValidFiles = PushArray(&Arena, FileCount, String_View);
    for(int i = 0; i < FileCount; i++)
    {
        String_View *FileData = InputData + InputDataCount;
        *FileData = sv_ReadEntireFile(Files[i]);
        if(FileData->data) {
            ValidFiles[InputDataCount] = sv_from_cstr(Files[i]);
            InputDataCount++;
        }
        else {
            fprintf(stderr, "ERROR: Could not read file %s: %s\n", Files[i], strerror(errno));
        }
    }
    
    size_t TotalLineCount = 0;
    size_t *LineCount = PushArray(&Arena, InputDataCount, size_t);
    for(int FileIndex = 0; FileIndex < InputDataCount; FileIndex++)
    {
        LineCount[FileIndex] = 1;
        for(size_t i = 0; i < InputData[FileIndex].count; i++) {
            if(InputData[FileIndex].data[i] == '\n') LineCount[FileIndex]++;
        }
        TotalLineCount += LineCount[FileIndex];
    }
    
    tmp_cstr tc;
    tc.Capacity = 1024,
    tc.Cstr = (char *)malloc(1024);
    
    line_map **LineMappings = PushArray(&Arena, InputDataCount, line_map *);
    for(int i = 0; i < InputDataCount; i++)
        LineMappings[i] = PushArray(&Arena, LineCount[i], line_map);
    line_of_code *Program = PushArray(&Arena, TotalLineCount, line_of_code);
    symbol_table *SymbolTable = PushStruct(&Arena, symbol_table);
    
    lexer Lexer = {
        .Arena = &Arena,
        .tc = &tc,
        .Program = Program,
        .StartSymbol = 0,
        .StartLOC = 0,
        .SymbolTable = SymbolTable,
        .CurrentSymbolTable = &SymbolTable,
    };
    
    size_t LOCCount = 0;
    for(int FileIndex = 0; FileIndex < InputDataCount; FileIndex++)
    {
        Lexer.FileIndex = FileIndex;
        Lexer.File = ValidFiles + FileIndex;
        LOCCount = ParseCode(InputData[FileIndex], &Lexer, Flags,
                             LineMappings[FileIndex], LOCCount);
    }
    
    if(Lexer.StartLOC == -1) {
        fprintf(stderr, "ERROR: Start label not found\n");
        exit(1);
    }
    
    for(symbol_table *Table = Lexer.SymbolTable;
        Table;
        Table = Table->Next)
    {
        for(int i = 0; i < Table->Used; i++)
        {
            symbol *Symbol = Table->Symbols + i;
            if(!Table->Symbols[i].Evaluated) {
                fprintf(stderr, "%s(%zu): ERROR: Undefined label '"SV_Fmt"'\n",
                        Files[0], Symbol->LineRef, SV_Arg(Symbol->Name));
                exit(1);
            }
        }
    }
    
    //printf("Program starting point: %zu", Lexer.Program[Lexer.StartLOC].LineInFile);
    
    Evaluate(ValidFiles, &Lexer, LineMappings, LineCount, LOCCount, Flags);
    
    return 0;
}