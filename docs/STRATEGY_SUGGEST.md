# DNS客户端请求策略设计文档

## 1. 基础查询策略

### 基础策略 1: 多服务器并发访问
**原理**: 同时向多个DNS服务器发送请求，取最快响应
**优势**: 
- 大幅降低延迟（通常减少50-80%）
- 提高可用性和容错能力
- 自动选择最优服务器

**实现要点**:
```cpp
// 并发请求，取最快响应
std::vector<std::future<DNSResult>> futures;
for (auto& server : servers) {
    futures.push_back(std::async(std::launch::async, [&]() {
        return server.query(domain);
    }));
}
```

### 基础策略 2: 智能重试 + 回退
**原理**: 失败时按策略重试，多级回退机制
**优势**: 
- ✅ 提高成功率（从95%提升到99%+）
- ✅ 处理网络抖动和临时故障
- ✅ 可与并发策略组合使用

**修正**: ~~时间长（不太推荐）~~ → **推荐与并发组合使用**

**实现策略**:
```cpp
// 重试策略: 指数退避 + 最大重试次数
RetryPolicy policy{
    .max_retries = 3,
    .base_delay = 100ms,
    .max_delay = 2000ms,
    .backoff_multiplier = 2.0
};

// 回退链: DoH -> DoT -> 系统DNS
FallbackChain chain{
    DoHMethod::JSON_GET,
    DoHMethod::POST, 
    SystemDNS
};
```

### 基础策略 3: 系统DNS回退
**原理**: DoH失败时回退到系统DNS
**优势**: 保证基础可用性
**适用场景**: 网络受限环境、企业内网

### 🆕 基础策略 4: 负载均衡策略
**新增策略**:
- **轮询 (Round Robin)**: 均匀分配请求
- **加权轮询**: 根据服务器性能分配
- **最少连接**: 选择当前连接数最少的服务器
- **一致性哈希**: 相同域名路由到相同服务器

### 🆕 基础策略 5: 安全验证策略
**新增安全考虑**:
- 域名白名单/黑名单验证
- DoH服务器证书验证
- 查询结果安全性检查
- 防DNS劫持机制

### 🆕 基础策略 6: 智能路由策略
**原理**: 根据网络环境动态选择最优DNS服务器
**策略类型**:
- **地理位置就近选择**: 选择物理距离最近的服务器
- **网络延迟动态路由**: 实时测量延迟，选择最快服务器
- **ISP优化路由**: 根据运营商特性选择最优服务器
- **CDN感知路由**: 选择能提供最优CDN节点的DNS服务器

**应用场景**:
- 全球化应用的地域优化
- 企业网络的ISP适配
- CDN加速应用的性能优化

**详细实现**: 参见 [智能路由策略详细设计](INTELLIGENT_ROUTING.md)

## 2. 性能优化策略

### 性能策略 1: 智能缓存
**缓存策略**:
- **TTL管理**: 遵循DNS记录TTL，支持最小/最大TTL限制
- **缓存大小**: 推荐10000-50000条记录
- **LRU替换**: 最近最少使用算法
- **分层缓存**: 内存缓存 + 持久化缓存

**推荐配置**:
```cpp
CacheConfig cache_config{
    .max_entries = 10000,
    .default_ttl = 300s,    // 5分钟
    .min_ttl = 60s,         // 最小1分钟
    .max_ttl = 3600s,       // 最大1小时
    .enable_persistence = true
};
```

### 性能策略 2: 智能预解析
**补充完善预解析策略**:

**时机分析**:
- ✅ **应用启动**: 预解析常用域名(API、CDN等)
- ✅ **用户行为**: 基于访问模式预测
- ✅ **时间窗口**: 根据使用习惯定时预解析
- ✅ **缓存预刷新**: TTL到期前主动更新

**实现策略**:
```cpp
// 启动时预解析
std::vector<std::string> startup_domains = {
    "api.example.com", "cdn.example.com", "fonts.googleapis.com"
};

// 智能预测预解析
class PredictivePrefetch {
    void analyze_access_pattern();
    void schedule_prefetch_based_on_frequency();
    void time_window_prefetch(); // 早上、工作时间、晚上不同域名
};
```

### 🆕 性能策略 3: 连接复用
**HTTP/2支持** (对应你的11.0.0版本):
- 单连接多路复用
- 减少握手开销
- 支持HTTP/1.1回退

### 🆕 性能策略 4: 批量查询优化
- 合并多个查询请求
- 减少网络往返次数
- 提高查询吞吐量

## 3. 组合策略分析

### 修正: 并发 + 重试的组合使用

**原有观点问题**: 
~~"多服务并发请求 和 智能重试，还是有点冲突的，并发本身全部失败的概率就很小"~~

