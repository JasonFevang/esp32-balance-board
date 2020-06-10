# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Adds colored build output for finding errors more easily
# Combine with GCC_COLORS environment variable
add_compile_options (-fdiagnostics-color=always)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-discord-webhook-interface)