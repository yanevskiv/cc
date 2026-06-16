/* test.c - exercises the language subset that `cc` understands:
 * prototypes, locals, for-loops, if/else-if chains, %, comparisons and calls. */

int printf(const char *fmt, ...);

int main(void)
{
    int i;

    printf("hello, world\n");

    for (i = 1; i <= 20; i = i + 1) {
        if (i % 15 == 0)
            printf("FizzBuzz\n");
        else if (i % 3 == 0)
            printf("Fizz\n");
        else if (i % 5 == 0)
            printf("Buzz\n");
        else
            printf("%d\n", i);
    }

    return 0;
}
