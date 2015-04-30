cmake_minimum_required(VERSION 2.8)
project(llvm_nts_po)

set( CMAKE_EXPORT_COMPILE_COMMANDS 1 )


list( APPEND CMAKE_CXX_FLAGS "-std=c++14 -Wall -Wextra" )

find_package ( LLVM        REQUIRED CONFIG )
find_package ( libNTS_cpp     REQUIRED CONFIG )
find_package ( llvm2nts    REQUIRED CONFIG )

add_subdirectory ( src )
