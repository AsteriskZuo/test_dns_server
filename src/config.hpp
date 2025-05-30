#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

/**
 * @brief DoH服务器配置结构
 */
struct DoHServerConfig {
    std::string name;
    std::string url;
    std::vector<std::string> methods;
    int priority = 1;
    int timeout = 10;
    bool enabled = true;
};

/**
 * @brief 缓存配置结构
 */
struct CacheConfig {
    bool enabled = true;
    int max_size = 1000;
    int default_ttl = 300;
};

/**
 * @brief 日志配置结构
 */
struct LogConfig {
    std::string level = "info";
    bool enable_file_logging = true;
    std::string log_file_path = "logs/doh_client.log";
    bool enable_console_logging = true;
};

/**
 * @brief 主配置类
 */
class Config {
private:
    json config_data_;
    std::string config_file_path_;
    bool loaded_ = false;

public:
    // 默认配置
    std::string default_server = "https://cloudflare-dns.com/dns-query";
    int timeout = 10;
    int connect_timeout = 5;
    int retry_count = 3;
    bool enable_fallback = true;
    
    CacheConfig cache;
    LogConfig log;
    std::vector<DoHServerConfig> servers;

    /**
     * @brief 构造函数
     * @param config_file 配置文件路径
     */
    explicit Config(const std::string& config_file = "config/doh_config.json");

    /**
     * @brief 加载配置文件
     * @return 是否成功加载
     */
    bool load();

    /**
     * @brief 保存配置到文件
     * @return 是否成功保存
     */
    bool save();

    /**
     * @brief 从命令行参数更新配置
     * @param argc 参数数量
     * @param argv 参数数组
     */
    void update_from_args(int argc, char* argv[]);

    /**
     * @brief 获取指定名称的服务器配置
     * @param name 服务器名称
     * @return 服务器配置，如果未找到返回nullptr
     */
    const DoHServerConfig* get_server_config(const std::string& name) const;

    /**
     * @brief 获取按优先级排序的服务器列表
     * @return 排序后的服务器配置列表
     */
    std::vector<DoHServerConfig> get_servers_by_priority() const;

    /**
     * @brief 验证配置的有效性
     * @return 是否有效
     */
    bool validate() const;

    /**
     * @brief 打印当前配置
     */
    void print() const;

    /**
     * @brief 创建默认配置文件
     * @param file_path 配置文件路径
     * @return 是否成功创建
     */
    static bool create_default_config(const std::string& file_path);

    /**
     * @brief 重置为默认配置（用于测试）
     */
    void reset_to_defaults();

private:
    /**
     * @brief 初始化默认配置
     */
    void init_defaults();

    /**
     * @brief 从JSON加载配置
     * @param j JSON对象
     */
    void load_from_json(const json& j);

    /**
     * @brief 转换为JSON
     * @return JSON对象
     */
    json to_json() const;
};

// 实现
Config::Config(const std::string& config_file) : config_file_path_(config_file) {
    init_defaults();
}

bool Config::load() {
    try {
        // 检查配置文件是否存在
        if (!std::filesystem::exists(config_file_path_)) {
            std::cout << "Config file not found: " << config_file_path_ 
                      << ", creating default config..." << std::endl;
            
            // 创建目录
            auto parent_path = std::filesystem::path(config_file_path_).parent_path();
            if (!parent_path.empty()) {
                std::filesystem::create_directories(parent_path);
            }
            
            // 创建默认配置文件
            if (!create_default_config(config_file_path_)) {
                std::cerr << "Failed to create default config file" << std::endl;
                return false;
            }
        }

        // 读取配置文件
        std::ifstream file(config_file_path_);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << config_file_path_ << std::endl;
            return false;
        }

        json j;
        file >> j;
        load_from_json(j);
        
        loaded_ = true;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return false;
    }
}

bool Config::save() {
    try {
        // 创建目录
        auto parent_path = std::filesystem::path(config_file_path_).parent_path();
        if (!parent_path.empty()) {
            std::filesystem::create_directories(parent_path);
        }

        std::ofstream file(config_file_path_);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file for writing: " << config_file_path_ << std::endl;
            return false;
        }

        json j = to_json();
        file << j.dump(4); // 4 spaces indentation
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving config: " << e.what() << std::endl;
        return false;
    }
}

void Config::update_from_args(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--log-level" && i + 1 < argc) {
            log.level = argv[++i];
        } else if (arg == "--timeout" && i + 1 < argc) {
            timeout = std::stoi(argv[++i]);
        } else if (arg == "--no-fallback") {
            enable_fallback = false;
        } else if (arg == "--server" && i + 1 < argc) {
            default_server = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            config_file_path_ = argv[++i];
        }
    }
}

const DoHServerConfig* Config::get_server_config(const std::string& name) const {
    for (const auto& server : servers) {
        if (server.name == name) {
            return &server;
        }
    }
    return nullptr;
}

std::vector<DoHServerConfig> Config::get_servers_by_priority() const {
    auto sorted_servers = servers;
    std::sort(sorted_servers.begin(), sorted_servers.end(),
              [](const DoHServerConfig& a, const DoHServerConfig& b) {
                  return a.priority < b.priority;
              });
    return sorted_servers;
}

bool Config::validate() const {
    if (servers.empty()) {
        std::cerr << "No servers configured" << std::endl;
        return false;
    }
    
    if (timeout <= 0 || connect_timeout <= 0) {
        std::cerr << "Invalid timeout values" << std::endl;
        return false;
    }
    
    return true;
}

