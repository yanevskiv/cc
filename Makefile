TARGET_ARCH := x86_64

CC      := gcc
CFLAGS  := -std=gnu11 -O2 -Ih -Iout -DTARGET_ARCH=$(TARGET_ARCH)
WARN    := -Wall -Wextra
LEX     := flex
YACC    := bison

OUT     := out
BUILD   := build

# Tool mains (each provides its own int main()); everything else is shared.
MAIN_SRCS := src/cc.c src/ld.c
ALL_SRCS  := $(shell find src -name '*.c')
LIB_SRCS  := $(filter-out $(MAIN_SRCS),$(ALL_SRCS))
LIB_OBJS  := $(patsubst src/%.c,$(OUT)/%.o,$(LIB_SRCS))
GEN_OBJS  := $(OUT)/lex.yy.o $(OUT)/parser.tab.o

# cc needs the front end (lexer/parser/AST), code generator and ELF writer;
# ld needs only the self-contained ELF link core.
CC_OBJS := $(OUT)/cc.o $(LIB_OBJS) $(GEN_OBJS)
LD_OBJS := $(OUT)/ld.o $(OUT)/util/elf.o

CC_BIN := $(BUILD)/bin/$(TARGET_ARCH)-cc
LD_BIN := $(BUILD)/bin/$(TARGET_ARCH)-ld

USER_SRCS := $(wildcard user/*.c)

# --- phony recipes ---
all: $(CC_BIN) $(LD_BIN) $(BUILD)/user $(BUILD)/Makefile

test: all
	$(MAKE) -C $(BUILD) run

# Stage 5 link test: compile two objects with cc, then exercise our own ld in
# every mode (static link, -r relocatable merge, -place) and check each result
# returns the expected status -- no system ld involved.
stage5: $(CC_BIN) $(LD_BIN) | $(OUT)
	$(CC_BIN) --start-obj test/link_main.c -o $(OUT)/link_main.o
	$(CC_BIN) -c          test/link_add.c  -o $(OUT)/link_add.o
	$(LD_BIN) $(OUT)/link_main.o $(OUT)/link_add.o -o $(OUT)/link_exec
	$(LD_BIN) -r $(OUT)/link_main.o $(OUT)/link_add.o -o $(OUT)/link_merged.o
	$(LD_BIN) $(OUT)/link_merged.o -o $(OUT)/link_relinked
	$(LD_BIN) -place=text@0x8000000 -place=data@0x20000000 \
		$(OUT)/link_main.o $(OUT)/link_add.o -o $(OUT)/link_placed
	@for t in exec relinked placed; do \
		$(OUT)/link_$$t; status=$$?; \
		echo "link_$$t returned $$status (expect 42)"; \
		[ $$status -eq 42 ] || { echo "STAGE 5 FAILED ($$t)"; exit 1; }; \
	done
	@echo "STAGE 5 OK: static link, -r round-trip and -place via our own ld"

clean:
	rm -rf $(BUILD) $(OUT)

# --- tool recipes ---
$(CC_BIN): $(CC_OBJS) | $(BUILD)/bin
	$(CC) $(CFLAGS) $(WARN) $^ -o $@

$(LD_BIN): $(LD_OBJS) | $(BUILD)/bin
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

.PHONY: all clean test stage5
