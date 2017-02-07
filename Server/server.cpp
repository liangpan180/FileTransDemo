#include "server.h"
#include "ui_server.h"
#include <QDebug>
#include <QTcpSocket>
#include <QFileDialog>

Server::Server(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Server),
    totalsize(0),
    read_status(STATUS_NONE)
{
    ui->setupUi(this);

    tcp_server = new QTcpServer(this);
    if (!tcp_server->listen(QHostAddress::AnyIPv4, LISTEN_PORT))
    {
        qDebug() << "Listen failed";
        close();
    }

    connect(tcp_server, SIGNAL(newConnection()),
            this, SLOT(handle_connect()));
}

Server::~Server()
{
    delete tcp_server;
    delete ui;
}

void Server::handle_connect()
{
    QTcpSocket *socket = tcp_server->nextPendingConnection();
    qDebug() << "Client: " << socket->peerAddress().toString()
             << ":" << socket->peerPort();

    connect(socket, SIGNAL(disconnected()),
            this, SLOT(handle_disconnect()));
    connect(socket, SIGNAL(readyRead()),
            this, SLOT(handle_msg()));

    ClientItem *client_item = new ClientItem(socket, ui->client_list);
    client_item->SetData(socket->peerAddress().toString(),
                         QString::number(socket->peerPort()),
                         tr("Connected"), tr("Disconnect"));
    client_item->Show();

    QString addr = socket->peerAddress().toString() + ":" +
            QString::number(socket->peerPort());
    hash_clients.insert(addr, client_item);
}

void Server::handle_disconnect()
{
    QTcpSocket *socket= qobject_cast<QTcpSocket *>(sender());
    QString addr = socket->peerAddress().toString() + ":" +
            QString::number(socket->peerPort());
    qDebug() << "Client: " << addr << " disconnect";

    QHash<QString, ClientItem*>::iterator it = hash_clients.find(addr);
    ClientItem *close_item = (*it);
    close_item->SetData(socket->peerAddress().toString(),
                        QString::number(socket->peerPort()),
                        tr("Disconnected"), tr("Connect"));
    close_item->Show();
    close_item->RemoveItem();
    delete close_item;
    hash_clients.erase(it);
}

void Server::on_add_button_clicked()
{
    QStringList select_file;
    QDir dir;
    QString fname;
    QFileInfo file_info;
    QFileDialog *filedialog = new QFileDialog(this, tr("Select Dirs"));
    filedialog->setFileMode(QFileDialog::Directory);
    if(filedialog->exec() == QDialog::Accepted)
    {
        select_file = filedialog->selectedFiles();
        fname = select_file.at(0).toLocal8Bit().constData();
        dir = filedialog->directory();
        file_info = QFileInfo(fname);
        qDebug() << "Selected " << select_file;
        qDebug() << "name " << dir.absoluteFilePath(fname);

        FileItem *file_item = new FileItem(fname,
                                           file_info.fileName(),
                                           ui->file_list);
        file_item->SetData(file_info.fileName());
        file_item->Show();

        hash_files.insert(file_info.fileName(), file_item);
    }
}

void Server::on_delete_button_clicked()
{
    QList<QListWidgetItem*> list = ui->file_list->selectedItems();

    if(list.size() == 0)
        return;


    QListWidgetItem* sel = list[0];
    if (sel)
    {
        FileItem *fitem = static_cast<FileItem *>(ui->file_list->itemWidget(sel));
        qDebug() << "Path " << fitem->dirpath;
        QHash<QString, FileItem*>::iterator it = hash_files.find(fitem->name);
        hash_files.erase(it);

        int r = ui->file_list->row(sel);
        ui->file_list->takeItem(r);
    }
}

void Server::handle_msg()
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
            case MSG_TAG_SYNC:
                send_dir_entry(socket);
                break;
            case MSG_TAG_ENTRY:
                /* wait for msg ready */
                if (socket->bytesAvailable() < totalsize - 2 * (int)sizeof(int))
                    return;

                in >> msg;
                send_files_entry(socket);
                break;
            case MSG_TAG_FILE:
                /* wait for msg ready */
                if (socket->bytesAvailable() < totalsize - 2 * (int)sizeof(int))
                    return;

                in >> msg;
                send_files_data(socket);
                break;
            default:
                qDebug() << tr("IO Error");
                break;
            }

            read_status = STATUS_NONE;
            break;
        default:
            break;
        }
    } while (read_status != STATUS_NONE || socket->bytesAvailable());

    totalsize = 0;
    tag = 0;
}

void Server::send_dir_entry(QTcpSocket *socket)
{
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_5_5);

    out << (int)0;
    out << (int)MSG_TAG_LIST;

    /* dir list data */
    QString data;
    QHash<QString, FileItem*>::iterator it = hash_files.begin();
    for (; it != hash_files.end(); it++)
    {
        FileItem *fitem = static_cast<FileItem *>(*it);
        data += fitem->name + "#";
    }
    out << data;

    out.device()->seek(0);
    out << (int)(block.size() - sizeof(int));

    socket->write(block);
}

