# 客户端 DNS 设计讨论

本文档将给出 dns 解析的设计方案。

## 核心设计

先调用系统 dns 方法， 解析失败后，在使用 doh 进行 dns 解析。

1. windows 平台使用 `getaddrinfo` 方法（`gethostbyname` 已经过期，不推荐使用）解析 dns
2. linux/unix 平台使用 `getaddrinfo` 方法 解析 dns
3. 调用 系统 dns 方法解析失败，根据错误码，进行分类处理。
4. 解析结果不做缓存，直接使用。
5. doh 基本配置：ip 地址、域名信息 放在代码中
6. doh 敏感信息：token、key 等，由服务器下发，非明文加密保存在文件中。
7. doh 解析成功或者失败 结果 记录在缓存，用来判断连接服务器失败之后是否还需要进行 doh 解析，避免循环调用。

### 系统 和 doh 都解析失败的情况

**原因分析**

系统 dns 解析失败的原因可能是:

- 网络问题
- dns 攻击

doh dns 解析失败的原因可能是:

- 网络问题
- 防火墙问题: 指定 ip 或者端口无法使用

失败原因组合：

- 网络问题
- dns 攻击 + 防火墙

**解决方案**

1. 网络问题
   1. 暂停网络访问，网络正常后再尝试
2. dns 攻击或者污染
   1. 使用 doh 解析
3. 防火墙问题: ip 或者 port 被封禁
   1. 开放 doh 需要的 port 和 ip

### 系统解析失败 和 doh 解析成功 的情况

**原因分析**

- dns 攻击

**解决方案**

// todo: 方案选择讨论点: done

1. 一段时间内，只使用 doh 解析 ✅
   1. 保证解析的正确性
   2. 如果使用免费 doh，需要考虑 429 错误
   3. 如果使用收费 doh，需要考虑 敏感信息安全、账户余额、token 时效 等问题
2. 每次都先尝试 系统，失败后，在尝试 doh
   1. 使用原有流程，简化实现复杂性
   2. 相比第一种方案，更少的、非并发的请求，预计 免费 doh 就可以满足需求

**一段时间内，只使用 doh 解析 的情况**

- timestamp: 过期时间戳

1. 一段时间内仅使用 doh 解析
2. 需要 缓存 + 持久化
3. 过期之后，时间戳设置为 0，表示恢复原来的 先系统 dns 解析，失败，再 doh 解析的流程

### 系统解析假成功 的情况

**原因分析**

系统解析 返回 错误的 IP 地址，使用错误 IP 连接服务器:

1. connect 连接失败
2. provision 客户端身份验证失败

_以上两种是已有业务逻辑_

**解决方案**

1. 使用 doh 进行域名解析
   1. 如果 doh 失败
      1. 暂停网络连接
   2. 如果 doh 成功
      1. 再次 connect 和 provision 成功
         1. 结束
      2. 再次 connect 或者 provision 失败，判断 doh 最近一次解析结果
         1. 成功：不尝试 doh 解析，走原有业务逻辑
         2. 失败：尝试 doh 解析

## 局部设计

### 错误码

**需要处理的错误码**

**Windows 平台**

- WSAHOST_NOT_FOUND # DNS 无法解析域名，可能是 DNS 劫持/污染
- WSATRY_AGAIN # DNS 服务器临时性故障
- WSANO_RECOVERY # DNS 服务器严重错误

**Linux/Unix 平台**

- EAI_NONAME # 对应 WSAHOST_NOT_FOUND
- EAI_AGAIN # 对应 WSATRY_AGAIN
- EAI_FAIL # 对应 WSANO_RECOVERY

重试原因：

- 可能是 ISP 的 DNS 被劫持
- 可能是 DNS 服务器故障
- 可能是网络环境问题
- DoH 可以绕过本地 DNS 解析

**忽略的错误码**

**Windows 平台**

- WSAEINVAL # 参数错误，代码 bug
- WSA_NOT_ENOUGH_MEMORY # 内存不足，系统问题
- WSAEAFNOSUPPORT # 系统不支持某地址族
- WSANO_DATA # 域名存在但无对应记录

**Linux/Unix 平台**

- EAI_MEMORY # 对应 WSA_NOT_ENOUGH_MEMORY
- EAI_NODATA # 对应 WSANO_DATA

