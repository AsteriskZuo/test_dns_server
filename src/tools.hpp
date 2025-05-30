#ifndef TOOLS_HPP
#define TOOLS_HPP

#include <iomanip>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

// DNS记录类型枚举
enum class DNSRecordType { A = 1, AAAA = 28, CNAME = 5, MX = 15, NS = 2, TXT = 16 };

// DNS查询结果结构
struct DNSRecord {
    std::string name;    // 域名
    DNSRecordType type;  // 记录类型
    uint32_t ttl;        // 生存时间
    std::string data;    // 记录数据
};

// Base64URL编码实现
inline std::string base64url_encode(const std::string &input) {
    static const std::string base64url_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";

    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    const char *bytes_to_encode = input.c_str();
    int in_len = input.length();
    int pos = 0;

    while (in_len--) {
        char_array_3[i++] = bytes_to_encode[pos++];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                ret += base64url_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; j < i + 1; j++) {
            ret += base64url_chars[char_array_4[j]];
        }
    }

    // 根据base64url规范，不添加填充字符'='
    return ret;
}

// DNS消息创建函数 - 生成简单的DNS查询请求
inline std::string create_dns_query_message(const std::string &domain, uint16_t query_type = 1) {
    std::string message;

    // Header section (12 bytes)
    uint16_t id = 0x1234;                                    // 随机ID
    message.push_back(static_cast<char>((id >> 8) & 0xFF));  // ID (高位)
    message.push_back(static_cast<char>(id & 0xFF));         // ID (低位)

    uint16_t flags = 0x0100;                                    // 标准查询，递归
    message.push_back(static_cast<char>((flags >> 8) & 0xFF));  // 标志 (高位)
    message.push_back(static_cast<char>(flags & 0xFF));         // 标志 (低位)

    message.push_back(0);  // QDCOUNT (高位) - 问题计数 = 1
    message.push_back(1);  // QDCOUNT (低位)

    message.push_back(0);  // ANCOUNT (高位) - 回答计数 = 0
    message.push_back(0);  // ANCOUNT (低位)

    message.push_back(0);  // NSCOUNT (高位) - 名称服务器计数 = 0
    message.push_back(0);  // NSCOUNT (低位)

    message.push_back(0);  // ARCOUNT (高位) - 额外记录计数 = 0
    message.push_back(0);  // ARCOUNT (低位)

    // Question section - 域名编码
    std::istringstream domainStream(domain);
    std::string label;
    while (std::getline(domainStream, label, '.')) {
        message.push_back(static_cast<char>(label.length()));
        message.append(label);
    }
    message.push_back(0);  // 0长度表示域名结束

    // QTYPE
    message.push_back(static_cast<char>((query_type >> 8) & 0xFF));  // QTYPE (高位)
    message.push_back(static_cast<char>(query_type & 0xFF));         // QTYPE (低位)

    // QCLASS = 1 (IN - Internet)
    message.push_back(0);  // QCLASS (高位)
    message.push_back(1);  // QCLASS (低位)

    return message;
}

