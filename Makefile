CC      := gcc
CFLAGS  := -std=gnu11 -O2 -Ih -Iout
WARN    := -Wall -Wextra
LEX     := flex
YACC    := bison

OUT     := out
BUILD   := build

# Tool mains (each provides its own int main()); everything else is shared.
MAIN_SRCS := src/cc.c
ALL_SRCS  := $(shell find src -name '*.c')
LIB_SRCS  := $(filter-out $(MAIN_SRCS),$(ALL_SRCS))
LIB_OBJS  := $(patsubst src/%.c,$(OUT)/%.o,$(LIB_SRCS))
GEN_OBJS  := $(OUT)/lex.yy.o $(OUT)/parser.tab.o

USER_SRCS := $(wildcard user/*.c)

# --- phony recipes ---
all: $(BUILD)/bin/cc $(BUILD)/user $(BUILD)/Makefile

test: all
	$(MAKE) -C $(BUILD) run

clean:
	rm -rf $(BUILD) $(OUT)

# --- tool recipes ---
$(BUILD)/bin/cc: $(OUT)/cc.o $(LIB_OBJS) $(GEN_OBJS) | $(BUILD)/bin
	$(CC) $(CFLAGS) $(WARN) $^ -o $@

# --- front-end generators ---
$(OUT)/parser.tab.c $(OUT)/parser.tab.h: src/ast/parser.y | $(OUT)
	$(YACC) -d -o $(OUT)/parser.tab.c $<

$(OUT)/lex.yy.c: src/ast/lexer.flex $(OUT)/parser.tab.h | $(OUT)
	$(LEX) -o $@ $<

$(OUT)/lex.yy.o: $(OUT)/lex.yy.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/parser.tab.o: $(OUT)/parser.tab.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- objects (mirrors the src/ tree under out/) ---
$(OUT)/%.o: src/%.c $(OUT)/parser.tab.h | $(OUT)
	@mkdir -p $(dir $@)
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
