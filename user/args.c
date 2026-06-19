/*
  Description: Pass more than six arguments (System V ABI spills to the stack)
  Compile:     ./cc args.c -o args
  Run:         ./args
 */
int printf(const char *fmt, ...);

// The first six arguments arrive in registers; g and h come off the stack.
int sum(int a, int b, int c, int d, int e, int f, int g, int h)
{
    return a + b + c + d + e + f + g + h;
}

int main(void)
{
    printf("%d\n", sum(1, 2, 3, 4, 5, 6, 7, 8));
    return 0;
}
