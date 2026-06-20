#ifndef TXT_X86_64_H
#define TXT_X86_64_H

#include <stdio.h>
#include "arch/x86_64/asm.h"

// AT&T syntax
void Txt_x86_64_Att_WriteOperand(FILE *out, const Asm_x86_64_Operand *op);
void Txt_x86_64_Att_WriteInstr(FILE *out, const Asm_x86_64_Item *item);
void Txt_x86_64_Att_Write(FILE *out);

#endif // TXT_X86_64_H
