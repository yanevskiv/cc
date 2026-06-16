# Top-level Makefile for the `cc` compiler.
#
# Layout:
#   lex/    flex (.flex) and bison (.y) grammar sources
#   src/    compiler C sources       h/   compiler C headers
#   test/   sample program to compile (test.c)
#   mk/     build.mk, which becomes build/Makefile
#   out/    intermediate artifacts: generated scanner/parser + object files
#   build/  the deliverables only: cc, test.c, Makefile
#
# `make` produces, in build/:
#   cc          our compiler
#   test.c      a copy of the sample program
#   Makefile    a copy of mk/build.mk
#
# so that `make -C build` compiles test.c with ./cc.  Everything else
# (lex.yy.c, parser.tab.c/.h, *.o) is kept out of the way in out/.

CC      := gcc
CFLAGS  := -std=gnu11 -O2 -Ih -Iout
WARN    := -Wall -Wextra
LEX     := flex
YACC    := bison

OUT     := out
BUILD   := build

SRCS    := $(wildcard src/*.c)
OBJS    := $(patsubst src/%.c,$(OUT)/%.o,$(SRCS))
GEN_OBJS:= $(OUT)/lex.yy.o $(OUT)/parser.tab.o

.PHONY: all clean test

all: $(BUILD)/cc $(BUILD)/test.c $(BUILD)/Makefile

# --- the compiler binary (the only build artifact that links objects) ------
$(BUILD)/cc: $(OBJS) $(GEN_OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $(WARN) $^ -o $@

# --- generated scanner and parser (live in out/) ---------------------------
# bison emits the header that both the scanner and the C sources include.
$(OUT)/parser.tab.c $(OUT)/parser.tab.h: lex/parser.y | $(OUT)
	$(YACC) -d -o $(OUT)/parser.tab.c $<

$(OUT)/lex.yy.c: lex/lexer.flex $(OUT)/parser.tab.h | $(OUT)
	$(LEX) -o $@ $<

# Generated code is compiled without -Wall/-Wextra (flex/bison output is noisy).
$(OUT)/lex.yy.o: $(OUT)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/parser.tab.o: $(OUT)/parser.tab.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- our own sources (objects in out/) -------------------------------------
$(OUT)/%.o: src/%.c $(OUT)/parser.tab.h | $(OUT)
	$(CC) $(CFLAGS) $(WARN) -c $< -o $@

# --- payload copied into build/ --------------------------------------------
$(BUILD)/test.c: test/test.c | $(BUILD)
	cp $< $@

$(BUILD)/Makefile: mk/build.mk | $(BUILD)
	cp $< $@

$(OUT):
	mkdir -p $(OUT)

$(BUILD):
	mkdir -p $(BUILD)

# Build everything, then run the compiler over the sample program.
test: all
	$(MAKE) -C $(BUILD) run

clean:
	rm -rf $(BUILD) $(OUT)
