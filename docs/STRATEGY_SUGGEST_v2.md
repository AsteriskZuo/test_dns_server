# DNS 客户端设计文档 V2

## 使用场景

我们项目是头文件库，被其他应用集成和使用，提供基本的 DNS 请求服务。

## 核心功能

### 1. 并发请求

**功能**: 并发请求指定 DNS 列表，通常 3-5 个
**实现**: 使用异步请求，同时向多个 DNS 服务器发送查询

```cpp
struct DNSServer {
    std::string url;
    int priority = 5;  // 1-9, 数字越大优先级越高
};

// 并发请求多个DNS服务器
std::vector<std::future<DNSResult>> concurrent_query(
    const std::vector<DNSServer>& servers, 
    const std::string& domain
);
```

### 2. 选择响应最快的结果

**功能**: 使用第一个返回成功结果的 DNS 服务器响应
**实现**: 哪个 DNS 服务器最先返回有效结果就使用哪个

### 3. 系统 DNS 回退

**功能**: 所有 DNS 请求失败时，使用系统 DNS (`getaddrinfo`)
**触发条件**: 所有配置的 DNS 服务器都超时或失败

### 4. 结果延迟测试

**功能**: 对 DNS 返回的多个 IP 地址进行延迟测试，选择最快的
**实现**: 并发测试所有返回的 IP 地址延迟

```cpp
struct IPLatencyTest {
    std::string ip;
    std::chrono::milliseconds latency;
    bool is_reachable = false;
};

// 选择延迟最小的IP
std::string select_fastest_ip(const std::vector<std::string>& ips) {
    // TCP连接测试，选择延迟最小且可达的IP
    // 如果测试失败，返回列表第一个IP
}
```

### 5. 延迟测试失败处理

**功能**: 如果延迟测试失败，选择 IP 列表的第一个结果
**目的**: 保证始终能返回可用的 IP 地址

### 6. HTTP/2 性能优化

**功能**: 使用 HTTP/2 进行 DoH 请求，提高性能
**优势**: 连接复用，减少握手开销

### 7. 人工优先级设置

**功能**: 支持手动设置 DNS 服务器优先级
**用途**: 根据网络环境或区域优化 DNS 选择

```cpp
// DNS服务器优先级配置
std::vector<DNSServer> dns_servers = {
    {"https://223.5.5.5/dns-query", 9},     // 高优先级
    {"https://1.1.1.1/dns-query", 8},       // 中优先级  
    {"https://8.8.8.8/dns-query", 7}        // 低优先级
};
```

## 实现流程

### 完整查询流程

1. **优先级排序**: 按人工设置的优先级排序 DNS 服务器列表
2. **并发请求**: 同时向前 3-5 个 DNS 服务器发送请求
3. **最快响应**: 使用第一个成功返回的 DNS 结果
4. **IP 选择**: 对返回的多个 IP 进行延迟测试
5. **选择最优**: 选择延迟最小的 IP，测试失败则选第一个
6. **系统回退**: 如果所有 DNS 都失败，使用系统 DNS

### 代码示例

```cpp
class SimpleDNSClient {
public:
    std::string resolve(const std::string& domain) {
        // 1. 按优先级排序DNS服务器
        auto sorted_servers = sort_by_priority(dns_servers_);
        
        // 2. 并发请求DNS
        auto dns_results = concurrent_query(sorted_servers, domain);
        
        // 3. 获取第一个成功结果
        auto ips = get_first_success_result(dns_results);
        
        // 4. 如果DNS都失败，使用系统DNS
        if (ips.empty()) {
            ips = system_dns_query(domain);
        }
        
        // 5. 从IP列表中选择最优IP
        return select_fastest_ip(ips);
    }
    
private:
    std::vector<DNSServer> dns_servers_;
};
```

## 配置示例

```cpp
// 简单配置
DNSClientConfig config {
    .servers = {
        {"https://223.5.5.5/dns-query", 9},
        {"https://1.1.1.1/dns-query", 8}, 
        {"https://8.8.8.8/dns-query", 7}
    },
    .concurrent_limit = 3,
    .timeout_ms = 2000,
    .enable_http2 = true
};
```

## 实现要点

- **简单高效**: 专注核心功能，避免过度复杂
- **快速响应**: 并发请求 + 最快响应策略
- **可靠回退**: 多级回退保证可用性  
- **智能选择**: 延迟测试选择最优 IP
- **可配置**: 支持优先级和基本参数配置
