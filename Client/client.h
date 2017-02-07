#ifndef CLIENT_H
#define CLIENT_H

#include <QWidget>
#include <QTcpSocket>
#include <QListWidgetItem>
#include <QHBoxLayout>
#include <QFile>
#include <QProgressDialog>

/* Server listen port */
#define LISTEN_PORT 6789

/* message type tag */
#define MSG_TAG_SYNC    1       // sync file request
#define MSG_TAG_FILE    2       // download file
#define MSG_TAG_LIST    3       // dir list
#define MSG_TAG_ENTRY   4       // list file entry request

/* handle msg status */
#define STATUS_NONE                 1
#define STATUS_READ_TOTAL_SIZE      2
#define STATUS_READ_TAG             3
#define STATUS_READ_FILE_LEN        4
#define STATUS_READ_FILENAME_LEN    5
#define STATUS_READ_FILENAME        6
#define STATUS_READ_FILE_DATA       7

namespace Ui {
class Client;
}

class Client : public QWidget
{
    Q_OBJECT

public:
    explicit Client(QWidget *parent = 0);
    ~Client();

private slots:
    void connect_server();
    void socket_connected();
    void handle_disconnect();
    void handle_socket_error();
    void on_sync_button_clicked();
    void on_download_button_clicked();
    void handle_msg();
    void get_files_entry(QListWidgetItem *sender);

private:
    Ui::Client *ui;
    QTcpSocket *client_socket;
    /* just meens did connect operation, but may not connected */
    bool do_connected;
    /* connect successfully */
    bool is_connected;

    int totalsize;
    int tag;    // recv msg tag
    QString msg;    // recv msg

    int read_status;

    QFile *download_file;
    int filename_len;
    QString dfile_name;
    int left_file_size;

    void sendSyncMessage();
    void handle_msg_list();
    void list_files();
    void getDownloadFiles();
};

#endif // CLIENT_H
