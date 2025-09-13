// test6.c
int diff_test(int a, int b, int c) {
    int x;
    int y = a; // Use 'a' to prevent hoisting 'a+b' too early if 'a' wasn't used otherwise

    // The expression 'a + b' is anticipated before this 'if',
    // but not available. It's earliest in both the 'then' and 'else'
    // blocks, but latest in the entry block according to the LCM algorithm.
    if (c > 0) {
        // a+b is earliest here
        x = a + b;
    } else {
        // a+b is earliest here too
        x = a + b;
    }
    // Use x after the merge
    return x + y;
}

