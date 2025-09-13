// test_loop_invariant.c
int test_loop_invariant(int a, int b, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        // a + b is loop invariant
        int invariant_expr = a + b;
        sum += invariant_expr * i;
    }
    // Expected: 'a + b' computation hoisted before the loop.
    return sum;
}

// Simpler variant if the above gives issues with loop structure analysis:
int test_loop_invariant_simple(int a, int b, int n) {
    int x = a + b; // Loop invariant part
    int res = 0;
    int i = 0;
    while (i < n) {
        res += x; // Use invariant
        i++;
    }
    return res;
}
