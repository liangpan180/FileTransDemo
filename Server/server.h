#ifndef SERVER_H
#define SERVER_H

#include <QWidget>
#include <QTcpServer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidgetItem>
#include <QHash>

class QTcpServer;

/* Server listen port */
#define LISTEN_PORT 6789

#define MSG_TAG_SYNC    1       // sync file request
#define MSG_TAG_FILE    2       // download file
#define MSG_TAG_LIST    3       // dir list
#define MSG_TAG_ENTRY   4       // list file entry request

#define BLOCK_SIZE      4 * 1024   // 4K

/* handle msg status */
#define STATUS_NONE             1
#define STATUS_READ_TOTAL_SIZE  2
#define STATUS_READ_TAG         3
#define STATUS_READ_FILE_LEN    4
#define STATUS_READ_FILE        5

namespace Ui {
class Server;
}

class ClientItem : public QWidget
{
    Q_OBJECT

private:
    QTcpSocket *socket;
    QListWidget *listwidget;
    QLabel *ip;
    QLabel *port;
    QLabel *status;
    QPushButton *button;
    QHBoxLayout *layout;
    QListWidgetItem *item;

private slots:
    void Remove();

public:
    explicit ClientItem(QTcpSocket *socket, QListWidget *listwidget);
    ~ClientItem();
    void SetData(QString ip_str, QString port_str,
                 QString status_str, QString button_str);
    void Show();
    void RemoveItem();
};

class FileItem : public QWidget
{
    Q_OBJECT

public:
    QString dirpath;
    QString name;
    explicit FileItem(QString path, QString name, QListWidget *listwidget);
    ~FileItem();
    void SetData(QString str);
    void Show();

private slots:
    void list_files(QListWidgetItem *sender);

private:

    QLabel *filename;
    QListWidgetItem *item;
    QHBoxLayout *layout;
};

class Server : public QWidget
{
    Q_OBJECT

public:
    explicit Server(QWidget *parent = 0);
    ~Server();

private slots:
    /* Client new connect tigger */
    void handle_connect();
    void handle_disconnect();
    void on_add_button_clicked();
    void on_delete_button_clicked();
    void handle_msg();

private:
    Ui::Server *ui;
    QTcpServer *tcp_server;
    QHash<QString, ClientItem *> hash_clients;
    QHash<QString, FileItem *> hash_files;

    int totalsize;
    int tag;    // recv msg tag
    QString msg;    // recv msg

    int read_status;

    /* send dir list */
    void send_dir_entry(QTcpSocket *socket);
    void send_files_entry(QTcpSocket *socket);
    void send_files_data(QTcpSocket *socket);
};

#endif // SERVER_H
