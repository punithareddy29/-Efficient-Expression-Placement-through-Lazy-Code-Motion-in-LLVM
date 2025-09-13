int nested_if_test(int a, int b, int c, int d) {
    int x = 0;
    int y;
    // The expression 'c * d' appears in multiple branches.
    // LCM should identify it as common and hoist it before the outer 'if'.
    if (a > 0) {
        if (b > 0) {
            x = c * d; // Inner computation
        } else {
            x = c + d; // Different expression
        }
        y = c * d; // Outer computation (same branch)
    } else {
        y = c * d; // Other branch computation
        x = c - d; // Different expression
    }
    // 'x' and 'y' depend on the path taken.
    return x + y;
}
