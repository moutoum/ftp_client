#include "ftp.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    FTP w;
    w.show();

    return a.exec();
}
