# 客户端 DNS 解析讨论

## 事件背景

曾经发生过 DNS 劫持，导致 客户端解析失败，无法连接服务器

1. 小概率事件
2. 发生时间点无法预测
3. 造成损失无法估计

## 如何解决？

**建议 1** 失败的时候尝试使用 DNS over Http 技术解析服务器地址

**主要流程**

客户端采用 `getaddrinfo` 和 `gethostbyname` 解析 DNS，得到 IP 地址后，连接服务器。

如果连接失败，获取错误码，部分错误码可以通过尝试 DoH 服务获取 IP 地址。

如果连接成功， provision 失败，可以尝试 使用 DoH 其他 IP 地址。

**建议 2** 最少使用 DNS over Http 技术解析服务器地址

连接之后，其他地址，由服务器下发和者客户端主动获取两种方式获得。

在重新连接时机，再次走相同的获取流程

## 厂商介绍

google 和 cloudflare 官网没有提供 QPS 等参数，收费版本通过 Zone 管理域名。

国内厂商 阿里 和 腾讯 提供了相应的 QPS 等参数，收费版本通过 域名 管理，用户不需要直接管理 Zone。

_如果顶级厂商的 DNS 服务都无法满足要求肯定是设计哪里需要优化_

### google

**1. public DNS (免费)**

api: https://dns.google/resolve

**2. cloud DNS（收费）**

**注意** 托管域名解析服务。这种服务不推荐多家同时托管。例如：使用了谷歌又要使用 cloudflare。

api: https://dns.googleapis.com/dns/v1/projects/{project}/managedZones/{zone}/rrsets

**主服务器** 8.8.8.8
**备用服务器** 8.8.4.4

费用介绍：https://cloud.google.com/dns/pricing?hl=zh-cn

### cloudflare

**1. doh (免费)**

api: https://cloudflare-dns.com/dns-query

**2. dns（免费和收费）**

api: https://api.cloudflare.com/client/v4/zones/{zone_id}/dns_records

**主服务器** 1.1.1.1
**备用服务器** 1.0.0.1

费用介绍：https://www.cloudflare.com/zh-cn/application-services/products/dns/

endpoints: https://developers.cloudflare.com/1.1.1.1/infrastructure/network-operators/

dns resolve： https://developers.cloudflare.com/1.1.1.1/

dns 托管： https://developers.cloudflare.com/dns/

### ali

**各个版本和性能参数**

https://help.aliyun.com/zh/dns/details?spm=5176.13984893.commonbuy2container.2.6c99778b9Gidwx

api: http://dns.alidns.com/resolve

**主服务器** 223.5.5.5
**备用服务器** 223.6.6.6

### tencent

**各个版本和性能参数**

https://cloud.tencent.com/document/product/302/106264
https://www.dnspod.cn/products/publicdns

api: https://doh.pub/dns-query

**主服务器** ~~119.29.29.29~~
**备用服务器** ~~182.254.116.116~~

**注意** 免费解析已停止运营 https://cloud.tencent.com/document/product/379/56885

## 错误码介绍

参考代码 [here](../examples/system_dns_parser_example.cpp)

## 竞品客户端使用 DNS 解析情况

1. sendbird（未使用）
   1. 闭源，通过抓包关键字 dns
2. getstream（未使用）
   1. 开源
   2. 通过抓包关键字 dns
3. tencent（未使用）
   1. 闭源，通过抓包关键字 dns
4. rongcloud

[sendbird_demo](/Users/asterisk/tmp2025/2025-06-25/test_sendbird_demo)

[getstream_demo](/Users/asterisk/tmp2025/2025-07-01/test_getstream_demo)

[tencent_demo](/Users/asterisk/tmp2025/2025-07-01/chat-uikit-vue/Vue3/Demo)

## 降低 doh 等加密 DNS 解析策略

1. 除了第一次 doh 请求之外，其他地址都是通过已经连接的服务获取 IP 地址。只要第一个 IP 地址不被劫持，后面就不需要 doh 解析了
2. 增加缓存，只有必要的机会才触发 doh 请求，例如：登录、重连
3. 合并同类请求的 doh 请求，例如：发送文件消息、发送图片消息、获取联系人列表等

## 其他 IM 是如何避免 DNS 劫持的？

1. DoT
2. 多并发 DNS 查询验证
3. IP 直连

## 开会讨论结论：

采用方案 1： 失败之后再使用 doh 服务
细节 1：失败之后，doh 解析结果成功，保存 24 小时（时间是变量默认 24 小时）

## todo

1. getaddrinfo 是否可以设置 8.8.8.8 作为参数？
   1. 不能
2. 第二阶段，采用服务器下发 IP 地址列表？
3. 只有 rs。登录域名需要解析，其他不需要
4. 更新 HLD 文档

---

2025-07-14

# 落地

## 腾讯 httpDNS

https://console.cloud.tencent.com/httpdns/configure

https://cloud.tencent.com/document/product/379/95498

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

## 阿里 httpDNS

https://help.aliyun.com/zh/dns/httpdns-doh-json-api?spm=5176.28197678_2080504283.console-base_help.dexternal.59351b87ugUfyQ

https://dnsnext.console.aliyun.com/pubDNS

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

## 方案 1 的详细设计

**doh 部分细节**

1. ip 作为常量写在代码里（仓库私有）
2. 连接服务成功后，服务下发 token、key 等信息，并且保存在本地
3. 如果服务器没有下发，使用免费 doh
4. 如果服务器下发，优先使用收费 doh
5. 如果收费 doh，权限失败，使用免费 doh

免费 doh 使用阿里、谷歌、cloudflare。

**注意** 服务器目前是否有 客户端身份验证，如果没有的话，需要添加身份验证，主要用户敏感信息下发

