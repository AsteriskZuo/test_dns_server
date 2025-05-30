#ifndef DOH_CLIENT_HPP
#define DOH_CLIENT_HPP

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// 系统DNS相关头文件
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "tools.hpp"

// 使用nlohmann的JSON库解析Cloudflare的DoH响应
using json = nlohmann::json;

// 创建 dns server list
// DoH服务器列表
inline std::vector<std::string> dohServers = {
    "https://cloudflare-dns.com/dns-query",  // ok
    "https://dns.google/dns-query",          // only support get and post
    "https://dns.google/resolve",            // only support json
    "https://dns.quad9.net/dns-query",       // ok
    "https://doh.opendns.com/dns-query",     // ok
    "https://223.5.5.5/dns-query",           // ok
    "https://doh.360.cn/dns-query",          // ok
    "https://doh.pub/dns-query",             // ok
    "https://9.9.9.9/dns-query",             // ok
    "https://8.8.8.8/dns-query",             // not support
    "https://1.1.1.1/dns-query",             // ok
    "http://dns.alidns.com/resolve",         // only support json
    "https://dns.alidns.com/dns-query",      // only support get and post
};

// 回调函数，用于处理curl接收到的数据
inline size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s) {
    size_t newLength = size * nmemb;
    try {
        s->append((char *)contents, newLength);
    } catch (std::bad_alloc &e) {
        return 0;
    }
    return newLength;
}

// DoH 查询方法枚举
enum class DoHMethod {
    GET,      // RFC 8484 GET - 使用二进制DNS消息，Base64URL编码
    POST,     // RFC 8484 POST - 使用二进制DNS消息，直接POST
    JSON_GET  // Google JSON API - 使用JSON格式，GET请求
};

template <typename T = void>
class DoHClientImpl {
   private:
    std::string dohServer;  // DoH服务器URL
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl;