**正确的组合策略**:
```cpp
// 推荐组合: 并发 + 有限重试
class CombinedStrategy {
    // 第一轮: 并发查询多个服务器
    auto results = concurrent_query(servers, domain);
    
    if (results.empty()) {
        // 第二轮: 对失败的服务器进行重试
        auto retry_results = retry_failed_servers(failed_servers, domain);
        
        if (retry_results.empty()) {
            // 第三轮: 系统DNS回退
            return system_dns_fallback(domain);
        }
    }
    
    return select_best_result(results);
}
```

**组合优势**:
- 正常情况: 并发获得最快响应
- 异常情况: 重试提高成功率
- 极端情况: 系统DNS保底

### 推荐的策略组合

**🌟 推荐组合1**: 高性能场景
```
并发查询 + 智能缓存 + 预解析 + 连接复用
```

**🌟 推荐组合2**: 高可用场景  
```
并发查询 + 智能重试 + 多级回退 + 负载均衡
```

**🌟 推荐组合3**: 企业级场景
```
负载均衡 + 安全验证 + 智能缓存 + 性能监控
```

## 4. 结果处理策略

### 实用性评估和改进

**🟢 高实用性策略**:
- ✅ **策略2**: 人工指定优先级 - 必需功能
- ✅ **策略4**: 延迟时间排序 - 核心性能指标  
- ✅ **策略6**: 成功率比较排序 - 可靠性保证

**🟡 中等实用性策略**:
- ⚠️ **策略3**: 地理位置排序 - 需要额外的地理位置服务
- ⚠️ **策略1**: 随机选择 - 适用于负载均衡场景

**🔴 低实用性策略**:
- ❌ **策略5**: 稳定性排序 - 需要长期统计，实现复杂
- ❌ **策略7**: 路由数量排序 - 测试成本高，实际意义有限

### 🆕 补充实用策略

**策略8**: **健康状态检查**
```cpp
struct ServerHealth {
    bool is_available = true;
    std::chrono::milliseconds avg_response_time{0};
    double success_rate = 1.0;
    std::chrono::system_clock::time_point last_check;
};
```

**策略9**: **动态权重调整**
```cpp
// 根据实时性能动态调整权重
class DynamicWeightAdjuster {
    void adjust_weights_based_on_performance();
    void penalize_poor_performers();
    void boost_good_performers();
};
```

## 5. 综合权重计算

### 权重计算公式

**基础权重维度**:
```cpp
struct ServerWeights {
    float manual_priority;    // 0-9 (管理员设置)
    float latency_score;      // 0-10 (延迟转换分数)
    float success_rate;       // 0-1 (历史成功率)
    float availability;       // 0-1 (可用性)
    float geo_proximity;      // 0-1 (地理位置接近度)
};
```

**推荐权重公式**:
```cpp
float calculate_final_weight(const ServerWeights& w) {
    // 归一化处理
    float norm_priority = w.manual_priority / 9.0f;
    float norm_latency = w.latency_score / 10.0f;
    
    // 加权计算 (权重系数可配置)
    return norm_priority * 0.4f +           // 人工优先级 40%
           norm_latency * 0.3f +            // 延迟性能 30%
           w.success_rate * 0.2f +          // 成功率 20%
           w.availability * 0.1f;           // 可用性 10%
}
```

### 🆕 动态权重调整机制

**时间敏感调整**:
- 高峰期增加延迟权重
- 非高峰期增加地理位置权重
- 网络异常时增加可用性权重

**自适应学习**:
- 根据历史表现调整权重
- 机器学习优化权重参数
- A/B测试验证权重效果

## 6. 实现建议

### 配置文件结构
```yaml
dns_strategy:
  query_strategy:
    type: "concurrent"          # concurrent/sequential/load_balanced
    concurrent_servers: 3
    enable_retry: true
    max_retries: 2
    
  cache_strategy:
    enable: true
    max_entries: 10000
    default_ttl: 300
    
  prefetch_strategy:
    enable: true
    startup_domains: ["api.example.com"]
    predictive: true
    
  weight_config:
    manual_priority_factor: 0.4
    latency_factor: 0.3
    success_rate_factor: 0.2
    availability_factor: 0.1
```

### 性能监控
```cpp
class StrategyMetrics {
    std::atomic<size_t> total_queries{0};
    std::atomic<size_t> cache_hits{0};
    std::atomic<size_t> concurrent_wins{0};
    std::chrono::milliseconds avg_response_time{0};
    
    void report_performance();
    void suggest_optimization();
};
```

## 7. 总结

### 核心推荐策略
1. **并发查询** - 大幅提升性能
2. **智能缓存** - 减少网络请求  
3. **权重排序** - 动态选择最优服务器
4. **回退机制** - 保证基础可用性
5. **预解析** - 提升用户体验

### 实现优先级
- **P0**: 并发查询 + 基础缓存 + 系统DNS回退
- **P1**: 权重计算 + 智能重试 + 健康检查
- **P2**: 预解析 + 连接复用 + 性能监控
