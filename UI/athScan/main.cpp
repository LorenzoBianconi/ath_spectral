#include "athscan.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    AthScan w;
    w.show();

    return a.exec();
}
