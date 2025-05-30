## 参考

[谷歌](https://developers.google.com/speed/public-dns/docs/doh?hl=zh-cn)
[360](https://sdns.360.net/dnsPublic.html)
[rfc8484-DoH](https://datatracker.ietf.org/doc/html/rfc8484)
[rfc4648-base64](https://datatracker.ietf.org/doc/html/rfc4648)

## Cloudflare DNS (1.1.1.1)

- 文档链接：https://developers.cloudflare.com/1.1.1.1/encryption/dns-over-https/
- 支持格式：同时支持 JSON 格式和 DNS wireformat
- API 端点：`https://cloudflare-dns.com/dns-query` 或 `https://1.1.1.1/dns-query`
- 完全免费
- 国内无法使用

      Speed up your online experience with Cloudflare's public DNS resolver.

      Available on all plans
      1.1.1.1 is Cloudflare’s public DNS resolver. It offers a fast and private way to browse the Internet. DNS resolvers ↗ translate domains like cloudflare.com into the IP addresses necessary to reach the website (like 104.16.123.96).

      Unlike most DNS resolvers, 1.1.1.1 does not sell user data to advertisers. 1.1.1.1 has also been measured to be the fastest DNS resolver available ↗ — it is deployed in hundreds of cities worldwide ↗, and has access to the addresses of millions of domain names on the same servers it runs on.

      1.1.1.1 is **completely free**. Setting it up takes minutes and requires no special software.

      **注意：需要翻墙使用。**

## Google Public DNS (8.8.8.8)

- 文档链接：https://developers.google.com/speed/public-dns/docs/doh
- 支持格式：同时支持 JSON 格式和 DNS wireformat
- API 端点：`https://dns.google/dns-query`
- 完全免费
- 国内无法使用

      `https://developers.google.com/speed/public-dns/docs/using?hl=zh-cn` 中详细介绍了各个系统的 DNS 配置方式，以及必要说明。

      **完全免费**

      **注意：需要翻墙使用。**

## 阿里 DNS (AliDNS)

- 文档链接：https://www.alidns.com/knowledge?type=SETTING_DOCS
- DoH 端点：`https://dns.alidns.com/dns-query`
- 仅支持 DNS wireformat (RFC 8484)，需要使用 base64url 编码的 DNS 消息
- 支持海外和国内
- 购买链接： `https://www.alidns.com/`
- 产品对比：`https://help.aliyun.com/zh/dns/details?spm=5176.13984893.commonbuy2container.2.6c99778b9Gidwx`
- 个人版本：19.9 一年（2025-05-27）
- 企业版本：482 一年（2025-05-27）

## 360 安全 DNS

- 文档链接：https://sdns.360.net/dnsPublic.html
- DoH 端点：`https://doh.360.cn/dns-query`
- 同时支持 JSON 格式和 DNS wireformat
- 产品链接：`https://sdns.360.net/`
- 需要电话咨询，官网无详情

## 百度 DNS

- DoH 端点：`https://dns.baidu.com/dns-query`
- 支持 DNS wireformat

## 腾讯 DNS (DNSPod)

- 文档链接：https://www.dnspod.cn/products/publicdns
- DoH 端点：`https://doh.pub/dns-query`
- 支持 JSON 格式
- 产品链接：`https://cloud.tencent.com/product/dns`
- 专业 99，企业 1800，尊享 29800

## Quad9 DNS (9.9.9.9)

- 文档链接：https://www.quad9.net/support/doh-quad9-net/
- DoH 端点：`https://dns.quad9.net/dns-query`
- 同时支持 JSON 格式和 DNS wireformat
- 完全免费
- 国内无法使用

## DoH 标准规范

对于更全面的理解，您可以参考 DoH 的官方 RFC 文档：

1. **RFC 8484 - DNS Queries over HTTPS (DoH)**：

   - 链接：https://datatracker.ietf.org/doc/html/rfc8484
   - 这是 DNS over HTTPS 的官方规范，定义了如何使用 DNS wireformat

2. **DNS-JSON 格式**：
   - 这种格式没有正式的 RFC 标准，但谷歌和 Cloudflare 都提供了详细文档
   - Google 文档：https://developers.google.com/speed/public-dns/docs/doh/json

## 实现建议

1. 对于 DNS wireformat（如阿里 DNS）：

   - 使用 Content-Type: application/dns-message
   - 将 DNS 请求消息正确地 base64url 编码
   - 提交到 ?dns= 参数

2. 对于 JSON 格式（如 Cloudflare、Google）：

   - 使用 Content-Type: application/dns-json
   - 简单地使用 ?name= 和 &type= 参数

3. 考虑添加更多的错误处理和格式检测逻辑，因为不同提供商的响应可能略有不同。
