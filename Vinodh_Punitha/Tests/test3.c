int loop_test(int a, int b, int n) {
    int x = a + b; // Compute outside
    int y = 0;
    // The expression 'a + b' is computed repeatedly inside the loop.
    // LCM should hoist it before the loop starts.
    for (int i = 0; i < n; ++i) {
        y += a + b; // Compute inside loop
    }
    // 'x' uses the initial computation, 'y' uses the loop computations (which should be replaced by a temp)
    return y + x;
}