不重试原因：

- 程序参数错误 → DoH 也会参数错误
- 系统内存不足 → DoH 请求也需要内存
- 系统不支持 → DoH 也无法解决系统限制
- 域名无记录 → 任何 DNS 都返回相同结果

#### Windows 平台错误码（WinSock）

1. **WSAHOST_NOT_FOUND**

- **字面含义**: 主机未找到
- **可能场景**:
  - 域名不存在或拼写错误
  - DNS 服务器无法解析该域名
  - 网络断开连接（如飞行模式、网线断开）
  - 防火墙阻止 DNS 查询
  - DNS 服务器配置错误或不可达
  - 本地 hosts 文件中没有对应条目且 DNS 解析失败

2. **WSATRY_AGAIN**

- **字面含义**: 临时性域名解析失败
- **可能场景**:
  - DNS 服务器暂时过载或响应缓慢
  - 网络拥塞导致 DNS 查询超时
  - DNS 服务器正在维护或重启
  - 本地网络连接不稳定（WiFi 信号弱）
  - ISP 的 DNS 服务器临时故障

3. **WSANO_RECOVERY**

- **字面含义**: 不可恢复的域名解析失败
- **可能场景**:
  - DNS 服务器返回严重错误
  - 域名服务器配置严重错误
  - DNS 协议层面的严重问题
  - 权威 DNS 服务器永久性故障

4. **WSANO_DATA**

- **字面含义**: 有效域名但没有请求类型的数据记录
- **可能场景**:
  - 域名存在但没有 A 记录（IPv4）或 AAAA 记录（IPv6）
  - 只有 CNAME 记录但没有最终的 IP 地址记录
  - DNS 记录类型不匹配查询类型
  - 域名配置不完整

5. **WSAEINVAL**

- **字面含义**: 无效参数
- **可能场景**:
  - 传入的域名格式不正确
  - hints 结构体参数设置错误
  - 端口号超出有效范围
  - 空指针或无效的字符串参数

6. **WSA_NOT_ENOUGH_MEMORY**

- **字面含义**: 内存不足
- **可能场景**:
  - 系统内存不足
  - 进程内存限制
  - 内存泄漏导致可用内存减少
  - 大量并发 DNS 查询占用内存

7. **WSAEAFNOSUPPORT**

- **字面含义**: 地址族不支持
- **可能场景**:
  - 系统不支持 IPv6 但请求 IPv6 解析
  - 网络协议栈配置问题
  - 驱动程序问题

#### Linux/Unix 平台错误码（getaddrinfo）

1. **EAI_NONAME**

- **字面含义**: 主机未找到
- **可能场景**:
  - 与 Windows 的 WSAHOST_NOT_FOUND 类似
  - 域名不存在或 DNS 解析失败
  - 网络连接问题
  - /etc/resolv.conf 配置错误

2. **EAI_AGAIN**

- **字面含义**: 临时性域名解析失败
- **可能场景**:
  - DNS 服务器暂时不可用
  - 网络连接不稳定
  - 系统负载过高
  - DNS 缓存问题

3. **EAI_FAIL**

- **字面含义**: 不可恢复的域名解析失败
- **可能场景**:
  - DNS 服务器返回永久性错误
  - 网络配置严重错误
  - DNS 协议违规

4. **EAI_NODATA**

- **字面含义**: 有效域名但没有数据
- **可能场景**:
  - 域名存在但没有对应的 IP 记录
  - DNS 记录类型不匹配

5. **EAI_MEMORY**

- **字面含义**: 内存分配失败
- **可能场景**:
  - 系统内存不足
  - 内存分配器问题
  - 进程内存限制

### 敏感信息保存

将 doh 敏感信息保存在文件中

**设计思路**

