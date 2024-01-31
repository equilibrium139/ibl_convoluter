cmake_minimum_required(VERSION 3.17)

project(ibl_convoluter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
    
add_executable(ibl_convoluter src/Main.cpp
                              src/Shader.cpp
                              src/Shader.h
                              src/glad.cpp
                              src/stb_image.h
                              src/stb_image.cpp
                              src/stb_image_write.h
                              src/stb_image_write.cpp
)

find_package(glfw3 CONFIG REQUIRED)

add_library(ispc_texcomp STATIC IMPORTED) 
set_target_properties(ispc_texcomp PROPERTIES
  IMPORTED_LOCATION "C:/Libraries/ISPCTextureCompressor/ispc_texcomp/x64/Debug/ispc_texcomp.lib"
  INTERFACE_INCLUDE_DIRECTORIES "C:/Libraries/ISPCTextureCompressor/ispc_texcomp"
)

target_link_libraries(ibl_convoluter ispc_texcomp glfw)

target_include_directories(ibl_convoluter PRIVATE include)

if(MSVC)
  target_compile_options(ibl_convoluter PRIVATE /W4)
else()
  target_compile_options(ibl_convoluter PRIVATE -Wall -Wextra -pedantic)
endif()
