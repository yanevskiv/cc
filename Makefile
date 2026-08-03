TARGET_ARCH := x86_64

OUT     := out
BUILD   := build

CRT_OBJ  := $(BUILD)/lib/crt0.o
LIBC_OBJ := $(BUILD)/lib/libc.o

CC      := gcc
CFLAGS  := -std=gnu99 -O2 -Ih -Iout -DTARGET_ARCH=$(TARGET_ARCH) \
	-DCRT_PATH='"$(abspath $(CRT_OBJ))"' -DLIBC_PATH='"$(abspath $(LIBC_OBJ))"'
WARN    := -Wall -Wextra
LEX     := flex
YACC    := bison

# Tool mains (each provides its own int main()); everything else is shared.
MAIN_SRCS := src/cc.c src/ld.c src/as.c
ALL_SRCS  := $(shell find src -name '*.c')
LIB_SRCS  := $(filter-out $(MAIN_SRCS),$(ALL_SRCS))
LIB_OBJS  := $(patsubst src/%.c,$(OUT)/%.o,$(LIB_SRCS))
GEN_OBJS  := $(OUT)/lex.yy.o $(OUT)/parser.tab.o

# cc needs the front end (lexer/parser/AST), code generator and ELF writer;
# as needs the textual parser plus the encoder and ELF writer;
# ld needs the ELF library plus the link/rel policy layer.
CC_OBJS := $(OUT)/cc.o $(LIB_OBJS) $(GEN_OBJS)
AS_OBJS := $(OUT)/as.o $(OUT)/util/file.o $(OUT)/util/str.o $(OUT)/util/elf.o \
	$(OUT)/arch/$(TARGET_ARCH)/txt.o $(OUT)/arch/$(TARGET_ARCH)/asm.o \
	$(OUT)/arch/$(TARGET_ARCH)/enc.o
LD_OBJS := $(OUT)/ld.o $(OUT)/util/elf.o $(OUT)/util/link.o $(OUT)/arch/$(TARGET_ARCH)/rel.o

CC_BIN := $(BUILD)/bin/$(TARGET_ARCH)-cc
AS_BIN := $(BUILD)/bin/$(TARGET_ARCH)-as
LD_BIN := $(BUILD)/bin/$(TARGET_ARCH)-ld

USER_SRCS := $(wildcard user/*.c)

# --- phony recipes ---
all: $(CC_BIN) $(AS_BIN) $(LD_BIN) $(CRT_OBJ) $(LIBC_OBJ) $(BUILD)/user $(BUILD)/Makefile

test: all
	$(MAKE) -C $(BUILD) run

clean:
	rm -rf $(BUILD) $(OUT)

# --- tool recipes ---
$(CC_BIN): $(CC_OBJS) | $(BUILD)/bin
	$(CC) $(CFLAGS) $(WARN) $^ -o $@

$(AS_BIN): $(AS_OBJS) | $(BUILD)/bin
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

# --- runtime (libc/) recipes ---
$(CRT_OBJ): libc/arch/$(TARGET_ARCH)/crt0.s $(AS_BIN) | $(BUILD)/lib
	$(AS_BIN) $< -o $@

$(LIBC_OBJ): libc/libc.c $(CC_BIN) | $(BUILD)/lib
	$(CC_BIN) -c $< -o $@

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

$(BUILD)/lib: | $(BUILD)
	mkdir -p $(BUILD)/lib

.PHONY: all clean test
