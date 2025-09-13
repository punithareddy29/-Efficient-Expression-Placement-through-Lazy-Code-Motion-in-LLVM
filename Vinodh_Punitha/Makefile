# Makefile to test individual dataflow analysis passes and LCM

# --- Configuration ---
# Use the LLVM 17 tools you installed
LLVM_CONFIG = llvm-config-17
CLANG = clang-17
OPT = opt-17
CXX = clang++-17

# Get flags from llvm-config-17
CXXFLAGS = -fPIC -g -O0 -std=c++17 -stdlib=libc++ $(shell $(LLVM_CONFIG) --cxxflags) $(INC)
LDFLAGS = -stdlib=libc++ $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs core analysis transformutils) -Wl,--exclude-libs,ALL

# --- Prerequisites ---

# Default target: Build the necessary components but don't run any pass
all: UnifiedPass.so test.ll
	@echo "Prerequisites built. Use 'make test-<passname>' to run an analysis."
	@echo "Example: 'make test-available' or 'make test-lcm'"

# Build the pass shared library
UnifiedPass.so: unifiedpass.o
	$(CXX) -shared $^ -o $@ $(LDFLAGS)

# Compile the pass source code
unifiedpass.o: unifiedpass.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile the test C code to LLVM IR (.ll file)
# Use -O0 to minimize preliminary optimizations by clang
# Use -Xclang -disable-llvm-passes to ensure clang doesn't run its own passes
test.ll: test.c
	$(CLANG) -S -emit-llvm -O0 -Xclang -disable-llvm-passes $< -o $@

# --- Targets for Individual Analysis Passes ---
# These run the pass and print its output to the console

test-available: UnifiedPass.so test.ll
	@echo "\n--- Running AvailableExpressions ---"
	$(OPT) -load-pass-plugin ./UnifiedPass.so -passes=available -disable-output < test.ll
	@echo "------------------------------------"

test-anticip: UnifiedPass.so test.ll
	@echo "\n--- Running AnticipatedExpressions ---"
	$(OPT) -load-pass-plugin ./UnifiedPass.so -passes=anticip -disable-output < test.ll
	@echo "--------------------------------------"

test-postpon: UnifiedPass.so test.ll
	@echo "\n--- Running PostponableExpressions ---"
	$(OPT) -load-pass-plugin ./UnifiedPass.so -passes=postpon -disable-output < test.ll
	@echo "--------------------------------------"

test-used: UnifiedPass.so test.ll
	@echo "\n--- Running UsedExpressions ---"
	$(OPT) -load-pass-plugin ./UnifiedPass.so -passes=used -disable-output < test.ll
	@echo "-------------------------------"

# --- Target for Lazy Code Motion Transformation ---

# Run the LCM optimization pass on the LLVM IR
# Use -S to output human-readable LLVM assembly
test.lcm.ll: test.ll UnifiedPass.so
	$(OPT) -S -load-pass-plugin ./UnifiedPass.so -passes=lazycodemotion $< -o $@

# Target to explicitly run LCM and generate the output file
test-lcm: test.lcm.ll
	@echo "Lazy Code Motion pass run. Output IR is in test.lcm.ll"

# --- Targets for Viewing IR ---

# Target to just view the original LLVM IR
view-orig: test.ll
	@echo "\n--- Original LLVM IR (test.ll) ---"
	@cat test.ll
	@echo "----------------------------------"

# Target to just view the LCM-optimized LLVM IR
view-lcm: test.lcm.ll
	@echo "\n--- Optimized LLVM IR (test.lcm.ll) ---"
	@cat test.lcm.ll
	@echo "---------------------------------------"

# Clean up generated files
clean:
	rm -f unifiedpass.o UnifiedPass.so test.ll test.lcm.ll *~

.PHONY: all clean view-orig view-lcm test-available test-anticip test-postpon test-used test-lcm