// 解析DNS wireformat响应 - 用于RFC 8484 API响应
inline std::vector<DNSRecord> parse_dns_wireformat_response(const std::string &response) {
    std::vector<DNSRecord> records;
    std::cout << "Received binary DNS response, length: " << response.length() << " bytes" << std::endl;

    // 检查响应格式是否合法
    if (response.length() < 12) {  // DNS header is 12 bytes
        std::cerr << "DNS response too short" << std::endl;
        return records;
    }

    // 解析DNS Header
    const unsigned char *dns = reinterpret_cast<const unsigned char *>(response.data());

    // 提取事务ID (Transaction ID)
    uint16_t transactionId = (dns[0] << 8) | dns[1];

    // 提取标志 (Flags)
    uint16_t flags = (dns[2] << 8) | dns[3];
    bool isResponse = (flags & 0x8000) != 0;  // QR bit

    // 提取问题数 (Question Count)
    uint16_t qdcount = (dns[4] << 8) | dns[5];

    // 提取回答数 (Answer Count)
    uint16_t ancount = (dns[6] << 8) | dns[7];

    // 提取权威域名服务器数 (Authority Count)
    uint16_t nscount = (dns[8] << 8) | dns[9];

    // 提取附加记录数 (Additional Count)
    uint16_t arcount = (dns[10] << 8) | dns[11];

    std::cout << "DNS Header: ID=" << transactionId << ", Response=" << (isResponse ? "Yes" : "No")
              << ", QDCount=" << qdcount << ", ANCount=" << ancount << ", NSCount=" << nscount
              << ", ARCount=" << arcount << std::endl;

    // 如果不是响应，返回空
    if (!isResponse) {
        std::cerr << "Not a DNS response" << std::endl;
        return records;
    }

    // 跳过问题部分
    // 注意：这是一个简化实现，完整实现需要正确解析压缩的域名
    size_t offset = 12;  // 跳过 header

    // 遍历问题部分
    for (int i = 0; i < qdcount && offset < response.length(); ++i) {
        // 跳过问题中的域名
        while (offset < response.length() && dns[offset] != 0) {
            offset += dns[offset] + 1;  // 标签长度 + 标签长度字节本身
        }
        offset += 1;  // 跳过 null 终止符

        // 跳过问题中的 QTYPE 和 QCLASS（各2字节）
        offset += 4;
    }

    // 解析回答部分
    for (int i = 0; i < ancount && offset + 12 <= response.length(); ++i) {
        DNSRecord record;

        // 解析名称（这里是简化处理，真实DNS实现需要处理名称压缩）
        std::string name;
        if (dns[offset] & 0xC0) {  // 如果是压缩指针
            // 压缩指针占2字节
            uint16_t pointer = ((dns[offset] & 0x3F) << 8) | dns[offset + 1];
            // 实际应该跟随指针获取名称，这里简化处理
            name = "compressed.name";
            offset += 2;
        } else {
            // 未压缩的名称，简化处理
            name = "uncompressed.name";
            // 跳过域名
            while (offset < response.length() && dns[offset] != 0) {
                offset += dns[offset] + 1;
            }
            offset += 1;  // 跳过终止符
        }
        record.name = name;

        // 获取记录类型
        if (offset + 2 <= response.length()) {
            uint16_t recordType = (dns[offset] << 8) | dns[offset + 1];
            record.type = static_cast<DNSRecordType>(recordType);
            offset += 2;
        } else {
            break;
        }

        // 跳过记录类
        offset += 2;

        // 获取TTL（4字节）
        if (offset + 4 <= response.length()) {
            record.ttl = (dns[offset] << 24) | (dns[offset + 1] << 16) | (dns[offset + 2] << 8) | dns[offset + 3];
            offset += 4;
        } else {
            break;
        }

        // 获取数据长度
        if (offset + 2 <= response.length()) {
            uint16_t dataLength = (dns[offset] << 8) | dns[offset + 1];
            offset += 2;

            // 确保有足够的数据
            if (offset + dataLength <= response.length()) {
                // 根据记录类型解析数据
                switch (record.type) {
                    case DNSRecordType::A:
                        if (dataLength == 4) {  // IPv4地址是4字节
                            char ip[16];
                            snprintf(ip, sizeof(ip), "%d.%d.%d.%d", dns[offset], dns[offset + 1], dns[offset + 2],
                                     dns[offset + 3]);
                            record.data = ip;
                        }
                        break;

                    case DNSRecordType::AAAA:
                        if (dataLength == 16) {  // IPv6地址是16字节
                            char ip[40];
                            snprintf(ip, sizeof(ip),
                                     "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                                     dns[offset], dns[offset + 1], dns[offset + 2], dns[offset + 3], dns[offset + 4],
                                     dns[offset + 5], dns[offset + 6], dns[offset + 7], dns[offset + 8],
                                     dns[offset + 9], dns[offset + 10], dns[offset + 11], dns[offset + 12],
                                     dns[offset + 13], dns[offset + 14], dns[offset + 15]);
                            record.data = ip;
                        }
                        break;

                    default:
                        // 对于其他记录类型，返回十六进制数据
                        std::stringstream ss;
                        for (size_t j = 0; j < dataLength; ++j) {
                            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(dns[offset + j]);
                        }
                        record.data = ss.str();
                        break;
                }

                offset += dataLength;
            } else {
                break;
            }
        } else {
            break;
        }

        records.push_back(record);
    }

    return records;
}

// 解析JSON格式响应 - 用于Google JSON API响应
inline std::vector<DNSRecord> parse_json_response(const std::string &response) {
    std::vector<DNSRecord> records;
    std::cout << "Raw response: " << response << std::endl;  // 输出原始响应

    try {
        auto jsonResponse = json::parse(response);
        std::cout << "Response: " << jsonResponse.dump(4) << std::endl;  // 输出格式化的JSON响应

        // 检查是否有Answer字段
        if (jsonResponse.contains("Answer") && jsonResponse["Answer"].is_array()) {
            for (const auto &answer : jsonResponse["Answer"]) {
                DNSRecord record;
                record.name = answer["name"].get<std::string>();
                record.type = static_cast<DNSRecordType>(answer["type"].get<int>());
                record.ttl = answer["TTL"].get<uint32_t>();

                // 根据记录类型解析data字段
                switch (record.type) {
                    case DNSRecordType::A:
                        record.data = answer["data"].get<std::string>();
                        break;
                    case DNSRecordType::AAAA:
                        record.data = answer["data"].get<std::string>();
                        break;
                    case DNSRecordType::CNAME:
                    case DNSRecordType::NS:
                        // 移除末尾的点号(如果有)
                        record.data = answer["data"].get<std::string>();
                        if (!record.data.empty() && record.data.back() == '.') {
                            record.data.pop_back();
                        }
                        break;
                    default:
                        record.data = answer["data"].dump();
                        break;
                }

                records.push_back(record);
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
    }

    return records;
}

inline void print_usage(const char *programName) {
    std::cout << "Usage: " << programName << " [OPTIONS] [domain]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -d, --domain <domain>     Domain name to query (e.g., example.com)" << std::endl;
    std::cout << "  --server <url>            DoH server URL" << std::endl;
    std::cout << "  --method <method>         DoH method: get, post, or json (default: json)" << std::endl;
    std::cout << "  --log-level <level>       Log level: trace, debug, info, warn, error, critical" << std::endl;
    std::cout << "  --timeout <seconds>       Request timeout in seconds" << std::endl;
    std::cout << "  --no-fallback             Disable system DNS fallback" << std::endl;
    std::cout << "  --config <file>           Configuration file path" << std::endl;
    std::cout << "  -h, --help                Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " example.com                    # Query example.com with defaults"
              << std::endl;
    std::cout << "  " << programName << " --domain example.com          # Same as above" << std::endl;
    std::cout << "  " << programName << " -d example.com --method post  # Use POST method" << std::endl;
    std::cout << "  " << programName << " --domain example.com --server https://dns.google/dns-query" << std::endl;
    std::cout << "  " << programName << " --log-level debug --domain example.com" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: For backward compatibility, the first non-option argument is treated as domain name."
              << std::endl;
}

#endif  // TOOLS_HPP
