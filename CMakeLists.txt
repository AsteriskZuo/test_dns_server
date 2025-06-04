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

# 包含 FetchContent 模块
include(FetchContent)

# 配置 RapidJSON 选项，避免构建示例和测试
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "Build rapidjson documentation." FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "Build rapidjson examples." FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "Build rapidjson tests." FORCE)

# 下载并配置 RapidJSON (使用标头库，不需要构建)
FetchContent_Declare(
    rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG v1.1.0
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

# RapidJSON 是纯头文件库，需要特殊处理
FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(rapidjson)
    # 不要调用 add_subdirectory，因为我们只需要头文件
endif()

# 获取其他依赖
FetchContent_MakeAvailable(spdlog googletest)

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

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CURL_INCLUDE_DIRS}
    ${rapidjson_SOURCE_DIR}/include
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
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
        ${rapidjson_SOURCE_DIR}/include
    )
    
    target_link_libraries(${PROJECT_NAME}_tests 
        PRIVATE 
        gtest_main
        gmock_main
        spdlog::spdlog
        ${CURL_LIBRARIES}
    )
    
    # 添加测试
    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_tests)
endif()