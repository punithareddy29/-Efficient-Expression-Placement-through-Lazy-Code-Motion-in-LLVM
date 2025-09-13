---------------------------------------------------------------------------------------------
Environment Setup
---------------------------------------------------------------------------------------------
The development and testing of the LCM pass were conducted in the following environment:
•	Operating System: Ubuntu 22.04.5 LTS (Jammy Jellyfish)
•	LLVM Version: 17.0.6
•	Clang Version: 17.0.6
•	Build System: CMake 3.22.1, Make
•	Host C++ Compiler: g++ (Ubuntu 11.4.0)
•	C++ Standard: C++17
•	Target Architecture: x86_64-pc-linux-gnu
The implementation leverages LLVM's C++ APIs and integrates with its New Pass Manager (NPM) via a dynamically loaded plugin.

Install LLVM 17.0.6 if not installed yet.(follow the instruction as per the (Installing LLVM 17.0.6.txt) file uploaded in the same directory)
In the VM, Move the folder Vinodh_Punitha to the desktop.

# First, go back to the project root directory
cd ~/Desktop/Vinodh_Punitha/

# Remove any existing build directory to start fresh
rm -rf build

# --- Build ---
# Create build directory (if it doesn't exist) and enter it
mkdir -p build
cd build

# Configure using CMake (points to CMakeLists.txt in parent dir)
cmake ..

# Compile the pass
make

# --- Prepare Test IR ---
# Go back to project directory
cd ..

# Use the test.c file
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test.c -o Tests/test.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test.O0.no-optnone.bc -o Tests/test.mem2reg.bc

# Convert the bitcode after mem2reg to readable LLVM IR (.ll)
llvm-dis-17 Tests/test.mem2reg.bc -o Tests/test.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test.mem2reg.bc -o Tests/test.lcm-final.ll

#Detailed process of LCM execution can be viewed on the terminal

#Compare the Results by opening test.mem2reg.ll and test.lcm-final.ll side by side.

===========================================================
To Run LCM-L and LCM-E
===========================================================
For LCM - L ( default, no need to change unifiedpass.cpp )

# Example for Latest mode (useEarliestInsertion = false;)
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test.mem2reg.bc -o Tests/test.lcm-L.ll


# Example for Earliest mode
#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Run LCM-E
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test.mem2reg.bc -o Tests/test.lcm-E.ll

#Compare the Results by opening test.lcm-L.ll and test.lcm-E.ll side by side.

============================================================
To Run Critical Edge Detection
============================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Critical Edge detection on LCM-L
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test_partial_redundancy.c -o Tests/test_partial_redundancy.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test_partial_redundancy.O0.no-optnone.bc -o Tests/test_partial_redundancy.mem2reg.bc
llvm-dis-17 Tests/test_partial_redundancy.mem2reg.bc -o Tests/test_partial_redundancy.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_partial_redundancy.mem2reg.bc -o Tests/test_partial_redundancy.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_partial_redundancy.mem2reg.bc -o Tests/test_partial_redundancy.lcm-E.ll


=================================================================
To run other test cases,

==================================================
Run for test_partial_redundancy.c
==================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test_partial_redundancy.c -o Tests/test_partial_redundancy.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test_partial_redundancy.O0.no-optnone.bc -o Tests/test_partial_redundancy.mem2reg.bc
llvm-dis-17 Tests/test_partial_redundancy.mem2reg.bc -o Tests/test_partial_redundancy.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_partial_redundancy.mem2reg.bc -o Tests/test_partial_redundancy.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_partial_redundancy.mem2reg.bc -o Tests/test_partial_redundancy.lcm-E.ll


====================================
Run for test_complex_cfg.c
====================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test_complex_cfg.c -o Tests/test_complex_cfg.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test_complex_cfg.O0.no-optnone.bc -o Tests/test_complex_cfg.mem2reg.bc
llvm-dis-17 Tests/test_complex_cfg.mem2reg.bc -o Tests/test_complex_cfg.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_complex_cfg.mem2reg.bc -o Tests/test_complex_cfg.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_complex_cfg.mem2reg.bc -o Tests/test_complex_cfg.lcm-E.ll



====================================
Run for test_loop_invariant.c
====================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test_loop_invariant.c -o Tests/test_loop_invariant.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test_loop_invariant.O0.no-optnone.bc -o Tests/test_loop_invariant.mem2reg.bc
llvm-dis-17 Tests/test_loop_invariant.mem2reg.bc -o Tests/test_loop_invariant.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_loop_invariant.mem2reg.bc -o Tests/test_loop_invariant.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test_loop_invariant.mem2reg.bc -o Tests/test_loop_invariant.lcm-E.ll


============================================================
To Run test2.c
============================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Critical Edge detection on LCM-L
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test2.c -o Tests/test2.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test2.O0.no-optnone.bc -o Tests/test2.mem2reg.bc
llvm-dis-17 Tests/test2.mem2reg.bc -o Tests/test2.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test2.mem2reg.bc -o Tests/test2.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test2.mem2reg.bc -o Tests/test2.lcm-E.ll


============================================================
To Run test3.c
============================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Critical Edge detection on LCM-L
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test3.c -o Tests/test3.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test3.O0.no-optnone.bc -o Tests/test3.mem2reg.bc
llvm-dis-17 Tests/test3.mem2reg.bc -o Tests/test3.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test3.mem2reg.bc -o Tests/test3.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test3.mem2reg.bc -o Tests/test3.lcm-E.ll


============================================================
To Run test4.c
============================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Critical Edge detection on LCM-L
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test4.c -o Tests/test4.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test4.O0.no-optnone.bc -o Tests/test4.mem2reg.bc
llvm-dis-17 Tests/test4.mem2reg.bc -o Tests/test4.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test4.mem2reg.bc -o Tests/test4.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test4.mem2reg.bc -o Tests/test4.lcm-E.ll


============================================================
To Run test5.c
============================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Critical Edge detection on LCM-L
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test5.c -o Tests/test5.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test5.O0.no-optnone.bc -o Tests/test5.mem2reg.bc
llvm-dis-17 Tests/test5.mem2reg.bc -o Tests/test5.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test5.mem2reg.bc -o Tests/test5.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test5.mem2reg.bc -o Tests/test5.lcm-E.ll

============================================================
To Run test6.c
============================================================
#change the useEarliestInsertion from true to false and save it.
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

#Critical Edge detection on LCM-L
clang-17 -fno-discard-value-names -Xclang -disable-O0-optnone -O0 -emit-llvm -c Tests/test6.c -o Tests/test6.O0.no-optnone.bc
opt-17 -passes=mem2reg Tests/test6.O0.no-optnone.bc -o Tests/test6.mem2reg.bc
llvm-dis-17 Tests/test6.mem2reg.bc -o Tests/test6.mem2reg.ll
opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test6.mem2reg.bc -o Tests/test6.lcm-L.ll

#find and change the useEarliestInsertion from false to true (useEarliestInsertion = true;) in the unified pass.cpp and save it
#Then recompile again using the following
rm -rf build
mkdir build
cd build
cmake ..
make clean && make
cd ..

opt-17 -load-pass-plugin=./build/UnifiedPass.so -passes=lcm -S Tests/test6.mem2reg.bc -o Tests/test6.lcm-E.ll



