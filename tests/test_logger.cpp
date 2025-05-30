#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "logger.hpp"

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file_ = "test_logs/test.log";
        
        // 确保测试开始时日志文件不存在
        if (std::filesystem::exists(test_log_file_)) {
            std::filesystem::remove(test_log_file_);
        }
        
        // 创建日志目录
        auto parent_path = std::filesystem::path(test_log_file_).parent_path();
        if (!parent_path.empty()) {
            std::filesystem::create_directories(parent_path);
        }
    }

    void TearDown() override {
        Logger::shutdown();
        
        // 清理测试文件
        if (std::filesystem::exists(test_log_file_)) {
            std::filesystem::remove(test_log_file_);
        }
        
        // 清理测试目录
        auto parent_path = std::filesystem::path(test_log_file_).parent_path();
        if (std::filesystem::exists(parent_path) && std::filesystem::is_empty(parent_path)) {
            std::filesystem::remove(parent_path);
        }
    }

    std::string test_log_file_;
};

TEST_F(LoggerTest, InitializationWithDefaults) {
    Logger::init();
    
    // 测试基本日志功能
    EXPECT_NO_THROW(Logger::info("Test info message"));
    EXPECT_NO_THROW(Logger::debug("Test debug message"));
    EXPECT_NO_THROW(Logger::warn("Test warning message"));
    EXPECT_NO_THROW(Logger::error("Test error message"));
}

TEST_F(LoggerTest, InitializationWithCustomSettings) {
    Logger::init("debug", true, test_log_file_);
    
    // 测试日志记录
    Logger::info("Test message for file logging");
    Logger::flush();
    
    // 验证日志文件是否创建
    EXPECT_TRUE(std::filesystem::exists(test_log_file_));
    
    // 读取日志文件内容
    std::ifstream log_file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("Test message for file logging"), std::string::npos);
}

TEST_F(LoggerTest, LogLevels) {
    Logger::init("trace", true, test_log_file_);
    
    // 测试所有日志级别
    Logger::debug("Debug message");
    Logger::info("Info message");
    Logger::warn("Warning message");
    Logger::error("Error message");
    Logger::critical("Critical message");
    
    Logger::flush();
    
    // 验证日志文件包含所有消息
    std::ifstream log_file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_NE(content.find("Debug message"), std::string::npos);
    EXPECT_NE(content.find("Info message"), std::string::npos);
    EXPECT_NE(content.find("Warning message"), std::string::npos);
    EXPECT_NE(content.find("Error message"), std::string::npos);
    EXPECT_NE(content.find("Critical message"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    Logger::init("warn", true, test_log_file_);
    
    // 记录不同级别的消息
    Logger::debug("Debug message - should not appear");
    Logger::info("Info message - should not appear");
    Logger::warn("Warning message - should appear");
    Logger::error("Error message - should appear");
    
    Logger::flush();
    
    // 验证只有warn及以上级别的消息被记录
    std::ifstream log_file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_EQ(content.find("Debug message"), std::string::npos);
    EXPECT_EQ(content.find("Info message"), std::string::npos);
    EXPECT_NE(content.find("Warning message"), std::string::npos);
    EXPECT_NE(content.find("Error message"), std::string::npos);
}

TEST_F(LoggerTest, SetLevelAfterInit) {
    Logger::init("info", true, test_log_file_);
    
    // 初始级别为info，debug消息不应该出现
    Logger::debug("Debug message 1 - should not appear");
    Logger::info("Info message 1 - should appear");
    
    // 改变日志级别为debug
    Logger::set_level("debug");
    
    Logger::debug("Debug message 2 - should appear");
    Logger::info("Info message 2 - should appear");
    
    Logger::flush();
    
    std::ifstream log_file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_EQ(content.find("Debug message 1"), std::string::npos);
    EXPECT_NE(content.find("Info message 1"), std::string::npos);
    EXPECT_NE(content.find("Debug message 2"), std::string::npos);
    EXPECT_NE(content.find("Info message 2"), std::string::npos);
}

TEST_F(LoggerTest, FormattedLogging) {
    Logger::init("info", true, test_log_file_);
    
    // 测试格式化日志
    std::string domain = "example.com";
    int count = 42;
    Logger::info("Found {} records for domain: {}", count, domain);
    
    Logger::flush();
    
    std::ifstream log_file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_NE(content.find("Found 42 records for domain: example.com"), std::string::npos);
}

TEST_F(LoggerTest, FileLoggingDisabled) {
    Logger::init("info", false);  // 禁用文件日志
    
    Logger::info("Test message");
    Logger::flush();
    
    // 验证日志文件未创建
    EXPECT_FALSE(std::filesystem::exists("test_logs/test.log"));
}

TEST_F(LoggerTest, MultipleInitCalls) {
    // 第一次初始化
    Logger::init("info", true, test_log_file_);
    Logger::info("First init message");
    
    // 第二次初始化应该被忽略
    Logger::init("debug", true, "another_file.log");
    Logger::info("Second init message");
    
    Logger::flush();
    
    // 验证只有第一个日志文件被创建
    EXPECT_TRUE(std::filesystem::exists(test_log_file_));
    EXPECT_FALSE(std::filesystem::exists("another_file.log"));
    
    std::ifstream log_file(test_log_file_);
    std::string content((std::istreambuf_iterator<char>(log_file)),
                        std::istreambuf_iterator<char>());
    
    EXPECT_NE(content.find("First init message"), std::string::npos);
    EXPECT_NE(content.find("Second init message"), std::string::npos);
} 