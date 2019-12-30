#ifndef UDPFILETRANSFERWINDOW_H
#define UDPFILETRANSFERWINDOW_H

#define MAX_DATA_SIZE 1024
#define HEADER_SIZE sizeof(PACKET_TYPES)+ 8
#include <QMainWindow>
#include <QtNetwork>
#include <QDataStream>
#include <QFileDialog>
#include <QList>
#include <QFile>
#include <QTimer>
#include <QMovie>
#include <QDir>
#include <QFileInfo>
namespace Ui {
class UDPFileTransferWindow;
}


enum PACKET_TYPES
{
    START,
    ACK,
    DONE,
    DATA,
    MAX_TYPE=0x0FFFF

};
struct PACKET {

    PACKET_TYPES type;

    unsigned short length;

    unsigned short checksum;

    unsigned long seqNum;

    unsigned char data[MAX_DATA_SIZE];

};
class UDPFileTransferWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit UDPFileTransferWindow(QWidget *parent = 0);
    ~UDPFileTransferWindow();

    void initializeData(PACKET* p);

private slots:
    void recieveMsg();

    void on_connectPB_clicked();

    void on_sendFilePB_clicked();

    void on_injectErrorsCB_stateChanged(int arg1);

    void startTimedOut();

    void dataTimedOut();

    void doneTimedOut();


private:

    void startSendingData();
    void sendAck(unsigned long sn);
    unsigned short calculateCheckSum(PACKET *p);
    unsigned long expectedSeqNum;
    bool injectErrors;
    QMovie* m;
    QTimer* startTimer;
    QTimer* dataTimer;
    QTimer* doneTimer;
    Ui::UDPFileTransferWindow *ui;
    QDir* curDir;
    QHostAddress* dstIP;
    quint16 dstPN;
    quint16 srcPN;
    QUdpSocket* sendSckt;
    QUdpSocket* rcvSckt;
    QDataStream in;
    QDataStream out;
    QList<PACKET*> pcktList;
    QFile* sentFile;
    QFile* recievedFile;
    QString fileName;
};

#endif // UDPFILETRANSFERWINDOW_H
