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

USER_SRCS := $(wildcard user/*.c)

# --- phony recipes ---
all: $(BUILD)/bin/cc $(BUILD)/user $(BUILD)/Makefile

test: all
	$(MAKE) -C $(BUILD) run

clean:
	rm -rf $(BUILD) $(OUT)

# --- compiler recipes ---
$(BUILD)/bin/cc: $(OBJS) $(GEN_OBJS) | $(BUILD)/bin
	$(CC) $(CFLAGS) $(WARN) $^ -o $@

$(OUT)/parser.tab.c $(OUT)/parser.tab.h: src/parser.y | $(OUT)
	$(YACC) -d -o $(OUT)/parser.tab.c $<

$(OUT)/lex.yy.c: src/lexer.flex $(OUT)/parser.tab.h | $(OUT)
	$(LEX) -o $@ $<

$(OUT)/lex.yy.o: $(OUT)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/parser.tab.o: $(OUT)/parser.tab.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/%.o: src/%.c $(OUT)/parser.tab.h | $(OUT)
	$(CC) $(CFLAGS) $(WARN) -c $< -o $@

# --- build/ recipes ---
$(BUILD)/user: $(USER_SRCS) | $(BUILD)
	rm -rf $@
	cp -r user $@

$(BUILD)/Makefile: build.mk | $(BUILD)
	cp $< $@

$(OUT):
	mkdir -p $(OUT)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/bin: | $(BUILD)
	mkdir -p $(BUILD)/bin

.PHONY: all clean test
