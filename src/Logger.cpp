#include "Logger.h"
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>

static const char *LOG_BASENAME = "TypeGo.log";
static const int LOG_KEEP_DAYS = 7;

static void cleanupOldLogs(const QString &logDir)
{
    QDir dir(logDir);
    if (!dir.exists()) return;

    QDateTime now = QDateTime::currentDateTime();
    QRegularExpression re(QStringLiteral(R"(TypeGo_(\d{4})-(\d{2})-(\d{2})\.log)"));

    for (const QFileInfo &info : dir.entryInfoList(QStringList() << "TypeGo_*.log", QDir::Files)) {
        QRegularExpressionMatch match = re.match(info.fileName());
        if (match.hasMatch()) {
            int year = match.captured(1).toInt();
            int month = match.captured(2).toInt();
            int day = match.captured(3).toInt();
            QDateTime fileDate(QDate(year, month, day), QTime(0, 0));
            if (fileDate.daysTo(now) > LOG_KEEP_DAYS) {
                QFile::remove(info.absoluteFilePath());
            }
        }
    }
}

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
{
}

Logger::~Logger() = default;

void Logger::init(const QString &logDir)
{
    QString dir = logDir;
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    // 确保在 TypeGo 子目录下，即使 AppDataLocation 未包含应用名。
    if (!dir.endsWith("TypeGo", Qt::CaseInsensitive)) {
        dir += "/TypeGo";
    }
    QDir().mkpath(dir);

    cleanupOldLogs(dir);

    QString logPath = dir + "/" + LOG_BASENAME;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
        logPath.toStdString(), 0, 0, false, 0);

    console_sink->set_level(spdlog::level::debug);
    file_sink->set_level(spdlog::level::debug);

    m_logger = std::make_shared<spdlog::logger>("TypeGo",
        spdlog::sinks_init_list{console_sink, file_sink});
    m_logger->set_level(spdlog::level::debug);
    m_logger->flush_on(spdlog::level::debug);

    LOG_INFO("Logger initialized, log dir: {}", dir.toStdString());
}

void Logger::qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";

    switch (type) {
    case QtDebugMsg:
        Logger::instance().debug("[QT] {}:{} {} - {}", file, context.line, function, localMsg.constData());
        break;
    case QtInfoMsg:
        Logger::instance().info("[QT] {}:{} {} - {}", file, context.line, function, localMsg.constData());
        break;
    case QtWarningMsg:
        Logger::instance().warn("[QT] {}:{} {} - {}", file, context.line, function, localMsg.constData());
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        Logger::instance().error("[QT] {}:{} {} - {}", file, context.line, function, localMsg.constData());
        break;
    }
}
