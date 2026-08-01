  # _start: call main, then exit with its return value.
  .text
  .globl _start
_start:
  call main
  mov %rax, %rdi
  mov $60, %rax
  syscall

  # putchar(c): write one byte to fd 1, return the byte written.
  .globl putchar
putchar:
  push %rbp
  mov %rsp, %rbp
  sub $8, %rsp
  mov %rdi, -8(%rbp)
  mov $1, %rax
  mov $1, %rdi
  lea -8(%rbp), %rsi
  mov $1, %rdx
  syscall
  mov -8(%rbp), %rax
  mov %rbp, %rsp
  pop %rbp
  ret
