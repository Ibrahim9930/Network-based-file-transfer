#include "udpfiletransferwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    UDPFileTransferWindow w;
    w.show();

    return a.exec();
}
