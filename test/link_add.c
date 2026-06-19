/*
  Stage 4 link-core harness: defines add(), referenced by main() in a separate
  object.  Compiled with -c into a plain relocatable object; our linker resolves
  the cross-object reference and returns 40 + 2 = 42 as the program's status.
 */
int add(int a, int b)
{
    return a + b;
}