   public:
    // 构造函数，初始化curl和DoH服务器
    explicit DoHClientImpl(const std::string &server = "https://cloudflare-dns.com/dns-query")
        : dohServer(server), curl(curl_easy_init(), curl_easy_cleanup) {
        if (!curl) {
            throw std::runtime_error("Failed to initialize curl");
        }
        // 设置通用选项
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

        // 设置超时选项
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 10L);            // 总超时时间：10秒
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);      // 连接超时：5秒
        curl_easy_setopt(curl.get(), CURLOPT_DNS_CACHE_TIMEOUT, 60L);  // DNS缓存：60秒

        // 设置用户代理
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "DoH-Client/1.0");
    }

    // 执行DNS查询 - 根据指定的方法选择不同的查询方式，失败时自动fallback到系统DNS
    std::vector<DNSRecord> query(const std::string &domain, DNSRecordType type = DNSRecordType::A,
                                 DoHMethod method = DoHMethod::JSON_GET, bool enable_fallback = true) {
        std::cout << "Using method: " << method_to_string(method) << std::endl;

        std::vector<DNSRecord> results;

        // 尝试DoH查询
        switch (method) {
            case DoHMethod::GET:
                results = query_with_get(domain, type);
                break;
            case DoHMethod::POST:
                results = query_with_post(domain, type);
                break;
            case DoHMethod::JSON_GET:
                results = query_with_json_get(domain, type);
                break;
            default:
                std::cerr << "Unknown DoH method, falling back to JSON_GET" << std::endl;
                results = query_with_json_get(domain, type);
                break;
        }

        // 如果DoH查询失败且启用了fallback，则使用系统DNS
        if (results.empty() && enable_fallback) {
            std::cout << "DoH query failed, trying system DNS fallback..." << std::endl;
            results = query_with_system_dns(domain, type);
        }

        return results;
    }

    // 1. RFC 8484 GET 方法 - 使用DNS wireformat，通过GET请求和Base64URL编码
    std::vector<DNSRecord> query_with_get(const std::string &domain, DNSRecordType type = DNSRecordType::A) {
        // 创建DNS查询消息
        std::string dns_message = create_dns_query_message(domain, static_cast<uint16_t>(type));

        // Base64URL编码
        std::string encoded_dns = base64url_encode(dns_message);

        // 构建URL - RFC 8484规范：?dns=参数
        std::string url = dohServer + "?dns=" + encoded_dns;
        std::cout << "GET Request URL: " << url << std::endl;

        // 设置请求头 - RFC 8484需要的Content-Type和Accept
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/dns-message");

        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());

        // 重置为GET请求（默认）
        curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);

        // 存储响应
        std::string response;
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

        // 执行请求
        CURLcode res = curl_easy_perform(curl.get());
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "GET request failed: " << curl_easy_strerror(res) << std::endl;
            return {};
        }

        // 检查HTTP状态码
        long response_code;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200) {
            std::cerr << "HTTP error: " << response_code << std::endl;
            return {};
        }

        // 因为我们在请求中明确指定了Accept: application/dns-message
        // 所以期望服务器返回DNS wireformat格式的响应，直接使用DNS二进制格式解析
        std::cout << "Expecting DNS wireformat response" << std::endl;
        return parse_dns_wireformat_response(response);
    }

    // 2. RFC 8484 POST 方法 - 使用DNS wireformat，通过POST请求
    std::vector<DNSRecord> query_with_post(const std::string &domain, DNSRecordType type = DNSRecordType::A) {
        // 创建DNS查询消息
        std::string dns_message = create_dns_query_message(domain, static_cast<uint16_t>(type));

        // 构建URL
        std::string url = dohServer;
        std::cout << "POST Request URL: " << url << std::endl;

        // 设置请求头 - RFC 8484需要的Content-Type和Accept
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/dns-message");
        headers = curl_slist_append(headers, "Content-Type: application/dns-message");

        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());

        // 设置为POST请求并提供数据
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, dns_message.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, dns_message.length());

        // 存储响应
        std::string response;
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

        // 执行请求
        CURLcode res = curl_easy_perform(curl.get());
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "POST request failed: " << curl_easy_strerror(res) << std::endl;
            return {};
        }

        // 检查HTTP状态码
        long response_code;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200) {
            std::cerr << "HTTP error: " << response_code << std::endl;
            return {};
        }

        // 因为我们在请求中明确指定了Accept: application/dns-message和Content-Type: application/dns-message
        // 所以期望服务器返回DNS wireformat格式的响应，直接使用DNS二进制格式解析
        std::cout << "Expecting DNS wireformat response" << std::endl;
        return parse_dns_wireformat_response(response);
    }

    // 3. Google JSON API GET 方法 - 使用JSON格式的请求和响应
    std::vector<DNSRecord> query_with_json_get(const std::string &domain, DNSRecordType type = DNSRecordType::A) {
        // 构建URL - Google JSON API格式：?name=&type=
        std::string url = dohServer + "?name=" + domain + "&type=" + std::to_string(static_cast<int>(type));
        std::cout << "JSON GET Request URL: " << url << std::endl;

        // 设置请求头 - JSON API需要的Accept
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/dns-json");

        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());

        // 重置为GET请求（默认）
        curl_easy_setopt(curl.get(), CURLOPT_HTTPGET, 1L);

        // 存储响应
        std::string response;
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

        std::cout << "Expecting JSON response" << std::endl;

        // 执行请求
        CURLcode res = curl_easy_perform(curl.get());
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            std::cerr << "JSON GET request failed: " << curl_easy_strerror(res) << std::endl;
            return {};
        }

        // 检查HTTP状态码
        long response_code;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200) {
            std::cerr << "HTTP error: " << response_code << std::endl;
            return {};
        }

        std::cout << "Response length: " << response.length() << " bytes" << std::endl;

        // 解析JSON响应
        return parse_json_response(response);
    }

    // 4. 系统DNS fallback - 使用getaddrinfo作为备用方案
    std::vector<DNSRecord> query_with_system_dns(const std::string &domain, DNSRecordType type = DNSRecordType::A) {
        std::cout << "Using system DNS fallback for: " << domain << std::endl;

        std::vector<DNSRecord> records;

        // 目前只支持A记录的系统DNS查询
        if (type != DNSRecordType::A) {
            std::cerr << "System DNS fallback only supports A records" << std::endl;
            return records;
        }

        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;  // IPv4
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(domain.c_str(), nullptr, &hints, &result);
        if (status != 0) {
            std::cerr << "System DNS query failed: " << gai_strerror(status) << std::endl;
            return records;
        }

        // 遍历结果
        for (struct addrinfo *p = result; p != nullptr; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);

                DNSRecord record;
                record.name = domain;
                record.type = DNSRecordType::A;
                record.data = ip_str;
                record.ttl = 300;  // 默认TTL 5分钟

                records.push_back(record);
                std::cout << "System DNS result: " << domain << " -> " << ip_str << std::endl;
            }
        }

        freeaddrinfo(result);
        return records;
    }

   private:
    // 将方法枚举转换为字符串（用于日志）
    std::string method_to_string(DoHMethod method) const {
        switch (method) {
            case DoHMethod::GET:
                return "GET (RFC 8484)";
            case DoHMethod::POST:
                return "POST (RFC 8484)";
            case DoHMethod::JSON_GET:
                return "JSON GET (Google API)";
            default:
                return "Unknown";
        }
    }
};

using DoHClient = DoHClientImpl<>;

#endif  // DOH_CLIENT_HPP