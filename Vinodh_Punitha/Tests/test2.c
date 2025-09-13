int foo(int a, int b, int c) {
    int x = a + b; // Should not be moved (used locally before branches)
    int y;
    int z;

    if (c > 0) {
        y = a * b; // Redundant computation 1
        z = x + 1;
    } else {
        y = a * b; // Redundant computation 2
        z = x - 1;
    }

    // Use of y and z from branches
    int result = y + z;
    return result;
}