- 采用自定义的方式进行保存和管理
  - 文件内容: 详见文件内容布局
  - 魔数: 固定字符的的字节表示
  - 文件格式版本: 管理文件版本，目前是 1
  - 加密算法类型: 目前仅支持 AES-256-GCM
  - 加密内容长度: 本身变长(1-3) 自定义编码(参考 LEB128)
  - 内容 **首先** 使用 对称加密算法 AES-256-GCM，加解密。
    - 秘钥 32 字节 (代码明文 + XOR)
    - IV/Nonce: SHA256("DOH_IV_SALT_2024").substr(0,12)
    - 秘钥长期不变, 就算后续版本修改，导致新版本无法解析老版本，通过文件管理规则，可以使用默认 doh，服务器下发后，更新替换老的文件内容
  - 内容 **其次** 使用 自定义的 base64 再次编解码
    - "ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210+/"
  - 校验和
    - 算法: CRC32 (初始值 0xFFFFFFFF，结果取反)
    - 范围: 从文件开头到校验和字段前的所有数据
    - 字节序: 小端序存储
    - 用途: 检测文件损坏、传输错误
- 通过服务器下发内容，
  - json 格式
- 通过文件管理
  - 文件被删除、修改、破坏
    - 使用默认 doh 配置
    - 收到服务器下发配置，删除文件，创建文件，写入新内容
      - 删除或者创建失败，继续使用默认 doh 配置
- 支持动态更新
  - 使用服务器下发 doh 配置更新文件内容
  - 如果更新失败，将删除本地信息，重新创建文件保存

**文件内容布局**

| 偏移    | 长度 | 字段名          | 值示例     | 说明                           |
| ------- | ---- | --------------- | ---------- | ------------------------------ |
| 0       | 4    | magic_number    | 0x444F4831 | "DOH1"的 ASCII                 |
| 4       | 1    | version         | 0x01       | 文件格式版本                   |
| 5       | 1    | encryption_type | 0x01       | 加密算法类型 1=AES, 2=XOR      |
| 6       | 1-3  | content_length  | 0xA403     | 自定义变长编码                 |
| 7-9     | N    | encrypted_data  | [加密数据] | 自定义 Base64 编码的加密内容   |
| 7-9 + N | 4    | checksum        | 0x12345678 | CRC32 校验和(整个文件除此字段) |

**校验和计算**

参与校验的数据（按顺序）：

1. magic_number (4 字节)
2. version (1 字节)
3. encryption_type (1 字节)
4. content_length (1-3 字节)
5. encrypted_data (N 字节)

不包含：校验和字段本身(4 字节)

- 位置：文件末尾 4 字节
- 字节序：小端序（Little Endian）
- 计算：CRC32 算法，初始值 0xFFFFFFFF，结果取反

```cpp
uint32_t calculateChecksum(const std::vector<uint8_t>& file_data) {
    // 计算除最后4字节外的所有数据
    size_t data_size = file_data.size() - 4;  // 排除校验和字段

    uint32_t crc = 0xFFFFFFFF;  // CRC32初始值
    for (size_t i = 0; i < data_size; i++) {
        crc = crc32_update(crc, file_data[i]);
    }
    return ~crc;  // 取反
}
void writeChecksum(std::ofstream& file, uint32_t checksum) {
    // 小端序写入4字节
    uint8_t bytes[4] = {
        static_cast<uint8_t>(checksum & 0xFF),
        static_cast<uint8_t>((checksum >> 8) & 0xFF),
        static_cast<uint8_t>((checksum >> 16) & 0xFF),
        static_cast<uint8_t>((checksum >> 24) & 0xFF)
    };
    file.write(reinterpret_cast<char*>(bytes), 4);
}
bool verifyChecksum(const std::vector<uint8_t>& file_data) {
    if (file_data.size() < 4) return false;

    // 读取文件末尾的校验和（小端序）
    size_t checksum_offset = file_data.size() - 4;
    uint32_t stored_checksum =
        file_data[checksum_offset] |
        (file_data[checksum_offset + 1] << 8) |
        (file_data[checksum_offset + 2] << 16) |
        (file_data[checksum_offset + 3] << 24);

    // 计算实际校验和
    uint32_t calculated_checksum = calculateChecksum(file_data);

    return stored_checksum == calculated_checksum;
}
```

**敏感信息对象示例**

- version: 敏感信息版本
- timestamp: 敏感信息更新时间戳
- data: 厂商信息
  - 通用
    - 主 IP
    - 备用 IP
    - 域名
  - 阿里
    - account_id
    - access_key_id
    - access_key_secret
  - 腾讯
    - token

