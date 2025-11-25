cmake_minimum_required(VERSION 3.20)
project(commit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)

# Include directories
include_directories(${LIBCURL_INCLUDE_DIRS} include)

# Add CLI11 as header-only
include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11)

# Source files
set(SOURCES
    src/main.cpp
    src/git_utils.cpp
    src/config.cpp
    src/backends/openrouter_backend.cpp
    src/backends/zen_backend.cpp
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBCURL_LIBRARIES} CLI11::CLI11)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE ${LIBCURL_CFLAGS_OTHER})

# Install
install(TARGETS ${PROJECT_NAME} DESTINATION bin)