**整体流程图**

_前置条件:_ 系统 dns 获取 ip 失败，或者 ip 错误。

sdk 对象: 主要管理 doh 相关信息（ip、token、key 等），选择哪个 doh、选择收费还是付费、并发请求还是串行请求等。

server 对象: 下发 doh 相关信息（token、key 等敏感信息）。

app 对象: 集成了 sdk 的应用， app 对于 sdk 管理 doh 无感知。

app 首次启动，使用免费 doh（至少 4 个服务商： 谷歌、腾讯、阿里、cloudflare） 获取 ip，如果失败，则暂停连接 server，如果成功，则 后续 指定时间内（1 小时） 优先使用 doh。

app 后续启动，优先使用 收费 doh（至少 2 个服务商： 谷歌、阿里）获取 ip，如果失败，尝试免费 doh，如果失败，则暂停连接服务，如果成功，则 后续 指定时间内（1 小时） 优先使用 doh。

server 连接成功，服务器下发 token、key 等信息，sdk 更新并保存 token、key 等信息。

如果 收费的 doh，获取 ip 失败，则使用 免费的 doh 尝试获取 ip。

## 局部设计

**doh 请求**

1. 并发请求（谷歌、cloudflare、腾讯、阿里）
2. 并发请求失败的概率非常低，除非网络发生故障
3. 优先使用先返回的结果，简单说：在中国大陆 阿里、腾讯返回快，在非中国大陆 谷歌、cloudflare 返回快
4. 优先各家收费请求，其次各家免费请求。
   1. 例如：谷歌收费、腾讯收费、cloudflare 免费、阿里免费，在国内至少腾讯或者阿里有一个可以返回结果，在国外至少谷歌或者 cloudflare 有一个可以返回结果
5. done

**敏感信息保存**

- 谷歌:
  - ip: 8.8.8.8
- cloudflare:
  - ip: 1.1.1.1
- 腾讯:
  - 免费
    - domain: 比较特殊，119.29.29.29 不能在使用 2022 年已经作废
  - 收费
    - ip: 119.29.29.99
    - token:
- 阿里:
  - 免费
    - ip: 223.5.5.5
  - 收费
    - ip: 223.5.5.5
    - Account ID:
    - AccessKey ID:
    - AccessKey Secret:

**除了 ip 之外的敏感信息保存**

1. 内容格式采用 json
2. 自定义编码（校验和、base64 等） // todo: 需要 AI 帮忙设计和实现
3. 保存到文件：自定义字接头 + 编码后的 json 内容
4. 从文件中读取：可逆解析
5. json 内容：使用 哈希校验参数值是否被 篡改。
6. 版本号：保留。可以更新厂商
7. 时间戳：表示内容失效时间点

将服务器下发和本地常量组合起来最终变成这样：

- 厂商名称、ip、domain 为代码常量
- token、key 等为服务器下发

```json
{
  "version": "1.0.0",
  "timestamp": 1644744000,
  "key": "xxx",
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

## sdk 配置

1. 全局开关，控制是否开启 doh 服务。
2. 保存 doh 配置信息
3. 更新 doh 配置信息
4. 读取 doh 配置信息

# 合规

如果集成了 google、cloudflare、ali、tencent 服务，需要做合规性审核？

1. 在国内
   1. google、cloudflare 使用是否合规？
2. 在国外
   1. 阿里 和 腾讯产品在国外是否可以合法使用？

只要保证客户个人数据不会出境就可以 只是 dns 解析的话 我们只是把服务端的域名发出去 然后收一个 ip 这个没事 (声网合规同学说)

```cpp
class ComplianceManager {
    enum class Region {
        CN_MAINLAND,    // 中国大陆：优先ali/tencent
        CN_HONGKONG,    // 香港：可用所有服务
        US,             // 美国：优先google/cloudflare
        EU,             // 欧盟：需符合GDPR
        OTHER           // 其他地区
    };
};
```

# 网络诊断

所有 doh 服务请求失败是否认为当前设备网络异常？

推荐策略：

- 所有 DoH 失败 → 先做网络诊断
- 如果基础网络正常 → 认为是 DNS 被屏蔽，启用备用方案
- 如果基础网络异常 → 认为是网络问题，延迟重试

```cpp
class NetworkDiagnostic {
public:
    enum class NetworkStatus {
        NORMAL,           // 网络正常
        DNS_BLOCKED,      // DNS被屏蔽
        NETWORK_LIMITED,  // 网络受限
        NETWORK_DOWN      // 网络断开
    };

    NetworkStatus diagnoseNetwork() {
        // 1. 基础连通性测试
        if (!canReachInternet()) {
            return NetworkStatus::NETWORK_DOWN;
        }

        // 2. HTTPS连通性测试
        if (!canReachHTTPS()) {
            return NetworkStatus::NETWORK_LIMITED;
        }

        // 3. DoH特定测试
        if (!canReachDoHProviders()) {
            return NetworkStatus::DNS_BLOCKED;
        }

        return NetworkStatus::NORMAL;
    }

private:
    bool canReachInternet() {
        // 尝试连接知名IP（不依赖DNS）
        return testConnection("8.8.8.8:80") ||
               testConnection("1.1.1.1:80");
    }

    bool canReachHTTPS() {
        // 测试HTTPS连接
        return testConnection("1.1.1.1:443");
    }

    bool canReachDoHProviders() {
        // 测试所有DoH提供商
        return testDoHConnection("google") ||
               testDoHConnection("cloudflare") ||
               testDoHConnection("ali") ||
               testDoHConnection("tencent");
    }
};
```

## 代码明文

存储 `223.5.5.5` `dns.google` 是否有安全风险？

使用逆向工程可以获取到这些信息，但是信息本身是公开的。
