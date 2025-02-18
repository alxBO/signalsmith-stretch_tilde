include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################
include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC
     "./src/*.h"
	 "./src/*.hpp"
	 "./src/*.c"
     "./src/*.cpp"
)
message("src: ${PROJECT_SRC}")
list(FILTER PROJECT_SRC EXCLUDE REGEX "./src/*test_signalsmith_stretch\\.cpp$")

add_library( 
	${PROJECT_NAME} 
	MODULE
	${PROJECT_SRC}
)

########## TEST DEINTERLEAVE

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.16.0 
)
FetchContent_MakeAvailable(googletest)

add_executable(test_${PROJECT_NAME}
	./src/test_signalsmith_stretch.cpp  # Your test file (add all test files here)
	./src/deinterleave.cpp
)

# Link the test executable with Google Test and your Max external module
target_link_libraries(test_${PROJECT_NAME}
    gtest
    gtest_main
)

# Ensure the test executable is placed correctly
set_target_properties(test_${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Enable testing
enable_testing()

# Register your tests with CTest
add_test(NAME TestSignalsmithStretch COMMAND ${CMAKE_BINARY_DIR}/bin/test_${PROJECT_NAME})

# Detect if we are using a multi-config generator (Xcode, VS, Ninja Multi-Config)
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (IS_MULTI_CONFIG)
    set(CTEST_COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
else()
    set(CTEST_COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
endif()

add_custom_target(run_tests
    COMMAND ${CTEST_COMMAND}
    DEPENDS test_${PROJECT_NAME}
)

add_dependencies(run_tests test_${PROJECT_NAME})

########## SIMD

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(SIMD_FLAGS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mavx2" AVX2_SUPPORTED)
        if(AVX2_SUPPORTED)
            set(SIMD_FLAGS "-mavx2")
        endif()
        if(NOT AVX2_SUPPORTED)
            check_cxx_compiler_flag("-mavx" AVX_SUPPORTED)
            if(AVX_SUPPORTED)
                set(SIMD_FLAGS "-mavx")
            endif()
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(SIMD_FLAGS "-mfpu=neon -mfloat-abi=softfp")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_CROSSCOMPILING)
	find_package(AVX)
    if(AVX_FOUND)
		set(SIMD_FLAGS ${AVX_FLAGS})
	endif()
endif()

add_compile_options(${SIMD_FLAGS})
message("SIMD: ${SIMD_FLAGS}")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
