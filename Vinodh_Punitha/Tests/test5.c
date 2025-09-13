int complex_cfg(int a, int b, int c) {
    int x, y;
    // The expression 'b + c' is computed on two paths (Path 1, Path 2).
    if (a > 0) {
        x = b + c; // Path 1
        y = a * 2;
    } else {
        if (b > 0) {
            x = b + c; // Path 2
            y = a * 3;
        } else {
            x = b - c; // Path 3 (different expression)
            y = a * 4;
        }
    }
    // Merge point
    // This computation of 'b + c' should be redundant if Path 1 or Path 2 was taken.
    // LCM should hoist 'b + c' before the first 'if' and replace uses in Path 1, Path 2, AND the computation of 'z'.
    int z = b + c;
    return x + y + z;
}
