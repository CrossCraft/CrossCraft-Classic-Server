cmake_minimum_required(VERSION 3.17)
project(Classic-Server)

set(CMAKE_CXX_STANDARD 17)

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
# set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

if(WIN32)
    add_compile_definitions(BUILD_PLAT=0)
else()
    add_compile_definitions(BUILD_PLAT=1)
endif()

add_subdirectory(ext/Stardust-Celeste Stardust)
add_subdirectory(ext/zlib ZLib)

file(GLOB_RECURSE SRC_FILES src/*.cpp)
file(GLOB_RECURSE INC_FILES include/*.hpp)

add_executable(Classic-Server ${SRC_FILES} ${INC_FILES})
target_include_directories(Classic-Server PUBLIC ext/Stardust-Celeste/ext/glm/glm/ ext/FNL/Cpp ext/zlib/)
target_link_libraries(Classic-Server SC-Entry Stardust-Celeste zlib)

if(WIN32)
    target_link_libraries(Classic-Server ws2_32.lib)
endif()
