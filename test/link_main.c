/*
  Stage 4 link-core harness: defines main, which calls add() defined in a
  separate object.  Compiled with --start-obj so the object also carries the
  _start runtime that our linker resolves as the entry point.
 */
int add(int a, int b);

int main(void)
{
    return add(40, 2);
}
