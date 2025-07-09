#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// 平台特定的头文件
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <sys/select.h>
    #define CLOSE_SOCKET close
#endif

/**
 * @brief 跨平台域名解析器类
 */
class SystemDnsParser {
private:
    bool initialized;

public:
    SystemDnsParser() : initialized(false) {
        initializeNetwork();
    }

    ~SystemDnsParser() {
        cleanupNetwork();
    }

    /**
     * @brief 初始化网络库（Windows 需要）
     */
    bool initializeNetwork() {
#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }
#endif
        initialized = true;
        return true;
    }

    /**
     * @brief 清理网络库
     */
    void cleanupNetwork() {
        if (initialized) {
#ifdef _WIN32
            WSACleanup();
#endif
            initialized = false;
        }
    }

    /**
     * @brief 解析域名为 IP 地址列表
     * @param hostname 要解析的域名
     * @param port 端口号（可选）
     * @return IP 地址列表
     */
    std::vector<std::string> resolveDomain(const std::string& hostname, const std::string& port = "") {
        std::vector<std::string> ipAddresses;
        
        if (!initialized) {
            std::cerr << "Network not initialized" << std::endl;
            return ipAddresses;
        }

        struct addrinfo hints, *result, *ptr;
        int status;

        // 初始化 hints 结构
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;     // IPv4 和 IPv6
        hints.ai_socktype = SOCK_STREAM; // TCP
        hints.ai_protocol = IPPROTO_TCP;

        // 执行域名解析
        const char* service = port.empty() ? nullptr : port.c_str();
        status = getaddrinfo(hostname.c_str(), service, &hints, &result);

        if (status != 0) {
            handleError(status, hostname);
            return ipAddresses;
        }

        // 遍历结果链表
        for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            char ipStr[INET6_ADDRSTRLEN];
            void* addr;
            std::string ipVersion;
            int actualPort = 0;

            if (ptr->ai_family == AF_INET) {
                // IPv4
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)ptr->ai_addr;
                addr = &(ipv4->sin_addr);
                actualPort = ntohs(ipv4->sin_port);
                ipVersion = "IPv4";
            } else if (ptr->ai_family == AF_INET6) {
                // IPv6
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ptr->ai_addr;
                addr = &(ipv6->sin6_addr);
                actualPort = ntohs(ipv6->sin6_port);
                ipVersion = "IPv6";
            } else {
                continue;
            }

            // 转换 IP 地址为字符串
            if (inet_ntop(ptr->ai_family, addr, ipStr, sizeof(ipStr)) != nullptr) {
                std::string result = std::string(ipStr) + " (" + ipVersion + ")";
                if (actualPort > 0) {
                    char portStr[16];
                    sprintf(portStr, ":%d", actualPort);
                    result += portStr;
                }
                ipAddresses.push_back(result);
            }
        }

        freeaddrinfo(result);
        return ipAddresses;
    }

    /**
     * @brief 测试端口连接性
     * @param ip IP 地址
     * @param port 端口号
     * @return 是否可连接
     */
    bool testPortConnection(const std::string& ip, int port) {
        if (port <= 0) return false;

        struct addrinfo hints, *result;
        int status;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char portStr[16];
        sprintf(portStr, "%d", port);
        
        status = getaddrinfo(ip.c_str(), portStr, &hints, &result);
        if (status != 0) {
            return false;
        }

        int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (sock == -1) {
            freeaddrinfo(result);
            return false;
        }

        // 设置非阻塞模式和超时
        bool connected = false;
        status = connect(sock, result->ai_addr, result->ai_addrlen);
        
        if (status == 0) {
            connected = true;
        }
#ifndef _WIN32
        else if (errno == EINPROGRESS) {
            // 非阻塞连接，需要等待
            fd_set writefds;
            struct timeval timeout;
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);
            timeout.tv_sec = 3;  // 3秒超时
            timeout.tv_usec = 0;
            
            if (select(sock + 1, NULL, &writefds, NULL, &timeout) > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                    connected = true;
                }
            }
        }
