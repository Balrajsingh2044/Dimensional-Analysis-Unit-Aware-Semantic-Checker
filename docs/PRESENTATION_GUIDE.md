# Presentation & Demo Guide

## For Shreyansh (Slides & Demo)
- **The Hook:** Start with the Mars Climate Orbiter disaster. 
- **The Climax:** Run `./dimc tests/invalid/04_metric_imperial.dim`. Show the custom error: *"same dimension but different unit size — metric vs imperial?"*.
- **The Magic:** Run `./output/dimc demo/mars_valid.dim`. Show the output: *"Our compiler reduces complex physics equations down to just 3 lines of pure, optimized C."*
- **The Answer (bonus flag):** Run `./output/dimc -p demo/mars_valid.dim` — one extra line appears in the generated `main()`: `printf("%g\n", impulse);`. Compile it and it prints **4500** — *"the compiler not only protects the calculation, it hands you the answer."*
- **The QA Flex:** Mention on the testing slide: *"We built a portable heap-auditor (`make audit`) and verified 0 memory leaks in the happy path and codegen."*

## For Ayansh (Q&A Prep)
- **Question:** "How does the semantic analyzer work?"
  **Answer:** "It does a bottom-up AST traversal. Every node gets a 7-D UnitVec representing [m, kg, s, A, K, cd, mol]. Addition/subtraction require exact vector equality. Multiplication/division use vector algebra."
- **Question:** "How do you handle metric vs imperial?"
  **Answer:** "We added a `double scale` field to the AST and Symbol Table. `meters` has scale 1.0, `feet` has 0.3048. If dimensions match but scales differ, we throw a compile-time error."
- **Question:** "Why is there a memory leak on parse errors?"
  **Answer:** "Bison 2.4.1 has a known issue where it frees values consumed by actions. Adding destructors causes use-after-free crashes. We chose compiler stability over cleaning up 343 bytes on a fatal error path."
