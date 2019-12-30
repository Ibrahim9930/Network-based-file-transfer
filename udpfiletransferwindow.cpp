#include "udpfiletransferwindow.h"
#include "ui_udpfiletransferwindow.h"

UDPFileTransferWindow::UDPFileTransferWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::UDPFileTransferWindow)
{
    ui->setupUi(this);

    expectedSeqNum = 0;
    injectErrors = false;

    sendSckt = new QUdpSocket(this);
    rcvSckt= new QUdpSocket(this);
    connect(rcvSckt, SIGNAL(readyRead()),this, SLOT(recieveMsg()));

    startTimer = new QTimer(this);
    connect(startTimer, SIGNAL(timeout()),this, SLOT(startTimedOut()));
    dataTimer = new QTimer(this);
    connect(dataTimer, SIGNAL(timeout()),this, SLOT(dataTimedOut()));
    doneTimer = new QTimer(this);
    connect(doneTimer, SIGNAL(timeout()),this, SLOT(doneTimedOut()));

    ui->sendFilePB->setEnabled(false);

    m=new QMovie(":/icons/loading-min2.gif");
    ui->loadingLbl->setMovie(m);

    QString curPath = QDir::currentPath();
    curDir=new QDir(curPath);
    if(QFileInfo::exists(curPath+"/downloaded_files")){
        qDebug()<<"A downloading directory already exists in : "+curPath;
    }
    else{
        curDir->mkdir("downloaded_files");
        qDebug()<<"Created a dowonloading directory at : "+curPath;
    }

}

UDPFileTransferWindow::~UDPFileTransferWindow()
{
    delete ui;
}

void UDPFileTransferWindow::on_connectPB_clicked()
{
    QRegExp re("\\d*");
    int PN1 = ui->srcPNLE->text().toInt();
    int PN2 = ui->dstPNLE->text().toInt();
    if (!re.exactMatch(ui->srcPNLE->text()) || ui->srcPNLE->text().isEmpty()||PN1>65536||PN1<0){
        ui->srcPNLE->setStyleSheet("border:2px solid rgb(199, 0, 0);");
        return;
    }
    else{
        ui->srcPNLE->setStyleSheet("border : none; border-bottom :2px solid black;font: 75 8pt \"Tahoma\";");
    }
    if (!re.exactMatch(ui->dstPNLE->text()) || ui->dstPNLE->text().isEmpty()||PN2>65536||PN2<0){
        ui->dstPNLE->setStyleSheet("border:2px solid rgb(199, 0, 0);");
        return;
    }
    else{
        ui->dstPNLE->setStyleSheet("border : none; border-bottom :2px solid black;font: 75 8pt \"Tahoma\";");
    }

    QString IP = ui->dstIPLE->text();
    QStringList IPParts = IP.split(".");
    if(IPParts.size()!=4){
        ui->dstIPLE->setStyleSheet("border:2px solid rgb(199, 0, 0);");
        return;
    }
    else
        for(int i=0;i<4;i++){
            int part = IPParts.at(i).toInt();
            if(part <0 || part >255){
                ui->dstIPLE->setStyleSheet("border:2px solid rgb(199, 0, 0);");
                return;
            }
        }
    ui->dstIPLE->setStyleSheet("border : none; border-bottom :2px solid black;font: 75 8pt \"Tahoma\";");


    srcPN = ui->srcPNLE->text().toUShort();
    dstPN = ui->dstPNLE->text().toUShort();
    dstIP = new QHostAddress(IP);

    rcvSckt->bind(srcPN);
    ui->sendFilePB->setEnabled(true);
}