```json
{
  "version": "1.0.0",
  "timestamp": 1644744000,
  "data": {
    "ali": {
      "ip1": "223.5.5.5",
      "ip2": "223.6.6.6",
      "domain": "dns.alidns.com",
      "account_id": "xxx",
      "access_key_id": "xxx",
      "access_key_secret": "xxx"
    },
    "cloudflare": {
      "ip1": "1.1.1.1",
      "ip2": "1.0.0.1",
      "domain": "cloudflare-dns.com"
    },
    "google": {
      "ip1": "8.8.8.8",
      "ip2": "8.8.4.4",
      "domain": "dns.google"
    },
    "tencent": {
      "ip1": "119.29.29.99",
      "ip2": "119.29.29.98",
      "domain": "doh.pub",
      "token": "xxx"
    }
  }
}
```

### doh 工具类 设计

**核心思路**

- 并发请求
  - 提高成功率，全部失败的概率降低
  - 提高响应时间，更快获取服务器 IP 地址
  - 限制并发数量（建议 3-5）
  - 设置超时时间（避免资源耗尽）
- 支持 IPv4
- 支持 IPv6
- 结果不缓存，需要请再次请求

**所需组件库**

- curl: 进行 http 协议请求
- json: 进行 结果解析
- 加解密库: 需要使用哈希算法、对称加密算法

**基础组件**

- http 工具类
- thread pool 工具类
- 加解密 工具类

**核心类设计示例**

```cpp
// DNS记录类型枚举
enum class DNSRecordType {
    A = 1,      // IPv4地址记录
    AAAA = 28,  // IPv6地址记录
    CNAME = 5,  // 别名记录
};

// DNS解析结果
struct DNSRecord {
    std::string name;       // 域名
    DNSRecordType type;     // 记录类型
    std::string data;       // 记录数据（IP地址、CNAME等）
    uint32_t ttl;           // 生存时间（秒）
};

enum class DNSErrorCode {
    SUCCESS = 0,
    SYSTEM_DNS_FAILED = 1,
    DOH_DNS_FAILED = 2,
    NETWORK_ERROR = 3,
    TIMEOUT = 4,
    INVALID_DOMAIN = 5
};

using FailCallback = std::function<void(const DNSErrorCode code, const std::string& errorMsg)>;

// DNS 解析器
// 使用 系统时间戳 和 doh时间戳 管理状态
// 使用 缓存列表 减少 dns 解析的实际请求
// ttl 默认 300秒，不使用系统解析或者doh解析返回的ttl
// 指定 doh_duration 为 系统失败，doh成功，使用doh的有效时间窗口
class SmartDNSResolver {
  public:

    // 基础查询接口
    // 解析失败结果为空
    // 错误信息通过回调返回
    std::vector<DNSRecord> resolve(const std::string& domain, DNSRecordType type = DNSRecordType::A, std::chrono::milliseconds timeout = std::chrono::milliseconds(3500), const FailCallback& callback = [](const int,const std::string&) {}) {
      // 判断 是否有缓存，缓存在ttl内，直接返回缓存结果
      // 判断是否使用 系统dns解析，查看 _system_dns_parse_fail_timestamp 值
      // _system_dns_parse_fail_timestamp == 0 或者 _system_dns_parse_fail_timestamp + doh_duration * 1000 > 当前时间戳
      // 其他情况使用 doh 解析 dns
      // 系统解析和doh解析的结果保存在缓存中，在应用的生命周期内有效，不做持久化。
    }


  private:
    // 系统解析
    std::vector<DNSRecord> system_resolve(const std::string& domain, DNSRecordType type = DNSRecordType::A,
                                   std::chrono::milliseconds timeout = std::chrono::milliseconds(3500), const FailCallback& callback = [](const int,const std::string&) {});

    // doh 解析
    std::vector<DNSRecord> doh_resolve(const std::string& domain, DNSRecordType type = DNSRecordType::A, std::chrono::milliseconds timeout = std::chrono::milliseconds(3500), const FailCallback& callback = [](const int,const std::string&) {}) {
      // 获取本地 doh信息，至少会有2个doh服务提供商
      // 如果有自建服务器，优先使用自建服务 请求 dns 解析（自建可能是未来要做的事情）
      // 如果没有自建，则先并发 2个服务商的主要服务器 请求 dns 解析
      // 如果失败，再次使用 2个服务商的备用服务器 请求 dns 解析
      // 如果其中任何一个解析成功，使用先返回的解析结果，并且缓存
      // 如果没有任何一个解析成功，返回 空结果
    }


  private:
    std::string make_cache_key(const std::string& domain, DNSRecordType type) {
        return domain + ":" + std::to_string(static_cast<int>(type));
    }

  private:
    // 系统dns解析失败的时间戳，在此时间点后 doh_duration 内，都将尝试doh, 0表示没有失败
    // 该时间戳会被持久化，下次继续使用
    // doh_duration 之后，继续尝试，如果成功，将它设置为 0，不成功，更新它的值
    // 单位毫秒
    // 键格式: "domain:type", 如 "example.com:1" (A记录), "example.com:28" (AAAA记录)
    std::map<std::string, long long> _system_dns_parse_fail_timestamp_list;
    // 所有 doh dns 解析失败的时间戳，在此后 ttl之内，不在尝试, 0表示没有失败
    // 在ttl内，将使用缓存
    // 单位毫秒
    // 键格式: "domain:type", 如 "example.com:1" (A记录), "example.com:28" (AAAA记录)
    std::map<std::string, long long> _doh_dns_parse_fail_timestamp_list;
    // 解析成功的时间戳，缓存有效期 为 该时间戳 + ttl
    // 系统或者doh解析任何一个成功，将它设置为当前时间戳
    // 单位毫秒
    // 键格式: "domain:type", 如 "example.com:1" (A记录), "example.com:28" (AAAA记录)
    std::map<std::string, long long> _dns_parse_success_timestamp_list;
    // 域名解析缓存结果，在ttl内有效，否则将继续 doh请求，更新该缓存
    // 键格式: "domain:type", 如 "example.com:1" (A记录), "example.com:28" (AAAA记录)
    std::map<std::string, std::vector<DNSRecord>> _tmp_record_list;

    const long long ttl = 300; // 5分钟
    const long long doh_duration = 60 * 60 * 24; // 24小时
};
```

