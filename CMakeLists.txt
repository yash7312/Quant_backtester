cmake_minimum_required(VERSION 3.20)
project(qback LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Locate Arrow/Parquet from pyarrow's bundled C++ libs ---
execute_process(
    COMMAND python3 -c "import pyarrow; print(pyarrow.get_library_dirs()[0])"
    OUTPUT_VARIABLE PYARROW_LIB_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _pyarrow_lib_result
)
execute_process(
    COMMAND python3 -c "import pyarrow; print(pyarrow.get_include())"
    OUTPUT_VARIABLE PYARROW_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _pyarrow_inc_result
)

if(_pyarrow_lib_result OR _pyarrow_inc_result)
    message(FATAL_ERROR
        "Could not locate pyarrow. Activate the venv first:\n"
        "  source /home/yash7312/Desktop/2026_autumn/.venv/bin/activate")
endif()

message(STATUS "PyArrow lib dir: ${PYARROW_LIB_DIR}")
message(STATUS "PyArrow include dir: ${PYARROW_INCLUDE_DIR}")

# Arrow
add_library(arrow SHARED IMPORTED)
file(GLOB _arrow_so "${PYARROW_LIB_DIR}/libarrow.so*")
list(GET _arrow_so 0 _arrow_so_path)
set_target_properties(arrow PROPERTIES
    IMPORTED_LOCATION "${_arrow_so_path}"
    INTERFACE_INCLUDE_DIRECTORIES "${PYARROW_INCLUDE_DIR}"
)

# Parquet
add_library(parquet SHARED IMPORTED)
file(GLOB _parquet_so "${PYARROW_LIB_DIR}/libparquet.so*")
list(GET _parquet_so 0 _parquet_so_path)
set_target_properties(parquet PROPERTIES
    IMPORTED_LOCATION "${_parquet_so_path}"
    INTERFACE_INCLUDE_DIRECTORIES "${PYARROW_INCLUDE_DIR}"
)

# --- qback static library (shared between main binary and tests) ---
add_library(qback_lib STATIC
    src/qback/domain/bar.cpp
    src/qback/data/parquet_reader.cpp
)
target_include_directories(qback_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(qback_lib PUBLIC arrow parquet)
set_target_properties(qback_lib PROPERTIES
    BUILD_RPATH "${PYARROW_LIB_DIR}"
)

# --- qback executable ---
add_executable(qback src/qback/main.cpp)
target_link_libraries(qback PRIVATE qback_lib)
set_target_properties(qback PROPERTIES
    BUILD_RPATH "${PYARROW_LIB_DIR}"
    INSTALL_RPATH "${PYARROW_LIB_DIR}"
)

# --- Google Test ---
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(qback_tests
    tests/unit/test_types.cpp
    tests/unit/test_event.cpp
    tests/unit/test_position.cpp
    tests/unit/test_order.cpp
    tests/unit/test_clock.cpp
    tests/unit/test_event_queue.cpp
    tests/unit/test_bar_feed.cpp
    tests/unit/test_engine.cpp
)
target_link_libraries(qback_tests PRIVATE qback_lib GTest::gtest_main)
set_target_properties(qback_tests PROPERTIES
    BUILD_RPATH "${PYARROW_LIB_DIR}"
)

include(GoogleTest)
gtest_discover_tests(qback_tests)
