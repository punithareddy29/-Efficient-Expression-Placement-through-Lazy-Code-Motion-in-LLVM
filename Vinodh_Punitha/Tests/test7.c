int test7_o0_vs_o1_v2(int a, int b, int cond1, int cond2) {
    int val;
    if (cond1 > 0) {
        if (cond2 > 5) {
            val = a + b;
        } else {
            val = a - b;
        }
    } else {
        val = a * b;
    }
    return val + 100;
}
