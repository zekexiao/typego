#include <QCoreApplication>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // 独立短生命周期进程用于验证等待退出，不访问用户配置或注册表。
    QTimer::singleShot(500, &app, &QCoreApplication::quit);
    return app.exec();
}
