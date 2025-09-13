# CMakeLists.txt for building LLVM analysis pass plugin for LLVM 17
# Using execute_process for llvm-config --libs/--ldflags
# Removed 'ir' component name which is unknown to llvm-config

cmake_minimum_required(VERSION 3.13.4)
project(UnifiedPass)

# --- Find LLVM using llvm-config ---
find_program(LLVM_CONFIG_EXECUTABLE llvm-config-17)
if(NOT LLVM_CONFIG_EXECUTABLE)
  message(FATAL_ERROR "llvm-config-17 not found. Make sure LLVM 17 is installed and in your PATH.")
endif()

# Get include directories
execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
  OUTPUT_VARIABLE LLVM_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE LLVM_CONFIG_INCLUDE_RESULT
)
if(NOT LLVM_CONFIG_INCLUDE_RESULT EQUAL 0)
  message(FATAL_ERROR "llvm-config --includedir failed!")
endif()
include_directories(${LLVM_INCLUDE_DIR})

# Get necessary definitions (e.g., -DNDEBUG in release builds)
execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --cxxflags
  OUTPUT_VARIABLE LLVM_CXX_FLAGS_STR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE LLVM_CONFIG_CXXFLAGS_RESULT
)
if(NOT LLVM_CONFIG_CXXFLAGS_RESULT EQUAL 0)
  message(FATAL_ERROR "llvm-config --cxxflags failed!")
endif()
# Add these flags to the compile options
add_compile_options(${LLVM_CXX_FLAGS_STR})


# --- Configure Compiler ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Recommended for LLVM


# --- Get Linker Flags and Libraries Directly from llvm-config ---
# Specify components needed by your pass (Removed 'ir')
set(LLVM_COMPONENTS core support passes analysis) # List required components

execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs ${LLVM_COMPONENTS}
  OUTPUT_VARIABLE LLVM_CONFIG_LIBS_STR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE LLVM_CONFIG_LIBS_RESULT
)
if(NOT LLVM_CONFIG_LIBS_RESULT EQUAL 0)
  message(FATAL_ERROR "llvm-config --libs failed! Components requested: ${LLVM_COMPONENTS}")
endif()

execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} --ldflags
  OUTPUT_VARIABLE LLVM_CONFIG_LDFLAGS_STR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE LLVM_CONFIG_LDFLAGS_RESULT
)
if(NOT LLVM_CONFIG_LDFLAGS_RESULT EQUAL 0)
  message(FATAL_ERROR "llvm-config --ldflags failed!")
endif()

message(STATUS "llvm-config libs: ${LLVM_CONFIG_LIBS_STR}")
message(STATUS "llvm-config ldflags: ${LLVM_CONFIG_LDFLAGS_STR}")


# --- Build the Plugin ---
add_library(UnifiedPass MODULE unifiedpass.cpp)

# Link against the libraries obtained directly from llvm-config
# Pass the raw string output to target_link_libraries
target_link_libraries(UnifiedPass PRIVATE ${LLVM_CONFIG_LIBS_STR})

# Add the linker flags obtained directly from llvm-config
# Pass the raw string output to target_link_options
target_link_options(UnifiedPass PRIVATE ${LLVM_CONFIG_LDFLAGS_STR})


# Ensure the library name matches what opt expects (UnifiedPass.so)
set_target_properties(UnifiedPass PROPERTIES
    PREFIX "" # Ensure the name is UnifiedPass.so, not libUnifiedPass.so
)

message(STATUS "CMake configuration complete. Run 'make' in build directory to build UnifiedPass.so")

