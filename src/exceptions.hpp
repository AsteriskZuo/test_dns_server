#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <exception>
#include <string>
#include <sstream>

/**
 * @brief DoH客户端基础异常类
 */
class DoHException : public std::exception {
protected:
    std::string message_;
    int error_code_;
    std::string context_;

public:
    /**
     * @brief 构造函数
     * @param message 错误消息
     * @param error_code 错误代码
     * @param context 错误上下文
     */
    DoHException(const std::string& message, int error_code = 0, const std::string& context = "")
        : message_(message), error_code_(error_code), context_(context) {
        
        std::ostringstream oss;
        oss << "DoH Error";
        if (error_code_ != 0) {
            oss << " [" << error_code_ << "]";
        }
        if (!context_.empty()) {
            oss << " (" << context_ << ")";
        }
        oss << ": " << message_;
        message_ = oss.str();
    }

    /**
     * @brief 获取错误消息
     */
    const char* what() const noexcept override {
        return message_.c_str();
    }

    /**
     * @brief 获取错误代码
     */
    int code() const noexcept {
        return error_code_;
    }

    /**
     * @brief 获取错误上下文
     */
    const std::string& context() const noexcept {
        return context_;
    }
};

/**
 * @brief 网络相关异常
 */
class NetworkException : public DoHException {
public:
    NetworkException(const std::string& message, int curl_code = 0, const std::string& url = "")
        : DoHException(message, curl_code, "Network: " + url) {}
};

/**
 * @brief HTTP相关异常
 */
class HttpException : public DoHException {
public:
    HttpException(const std::string& message, int http_code, const std::string& url = "")
        : DoHException(message, http_code, "HTTP: " + url) {}
};

/**
 * @brief DNS解析异常
 */
class ParseException : public DoHException {
public:
    ParseException(const std::string& message, const std::string& data_type = "")
        : DoHException(message, 0, "Parse: " + data_type) {}
};

/**
 * @brief 配置相关异常
 */
class ConfigException : public DoHException {
public:
    ConfigException(const std::string& message, const std::string& config_file = "")
        : DoHException(message, 0, "Config: " + config_file) {}
};

/**
 * @brief 超时异常
 */
class TimeoutException : public DoHException {
public:
    TimeoutException(const std::string& message, int timeout_seconds = 0)
        : DoHException(message, timeout_seconds, "Timeout") {}
};

/**
 * @brief 验证异常
 */
class ValidationException : public DoHException {
public:
    ValidationException(const std::string& message, const std::string& field = "")
        : DoHException(message, 0, "Validation: " + field) {}
};

/**
 * @brief 缓存相关异常
 */
class CacheException : public DoHException {
public:
    CacheException(const std::string& message, const std::string& operation = "")
        : DoHException(message, 0, "Cache: " + operation) {}
};

/**
 * @brief 编码/解码异常
 */
class EncodingException : public DoHException {
public:
    EncodingException(const std::string& message, const std::string& encoding_type = "")
        : DoHException(message, 0, "Encoding: " + encoding_type) {}
};

/**
 * @brief 异常工具类
 */
class ExceptionUtils {
public:
    /**
     * @brief 从CURL错误代码创建网络异常
     * @param curl_code CURL错误代码
     * @param url 请求URL
     * @return NetworkException
     */
    static NetworkException from_curl_error(int curl_code, const std::string& url = "") {
        std::string message = "CURL error: " + std::to_string(curl_code);
        
        // 添加常见CURL错误的描述
        switch (curl_code) {
            case 6:  // CURLE_COULDNT_RESOLVE_HOST
                message = "Could not resolve host";
                break;
            case 7:  // CURLE_COULDNT_CONNECT
                message = "Could not connect to server";
                break;
            case 28: // CURLE_OPERATION_TIMEDOUT
                message = "Operation timed out";
                break;
            case 35: // CURLE_SSL_CONNECT_ERROR
                message = "SSL connection error";
                break;
            case 51: // CURLE_PEER_FAILED_VERIFICATION
                message = "SSL peer certificate verification failed";
                break;
            case 52: // CURLE_GOT_NOTHING
                message = "Server returned nothing";
                break;
            case 56: // CURLE_RECV_ERROR
                message = "Failure receiving network data";
                break;
            default:
                message = "Unknown CURL error: " + std::to_string(curl_code);
                break;
        }
        
        return NetworkException(message, curl_code, url);
    }

    /**
     * @brief 从HTTP状态码创建HTTP异常
     * @param http_code HTTP状态码
     * @param url 请求URL
     * @return HttpException
     */
    static HttpException from_http_error(int http_code, const std::string& url = "") {
        std::string message = "HTTP error: " + std::to_string(http_code);
        
        // 添加常见HTTP错误的描述
        switch (http_code) {
            case 400:
                message = "Bad Request";
                break;
            case 401:
                message = "Unauthorized";
                break;
            case 403:
                message = "Forbidden";
                break;
            case 404:
                message = "Not Found";
                break;
            case 408:
                message = "Request Timeout";
                break;
            case 429:
                message = "Too Many Requests";
                break;
            case 500:
                message = "Internal Server Error";
                break;
            case 502:
                message = "Bad Gateway";
                break;
            case 503:
                message = "Service Unavailable";
                break;
            case 504:
                message = "Gateway Timeout";
                break;
            default:
                if (http_code >= 400 && http_code < 500) {
                    message = "Client Error: " + std::to_string(http_code);
                } else if (http_code >= 500) {
                    message = "Server Error: " + std::to_string(http_code);
                } else {
                    message = "HTTP Error: " + std::to_string(http_code);
                }
                break;
        }
        
        return HttpException(message, http_code, url);
    }

    /**
     * @brief 创建DNS解析异常
     * @param message 错误消息
     * @param response_data 响应数据（用于调试）
     * @return ParseException
     */
    static ParseException dns_parse_error(const std::string& message, const std::string& response_data = "") {
        std::string full_message = message;
        if (!response_data.empty()) {
            full_message += " (Response length: " + std::to_string(response_data.length()) + " bytes)";
        }
        return ParseException(full_message, "DNS");
    }

    /**
     * @brief 创建JSON解析异常
     * @param message 错误消息
     * @param json_data JSON数据（用于调试）
     * @return ParseException
     */
    static ParseException json_parse_error(const std::string& message, const std::string& json_data = "") {
        std::string full_message = message;
        if (!json_data.empty() && json_data.length() < 200) {
            full_message += " (JSON: " + json_data + ")";
        } else if (!json_data.empty()) {
            full_message += " (JSON length: " + std::to_string(json_data.length()) + " chars)";
        }
        return ParseException(full_message, "JSON");
    }
};

#endif // EXCEPTIONS_HPP 