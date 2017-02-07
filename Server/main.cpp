#include "server.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QCoreApplication::addLibraryPath("./");

    QApplication a(argc, argv);
    Server w;
    w.show();

    return a.exec();
}