#endif

        CLOSE_SOCKET(sock);
        freeaddrinfo(result);
        return connected;
    }

    /**
     * @brief 处理错误信息（跨平台）
     * @param errorCode 错误代码
     * @param hostname 域名
     */
    void handleError(int errorCode, const std::string& hostname) {
        std::cerr << "DNS resolution failed for '" << hostname << "': ";

#ifdef _WIN32
        // Windows 错误处理
        switch (errorCode) {
            case WSAHOST_NOT_FOUND:
                std::cerr << "Host not found (WSAHOST_NOT_FOUND)" << std::endl;
                break;
            case WSATRY_AGAIN:
                std::cerr << "Temporary failure in name resolution (WSATRY_AGAIN)" << std::endl;
                break;
            case WSANO_RECOVERY:
                std::cerr << "Non-recoverable failure in name resolution (WSANO_RECOVERY)" << std::endl;
                break;
            case WSANO_DATA:
                std::cerr << "Valid name, no data record of requested type (WSANO_DATA)" << std::endl;
                break;
            case WSAEINVAL:
                std::cerr << "Invalid argument (WSAEINVAL)" << std::endl;
                break;
            case WSA_NOT_ENOUGH_MEMORY:
                std::cerr << "Not enough memory (WSA_NOT_ENOUGH_MEMORY)" << std::endl;
                break;
            case WSAEAFNOSUPPORT:
                std::cerr << "Address family not supported (WSAEAFNOSUPPORT)" << std::endl;
                break;
            case WSATYPE_NOT_FOUND:
                std::cerr << "Service type not found (WSATYPE_NOT_FOUND)" << std::endl;
                break;
            case WSAESOCKTNOSUPPORT:
                std::cerr << "Socket type not supported (WSAESOCKTNOSUPPORT)" << std::endl;
                break;
            default:
                std::cerr << "Unknown error code: " << errorCode << std::endl;
                break;
        }
#else
        // Linux/Unix 错误处理
        switch (errorCode) {
            case EAI_NONAME:
                std::cerr << "Host not found (EAI_NONAME)" << std::endl;
                break;
            case EAI_AGAIN:
                std::cerr << "Temporary failure in name resolution (EAI_AGAIN)" << std::endl;
                break;
            case EAI_FAIL:
                std::cerr << "Non-recoverable failure in name resolution (EAI_FAIL)" << std::endl;
                break;
            case EAI_NODATA:
                std::cerr << "Valid name, no data record of requested type (EAI_NODATA)" << std::endl;
                break;
            case EAI_BADFLAGS:
                std::cerr << "Invalid flags (EAI_BADFLAGS)" << std::endl;
                break;
            case EAI_MEMORY:
                std::cerr << "Memory allocation failure (EAI_MEMORY)" << std::endl;
                break;
            case EAI_FAMILY:
                std::cerr << "Address family not supported (EAI_FAMILY)" << std::endl;
                break;
            case EAI_SERVICE:
                std::cerr << "Service not supported (EAI_SERVICE)" << std::endl;
                break;
            case EAI_SOCKTYPE:
                std::cerr << "Socket type not supported (EAI_SOCKTYPE)" << std::endl;
                break;
            default:
                std::cerr << gai_strerror(errorCode) << " (code: " << errorCode << ")" << std::endl;
                break;
        }
#endif
    }

    /**
     * @brief 测试域名解析并打印结果
     * @param hostname 域名
     * @param port 端口（可选）
     */
    void testDomainResolution(const std::string& hostname, const std::string& port = "") {
        std::cout << "\n=== 解析域名: " << hostname;
        if (!port.empty()) {
            std::cout << ":" << port;
        }
        std::cout << " ===" << std::endl;

        std::vector<std::string> ipAddresses = resolveDomain(hostname, port);
        
        if (ipAddresses.empty()) {
            std::cout << "未找到 IP 地址" << std::endl;
        } else {
            std::cout << "找到 " << ipAddresses.size() << " 个 IP 地址:" << std::endl;
            for (size_t i = 0; i < ipAddresses.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << ipAddresses[i];
                
                // 如果指定了端口，测试连接性
                if (!port.empty()) {
                    int portNum = atoi(port.c_str());
                    if (portNum > 0) {
                        // 提取IP地址（去掉协议版本信息）
                        std::string ipOnly = ipAddresses[i];
                        size_t pos = ipOnly.find(" (");
                        if (pos != std::string::npos) {
                            ipOnly = ipOnly.substr(0, pos);
                        }
                        
                        std::cout << " - ";
                        if (testPortConnection(ipOnly, portNum)) {
                            std::cout << "端口 " << port << " 可达";
                        } else {
                            std::cout << "端口 " << port << " 不可达";
                        }
                    }
                }
                std::cout << std::endl;
            }
        }
    }
};

/**
 * @brief 主函数 - 演示域名解析功能
 */
int main(int argc, char* argv[]) {
    std::cout << "=== 跨平台域名解析示例 ===" << std::endl;
    std::cout << "编译平台: ";
#ifdef _WIN32
    std::cout << "Windows" << std::endl;
#else
    std::cout << "Unix/Linux" << std::endl;
#endif

    SystemDnsParser parser;

    // 如果有命令行参数，解析指定的域名
    if (argc > 1) {
        std::string hostname = argv[1];
        std::string port = (argc > 2) ? argv[2] : "";
        parser.testDomainResolution(hostname, port);
    } else {
        // 默认测试一些常见域名
        std::vector<std::string> testDomains;
        testDomains.push_back("google.com");
        testDomains.push_back("github.com");
        testDomains.push_back("baidu.com");
        testDomains.push_back("localhost");
        testDomains.push_back("dns.google");
        testDomains.push_back("1.1.1.1");  // 测试 IP 地址反解析

        for (size_t i = 0; i < testDomains.size(); ++i) {
            parser.testDomainResolution(testDomains[i]);
        }

        // 测试带端口的解析
        std::cout << "\n=== 测试带端口的解析 ===" << std::endl;
        parser.testDomainResolution("google.com", "80");
        parser.testDomainResolution("github.com", "443");
    }

    std::cout << "\n=== 测试完成 ===" << std::endl;
    return 0;
}

// 编译命令
// 进入 cpp_study_note/test_dns_server 目录
// # Linux/macOS (C++98 兼容)
// g++ -o build/dns_parser examples/system_dns_parser_example.cpp

// # Linux/macOS (C++11 支持，如果需要)
// g++ -std=c++11 -o build/dns_parser examples/system_dns_parser_example.cpp

// # Windows (使用 MSVC)
// cl examples/system_dns_parser_example.cpp /Fe:build/dns_parser.exe

// # Windows (使用 MinGW, C++98 兼容)
// g++ -o build/dns_parser.exe examples/system_dns_parser_example.cpp -lws2_32

// # Windows (使用 MinGW, C++11 支持)
// g++ -std=c++11 -o build/dns_parser.exe examples/system_dns_parser_example.cpp -lws2_32


// 运行命令

// # 使用默认测试域名
// ./dns_parser

// # 解析指定域名
// ./dns_parser ap4-tls.agora.io

// # 解析指定域名和端口
// ./dns_parser ap4-tls.agora.io 443


// 错误码介绍