void Config::print() const {
    std::cout << "=== DoH Client Configuration ===" << std::endl;
    std::cout << "Config File: " << config_file_path_ << std::endl;
    std::cout << "Default Server: " << default_server << std::endl;
    std::cout << "Timeout: " << timeout << "s" << std::endl;
    std::cout << "Connect Timeout: " << connect_timeout << "s" << std::endl;
    std::cout << "Retry Count: " << retry_count << std::endl;
    std::cout << "Enable Fallback: " << (enable_fallback ? "Yes" : "No") << std::endl;
    std::cout << "Log Level: " << log.level << std::endl;
    std::cout << "Cache Enabled: " << (cache.enabled ? "Yes" : "No") << std::endl;
    std::cout << "Servers (" << servers.size() << "):" << std::endl;
    for (const auto& server : servers) {
        std::cout << "  - " << server.name << " (" << server.url << ") Priority: " << server.priority << std::endl;
    }
}

bool Config::create_default_config(const std::string& file_path) {
    Config default_config;
    default_config.config_file_path_ = file_path;
    return default_config.save();
}

void Config::reset_to_defaults() {
    init_defaults();
}

void Config::init_defaults() {
    // 初始化默认服务器列表
    servers = {
        {"cloudflare", "https://cloudflare-dns.com/dns-query", {"get", "post", "json"}, 1, 10, true},
        {"google", "https://dns.google/dns-query", {"get", "post"}, 2, 10, true},
        {"google_json", "https://dns.google/resolve", {"json"}, 3, 10, true},
        {"quad9", "https://dns.quad9.net/dns-query", {"get", "post", "json"}, 4, 10, true},
        {"alibaba", "https://dns.alidns.com/dns-query", {"get", "post"}, 5, 10, true},
        {"alibaba_json", "http://dns.alidns.com/resolve", {"json"}, 6, 10, true},
        {"360", "https://doh.360.cn/dns-query", {"get", "post", "json"}, 7, 10, true},
        {"tencent", "https://doh.pub/dns-query", {"get", "post", "json"}, 8, 10, true}
    };
}

void Config::load_from_json(const json& j) {
    if (j.contains("default_server")) {
        default_server = j["default_server"];
    }
    if (j.contains("timeout")) {
        timeout = j["timeout"];
    }
    if (j.contains("connect_timeout")) {
        connect_timeout = j["connect_timeout"];
    }
    if (j.contains("retry_count")) {
        retry_count = j["retry_count"];
    }
    if (j.contains("enable_fallback")) {
        enable_fallback = j["enable_fallback"];
    }
    
    // 加载缓存配置
    if (j.contains("cache")) {
        const auto& cache_json = j["cache"];
        if (cache_json.contains("enabled")) cache.enabled = cache_json["enabled"];
        if (cache_json.contains("max_size")) cache.max_size = cache_json["max_size"];
        if (cache_json.contains("default_ttl")) cache.default_ttl = cache_json["default_ttl"];
    }
    
    // 加载日志配置
    if (j.contains("log")) {
        const auto& log_json = j["log"];
        if (log_json.contains("level")) log.level = log_json["level"];
        if (log_json.contains("enable_file_logging")) log.enable_file_logging = log_json["enable_file_logging"];
        if (log_json.contains("log_file_path")) log.log_file_path = log_json["log_file_path"];
        if (log_json.contains("enable_console_logging")) log.enable_console_logging = log_json["enable_console_logging"];
    }
    
    // 加载服务器配置
    if (j.contains("servers")) {
        servers.clear();
        for (const auto& server_json : j["servers"]) {
            DoHServerConfig server;
            if (server_json.contains("name")) server.name = server_json["name"];
            if (server_json.contains("url")) server.url = server_json["url"];
            if (server_json.contains("methods")) server.methods = server_json["methods"];
            if (server_json.contains("priority")) server.priority = server_json["priority"];
            if (server_json.contains("timeout")) server.timeout = server_json["timeout"];
            if (server_json.contains("enabled")) server.enabled = server_json["enabled"];
            servers.push_back(server);
        }
    }
}

json Config::to_json() const {
    json j;
    j["default_server"] = default_server;
    j["timeout"] = timeout;
    j["connect_timeout"] = connect_timeout;
    j["retry_count"] = retry_count;
    j["enable_fallback"] = enable_fallback;
    
    // 缓存配置
    j["cache"]["enabled"] = cache.enabled;
    j["cache"]["max_size"] = cache.max_size;
    j["cache"]["default_ttl"] = cache.default_ttl;
    
    // 日志配置
    j["log"]["level"] = log.level;
    j["log"]["enable_file_logging"] = log.enable_file_logging;
    j["log"]["log_file_path"] = log.log_file_path;
    j["log"]["enable_console_logging"] = log.enable_console_logging;
    
    // 服务器配置
    j["servers"] = json::array();
    for (const auto& server : servers) {
        json server_json;
        server_json["name"] = server.name;
        server_json["url"] = server.url;
        server_json["methods"] = server.methods;
        server_json["priority"] = server.priority;
        server_json["timeout"] = server.timeout;
        server_json["enabled"] = server.enabled;
        j["servers"].push_back(server_json);
    }
    
    return j;
}

#endif // CONFIG_HPP 