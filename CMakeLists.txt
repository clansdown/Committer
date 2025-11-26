cmake_minimum_required(VERSION 3.20...3.30)
set(CMAKE_WARN_DEPRECATED OFF)
project(commit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

# Include directories
include_directories(${LIBCURL_INCLUDE_DIRS} ${LIBGIT2_INCLUDE_DIRS} include)

# Add CLI11 as header-only
include(FetchContent)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11)

# Add nlohmann/json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Add FTXUI
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)
FetchContent_GetProperties(ftxui SOURCE_DIR ftxui_SOURCE_DIR)
include_directories(${ftxui_SOURCE_DIR}/include)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Source files
set(SOURCES
    src/main.cpp
    src/git_utils.cpp
    src/config.cpp
    src/spinner.cpp
    src/backends/openrouter_backend.cpp
    src/backends/zen_backend.cpp
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} ${LIBCURL_LIBRARIES} ${LIBGIT2_LIBRARIES} CLI11::CLI11 nlohmann_json::nlohmann_json ftxui::screen ftxui::dom ftxui::component)

# Include FTXUI headers
target_include_directories(${PROJECT_NAME} PRIVATE ${ftxui_SOURCE_DIR}/include)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE ${LIBCURL_CFLAGS_OTHER} ${LIBGIT2_CFLAGS_OTHER})

# Install
install(TARGETS ${PROJECT_NAME} DESTINATION bin)