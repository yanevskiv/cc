#ifndef TXT_X86_64_H
#define TXT_X86_64_H

#include <stdio.h>
#include "arch/x86_64/asm.h"

// AT&T syntax writer
void Txt_x86_64_Att_WriteOperand(FILE *out, const Asm_x86_64_Operand *op);
void Txt_x86_64_Att_WriteInstr(FILE *out, const Asm_x86_64_Item *item);
void Txt_x86_64_Att_Write(FILE *out);

// Name-to-value lookups, the inverse of the writer's tables
int Txt_x86_64_RegByName(const char *name, int *width);
int Txt_x86_64_OpByName(const char *name);

// AT&T syntax parser
int  Txt_x86_64_Att_ParseOperand(const char *text, Asm_x86_64_Operand *op);
void Txt_x86_64_Att_EmitInts(const char *args, int width);
void Txt_x86_64_Att_EmitString(const char *args, int terminate);
void Txt_x86_64_Att_ParseInstr(const char *line);
void Txt_x86_64_Att_ParseDirective(const char *line);
void Txt_x86_64_Att_ParseLine(char *line);
void Txt_x86_64_Att_Parse(const char *text);

#endif // TXT_X86_64_H
