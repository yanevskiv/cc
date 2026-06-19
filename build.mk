# build/Makefile - standalone recipe shipped inside the staged build/ tree.
#
# It compiles every C program under user/ into a native executable sitting
# right next to its source:  user/fizzbuzz.c  ->  user/fizzbuzz.
#
# Per-program pipeline (our compiler only emits assembly, so the system
# assembler and linker finish the job):
#
#     .c  --(bin/cc)-->  .s  --(as)-->  .o  --(gcc)-->  executable
#
# CC: our compiler (.c -> .s).  AS: GNU assembler (.s -> .o).
# LD: links the object against the crt startup files + libc -> executable.
CC  := bin/cc
AS  := as
LD  := gcc

CFLAGS  :=
ASFLAGS :=
LDFLAGS :=

# One executable per C source under user/.
SRCS := $(wildcard user/*.c)
BINS := $(SRCS:.c=)
ASMS := $(SRCS:.c=.s)
OBJS := $(SRCS:.c=.o)

# Drop the built-in suffix rules.  They would invoke $(CC) the way a real C
# compiler is invoked (e.g. `-c` to produce an object file), but bin/cc only
# emits assembly, so we drive every step with explicit pattern rules below.
.SUFFIXES:

# Keep the intermediate .s/.o files around for inspection instead of letting
# make delete them as chained-rule by-products.
.SECONDARY:

.PHONY: all run clean

all: $(BINS)

# .c -> .s with our compiler.
user/%.s: user/%.c $(CC)
	$(CC) $(CFLAGS) -S -o $@ $<

# .s -> .o with GNU as.
user/%.o: user/%.s
	$(AS) $(ASFLAGS) -o $@ $<

# .o -> executable.
user/%: user/%.o
	$(LD) $(LDFLAGS) -o $@ $<

# Build every program and run each one in turn.
run: all
	@for bin in $(BINS); do \
		echo "=== $$bin ==="; \
		"./$$bin"; \
	done

clean:
	rm -f $(ASMS) $(OBJS) $(BINS)
