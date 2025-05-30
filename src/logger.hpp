#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>
#include <filesystem>

/**
 * @brief 日志管理器类
 * @details 提供结构化日志功能，支持控制台和文件输出
 */
class Logger {
private:
    static std::shared_ptr<spdlog::logger> console_logger_;
    static std::shared_ptr<spdlog::logger> file_logger_;
    static bool initialized_;

public:
    /**
     * @brief 初始化日志系统
     * @param log_level 日志级别 (trace, debug, info, warn, error, critical)
     * @param enable_file_logging 是否启用文件日志
     * @param log_file_path 日志文件路径
     */
    static void init(const std::string& log_level = "info", 
                    bool enable_file_logging = true,
                    const std::string& log_file_path = "logs/doh_client.log");

    /**
     * @brief 设置日志级别
     * @param level 日志级别字符串
     */
    static void set_level(const std::string& level);

    /**
     * @brief 记录调试信息
     */
    template<typename... Args>
    static void debug(const std::string& fmt, Args&&... args) {
        if (console_logger_) console_logger_->debug(fmt, std::forward<Args>(args)...);
        if (file_logger_) file_logger_->debug(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录一般信息
     */
    template<typename... Args>
    static void info(const std::string& fmt, Args&&... args) {
        if (console_logger_) console_logger_->info(fmt, std::forward<Args>(args)...);
        if (file_logger_) file_logger_->info(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录警告信息
     */
    template<typename... Args>
    static void warn(const std::string& fmt, Args&&... args) {
        if (console_logger_) console_logger_->warn(fmt, std::forward<Args>(args)...);
        if (file_logger_) file_logger_->warn(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录错误信息
     */
    template<typename... Args>
    static void error(const std::string& fmt, Args&&... args) {
        if (console_logger_) console_logger_->error(fmt, std::forward<Args>(args)...);
        if (file_logger_) file_logger_->error(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 记录严重错误信息
     */
    template<typename... Args>
    static void critical(const std::string& fmt, Args&&... args) {
        if (console_logger_) console_logger_->critical(fmt, std::forward<Args>(args)...);
        if (file_logger_) file_logger_->critical(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 刷新日志缓冲区
     */
    static void flush();

    /**
     * @brief 关闭日志系统
     */
    static void shutdown();

private:
    /**
     * @brief 字符串转换为spdlog日志级别
     */
    static spdlog::level::level_enum string_to_level(const std::string& level);
};

// 静态成员初始化
std::shared_ptr<spdlog::logger> Logger::console_logger_ = nullptr;
std::shared_ptr<spdlog::logger> Logger::file_logger_ = nullptr;
bool Logger::initialized_ = false;

// 实现
void Logger::init(const std::string& log_level, bool enable_file_logging, const std::string& log_file_path) {
    if (initialized_) {
        return;
    }

    try {
        // 创建控制台日志器
        console_logger_ = spdlog::stdout_color_mt("console");
        console_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        // 创建文件日志器（如果启用）
        if (enable_file_logging) {
            // 确保日志目录存在
            auto parent_path = std::filesystem::path(log_file_path).parent_path();
            if (!parent_path.empty()) {
                std::filesystem::create_directories(parent_path);
            }
            
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file_path, 1024 * 1024 * 5, 3); // 5MB per file, 3 files max
            file_logger_ = std::make_shared<spdlog::logger>("file", file_sink);
            file_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        }

        // 设置日志级别
        set_level(log_level);
        
        initialized_ = true;
        info("Logger initialized successfully with level: {}", log_level);
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
    }
}

void Logger::set_level(const std::string& level) {
    auto spdlog_level = string_to_level(level);
    if (console_logger_) console_logger_->set_level(spdlog_level);
    if (file_logger_) file_logger_->set_level(spdlog_level);
}

void Logger::flush() {
    if (console_logger_) console_logger_->flush();
    if (file_logger_) file_logger_->flush();
}

void Logger::shutdown() {
    if (console_logger_) {
        console_logger_->flush();
        spdlog::drop("console");
        console_logger_ = nullptr;
    }
    if (file_logger_) {
        file_logger_->flush();
        spdlog::drop("file");
        file_logger_ = nullptr;
    }
    initialized_ = false;
}

spdlog::level::level_enum Logger::string_to_level(const std::string& level) {
    if (level == "trace") return spdlog::level::trace;
    if (level == "debug") return spdlog::level::debug;
    if (level == "info") return spdlog::level::info;
    if (level == "warn") return spdlog::level::warn;
    if (level == "error") return spdlog::level::err;
    if (level == "critical") return spdlog::level::critical;
    return spdlog::level::info; // 默认级别
}

#endif // LOGGER_HPP 