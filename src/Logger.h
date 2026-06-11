#pragma once

#include <QObject>
#include <QString>
#include <spdlog/spdlog.h>
#include <memory>

// 封装应用日志初始化、分级记录和 Qt 消息转发。
class Logger : public QObject {
    Q_OBJECT
public:
    static Logger &instance();
    void init(const QString &logDir = QString());

    template<typename... Args>
    void debug(const char *fmt, Args &&...args) {
        if (m_logger) m_logger->debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(const char *fmt, Args &&...args) {
        if (m_logger) m_logger->info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const char *fmt, Args &&...args) {
        if (m_logger) m_logger->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const char *fmt, Args &&...args) {
        if (m_logger) m_logger->error(fmt, std::forward<Args>(args)...);
    }

    static void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();

    std::shared_ptr<spdlog::logger> m_logger;
};

#define LOG_DEBUG(...) Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::instance().info(__VA_ARGS__)
#define LOG_WARN(...)  Logger::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().error(__VA_ARGS__)
