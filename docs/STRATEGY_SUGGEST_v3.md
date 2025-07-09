# DNS 客户端设计文档 V3

## 使用场景

我们项目是头文件库，被其他应用集成和使用，提供基本的 DNS 请求服务。

## 线程池工具类设计

### 1. 轻量级线程池接口

**目标**: 提供一个简单高效的线程池，专门用于DNS并发查询
**特点**: 支持任务取消、快速响应、资源控制

```cpp
class DNSThreadPool {
public:
    explicit DNSThreadPool(size_t thread_count = std::thread::hardware_concurrency());
    ~DNSThreadPool();
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
        
private:
    // 实现细节隐藏
};
```

### 2. 任务取消机制

**功能**: 当获得第一个成功结果后，能快速取消其他DNS查询任务
**实现**: 使用共享的取消标志和超时控制

```cpp
class CancellationToken {
public:
    void cancel();
    bool is_cancelled() const;
    
    template<class Rep, class Period>
    bool wait_for_cancel(const std::chrono::duration<Rep, Period>& timeout);
    
private:
    // 实现细节隐藏
};
```

## 核心功能

### 1. 并发请求

**功能**: 并发请求指定 DNS 列表，通常 3-5 个
**实现**: 使用线程池异步请求，第一个成功后立即取消其他任务

```cpp
struct DNSRecord {
    enum Type {
        A, AAAA, CNAME, MX, TXT, NS, PTR
    };
    
    Type type;
    std::string name;      // 查询的域名
    std::string value;     // 记录值(IP地址等)
    uint32_t ttl;         // 生存时间
    uint16_t priority = 0; // MX记录优先级
};

struct DNSServer {
    std::string url;
    int priority = 5;  // 1-9, 数字越大优先级越高
};

// 查询结果封装
class DNSQueryResult {
public:
    std::vector<DNSRecord> records;
    bool success = false;
    std::string error_message;
    std::chrono::milliseconds response_time{0};
};

// 并发请求接口
DNSQueryResult concurrent_query(
    const std::vector<DNSServer>& servers, 
    const std::string& domain,
    DNSRecord::Type record_type = DNSRecord::A,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)
);
```

### 2. 选择响应最快的结果

**功能**: 使用第一个返回成功结果的 DNS 服务器响应
**实现**: 使用线程池并发查询，第一个成功后立即取消其他任务

**关键设计点**:
- 启动多个并发DNS查询任务
- 使用共享的取消令牌机制
- 第一个成功结果返回后立即取消其他查询
- 支持超时控制

### 3. 系统 DNS 回退

**功能**: 所有 DNS 请求失败时，使用系统 DNS (`getaddrinfo`)
**触发条件**: 所有配置的 DNS 服务器都超时或失败

### 4. HTTP/2 性能优化

**功能**: 使用 HTTP/2 进行 DoH 请求，提高性能
**优势**: 连接复用，减少握手开销

### 5. 人工优先级设置

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
3. **最快响应**: 使用第一个成功返回的 DNS 结果，立即取消其他查询
4. **系统回退**: 如果所有 DNS 都失败，使用系统 DNS

### 主要接口类

```cpp
class SmartDNSResolver {
public:
    explicit SmartDNSResolver(const std::vector<DNSServer>& servers = {
        {"https://223.5.5.5/dns-query", 9},
        {"https://1.1.1.1/dns-query", 8}, 
        {"https://8.8.8.8/dns-query", 7}
    });
    
    // 基础查询接口
    std::vector<DNSRecord> resolve(
        const std::string& domain, 
        DNSRecord::Type type = DNSRecord::A,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)
    );
    
    // 便利方法：直接获取第一个A记录的IP
    std::string resolve_ip(const std::string& domain);
    
    // 获取详细的查询结果（包含错误信息和响应时间）
    DNSQueryResult resolve_detailed(
        const std::string& domain, 
        DNSRecord::Type type = DNSRecord::A,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(2000)
    );
    
private:
    std::vector<DNSServer> dns_servers_;
    // 其他私有成员和方法
};
```

## 配置示例

```cpp
// 简单配置
DNSResolverConfig config {
    .servers = {
        {"https://223.5.5.5/dns-query", 9},
        {"https://1.1.1.1/dns-query", 8}, 
        {"https://8.8.8.8/dns-query", 7}
    },
    .concurrent_limit = 3,
    .timeout_ms = 2000,
    .enable_http2 = true
};

// 使用示例
SmartDNSResolver resolver(config.servers);
auto ip = resolver.resolve_ip("example.com");
auto detailed_result = resolver.resolve_detailed("example.com", DNSRecord::A);
```

## 技术实现要点

### 1. 线程池设计

- **线程复用**: 使用静态线程池，避免频繁创建销毁线程
- **任务队列**: 使用条件变量实现高效的任务分发
- **优雅关闭**: 析构时等待所有任务完成再退出

### 2. 取消机制设计

- **共享取消标志**: 多个查询任务共享同一个取消令牌
- **快速响应**: 第一个成功结果后立即取消其他任务
- **资源清理**: 确保被取消的任务能正确释放资源

### 3. 错误处理和监控

```cpp
// 错误信息结构
struct DNSError {
    std::string server_url;
    std::string error_message;
    std::chrono::milliseconds response_time;
};

// 性能统计接口
struct DNSStats {
    uint64_t total_queries;
    uint64_t successful_queries;
    uint64_t failed_queries;
    uint64_t cancelled_queries;
    
    double get_success_rate() const;
    double get_average_response_time() const;
};
```

## 实现要点

- **简单高效**: 专注核心功能，避免过度复杂
- **快速响应**: 线程池并发 + 最快响应 + 立即取消策略
- **可靠回退**: DNS失败 → 系统DNS → 错误报告的多级回退
- **可配置**: 支持优先级、超时、并发数等参数配置
- **监控友好**: 提供详细的性能统计和错误信息
