cmake_minimum_required(VERSION 3.17)

project(ibl_convoluter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
    
add_executable(ibl_convoluter src/Main.cpp
                              src/stb_image.h
                              src/stb_image.cpp
                              src/stb_image_write.h
                              src/stb_image_write.cpp
)


if(MSVC)
  target_compile_options(ibl_convoluter PRIVATE /W4)
else()
  target_compile_options(ibl_convoluter PRIVATE -Wall -Wextra -pedantic)
endif()