void UDPFileTransferWindow::on_sendFilePB_clicked()
{
    QString filePath=QFileDialog::getOpenFileName(this,"Choose the file you want to send","C:/Users/masri/Downloads/Uni/Computer Networks I/Homewroks/uploaded_files");

    if(filePath.isEmpty()){
        return;
    }

    // Get the file name
    QStringList format= filePath.split("/");
    fileName = format.at(format.size()-1);

    sentFile = new QFile(filePath);

    if(!sentFile->open(QIODevice::ReadOnly)){
        qDebug()<<"Error opening file";
        return;
    }

    in.setDevice(&*(sentFile));

    //Add packets to the list
    PACKET* dataPckt;
    int seqNum = 1;
    while(!sentFile->atEnd()){

        dataPckt = new PACKET;
        char* outChars = new char[1024];
        int size = in.readRawData(outChars,1024);
        dataPckt->type = DATA;
        dataPckt->length = size+12;
        dataPckt->checksum = 0;
        initializeData(dataPckt);
        memcpy(dataPckt->data,outChars,size);
        seqNum = (seqNum + 1)%2;
        dataPckt->seqNum = seqNum;
        unsigned short cs = calculateCheckSum(dataPckt);
        dataPckt->checksum = cs;
        pcktList.append(dataPckt);

    }

    sentFile->close();

    if(pcktList.isEmpty()){
        qDebug()<<"No data to send";
        return;
    }

    // Prepare start Packet
    PACKET* sentPckt= new PACKET;
    sentPckt->type = START;
    initializeData(sentPckt);
    memcpy(sentPckt->data,fileName.toStdString().c_str(),fileName.size()+1);
    sentPckt->length = fileName.length()+1+12;
    sentPckt->checksum = 0;
    sentPckt->seqNum = 0x0000ffff;
    unsigned short cs = calculateCheckSum(sentPckt);
    sentPckt->checksum = cs;

    // Send Packet
    sendSckt->writeDatagram((const char *)sentPckt,(qint64)(sizeof(PACKET)),*(dstIP),dstPN);
    qDebug()<<"Sent START request with sequence number "<<sentPckt->seqNum;

    // Start Timer
    startTimer->start(200);

    // Start Animation
    m->start();
}

void UDPFileTransferWindow::on_injectErrorsCB_stateChanged(int arg1)
{
    injectErrors = ui->injectErrorsCB->isChecked();
    qDebug()<<"Injecting errors : "<<injectErrors;
}

void UDPFileTransferWindow::recieveMsg(){

    PACKET *rcvPckt = new PACKET;

    char buffer[2048] = {0};
    int size = rcvSckt->pendingDatagramSize();
    rcvSckt->readDatagram(buffer,size);
    rcvPckt =(PACKET *)buffer;
    unsigned short cs = calculateCheckSum(rcvPckt);

    if(injectErrors && (rand()%10)==0){
        cs+=1;
    }
    bool isCorrupt = (cs != 0);

    if(rcvPckt->type == START){

        if(isCorrupt){
           qDebug()<<"Recieved a corrupt START packet";
           return;
        }

        fileName = QString::fromStdString(reinterpret_cast<char*>(rcvPckt->data));
        QString fileDir =curDir->path()+"/downloaded_files/"+fileName;
        recievedFile= new QFile(fileDir);
        qDebug()<<"Recieved START request with filename : "<<fileName;
        if(!recievedFile->open(QIODevice::WriteOnly)){
            qDebug()<<"problem creating file";
            return;
        }
        out.setDevice(&*(recievedFile));
        sendAck(rcvPckt->seqNum);

        m->start();
    }
    else if(rcvPckt->type == DATA){
        qDebug()<<"Recieved data with sequence number "<<rcvPckt->seqNum;

        if(rcvPckt->seqNum == expectedSeqNum && !isCorrupt){

            qDebug()<<"Writing data";
            char* inChars = new char[1024];
            inChars=reinterpret_cast<char*>(rcvPckt->data);
            out.writeRawData(inChars,(rcvPckt->length - 12));
            sendAck(rcvPckt->seqNum);
            expectedSeqNum = (expectedSeqNum + 1)%2;
        }
        else{
            QString error = isCorrupt?"Recieved a corrupt data packet":"Recieved an out of order packet";
            qDebug()<<error;
            if(expectedSeqNum == 0){
                sendAck(1);
            }
            else{
                sendAck(0);
            }
        }
    }
    else if(rcvPckt->type == ACK){

        qDebug()<<"Recieved ACK with sequence number : "<<rcvPckt->seqNum;

        if(isCorrupt){
            qDebug()<<"Recieved a corrupt ack";
            return;
        }

        if(rcvPckt->seqNum == 0x0000ffff){

              startTimer->stop();

              // Error proofing
              if(pcktList.isEmpty()){
                qDebug()<<"Error occured with the Packet list when the START ack was recieved";
                 dataTimer->start(200);
                 return;
        }

        // Start Sending Data
        sendSckt->writeDatagram((const char*)pcktList.first(),(qint64)sizeof(PACKET),*(dstIP),dstPN);
        qDebug()<<"Sent data with sequence number : "<<pcktList.first()->seqNum;
        dataTimer->start(200);
        }

        else if(rcvPckt->seqNum == 0xffffffff){
            doneTimer->stop();
            expectedSeqNum = 0;
            m->stop();
        }

        else if(rcvPckt->seqNum == expectedSeqNum){

            dataTimer->stop();
            expectedSeqNum = (expectedSeqNum + 1)%2;
            pcktList.removeFirst();

            // No packets left
            if(pcktList.isEmpty()){

                PACKET* donePckt=new PACKET;
                donePckt->type = DONE;
                donePckt->length = 12;
                initializeData(donePckt);
                donePckt->checksum = 0;
                donePckt->seqNum = 0xffffffff;
                unsigned short checkSum = calculateCheckSum(donePckt);
                donePckt->checksum = checkSum;

                if(injectErrors && (rand()%6)==0)
                    qDebug()<<"Missing a packet with sequence number : "<<donePckt->seqNum;
                else
                    sendSckt->writeDatagram((const char*)donePckt,(qint64)sizeof(PACKET),*(dstIP),dstPN);

                doneTimer->start(200);
            }
            else{
                if(injectErrors && (rand()%6)==0){
                    qDebug()<<"Missing a packet with sequence number : "<<pcktList.first()->seqNum;
                }
                else{
                    qDebug()<<"Sent data with sequence number : "<<pcktList.first()->seqNum;
                    sendSckt->writeDatagram((const char*)pcktList.first(),(qint64)sizeof(PACKET),*(dstIP),dstPN);
                }
                dataTimer->start(200);
            }
        }
    }
    else if(rcvPckt->type == DONE){
        qDebug()<<"Finished recieving data";

        recievedFile->close();
        sendAck(rcvPckt->seqNum);
        expectedSeqNum = 0;
        m->stop();
    }

}

