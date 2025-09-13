// test_critical_edge.c
int test_critical_edge_trigger(int a, int b, int c, int p_cond, int q_cond) {
    int x = c; // Value potentially carried from the 'else' path

    // Block 'P' region starts conceptually
    if (p_cond > 0) {
        // Block 'IfBody' region
        // Compute 'a+b' only on this path before the potential merge
        x = a + b;

        // Make Block 'P' have multiple successor paths:
        // One path exits early, the other continues to the merge ('Join').
        if (q_cond > 10) {
             // Path 'IfBody' -> 'Exit'
             return x; // Early exit
        }
        // else: Path 'IfBody' -> 'Join' (fallthrough)
    }
    // else block implicitly follows Path 'Entry'/'P' -> 'Join'

    // Block 'Join':
    // Predecessors: 'IfBody' (if q_cond <= 10) AND the implicit 'else' path from 'Entry'/'P'.
    // --> 'Join' has multiple predecessors.

    // Edge 'IfBody' -> 'Join':
    // Predecessor 'IfBody': Has multiple logical successors ('Exit' and 'Join').
    // Successor 'Join': Has multiple predecessors ('IfBody' and 'Entry'/'P').
    // --> This edge should be critical.

    // Use 'a+b' after the join point.
    // Heuristic check at 'Join' for 'a+b':
    // - ANTIC_IN[Join] should be true (used below).
    // - AVAIL_IN[Join] should be false (only computed on 'IfBody' path).
    // --> Heuristic (ANTIC && !AVAIL) should be true.
    int y = a + b;

    return x + y; // Use x to keep it potentially alive
}
