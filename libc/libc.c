// Prints one byte to standard output; implemented in crt0.s.
int putchar(int c);

// Prints the decimal digits of n, sign included, and returns n.
int putd(int n)
{
    if (n < 0) {
        putchar('-');
        return putd(-n);
    }
    if (n >= 10) {
        putd(n / 10);
    }
    putchar(n % 10 + '0');
    return n;
}
