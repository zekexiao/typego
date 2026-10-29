#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QStyleFactory>
#include <QTranslator>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    QTranslator translator;
    if (translator.load(QLocale::system(), "NeoJsonViewer", "_",
                        QCoreApplication::applicationDirPath() + "/translations")) {
        a.installTranslator(&translator);
    }

    a.setWindowIcon(QIcon(":/icon.png"));
    a.setStyle(QStyleFactory::create("fusion"));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