void UDPFileTransferWindow::sendAck(unsigned long sn){
    if(injectErrors && (rand()%8) == 0){
        qDebug()<<"Missing an ack with sequence number : "<<sn;
        return;
    }
    qDebug()<<"Sending ack with sequence number : "<<sn;

    //Prepare Ack Packet
    PACKET* ackPckt = new PACKET;
    ackPckt->type = ACK;
    ackPckt->length = 12;
    ackPckt->seqNum = sn;
    initializeData(ackPckt);
    ackPckt->checksum = 0;
    unsigned short cs = calculateCheckSum(ackPckt);
    ackPckt->checksum = cs;

    sendSckt->writeDatagram((const char *)ackPckt,(qint64)sizeof(PACKET),*(dstIP),dstPN);
}

void UDPFileTransferWindow::startTimedOut(){
    qDebug()<<"START timed out";

    PACKET* sentPckt= new PACKET;
    sentPckt->type = START;
    initializeData(sentPckt);
    memcpy(sentPckt->data,fileName.toStdString().c_str(),fileName.size()+1);
    sentPckt->length = fileName.length()+1+12;
    sentPckt->seqNum = 0x0000ffff;
    sentPckt->checksum = 0;
    unsigned short cs = calculateCheckSum(sentPckt);
    sentPckt->checksum = cs;

    sendSckt->writeDatagram((const char *)sentPckt,(qint64)sizeof(PACKET),*(dstIP),dstPN);
}

void UDPFileTransferWindow::dataTimedOut(){

    qDebug()<<"data timed out";
    if(pcktList.isEmpty()){
        qDebug()<<"An error occured with Packet List when data timed out";
        return;
    }
    sendSckt->writeDatagram((const char*)pcktList.first(),(qint64)sizeof(PACKET),*(dstIP),dstPN);
}

void UDPFileTransferWindow::doneTimedOut(){
    qDebug()<<"DONE timed out";

    PACKET* donePckt=new PACKET;
    donePckt->type = DONE;
    donePckt->length = 12;
    donePckt->seqNum = 0Xffffffff;
    initializeData(donePckt);
    donePckt->checksum = 0;
    unsigned short cs = calculateCheckSum(donePckt);
    donePckt->checksum = cs;

    sendSckt->writeDatagram((const char*)donePckt,(qint64)sizeof(PACKET),*(dstIP),dstPN);
}

unsigned short UDPFileTransferWindow::calculateCheckSum(PACKET *p){

    unsigned short* ptr = (unsigned short*)p;

    unsigned long sum = 0;

    for(int i=0;i<sizeof(PACKET)/2;i++){
        sum += *(ptr);
        if (sum & 0x00010000){
            sum +=1;
        }
        sum &= 0x0000ffff;
        ptr++;
    }
    sum &= 0x0000ffff;
    sum = ~sum;

    return (unsigned short)sum;
}

void UDPFileTransferWindow::initializeData(PACKET *p){

    for(int i=0;i<MAX_DATA_SIZE;i++){
        p->data[i] = 0;
    }
}