```cpp
struct ManufacturerDetailInfo {
  std::vector<std::string> ip_list;
  std::string domain;
  std::string path;
  std::map<std::string, std::string> params;
}

struct ManufacturerInfo {
  std::string manufacturer;
  std::string use_area;
  bool self_build;
  ManufacturerDetailInfo free_info;
  ManufacturerDetailInfo vip_info;
}

struct DohInfo {
  long long timestamp; // 时间戳，只要本地时间戳小，就更新
  std::vector<ManufacturerInfo> info_list;
}

// 敏感信息管理
// 参考 本文件 敏感信息保存 章节
class DohInfoManager {
  public:
    // 保存服务器下发信息
    // 文件保存失败可能的原因：目标文件不存在，文件存在没有写入权限
    // 文件不存在 应该自动创建，创建失败，应该删除原来的文件，删除失败报错，返回 false
    // 文件存在但是没有写入权限，应该删除原来的文件，删除失败报错，返回 false
    // 信息来自 源码常量
    // 信息可能来自 服务器下发
    // 信息更新 服务器下发信息 合并到 本地版本中
    bool save(const std::vector<DohInfo>& infos);

    // 获取本地信息
    // 不可能失败：获取方式有两种：1.本地文件，2.源码，就算本地文件失败，源码也不会失败
    // 文件获取失败的可能原因：本地没有该文件、文件解析失败
    // 文件解析失败的可能原因：文件被篡改、文件没有读取权限
    // 优先级: 文件版本优先，文件失败，在使用源码版本信息
    std::vector<DohInfo> get();

    // 移除本地信息: 文件被修改、无法打开等极端情况
    bool remove();
}
```

**doh 返回示例**

结果格式: rfc8484 规范，谷歌、阿里等厂商都采用同一个标准

