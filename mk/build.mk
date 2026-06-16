# build/Makefile - drives our compiler over the bundled test program.
#
# This file is copied verbatim from mk/build.mk into build/ by the top-level
# Makefile.  Running `make` here compiles test.c with the freshly built ./cc.

CC  := ./cc
SRC := test.c
BIN := test

.PHONY: all run clean

all: $(BIN)

$(BIN): $(CC) $(SRC)
	$(CC) $(SRC) -o $(BIN)

# Compile and immediately run the result.
run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
