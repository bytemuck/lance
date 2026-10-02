cmake_minimum_required(VERSION 3.20)
project(lance CXX C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "")
set(LLVM_BUILD_TOOLS OFF CACHE BOOL "")
set(LLVM_INCLUDE_TOOLS OFF CACHE BOOL "")
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
set(LLVM_BUILD_EXAMPLES OFF CACHE BOOL "")
set(LLVM_ENABLE_BINDINGS OFF CACHE BOOL "")

add_subdirectory(vendor/llvm-project/llvm ${CMAKE_BINARY_DIR}/llvm)

add_executable(lance)

file(GLOB_RECURSE LANCE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

target_sources(lance PRIVATE ${LANCE_SOURCES})
target_include_directories(lance PRIVATE 
        vendor/llvm-project/llvm/include 
        ${CMAKE_BINARY_DIR}/llvm/include 
        
        src/
)

target_link_libraries(lance PRIVATE 
        LLVMCore
        LLVMSupport
        LLVMAnalysis
        LLVMTarget

        LLVMX86AsmParser
        LLVMX86CodeGen
)