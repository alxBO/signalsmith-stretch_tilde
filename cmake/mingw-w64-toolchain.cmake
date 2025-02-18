set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Path to the MinGW-w64 compiler
set(CMAKE_C_COMPILER /opt/homebrew/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /opt/homebrew/bin/x86_64-w64-mingw32-g++)

# Tell CMake to use MinGW-w64
set(CMAKE_RC_COMPILER /opt/homebrew/bin/x86_64-w64-mingw32-windres)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")

# Set the linker flags
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

