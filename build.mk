# build/Makefile - builds every user/*.c into a native executable.
#   .c --(bin/x86_64-cc)--> .s --(as)--> .o --(gcc)--> executable
CC  := bin/x86_64-cc
AS  := as
LD  := gcc

CFLAGS  :=
ASFLAGS :=
LDFLAGS :=

SRCS := $(wildcard user/*.c)
BINS := $(SRCS:.c=)
ASMS := $(SRCS:.c=.s)
OBJS := $(SRCS:.c=.o)

.SUFFIXES:
.SECONDARY:
.PHONY: all run clean

all: $(BINS)

user/%.s: user/%.c $(CC)
	$(CC) $(CFLAGS) -S -o $@ $<

user/%.o: user/%.s
	$(AS) $(ASFLAGS) -o $@ $<

user/%: user/%.o
	$(LD) $(LDFLAGS) -o $@ $<

run: all
	@for bin in $(BINS); do \
		echo "=== $$bin ==="; \
		"./$$bin"; \
		echo "(exit $$?)"; \
	done

clean:
	rm -f $(ASMS) $(OBJS) $(BINS)
