#include "client.h"
#include "ui_client.h"
#include <QDebug>
#include <QDialog>
#include <QErrorMessage>

Client::Client(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Client),   
    do_connected(false),
    is_connected(false),
    read_status(STATUS_NONE),
    filename_len(0),
    left_file_size(0)
{
    ui->setupUi(this);
    ui->server_address->setPlaceholderText("Server IP");
    ui->sync_button->setDisabled(true);
    ui->download_button->setDisabled(true);

    client_socket = new QTcpSocket(this);
    client_socket->abort();

    connect(client_socket, SIGNAL(connected()),
            this, SLOT(socket_connected()));
    connect(client_socket, SIGNAL(disconnected()),
            this, SLOT(handle_disconnect()));
    connect(client_socket, SIGNAL(readyRead()),
            this, SLOT(handle_msg()));
    connect(client_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(handle_socket_error()));
    connect(ui->file_listwidget,
            SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(get_files_entry(QListWidgetItem*)));
}

Client::~Client()
{
    delete ui;
}

void Client::connect_server()
{
    if (!do_connected) {
        do_connected = true;
        ui->state_label->setText(tr("Connecting.."));
        QString address = ui->server_address->text();

        client_socket->connectToHost(address, LISTEN_PORT);
    } else {
        if (is_connected) {
            /* disconnect socket */
            client_socket->close();
        } else {
            /* info only */
            ui->state_label->setText(tr("too frequent"));
        }
    }
}

void Client::socket_connected()
{
    is_connected = true;
    ui->state_label->setText(tr("Connected !"));
    ui->connect_button->setText(tr("Disconnect"));
    ui->sync_button->setDisabled(false);
    ui->download_button->setDisabled(false);
}

void Client::handle_disconnect()
{
    is_connected = false;
    do_connected = false;
    ui->state_label->setText(tr(""));
    ui->connect_button->setText(tr("Connect"));
}

void Client::handle_socket_error()
{
    do_connected = false;
    is_connected = false;
    ui->state_label->setText(tr(""));
    ui->connect_button->setText(tr("Connect"));
    ui->state_label->setText(tr("Connect Error, retry ?"));
}

void Client::on_sync_button_clicked()
{
    /* can not be clicked before sync finish */
    ui->sync_button->setDisabled(true);

    sendSyncMessage();
}

void Client::on_download_button_clicked()
{
    /* can not be clicked before download finish */
    ui->download_button->setDisabled(true);

    getDownloadFiles();
}


void Client::handle_msg()
{
    QTcpSocket *socket= qobject_cast<QTcpSocket *>(sender());
    QDataStream in(socket);

    in.setVersion(QDataStream::Qt_5_5);

    do {
        switch (read_status) {
        case STATUS_NONE:
            /* wait for total data len */
            if (socket->bytesAvailable() < (int)sizeof(int))
                return;

            in >> totalsize;
            read_status = STATUS_READ_TOTAL_SIZE;
            break;
        case STATUS_READ_TOTAL_SIZE:
            /* wait for msg tag ready */
            if (socket->bytesAvailable() < (int)sizeof(int))
                return;

            in >> tag;
            read_status = STATUS_READ_TAG;
            break;
        case STATUS_READ_TAG:
            switch ((int)(tag))
            {
            case MSG_TAG_LIST:
                in >> msg;
                handle_msg_list();
                read_status = STATUS_NONE;
                break;
            case MSG_TAG_ENTRY:
                in >> msg;
                list_files();
                read_status = STATUS_NONE;
                break;
            case MSG_TAG_FILE:
                read_status = STATUS_READ_FILENAME_LEN;
                break;
            default:
                qDebug() << tr("IO Error");
                read_status = STATUS_NONE;
                break;
            }
            break;
        case STATUS_READ_FILENAME_LEN:
            /* wait for filename len ready */
            if (socket->bytesAvailable() < (int)sizeof(int))
                return;
            in >> filename_len;
            read_status = STATUS_READ_FILENAME;
            break;
        case STATUS_READ_FILENAME:
            /* wait for filename ready */
            if (socket->bytesAvailable() < filename_len)
                return;
            in >> dfile_name;
            download_file = new QFile(dfile_name);
            if (!download_file->open(QFile::WriteOnly))
            {
                qDebug() << "Open file Error";
                return;
            }
            left_file_size = totalsize - (int)filename_len -
                    3 * (int)sizeof(int);
            read_status = STATUS_READ_FILE_DATA;
            break;
        case STATUS_READ_FILE_DATA:
            if (left_file_size == 0)
            {
                download_file->close();
                delete download_file;
                read_status = STATUS_NONE;
            } else {
                QByteArray inblock;
                int real_size = qMin((int)socket->bytesAvailable(), (int)left_file_size);
                inblock = socket->read(real_size);
                left_file_size -= download_file->write(inblock);
                inblock.resize(0);
            }

            break;
        default:
            break;
        }
    } while (read_status != STATUS_NONE || socket->bytesAvailable());

    totalsize = 0;
    tag = 0;
    filename_len = 0;
}

void Client::get_files_entry(QListWidgetItem *sender)
{
    QString dirname = sender->text();

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_5_5);

    out << (int)0;
    out << (int)MSG_TAG_ENTRY;
    out << dirname;
    out.device()->seek(0);
    out << (int)(block.size() - sizeof(int));

    client_socket->write(block);
}

void Client::list_files()
{
    QDialog dialog;

    QHBoxLayout layout(&dialog);
    QListWidget lwidget;
    dialog.setLayout(&layout);
    dialog.setWindowTitle(tr("Include Files"));

    QStringList dirlist = msg.split("#");
    for (int n = 0; n < dirlist.size(); n++) {
        QListWidgetItem *item = new QListWidgetItem(
                    dirlist.at(n).toLocal8Bit().data(),
                    &lwidget);
        lwidget.addItem(item);
    }

    lwidget.show();
    layout.addWidget(&lwidget);

    dialog.exec();
}

void Client::getDownloadFiles()
{
    QListWidgetItem *item = ui->file_listwidget->currentItem();
    if (!item) {
        QErrorMessage *err_dialog = new QErrorMessage(this);
        err_dialog->setWindowTitle(tr("Error"));
        err_dialog->showMessage(tr("No slected item"));
        ui->download_button->setDisabled(false);
        return;
    }

    QString dirname = item->text();
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_5_5);

    out << (int)0;
    out << (int)MSG_TAG_FILE;
    out << dirname;
    out.device()->seek(0);
    out << (int)(block.size() - sizeof(int));

    client_socket->write(block);
    ui->download_button->setDisabled(false);
}

void Client::sendSyncMessage()
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_5_5);

    out << (int)0;
    out << (int)MSG_TAG_SYNC;
    out.device()->seek(0);
    out << (int)(block.size() - sizeof(int));

    client_socket->write(block);
    ui->sync_button->setDisabled(false);
}

void Client::handle_msg_list()
{
    ui->file_listwidget->clear();
    QStringList dirlist = msg.split("#");
    for (int n = 0; n < dirlist.size(); n++) {
        ui->file_listwidget->insertItem(
                    ui->file_listwidget->count()+1,
                    dirlist.at(n).toLocal8Bit().data());
    }
}
