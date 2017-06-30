#ifndef FTP_H
#define FTP_H

#include <QDialog>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <QTreeWidgetItem>

namespace Ui {
class FTP;
}

class FTP : public QDialog
{
    Q_OBJECT

public:
    explicit FTP(QWidget *parent = 0);
    ~FTP();
    void sendMessage(const QString &cmd);
    QString receiveMessage();
    void refreshDirectory();
    void getList();
    void download(const QString &fileName);
    void upload(const QString &fileName);

private slots:
    void on_Connect_clicked();
    void on_send(const QString &cmd);
    void on_Pwd_returnPressed();
    void onFtpError(const QString &error);

    void on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void on_pushButton_3_clicked();

signals:
    void send(const QString &cmd);
    void FtpError(const QString &error);

private:
    Ui::FTP *ui;
    struct protoent *protocol;
    int fd;
    struct sockaddr_in client;
    bool isConnected;
    QString fileSelected;
};

#endif // FTP_H