```json
{
  "Status": 0,
  "TC": false,
  "RD": true,
  "RA": true,
  "AD": false,
  "CD": false,
  "Question": {
    "name": "ap4-tls.agora.io.",
    "type": 1
  },
  "Answer": [
    {
      "name": "ap4-tls.agora.io.",
      "TTL": 280,
      "type": 5,
      "data": "ap1-tls.agora.io."
    },
    {
      "name": "ap1-tls.agora.io.",
      "TTL": 60,
      "type": 5,
      "data": "ap-tls.agoraio.cn."
    },
    {
      "name": "ap-tls.agoraio.cn.",
      "TTL": 83,
      "type": 5,
      "data": "gtm-cn-62l4b6ce002.agoraio.cn."
    },
    {
      "name": "gtm-cn-62l4b6ce002.agoraio.cn.",
      "TTL": 14,
      "type": 1,
      "data": "114.67.228.40"
    },
    {
      "name": "gtm-cn-62l4b6ce002.agoraio.cn.",
      "TTL": 14,
      "type": 1,
      "data": "106.54.57.121"
    },
    {
      "name": "gtm-cn-62l4b6ce002.agoraio.cn.",
      "TTL": 14,
      "type": 1,
      "data": "81.69.11.179"
    },
    {
      "name": "gtm-cn-62l4b6ce002.agoraio.cn.",
      "TTL": 14,
      "type": 1,
      "data": "103.228.162.59"
    },
    {
      "name": "gtm-cn-62l4b6ce002.agoraio.cn.",
      "TTL": 14,
      "type": 1,
      "data": "116.196.107.92"
    }
  ]
}
```

**并发细节讨论**

// todo: 方案选择讨论点: done

1. google、cloudflare、ali、tencent 同时并发请求
2. 22 串行和并发混用，先 google（海外）、ali（国内） 并发，失败后，再 cloudflare（海外） 和 tencent（国内） 并发
   1. 极大避免免费 doh QPS 受限
   2. 解析时间可能变长
3. 可以预先判断地理位置，国外 google，失败，再 cloudflare，国内 ali，失败，再 tencent。
   1. 非常节省资源，针对性强，成功率高
   2. 如果判断地里位置不准确，或者考虑 vpn 等情况，那么可能根本无法获取 服务器 IP 地址
4. 根据发布区域，选择不同的 doh 服务，例如：国内使用 ali，国外使用 google
5. 根据发布区域，国内 阿里和腾讯，国外 谷歌和 cloudflare ✅
   1. 先主后备，并发+串行
   2. 并发 1：主要服务器
   3. 失败后，并发 2：备用服务器

### 客户端和服务器约定

- version: 版本号，版本号大于本地版本号，将替换本地版本。
- timestamp: 该版本的时间戳
- list: doh 服务提供商，包括自建

  - ali
  - tencent
  - google
  - cloudflare
  - easemob-china: 自建国内
  - easemob-global: 自建国外

- name: doh 服务商名称
- use_area: 使用区域，如果服务可以做到根据客户端的区域下发，这个字段不需要 // todo:
- self_build: 是否是自建 doh 服务，如果有，那么，优先使用，保持了未来的可扩展性
- free: 免费 doh
- vip: 收费 doh

- ips: doh 服务地址列表
- domain: doh 地址
- path: 资源路径，保证服务商发生变化，可以及时更新
- params: 请求需要的参数
- examples: 这个是文档实例，不出现在 正式 格式中