void Server::send_files_entry(QTcpSocket *socket)
{
    QHash<QString, FileItem*>::iterator it = hash_files.find(msg);
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);

    out.setVersion(QDataStream::Qt_5_5);

    out << (int)0;
    out << (int)MSG_TAG_ENTRY;

    /* file entry data */
    QString data;
    QDir dir((*it)->dirpath);
    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); i++) {
        QFileInfo fileinfo = list.at(i);
        if (fileinfo.fileName() == "." || fileinfo.fileName() == "..")
            continue;
        data += fileinfo.fileName() + "#";
    }
    out << data;

    out.device()->seek(0);
    out << (int)(block.size() - sizeof(int));

    socket->write(block);
}

void Server::send_files_data(QTcpSocket *socket)
{
    QHash<QString, FileItem*>::iterator it = hash_files.find(msg);

    QDir dir((*it)->dirpath);
    QFileInfoList list = dir.entryInfoList();
    /*
     * Data layout: TotalSize + TAG + FileNameSize + FileName + FileData
     */
    for (int i = 0; i < list.size(); i++) {
        QFileInfo fileinfo = list.at(i);
        if (fileinfo.fileName() == "." || fileinfo.fileName() == ".." ||
                fileinfo.isDir())
            continue;

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        block.resize(0);

        out.setVersion(QDataStream::Qt_5_5);

        QString fname = fileinfo.fileName();
        /* TotalSize */
        int total = 0;

        out << (int)0;
        /* TAG */
        out << (int)MSG_TAG_FILE;
        /* FileNameSize */
        out << (int)0;
        /* FileName */
        out << fname;

        out.device()->seek(2 * sizeof(int));
        out << int(block.size() - 3 * sizeof(int));

        /* FileData */
        QFile current_file(fileinfo.absoluteFilePath());
        if (!current_file.open(QFile::ReadOnly)) {
            qDebug() << "Open file Error";
            return;
        }

        total = block.size() + current_file.size();

        /* set TotalSize */
        out.device()->seek(0);
        out << total;

        int out_size = (int)total - (int)socket->write(block);
        block.resize(0);
        while (out_size > 0) {
            block = current_file.read(qMin((int)out_size, (int)BLOCK_SIZE));
            out_size -= socket->write(block);
            block.resize(0);
        }
        current_file.close();
    }
}

void ClientItem::Remove()
{
    socket->close();
}

ClientItem::ClientItem(QTcpSocket *socket, QListWidget *listwidget) :
    QWidget(listwidget),
    socket(socket),
    listwidget(listwidget)
{
    layout = new QHBoxLayout(this);
    this->setLayout(layout);

    ip = new QLabel;
    port = new QLabel;
    status = new QLabel;
    button  = new QPushButton;
    layout->addWidget(ip);
    layout->addWidget(port);
    layout->addWidget(status);
    layout->addWidget(button);

    item = new QListWidgetItem(listwidget);
    listwidget->addItem(item);
    listwidget->setItemWidget(item, this);

    connect(button, SIGNAL(clicked()), this, SLOT(Remove()));
}

ClientItem::~ClientItem()
{
    delete ip;
    delete port;
    delete status;
    delete button;
    delete layout;
    delete item;
}

void ClientItem::SetData(QString ip_str, QString port_str,
                         QString status_str, QString button_str)
{
    ip->setText(ip_str);
    port->setText(port_str);
    status->setText(status_str);
    button->setText(button_str);
}

void ClientItem::Show()
{
    item->setSizeHint(QSize(0,50));
    this->show();
}

void ClientItem::RemoveItem()
{
    listwidget->removeItemWidget(item);
}


FileItem::FileItem(QString path, QString name, QListWidget *listwidget) :
    QWidget(listwidget),
    dirpath(path),
    name(name)
{
    layout = new QHBoxLayout(this);
    this->setLayout(layout);

    filename = new QLabel;
    layout->addWidget(filename);
    item = new QListWidgetItem(listwidget);
    listwidget->addItem(item);
    listwidget->setItemWidget(item, this);

    connect(listwidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            this, SLOT(list_files(QListWidgetItem*)));
}

FileItem::~FileItem()
{
    delete filename;
    delete layout;
    delete item;
}

void FileItem::SetData(QString str)
{
    filename->setText(str);
}

void FileItem::Show()
{
    item->setSizeHint(QSize(0,40));
    this->show();
}

void FileItem::list_files(QListWidgetItem *sender)
{
    if (sender != item)
        return;

    QDialog dialog;

    QHBoxLayout layout(&dialog);
    QListWidget lwidget;
    dialog.setLayout(&layout);
    dialog.setWindowTitle(tr("Include Files"));

    QDir dir(dirpath);
    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); i++) {
        QFileInfo fileinfo = list.at(i);
        if (fileinfo.fileName() == "." || fileinfo.fileName() == "..")
            continue;
        QListWidgetItem *item = new QListWidgetItem(fileinfo.fileName(), &lwidget);
        lwidget.addItem(item);
    }

    lwidget.show();
    layout.addWidget(&lwidget);

    dialog.exec();
}
