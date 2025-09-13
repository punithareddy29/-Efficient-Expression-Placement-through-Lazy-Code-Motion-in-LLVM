// test_complex_cfg.c
int test_complex_cfg(int a, int b, int c, int d) {
    int x = a + b; // Initial computation
    int y;
    if (a > 10) {
        // Branch 1
        y = c + d;
        if (b > 5) {
             x = x * 2; // Reuses x = a+b result
             y = y + (a + b); // Redundant a+b
        } else {
             x = c - d;
             y = y - (a + b); // Redundant a+b
        }
    } else {
        // Branch 2
        y = a - b;
        if (c > 0) {
             x = x + 5; // Reuses x = a+b result
             y = y * (a + b); // Redundant a+b
        } else {
             x = (a + b) * 3; // Redundant a+b
             y = y / 2;
        }
    }
    // Merge point
    int z = a + b; // Redundant a+b
    return x + y + z;
}