```json
{
  "version": "1.0.0",
  "timestamp": 1752820068,
  "list": [
    {
      "name": "ali",
      "use_area": "china",
      "self_build": false,
      "free": {
        "ips": ["223.5.5.5", "223.6.6.6"],
        "domain": "dns.alidns.com",
        "path": "/resolve",
        "params": [{ "name": "string" }, { "type": "string" }],
        "examples": [
          "https://dns.alidns.com/resolve?name={name}&type={A}",
          "https://233.5.5.5/resolve?name={name}&type={AAAA}"
        ]
      },
      "vip": {
        "ips": ["223.5.5.5", "223.6.6.6"],
        "domain": "dns.alidns.com",
        "path": "/resolve",
        "params": [
          { "name": "string" },
          { "type": "string" },
          { "uid": "string" },
          { "ak": "string" },
          { "key": "string" }
        ],
        "examples": [
          "https://dns.alidns.com/resolve?name={name}&type={A}&uid={uid}&ak={ak}&key={key}",
          "https://233.5.5.5/resolve?name={name}&type={AAAA}&uid={uid}&ak={ak}&key={key}"
        ]
      }
    },
    {
      "name": "tencent",
      "use_area": "china",
      "self_build": false,
      "free": {
        "ips": [],
        "domain": "doh.pub",
        "path": "/dns-query",
        "params": [{ "name": "string" }, { "type": "string" }],
        "examples": [
          "https://doh.pub/dns-query?name={name}&type={A}",
          "https://doh.pub/dns-query?name={name}&type={AAAA}"
        ]
      },
      "vip": {
        "ips": ["119.29.29.99", "119.29.29.98"],
        "domain": "",
        "path": "/d",
        "params": [{ "name": "string" }, { "token": "string" }],
        "examples": [
          "https://119.29.29.99/d?name={name}&token={token}",
          "https://119.29.29.98/d?name={name}&token={token}"
        ]
      }
    },
    {
      "name": "google",
      "use_area": "global",
      "self_build": false,
      "free": {
        "ips": ["8.8.8.8", "8.8.4.4"],
        "domain": "dns.google",
        "path": "/resolve",
        "params": [{ "name": "string" }, { "type": "string" }],
        "examples": [
          "https://dns.google/resolve?name={name}&type={A}",
          "https://8.8.8.8/resolve?name={name}&type={AAAA}"
        ]
      }
    },
    {
      "name": "cloudflare",
      "use_area": "global",
      "self_build": false,
      "free": {
        "ips": ["1.1.1.1", "1.0.0.1"],
        "domain": "cloudflare-dns.com",
        "path": "/dns-query",
        "params": [{ "name": "string" }, { "type": "string" }],
        "examples": [
          "https://cloudflare-dns.com/dns-query?name={name}&type={A}",
          "https://1.1.1.1/dns-query?name={name}&type={AAAA}"
        ]
      }
    },
    {
      "name": "easemob-china",
      "use_area": "china",
      "self_build": true,
      "free": {
        "ips": ["7.7.7.7", "7.7.7.8"],
        "domain": "easemob-dns.com",
        "path": "/dns-query",
        "params": [{ "name": "string" }, { "type": "string" }],
        "examples": [
          "https://easemob-dns.com/dns-query?name={name}&type={A}",
          "https://7.7.7.7/dns-query?name={name}&type={AAAA}"
        ]
      }
    },
    {
      "name": "easemob-global",
      "use_area": "global",
      "self_build": true,
      "free": {
        "ips": ["7.7.7.7", "7.7.7.8"],
        "domain": "easemob-dns.com",
        "path": "/dns-query",
        "params": [
          { "name": "string" },
          { "type": "string" },
          { "token": "string" }
        ],
        "examples": [
          "https://easemob-dns.com/dns-query?name={name}&type={A}&token={token}",
          "https://7.7.7.7/dns-query?name={name}&type={AAAA}&token={token}"
        ]
      }
    }
  ]
}
```

## 流程图

以下是根据前述设计方案制定的 DNS 解析流程图：

1. [DNS 解析主流程图](dns_resolution_flow.puml) - 展示了从系统 DNS 解析到 DoH 解析的完整流程
2. [DoH 解析详细流程图](doh_resolution_detail.puml) - 展示了 DoH 解析的详细实现和并发策略
3. [DNS 错误处理流程图](dns_error_handling.puml) - 展示了系统 DNS 解析错误码的处理逻辑

使用方法：使用支持 PlantUML 的工具（如 VS Code 的 PlantUML 插件、在线 PlantUML 渲染器等）打开这些 PUML 文件即可查看流程图。

# 厂商

使用 dns 解析，就需要了解厂商 dns 相关的服务。

doh 服务厂商 域名、IP 都是 **公开** 信息，即是放在源码中，也不会对产品产生安全问题，付费敏感信息，通过 服务器 **安全** 下发，加密保存、以及补救措施等，一定程度也可以保证安全。

## google

**1. public DNS (免费)**

restful api: https://dns.google/resolve

**2. cloud DNS（收费）**

**注意** 托管域名解析服务。这种服务不推荐多家同时托管。例如：使用了谷歌又要使用 cloudflare。

restful api: https://dns.googleapis.com/dns/v1/projects/{project}/managedZones/{zone}/rrsets

**主服务器** 8.8.8.8
**备用服务器** 8.8.4.4

