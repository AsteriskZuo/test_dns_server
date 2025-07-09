# 客户端 DNS 解析讨论

## 事件背景

曾经发生过 DNS 劫持，导致 客户端解析失败，无法连接服务器

1. 小概率事件
2. 发生时间无法预测
3. 造成损失无法估计

## 如何解决？

**建议** 失败的时候尝试使用 DNS over Http 技术解析服务器地址

**主要流程**

客户端采用 `getaddrinfo` 和 `gethostbyname` 解析 DNS，得到 IP 地址后，连接服务器。

如果连接失败，获取错误码，部分错误码可以通过尝试 DoH 服务获取 IP 地址。

如果连接成功， provision 失败，可以尝试 使用 DoH 其他 IP 地址。

## 厂商介绍

google 和 cloudflare 官网没有提供 QPS 等参数，收费版本通过 Zone 管理域名。

国内厂商 阿里 和 腾讯 提供了相应的 QPS 等参数，收费版本通过 域名 管理，用户不需要直接管理 Zone。

_如果顶级厂商的 DNS 服务都无法满足要求肯定是设计哪里需要优化_

### google

**1. public DNS (免费)**

api: https://dns.google/resolve

**2. cloud DNS（收费）**

api: https://dns.googleapis.com/dns/v1/projects/{project}/managedZones/{zone}/rrsets

**主服务器** 8.8.8.8
**备用服务器**

费用介绍：https://cloud.google.com/dns/pricing?hl=zh-cn

### cloudflare

**1. doh (免费)**

api: https://cloudflare-dns.com/dns-query

**2. dns（免费和收费）**

api: https://api.cloudflare.com/client/v4/zones/{zone_id}/dns_records

**主服务器** 1.1.1.1
**备用服务器**

费用介绍：https://www.cloudflare.com/zh-cn/application-services/products/dns/

### ali

**各个版本和性能参数**

https://help.aliyun.com/zh/dns/details?spm=5176.13984893.commonbuy2container.2.6c99778b9Gidwx

api: http://dns.alidns.com/resolve

**主服务器** 223.5.5.5
**备用服务器**

### tencent

**各个版本和性能参数**

https://cloud.tencent.com/document/product/302/106264

api: https://doh.pub/dns-query

**主服务器** 119.29.29.29
**备用服务器**

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
