cmake_minimum_required(VERSION 3.15) 
project(test_dns_server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置调试信息和编译选项
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 为 VS Code IntelliSense 生成 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 CMake 策略，以便兼容 nlohmann_json 项目
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

# 设置最低 CMake 策略版本，解决 nlohmann_json 的兼容性问题
set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0025 NEW)
# 使用 cmake 命令行传递这个参数
set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "")

# 包含 FetchContent 模块
include(FetchContent)

# 下载并配置 nlohmann/json (使用标头库，不需要构建)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
    GIT_SHALLOW TRUE
)

# 下载并配置 spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
    GIT_SHALLOW TRUE
)

# 下载并配置 Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

# 设置 gtest 选项
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nlohmann_json spdlog googletest)

# 查找系统安装的 curl
find_package(CURL REQUIRED)

# 显示 curl 库信息
message(STATUS "使用系统库 curl: ${CURL_VERSION}")
message(STATUS "curl 包含目录: ${CURL_INCLUDE_DIRS}")
message(STATUS "curl 库: ${CURL_LIBRARIES}")

# 添加源文件
aux_source_directory(${PROJECT_SOURCE_DIR}/src source_directory)
FILE(GLOB_RECURSE SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# 添加可执行文件
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# 获取 nlohmann_json 的头文件目录
get_target_property(JSON_INCLUDE_DIRS nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CURL_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    ${CURL_LIBRARIES}
)

# 启用测试
enable_testing()

# 添加测试可执行文件
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/tests/*.cpp)
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
    
    target_include_directories(${PROJECT_NAME}_tests PRIVATE 
        ${PROJECT_SOURCE_DIR}/src
        ${CURL_INCLUDE_DIRS}
        ${JSON_INCLUDE_DIRS}
    )
    
    target_link_libraries(${PROJECT_NAME}_tests 
        PRIVATE 
        gtest_main
        gmock_main
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        ${CURL_LIBRARIES}
    )
    
    # 添加测试
    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_tests)
endif()