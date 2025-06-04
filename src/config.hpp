#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

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
    rapidjson::Document config_data_;
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
    void load_from_json(const rapidjson::Document& j);

    /**
     * @brief 转换为JSON
     * @return JSON对象
     */
    rapidjson::Document to_json() const;
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
        
        // 将文件内容读入字符串
        std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        
        // 解析JSON
        rapidjson::Document doc;
        doc.Parse(json_str.c_str());
        
        if (doc.HasParseError()) {
            std::cerr << "Failed to parse config JSON: " << doc.GetParseError() << std::endl;
            return false;
        }
        
        load_from_json(doc);
        
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

        rapidjson::Document doc = to_json();
        
        // 将JSON格式化为字符串
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        
        // 写入文件
        file << buffer.GetString();
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

void Config::load_from_json(const rapidjson::Document& j) {
    if (j.HasMember("default_server") && j["default_server"].IsString()) {
        default_server = j["default_server"].GetString();
    }
    if (j.HasMember("timeout") && j["timeout"].IsInt()) {
        timeout = j["timeout"].GetInt();
    }
    if (j.HasMember("connect_timeout") && j["connect_timeout"].IsInt()) {
        connect_timeout = j["connect_timeout"].GetInt();
    }
    if (j.HasMember("retry_count") && j["retry_count"].IsInt()) {
        retry_count = j["retry_count"].GetInt();
    }
    if (j.HasMember("enable_fallback") && j["enable_fallback"].IsBool()) {
        enable_fallback = j["enable_fallback"].GetBool();
    }
    
    // 加载缓存配置
    if (j.HasMember("cache") && j["cache"].IsObject()) {
        const auto& cache_json = j["cache"];
        if (cache_json.HasMember("enabled") && cache_json["enabled"].IsBool()) {
            cache.enabled = cache_json["enabled"].GetBool();
        }
        if (cache_json.HasMember("max_size") && cache_json["max_size"].IsInt()) {
            cache.max_size = cache_json["max_size"].GetInt();
        }
        if (cache_json.HasMember("default_ttl") && cache_json["default_ttl"].IsInt()) {
            cache.default_ttl = cache_json["default_ttl"].GetInt();
        }
    }
    
    // 加载日志配置
    if (j.HasMember("log") && j["log"].IsObject()) {
        const auto& log_json = j["log"];
        if (log_json.HasMember("level") && log_json["level"].IsString()) {
            log.level = log_json["level"].GetString();
        }
        if (log_json.HasMember("enable_file_logging") && log_json["enable_file_logging"].IsBool()) {
            log.enable_file_logging = log_json["enable_file_logging"].GetBool();
        }
        if (log_json.HasMember("log_file_path") && log_json["log_file_path"].IsString()) {
            log.log_file_path = log_json["log_file_path"].GetString();
        }
        if (log_json.HasMember("enable_console_logging") && log_json["enable_console_logging"].IsBool()) {
            log.enable_console_logging = log_json["enable_console_logging"].GetBool();
        }
    }
    
    // 加载服务器配置
    if (j.HasMember("servers") && j["servers"].IsArray()) {
        servers.clear();
        const auto& server_array = j["servers"];
        for (rapidjson::SizeType i = 0; i < server_array.Size(); i++) {
            const auto& server_json = server_array[i];
            DoHServerConfig server;
            
            if (server_json.HasMember("name") && server_json["name"].IsString()) {
                server.name = server_json["name"].GetString();
            }
            if (server_json.HasMember("url") && server_json["url"].IsString()) {
                server.url = server_json["url"].GetString();
            }
            if (server_json.HasMember("methods") && server_json["methods"].IsArray()) {
                const auto& methods_array = server_json["methods"];
                server.methods.clear();
                for (rapidjson::SizeType j = 0; j < methods_array.Size(); j++) {
                    if (methods_array[j].IsString()) {
                        server.methods.push_back(methods_array[j].GetString());
                    }
                }
            }
            if (server_json.HasMember("priority") && server_json["priority"].IsInt()) {
                server.priority = server_json["priority"].GetInt();
            }
            if (server_json.HasMember("timeout") && server_json["timeout"].IsInt()) {
                server.timeout = server_json["timeout"].GetInt();
            }
            if (server_json.HasMember("enabled") && server_json["enabled"].IsBool()) {
                server.enabled = server_json["enabled"].GetBool();
            }
            
            servers.push_back(server);
        }
    }
}

rapidjson::Document Config::to_json() const {
    rapidjson::Document doc;
    doc.SetObject();
    
    // 使用文档的分配器
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    
    // 基本设置
    doc.AddMember("default_server", rapidjson::StringRef(default_server.c_str()), allocator);
    doc.AddMember("timeout", timeout, allocator);
    doc.AddMember("connect_timeout", connect_timeout, allocator);
    doc.AddMember("retry_count", retry_count, allocator);
    doc.AddMember("enable_fallback", enable_fallback, allocator);
    
    // 缓存配置
    rapidjson::Value cache_obj(rapidjson::kObjectType);
    cache_obj.AddMember("enabled", cache.enabled, allocator);
    cache_obj.AddMember("max_size", cache.max_size, allocator);
    cache_obj.AddMember("default_ttl", cache.default_ttl, allocator);
    doc.AddMember("cache", cache_obj, allocator);
    
    // 日志配置
    rapidjson::Value log_obj(rapidjson::kObjectType);
    log_obj.AddMember("level", rapidjson::StringRef(log.level.c_str()), allocator);
    log_obj.AddMember("enable_file_logging", log.enable_file_logging, allocator);
    log_obj.AddMember("log_file_path", rapidjson::StringRef(log.log_file_path.c_str()), allocator);
    log_obj.AddMember("enable_console_logging", log.enable_console_logging, allocator);
    doc.AddMember("log", log_obj, allocator);
    
    // 服务器配置
    rapidjson::Value servers_array(rapidjson::kArrayType);
    
    for (const auto& server : servers) {
        rapidjson::Value server_obj(rapidjson::kObjectType);
        
        server_obj.AddMember("name", rapidjson::StringRef(server.name.c_str()), allocator);
        server_obj.AddMember("url", rapidjson::StringRef(server.url.c_str()), allocator);
        
        // 方法列表
        rapidjson::Value methods_array(rapidjson::kArrayType);
        for (const auto& method : server.methods) {
            methods_array.PushBack(rapidjson::StringRef(method.c_str()), allocator);
        }
        server_obj.AddMember("methods", methods_array, allocator);
        
        server_obj.AddMember("priority", server.priority, allocator);
        server_obj.AddMember("timeout", server.timeout, allocator);
        server_obj.AddMember("enabled", server.enabled, allocator);
        
        servers_array.PushBack(server_obj, allocator);
    }
    
    doc.AddMember("servers", servers_array, allocator);
    
    return doc;
}

#endif // CONFIG_HPP 