# doh 讨论

1. 使用 doh 的目的？
   1. 最常见的原因是域名劫持，通过指定安全的 dns 解析服务，可以最大限度避免域名劫持
2. 域名解析的方式有哪些？
   1. 通过系统默认域名解析（域名劫持通常发生在这里）
   2. 通过公共 dns 服务提供商域名（例如：https://dns.google/resolve）
   3. 通过自定义域名解析服务
   4. 付费解析 （google、cloudflare、ali、tencent都有相关产品）
3. 面对可能的域名劫持风险，有哪些可能的应对策略
   1. 使用安全的域名解服务
   2. 使用自行搭建的域名解析服务
4. 如何判断域名被劫持？
   1. 根据 IP 范围来判断
   2. 通过对比系统 dns 解析和公共 dns 解析的结果
5. 在环信 IM 业务场景中，被劫持的区域有哪些？例如：在美国被劫持。
   1. 亚洲
   2. 美洲
   3. 非洲
   4. 欧洲
6. 在环信 IM 业务场景中，被劫持的时间有多少？例如：一年之中发生几次。
7. dns 解析有哪些服务提供商？
   1. Google
   2. Tencent
   3. Ali
   4. Cloudflare
8. 服务器提供商的服务质量如何？对比系统 dns 解析和公共 dns 解析的结果。
9. 系统 dns 解析好公共 dns 解析的主要区别有哪些？
10. 响应时间
11. 服务压力
12. 服务价格
13. 系统 dns 是否不需要服务器？公开 dns 服务需要服务器？
14. 如果系统 dns 和公共 dns 有哪些组合方式？
    1. 系统 dns 和公共 dns 同时使用
    2. 系统 dns 和公共 dns 互斥使用
       1. 系统 dns 优先，失败后使用公共 dns
       2. 公共 dns 优先，失败后使用系统 dns
15. IM 主流厂商 dns 解析服务使用情况（源码+抓包）
    1. sendbird
    2. getstream
    3. tencent
    4. rongcloud
16. IM客户端使用系统自带dns解析失败的情况：
    1.  没有得到IP地址：建议更换 DNS 解析服务
    2.  超时：建议重试，但是限制重试次数
    3.  得到错误IP地址：
        1.  如何判断是错误的IP地址？
        2.  如果是错误IP应该怎么做？
17. 如何判断是错误的IP地址？
    1.  指定IP地址链接失败
    2.  指定IP地址链接成功，但是证明身份的内容是错误的。
    3.  provision （easemob）
18. 判断出来是错误IP地址怎么做？
    1.  使用安全dns解析域名
19. 域名解析工作是否可以放在服务器？
    1.  服务器直接下发解析好的IP地址？
        1.  这样做有什么风险？
        2.  服务器海客依负载均衡
    2.  客户端解析域名
20. 域名解析服务商，付费版本有哪些特点？
    1.  针对不同域名进行参数设置。例如：TTL等
    2.  更好的性能（国内厂商）
    3.  谷歌和cloudflare采用 dns zone管理
    4.  阿里和腾讯采用 域名管理
21. 免费dns解析是否满足需求？
    1.  系统dns优先的情况，问题不大
    2.  全球顶级dns服务商如果不满足，肯定是自己的设计不合理

# 现有数据

## 阿里 dns 服务数据

[from](https://help.aliyun.com/zh/dns/details?spm=5176.13984893.commonbuy2container.2.6c99778b9Gidwx)

企业版：

1. 可用性 SLA: 100%
2. DNS 节点：中国内地 12 个、海外 15 个
3. 单主域下解析记录数量：10 万条
4. 子域名级数：20 级
5. TTL 值（最小）：1 秒
6. 线路类型切换（运营商线路与地域线路）：支持
7. 权重配置（A、CNAME、AAAA 记录）：支持
8. 负载均衡（单域名、单线路的 IP 地址容量）：100 条
9. DNS 解析峰值：不超过 20 万次/秒

// todo:

## 谷歌 dns 服务数据

[from](https://cloud.google.com/dns/docs/reference/performance)

// todo:

## 腾讯 dns 服务数据

// todo:

## Cloudflare dns 服务数据

// todo:

## 环信 IM 数据

1. 每天初始化次数 小于 1 亿次
2. minimax 所在集群 峰值 3000 QPS 登录请求。

# 厂商

## sendbird

1. 没有使用 dns 服务
2. 采用动态 域名解析。 例如： `api-adee63a9-ce46-4c11-9dfe-b3197215a7e6.sendbird.com`

**示例 demo**

`/Users/asterisk/tmp2025/2025-06-25/test_sendbird_demo`

## getstream

**示例 demo**

`/Users/asterisk/tmp2025/2025-07-01/test_getstream_demo`

[console_30_days_free](https://dashboard.getstream.io/app/1261226/moderation/users?tab=inbox)

## tencent

1. `https://webim.tim.qq.com/v4/imopenstat/tweb_trtccalling_report?sdkappid=1400782884`
2. 没有使用 dns 服务

**示例 demo**

`/Users/asterisk/tmp2025/2025-07-01/chat-uikit-vue/Vue3/Demo`

## rongcloud

// todo:

## 系统 dns 解析失败的原因

1. windows 使用 gethostbyname
2. 非 windows 使用 getaddressinfo

[getaddressinfo_man_page](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
[gethostbyname](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-gethostbyname)
[getaddressinfo_windows](https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo)
