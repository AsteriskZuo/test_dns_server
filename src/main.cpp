#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#include "doh_client.hpp"
#include "tools.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "exceptions.hpp"

// 使用示例
int main(int argc, char *argv[]) {
    try {
        // 加载配置
        Config config;
        config.update_from_args(argc, argv);
        
        if (!config.load()) {
            std::cerr << "Warning: Failed to load config, using defaults" << std::endl;
        }

        // 初始化日志系统
        Logger::init(config.log.level, config.log.enable_file_logging, config.log.log_file_path);
        Logger::info("DoH Client starting...");

        // 验证配置
        if (!config.validate()) {
            Logger::error("Invalid configuration");
            return 1;
        }
        config.print();

        // 初始化libcurl(全局初始化)
        curl_global_init(CURL_GLOBAL_DEFAULT);
        Logger::debug("CURL initialized");

        // 检查帮助请求
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
        }

        // 查询域名 - 统一使用命名参数
        std::string domain = "ap4-tls.agora.io";  // 默认域名
        bool found_domain = false;
        
        // 查找域名参数
        for (int i = 1; i < argc - 1; ++i) {
            if ((std::string(argv[i]) == "--domain" || std::string(argv[i]) == "-d") && i + 1 < argc) {
                domain = argv[i + 1];
                found_domain = true;
                break;
            }
        }
        
        if (!found_domain) {
            Logger::info("No domain specified. Using default: {}", domain);
        }
        Logger::info("Querying domain: {}", domain);

        // 创建DoH客户端实例，使用配置中的默认服务器
        DoHClient client(config.default_server);

        // 确定查询方法
        DoHMethod method = DoHMethod::JSON_GET;  // 默认使用JSON GET
        
        // 查找方法参数
        for (int i = 1; i < argc - 1; ++i) {
            if (std::string(argv[i]) == "--method" && i + 1 < argc) {
                std::string methodStr = argv[i + 1];
                std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                if (methodStr == "get") {
                    method = DoHMethod::GET;
                } else if (methodStr == "post") {
                    method = DoHMethod::POST;
                } else if (methodStr == "json" || methodStr == "json_get") {
                    method = DoHMethod::JSON_GET;
                } else {
                    Logger::warn("Unknown method: {}. Using default JSON_GET.", methodStr);
                }
                break;
            }
        }

        // 执行A记录查询
        Logger::debug("Starting DNS query with method: {}", static_cast<int>(method));
        auto records = client.query(domain, DNSRecordType::A, method, config.enable_fallback);

        // 输出结果
        if (records.empty()) {
            Logger::warn("No records found for domain: {}", domain);
            std::cout << "No records found." << std::endl;
        } else {
            Logger::info("Found {} DNS records for domain: {}", records.size(), domain);
            std::cout << "DNS records for " << domain << ":" << std::endl;
            for (const auto &record : records) {
                std::cout << "Name: " << record.name << std::endl;
                std::cout << "Type: ";

                switch (record.type) {
                    case DNSRecordType::A:
                        std::cout << "A";
                        break;
                    case DNSRecordType::AAAA:
                        std::cout << "AAAA";
                        break;
                    case DNSRecordType::CNAME:
                        std::cout << "CNAME";
                        break;
                    case DNSRecordType::MX:
                        std::cout << "MX";
                        break;
                    case DNSRecordType::NS:
                        std::cout << "NS";
                        break;
                    case DNSRecordType::TXT:
                        std::cout << "TXT";
                        break;
                    default:
                        std::cout << static_cast<int>(record.type);
                        break;
                }

                std::cout << std::endl;
                std::cout << "TTL: " << record.ttl << " seconds" << std::endl;
                std::cout << "Data: " << record.data << std::endl;
                std::cout << "------------------------" << std::endl;
            }
        }

        // 清理libcurl资源
        curl_global_cleanup();
        Logger::debug("CURL cleaned up");
        
        Logger::info("DoH Client finished successfully");
        Logger::shutdown();
        return 0;
        
    } catch (const DoHException &e) {
        Logger::error("DoH Error [{}]: {} (Context: {})", e.code(), e.what(), e.context());
        std::cerr << "DoH Error: " << e.what() << std::endl;
        Logger::shutdown();
        return 1;
    } catch (const std::exception &e) {
        Logger::error("Unexpected error: {}", e.what());
        std::cerr << "Exception: " << e.what() << std::endl;
        Logger::shutdown();
        return 1;
    }
}