[费用介绍](https://cloud.google.com/dns/pricing?hl=zh-cn)

## cloudflare

**1. doh (免费)**

restful api: https://cloudflare-dns.com/dns-query

**2. dns（免费和收费）**

restful api: https://api.cloudflare.com/client/v4/zones/{zone_id}/dns_records

**主服务器** 1.1.1.1
**备用服务器** 1.0.0.1

[费用介绍](https://www.cloudflare.com/zh-cn/application-services/products/dns/)

[endpoints](https://developers.cloudflare.com/1.1.1.1/infrastructure/network-operators/)

[dns resolve](https://developers.cloudflare.com/1.1.1.1/)

[dns 托管](https://developers.cloudflare.com/dns/)

## 阿里

**各个版本和性能参数**

[版本比较](https://help.aliyun.com/zh/dns/details?spm=5176.13984893.commonbuy2container.2.6c99778b9Gidwx)

restful api: http://dns.alidns.com/resolve

**主服务器** 223.5.5.5
**备用服务器** 223.6.6.6

[集成文档](https://help.aliyun.com/zh/dns/httpdns-doh-json-api?spm=5176.28197678_2080504283.console-base_help.dexternal.59351b87ugUfyQ)

[控制台](https://dnsnext.console.aliyun.com/pubDNS)

```http
# 收费doh请求
curl --location 'https://223.5.5.5/resolve?name=ap4-tls.agora.io&type=A&uid=694071&ak=694071_30386520106075136&key=2fe80ca217f7c06d8cd7fc2ca1b6de0e02b45ef9b78f5ede6304af9b2cd8a331&ts=1752543793' \
--header 'Accept: application/dns-json' \
--header 'User-Agent: DoH-Client/1.0'
```

```http
# 免费doh请求
curl --location 'https://dns.alidns.com/resolve?name=a1.easemob.com&type=1' \
--header 'Accept: application/dns-json' \
--header 'User-Agent: DoH-Client/1.0'
```

## 腾讯

**各个版本和性能参数**

[版本比较](https://cloud.tencent.com/document/product/302/106264)
[dnspod 官网](https://www.dnspod.cn/products/publicdns)

api: https://doh.pub/dns-query

**主服务器** ~~119.29.29.29~~
**备用服务器** ~~182.254.116.116~~

**注意** 免费解析已停止运营 https://cloud.tencent.com/document/product/379/56885, 只能使用 doh.pub 域名免费解析

[控制台](https://console.cloud.tencent.com/httpdns/configure)

[文档](https://cloud.tencent.com/document/product/379/95498)

```http
# 收费doh请求
curl --location 'https://119.29.29.99/d?dn=ap4-tls.agora.io&token=777043302' \
--header 'Accept: application/dns-json' \
--header 'User-Agent: DoH-Client/1.0'
```

```http
# 免费doh请求
curl --location 'https://doh.pub/dns-query?name=ap4-tls.agora.io&type=1' \
--header 'Accept: application/dns-json' \
--header 'User-Agent: DoH-Client/1.0'
```

# 合规

基本原则: 不获取个人非公开信息。

dns 域名解析过程中，不会对个人信息进行搜集和处理，不需要合规。

# 私有化部署

使用全局开关，控制是否使用 doh 解析。

# 网络状态检查

根据以往经验，ios 的网络检查工具并不好用，导致网络检查结果或者回调通知不准确。

私有化场景下，访问指定地址，检查网络连通性也不可行。指定地址也应该是公开的稳定的地址。例如：1.1.1.1。

# 竞品

1. sendbird（未使用）
   1. 闭源，通过抓包关键字 dns
2. getstream（未使用）
   1. 开源
   2. 通过抓包关键字 dns
3. tencent（未使用）
   1. 闭源，通过抓包关键字 `d?` `dns`
4. rongcloud

[sendbird_demo](/Users/asterisk/tmp2025/2025-06-25/test_sendbird_demo)

[getstream_demo](/Users/asterisk/tmp2025/2025-07-01/test_getstream_demo)

[tencent_demo](/Users/asterisk/tmp2025/2025-07-01/chat-uikit-vue/Vue3/Demo)

# 实施

// todo: 方案选择讨论点: done

- 第一阶段：最小可用版本（只支持免费 DoH）
- 第二阶段：增加付费 DoH 支持
- 第三阶段：增加服务端下发功能 ✅

# 监控以及日志

- 记录关键操作
- 记录失败信息
- 在开发模式打印延迟时间
