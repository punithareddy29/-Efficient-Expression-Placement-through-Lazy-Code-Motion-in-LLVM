// test.c
int bar(int a, int b) {
    // Expression 0: a + b
    int x = a + b;
    int y;

    if (a > 10) {
        // Expression 0: a + b (computed again)
        y = a + b;
        // Expression 1: a - b
        x = a - b; // x is overwritten
    } else {
        // Expression 2: a * b
        y = a * b;
    }
    // Expression 0: a + b (computed again)
    int z = a + b;
    // Expression 3: x * y (operands x and y depend on path)
    return x * y;
}

