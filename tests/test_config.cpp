#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include "config.hpp"

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_config_file_ = "test_config.json";
        // 确保测试开始时文件不存在
        if (std::filesystem::exists(test_config_file_)) {
            std::filesystem::remove(test_config_file_);
        }
    }

    void TearDown() override {
        // 清理测试文件
        if (std::filesystem::exists(test_config_file_)) {
            std::filesystem::remove(test_config_file_);
        }
    }

    std::string test_config_file_;
};

TEST_F(ConfigTest, DefaultConfiguration) {
    Config config(test_config_file_);
    
    // 测试默认值
    EXPECT_EQ(config.default_server, "https://cloudflare-dns.com/dns-query");
    EXPECT_EQ(config.timeout, 10);
    EXPECT_EQ(config.connect_timeout, 5);
    EXPECT_EQ(config.retry_count, 3);
    EXPECT_TRUE(config.enable_fallback);
    
    // 测试缓存配置
    EXPECT_TRUE(config.cache.enabled);
    EXPECT_EQ(config.cache.max_size, 1000);
    EXPECT_EQ(config.cache.default_ttl, 300);
    
    // 测试日志配置
    EXPECT_EQ(config.log.level, "info");
    EXPECT_TRUE(config.log.enable_file_logging);
    
    // 测试服务器列表
    EXPECT_FALSE(config.servers.empty());
    EXPECT_GE(config.servers.size(), 5);
}

TEST_F(ConfigTest, LoadNonExistentFile) {
    Config config(test_config_file_);
    
    // 加载不存在的文件应该创建默认配置
    EXPECT_TRUE(config.load());
    EXPECT_TRUE(std::filesystem::exists(test_config_file_));
}

TEST_F(ConfigTest, SaveAndLoadConfiguration) {
    Config config(test_config_file_);
    
    // 修改一些配置
    config.default_server = "https://dns.google/dns-query";
    config.timeout = 15;
    config.cache.enabled = false;
    config.log.level = "debug";
    
    // 保存配置
    EXPECT_TRUE(config.save());
    EXPECT_TRUE(std::filesystem::exists(test_config_file_));
    
    // 创建新的配置对象并加载
    Config loaded_config(test_config_file_);
    EXPECT_TRUE(loaded_config.load());
    
    // 验证加载的配置
    EXPECT_EQ(loaded_config.default_server, "https://dns.google/dns-query");
    EXPECT_EQ(loaded_config.timeout, 15);
    EXPECT_FALSE(loaded_config.cache.enabled);
    EXPECT_EQ(loaded_config.log.level, "debug");
}

TEST_F(ConfigTest, ValidationSuccess) {
    Config config(test_config_file_);
    EXPECT_TRUE(config.validate());
}

TEST_F(ConfigTest, ValidationFailure) {
    Config config(test_config_file_);
    
    // 清空服务器列表
    config.servers.clear();
    EXPECT_FALSE(config.validate());
    
    // 恢复服务器列表，设置无效超时
    config.reset_to_defaults();
    config.timeout = -1;
    EXPECT_FALSE(config.validate());
}

TEST_F(ConfigTest, GetServerConfig) {
    Config config(test_config_file_);
    
    // 测试获取存在的服务器配置
    const auto* cloudflare_config = config.get_server_config("cloudflare");
    ASSERT_NE(cloudflare_config, nullptr);
    EXPECT_EQ(cloudflare_config->name, "cloudflare");
    EXPECT_EQ(cloudflare_config->url, "https://cloudflare-dns.com/dns-query");
    
    // 测试获取不存在的服务器配置
    const auto* nonexistent_config = config.get_server_config("nonexistent");
    EXPECT_EQ(nonexistent_config, nullptr);
}

TEST_F(ConfigTest, GetServersByPriority) {
    Config config(test_config_file_);
    
    auto sorted_servers = config.get_servers_by_priority();
    EXPECT_FALSE(sorted_servers.empty());
    
    // 验证按优先级排序
    for (size_t i = 1; i < sorted_servers.size(); ++i) {
        EXPECT_LE(sorted_servers[i-1].priority, sorted_servers[i].priority);
    }
}

TEST_F(ConfigTest, UpdateFromArgs) {
    Config config(test_config_file_);
    
    // 模拟命令行参数
    const char* argv[] = {
        "program",
        "--log-level", "debug",
        "--timeout", "20",
        "--no-fallback",
        "--server", "https://dns.google/dns-query"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    config.update_from_args(argc, const_cast<char**>(argv));
    
    EXPECT_EQ(config.log.level, "debug");
    EXPECT_EQ(config.timeout, 20);
    EXPECT_FALSE(config.enable_fallback);
    EXPECT_EQ(config.default_server, "https://dns.google/dns-query");
}

TEST_F(ConfigTest, InvalidJsonFile) {
    // 创建无效的JSON文件
    std::ofstream file(test_config_file_);
    file << "{ invalid json content";
    file.close();
    
    Config config(test_config_file_);
    EXPECT_FALSE(config.load());
} 