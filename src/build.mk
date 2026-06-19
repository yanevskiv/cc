# CC: our compiler (.c -> .s).  AS: GNU assembler (.s -> .o).
# LD: links the object against the crt startup files + libc -> executable.
CC  := ./cc
AS  := as
LD  := gcc

SRC := test.c
ASM := $(SRC:.c=.s)
OBJ := $(SRC:.c=.o)
BIN := test

.PHONY: all run clean

all: $(BIN)

# .c -> .s with our compiler.
$(ASM): $(SRC) $(CC)
	$(CC) $(SRC) -o $(ASM)

# .s -> .o with GNU as.
$(OBJ): $(ASM)
	$(AS) $(ASM) -o $(OBJ)

# .o -> executable.
$(BIN): $(OBJ)
	$(LD) $(OBJ) -o $(BIN)

# Compile and immediately run the result.
run: $(BIN)
	./$(BIN)

clean:
	rm -f $(ASM) $(OBJ) $(BIN)
