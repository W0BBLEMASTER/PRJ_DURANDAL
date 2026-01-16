cmake_minimum_required(VERSION 3.10)
project(durandal_libretro)

set(CMAKE_CXX_STANDARD 17)

# Includes
include_directories(
    .
    Source_Files
    Source_Files/CSeries
    Source_Files/Files
    Source_Files/GameWorld
    Source_Files/Input
    Source_Files/Lua
    Source_Files/Misc
    Source_Files/ModelView
    Source_Files/Network
    Source_Files/Network/Metaserver
    Source_Files/Videos
    Source_Files/RenderMain
    Source_Files/RenderOther
    Source_Files/Sound
    Source_Files/XML
    Source_Files/TCPMess
    ../skeletor
)

# Defines
add_definitions(-DHAVE_CONFIG_H -D_THREAD_SAFE -D__STDC_CONSTANT_MACROS -DHAVE_SDL2)

# Source Files
file(GLOB_RECURSE SOURCES 
    "Source_Files/*.cpp"
    "Source_Files/*.c"
    "../skeletor/libretro.cpp"
)

# Filter out main and platform specific
list(FILTER SOURCES EXCLUDE REGEX "main.cpp")
list(FILTER SOURCES EXCLUDE REGEX "_posix.cpp")
list(FILTER SOURCES EXCLUDE REGEX "_macosx.cpp")

# Libraries
find_package(SDL2 REQUIRED)
# ... Add other libs

add_library(durandal_libretro SHARED ${SOURCES})
target_link_libraries(durandal_libretro SDL2::SDL2)
