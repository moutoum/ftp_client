#include "ftp.h"
#include "ui_ftp.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>

FTP::FTP(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FTP),
    isConnected(false)
{
    ui->setupUi(this);
    connect(this,SIGNAL(send(const QString &)),this,SLOT(on_send(const QString &)));
    connect(this, SIGNAL(FtpError(QString)), this, SLOT(onFtpError(QString)));
    ui->label->hide();
    ui->Pwd->setDisabled(true);
    ui->mode->setDisabled(true);
    ui->pushButton->setDisabled(true);
    ui->pushButton_2->setDisabled(true);
    ui->pushButton_3->setDisabled(true);
    ui->progressBar->hide();
}

FTP::~FTP()
{
    delete ui;
}

void FTP::on_Connect_clicked()
{
    QString ret;

    protocol = getprotobyname("TCP");
    fd = socket(AF_INET, SOCK_STREAM, protocol->p_proto);
    client.sin_family = AF_INET;
    client.sin_port = htons(ui->Port->text().toInt());
    client.sin_addr.s_addr = inet_addr(ui->IP->text().toStdString().data());
    if (::connect(fd, (const struct sockaddr *)&client, sizeof(struct sockaddr)) == -1)
    {
        ::close(fd);
        emit FtpError("Connect: " + QString(strerror(errno)));
        return;
    }
    ret = receiveMessage();
    if (ret.left(3).toInt() >= 300)
    {
        ::close(fd);
        emit FtpError(ret);
        return;
    }
    sendMessage(tr("USER %1\r\n").arg(ui->Login->text()));
    ret = receiveMessage();
    if (ret.left(3).toInt() >= 400)
    {
        ::close(fd);
        emit FtpError(ret);
        return;
    }
    sendMessage(tr("PASS %1\r\n").arg(ui->Pass->text()));
    ret = receiveMessage();
    if (ret.left(3).toInt() >= 300)
    {
        ::close(fd);
        emit FtpError(ret);
        return;
    }
    ui->IP->setDisabled(true);
    ui->Port->setDisabled(true);
    ui->Login->setDisabled(true);
    ui->Pass->setDisabled(true);
    ui->Connect->setDisabled(true);
    ui->Pwd->setDisabled(false);
    ui->mode->setDisabled(false);
    ui->pushButton->setDisabled(false);
    ui->pushButton_3->setDisabled(false);
    ui->label->hide();
    isConnected = true;
    refreshDirectory();
}

void FTP::sendMessage(const QString &cmd)
{
    if (::write(fd, cmd.toStdString().data(), cmd.length()) == -1)
    {
        emit FtpError(strerror(errno));
        return;
    }
}

QString FTP::receiveMessage()
{
    char buff[2] = {0};
    QString str;
    int out;

    while ((out = read(fd, buff, 1)) > 0)
    {
        str += buff;
        if (str.contains("\r\n"))
        {
            return (str);
        }
        buff[0] = 0;
    }
    if (out == -1)
    {
        emit FtpError(strerror(errno));
        return ("");
    }
    return (str);
}

void FTP::onFtpError(const QString &error)
{
    ui->label->setText(error);
    ui->label->show();
}

void FTP::refreshDirectory()
{
    QString ret;

    sendMessage("PWD\r\n");
    ret = receiveMessage();
    if (ret.toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
    int begin = ret.indexOf("\"") + 1, end = ret.lastIndexOf("\"");
    QString path = ret.mid(begin, end - begin);
    ui->Pwd->setText(path);
    getList();
}

void FTP::getList()
{
    struct protoent *tmpProtocol;
    int tmpFd;
    struct sockaddr_in tmpClient;
    struct sockaddr_in sockinfo;
    int acceptedFd;
    QString ret, str;
    char buff[3096] = {0};
    socklen_t t;
    int port = UINT16_MAX;
    int out;

    if (ui->mode->currentIndex() == 0)
    {
        // Create server
        while (1)
        {
            tmpProtocol = getprotobyname("TCP");
            tmpFd = socket(AF_INET, SOCK_STREAM, tmpProtocol->p_proto);
            int iSetOption = 1;
            setsockopt(tmpFd, SOL_SOCKET, SO_REUSEADDR, (char *)&iSetOption, sizeof(iSetOption));
            while (1)
            {
                port--;
                sockinfo.sin_family = AF_INET;
                sockinfo.sin_port = htons(port);
                sockinfo.sin_addr.s_addr = INADDR_ANY;
                if (bind(tmpFd, (const struct sockaddr *)&sockinfo, sizeof(struct sockaddr)) != -1)
                    break;
                if (port <= 1024)
                {
                    emit FtpError("Error with port");
                    ::close(tmpFd);
                    return;
                }
            }
            listen(tmpFd, 42);
            int a = port / 0x100, b = port % 0x100;
            str.sprintf("PORT 127,0,0,1,%d,%d\r\n", a, b);
            sendMessage(str);
            ret = receiveMessage();
            if (ret.left(3).toInt() >= 300)
            {
                ::close(tmpFd);
                continue;
            }
            break;
        }
        sendMessage("LIST\r\n");
        ret = receiveMessage();
        if (ret.left(3).toInt() >= 300)
        {
            emit FtpError(ret);
            ::close(tmpFd);
            return;
        }
        bzero(&tmpClient, sizeof(struct sockaddr_in));
        if ((acceptedFd = ::accept(tmpFd, NULL, NULL)) == -1)
        {
            emit FtpError("Accept: " + QString(strerror(errno)));
            ::close(tmpFd);
            return;
        }
    }
    else
    {
        // Connect to server
        //QMessageBox::information(this, "", "totos");
        tmpProtocol = getprotobyname("TCP");
        tmpFd = socket(AF_INET, SOCK_STREAM, tmpProtocol->p_proto);
        int iSetOption = 1;
        setsockopt(tmpFd, SOL_SOCKET, SO_REUSEADDR, (char *)&iSetOption, sizeof(iSetOption));
        sendMessage("PASV\r\n");
        ret = receiveMessage();
        if (ret.left(3).toInt() >= 300)
        {
            emit FtpError(ret);
            ::close(tmpFd);
            return;
        }
        ret = ret.mid(ret.indexOf("(") + 1, ret.indexOf(")", ret.indexOf("(") + 1) - (ret.indexOf("(") + 1));
        for (int i = 0; i < 3; i++)
        {
            ret[ret.indexOf(",")] = '.';
        }
        tmpProtocol = getprotobyname("TCP");
        acceptedFd = socket(AF_INET, SOCK_STREAM, protocol->p_proto);
        tmpClient.sin_family = AF_INET;
        int port1 = ret.mid(ret.indexOf(",") + 1, ret.lastIndexOf(",") - ret.lastIndexOf(",") + 1).toInt(),
            port2 = ret.right(ret.length() - ret.lastIndexOf(",") + 1).toInt();
        tmpClient.sin_port = htons(port1 * 256 + port2);
        tmpClient.sin_addr.s_addr = inet_addr(ret.left(ret.indexOf(",")).toStdString().data());
        if (::connect(acceptedFd, (const struct sockaddr *)&tmpClient, sizeof(struct sockaddr)) == -1)
        {
            emit FtpError("Connect: " + QString(strerror(errno)));
            return;
        }
        sendMessage("LIST\r\n");
        ret = receiveMessage();
        if (ret.left(3).toInt() >= 300)
        {
            emit FtpError(ret);
            ::close(tmpFd);
            return;
        }
    }
    str = "";
    while ((out = read(acceptedFd, buff, 3095)) > 0)
    {
        str += buff;
        bzero(buff, 3096);
    }
    if (out == -1)
    {
        emit FtpError("read: " + QString(strerror(errno)));
        ::close(tmpFd);
        return;
    }
    ::close(acceptedFd);
    ::close(tmpFd);
    ret = receiveMessage();
    if (ret.toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }

    QList<QTreeWidgetItem *> items;
    QTreeWidgetItem *it;
    QStringList lines = str.split("\r\n");

    for (int i = 1; i < lines.length(); i++)
    {
        QStringList line = lines[i].split(" ");

        for (int j = 0; j < line.length(); j++)
        {
            if (line[j].isEmpty())
            {
                line.removeAt(j);
                j = -1;
                continue;
            }
        }
        if (line.length() >= 9)
        {
            it = new QTreeWidgetItem;
            if (line[0][0] == 'd')
            {
                it->setIcon(0, QIcon("../assets/folder.png"));
            }
            it->setText(0, tr("%1").arg(line[8]));
            it->setText(1, tr("%1").arg(line[4]));
            it->setText(2, tr("%1 %2 %3").arg(line[6], line[5], line[7]));
            items.append(it);
        }
    }
    ui->treeWidget->clear();
    ui->treeWidget->insertTopLevelItems(0, items);
}

void FTP::download(const QString &fileName)
{
    struct protoent *tmpProtocol;
    int tmpFd;
    struct sockaddr_in tmpClient;
    struct sockaddr_in sockinfo;
    int acceptedFd;
    QString ret, str;
    char buff[3096] = {0};
    socklen_t t;
    int port = 1024;
    int out;

    if (ui->mode->currentIndex() == 0)
    {
        // Create server
        tmpProtocol = getprotobyname("TCP");
        tmpFd = socket(AF_INET, SOCK_STREAM, tmpProtocol->p_proto);

        while (1)
        {
            sockinfo.sin_family = AF_INET;
            sockinfo.sin_port = htons(port);
            sockinfo.sin_addr.s_addr = INADDR_ANY;
            if (bind(tmpFd, (const struct sockaddr *)&sockinfo, sizeof(struct sockaddr)) != -1)
                break;
            port++;
        }
        listen(tmpFd, 42);
        int a = port / 256, b = port % 256;
        str.sprintf("PORT 127,0,0,1,%d,%d\r\n", a, b);
        sendMessage(str);
        ret = receiveMessage();
        if (ret.toInt() >= 300)
        {
            emit FtpError(ret);
            return;
        }
        sendMessage(tr("RETR %1\r\n").arg(ui->treeWidget->currentItem()->text(0)));
        ret = receiveMessage();
        if (ret.toInt() >= 300)
        {
            emit FtpError(ret);
            return;
        }
        acceptedFd = ::accept(tmpFd, (struct sockaddr *)&tmpClient, &t);
    }
    else
    {
        // Connect to server
        tmpProtocol = getprotobyname("TCP");
        tmpFd = socket(AF_INET, SOCK_STREAM, tmpProtocol->p_proto);
        int iSetOption = 1;
        setsockopt(tmpFd, SOL_SOCKET, SO_REUSEADDR, (char *)&iSetOption, sizeof(iSetOption));
        sendMessage("PASV\r\n");
        ret = receiveMessage();
        if (ret.left(3).toInt() >= 300)
        {
            emit FtpError(ret);
            ::close(tmpFd);
            return;
        }
        ret = ret.mid(ret.indexOf("(") + 1, ret.indexOf(")", ret.indexOf("(") + 1) - (ret.indexOf("(") + 1));
        for (int i = 0; i < 3; i++)
        {
            ret[ret.indexOf(",")] = '.';
        }
        tmpProtocol = getprotobyname("TCP");
        acceptedFd = socket(AF_INET, SOCK_STREAM, protocol->p_proto);
        tmpClient.sin_family = AF_INET;
        int port1 = ret.mid(ret.indexOf(",") + 1, ret.lastIndexOf(",") - ret.lastIndexOf(",") + 1).toInt(),
            port2 = ret.right(ret.length() - ret.lastIndexOf(",") + 1).toInt();
        tmpClient.sin_port = htons(port1 * 256 + port2);
        tmpClient.sin_addr.s_addr = inet_addr(ret.left(ret.indexOf(",")).toStdString().data());
        sendMessage(tr("RETR %1\r\n").arg(ui->treeWidget->currentItem()->text(0)));
        ret = receiveMessage();
        if (ret.toInt() >= 300)
        {
            emit FtpError(ret);
            return;
        }
        if (::connect(acceptedFd, (const struct sockaddr *)&tmpClient, sizeof(struct sockaddr)) == -1)
        {
            emit FtpError("Connect: " + QString(strerror(errno)));
            return;
        }
    }
    int size = ret.mid(ret.indexOf("(") + 1, ret.indexOf(" ", ret.indexOf("(")) - ret.indexOf("(")).toInt();
    int asize = 0;
    str = "";
    QFile f(fileName);

    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    ui->pushButton_2->hide();
    ui->pushButton_3->hide();
    ui->progressBar->show();
    while ((out = read(acceptedFd, buff, 3095)) > 0)
    {
        asize += out;
        str += buff;
        QTextStream(&f) << str;
        bzero(buff, 3096);
        str = "";
        ui->progressBar->setValue((asize * 100) / size);
    }
    ui->progressBar->hide();
    ui->pushButton_2->show();
    ui->pushButton_3->show();
    if (out == -1)
    {
        emit FtpError(strerror(errno));
        return;
    }
    ::close(acceptedFd);
    ::close(tmpFd);
    ret = receiveMessage();
    if (ret.left(3).toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
}

void FTP::upload(const QString &fileName)
{
    struct protoent *tmpProtocol;
    int tmpFd;
    struct sockaddr_in tmpClient;
    struct sockaddr_in sockinfo;
    int acceptedFd;
    QString ret, str;
    char buff[3096] = {0};
    socklen_t t;
    int port = 1024;
    int out;

    if (ui->mode->currentIndex() == 0)
    {
        // Create server
        tmpProtocol = getprotobyname("TCP");
        tmpFd = socket(AF_INET, SOCK_STREAM, tmpProtocol->p_proto);

        while (1)
        {
            sockinfo.sin_family = AF_INET;
            sockinfo.sin_port = htons(port);
            sockinfo.sin_addr.s_addr = INADDR_ANY;
            if (bind(tmpFd, (const struct sockaddr *)&sockinfo, sizeof(struct sockaddr)) != -1)
                break;
            port++;
        }
        listen(tmpFd, 42);
        int a = port / 256, b = port % 256;
        str.sprintf("PORT 127,0,0,1,%d,%d\r\n", a, b);
        sendMessage(str);
        ret = receiveMessage();
        if (ret.toInt() >= 300)
        {
            emit FtpError(ret);
            return;
        }
        acceptedFd = ::accept(tmpFd, (struct sockaddr *)&tmpClient, &t);
    }
    else
    {
        // Connect to server
        // Connect to server
        //QMessageBox::information(this, "", "totos");
        tmpProtocol = getprotobyname("TCP");
        tmpFd = socket(AF_INET, SOCK_STREAM, tmpProtocol->p_proto);
        int iSetOption = 1;
        setsockopt(tmpFd, SOL_SOCKET, SO_REUSEADDR, (char *)&iSetOption, sizeof(iSetOption));
        sendMessage("PASV\r\n");
        ret = receiveMessage();
        if (ret.left(3).toInt() >= 300)
        {
            emit FtpError(ret);
            ::close(tmpFd);
            return;
        }
        ret = ret.mid(ret.indexOf("(") + 1, ret.indexOf(")", ret.indexOf("(") + 1) - (ret.indexOf("(") + 1));
        for (int i = 0; i < 3; i++)
        {
            ret[ret.indexOf(",")] = '.';
        }
        tmpProtocol = getprotobyname("TCP");
        acceptedFd = socket(AF_INET, SOCK_STREAM, protocol->p_proto);
        tmpClient.sin_family = AF_INET;
        int port1 = ret.mid(ret.indexOf(",") + 1, ret.lastIndexOf(",") - ret.lastIndexOf(",") + 1).toInt(),
            port2 = ret.right(ret.length() - ret.lastIndexOf(",") + 1).toInt();
        tmpClient.sin_port = htons(port1 * 256 + port2);
        tmpClient.sin_addr.s_addr = inet_addr(ret.left(ret.indexOf(",")).toStdString().data());
        if (::connect(acceptedFd, (const struct sockaddr *)&tmpClient, sizeof(struct sockaddr)) == -1)
        {
            emit FtpError("Connect: " + QString(strerror(errno)));
            return;
        }
    }
    QMessageBox::information(this, "FileName", QFileInfo(fileName).fileName());
    sendMessage(tr("STOR %1\r\n").arg(QFileInfo(fileName).fileName()));
    ret = receiveMessage();
    QMessageBox::warning(this, "", ret);
    if (ret.toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
    int asize = 0;
    QFile f(fileName);

    f.open(QIODevice::ReadOnly);
    int size = f.size();

    ui->pushButton_2->hide();
    ui->pushButton_3->hide();
    ui->progressBar->show();
    while ((out = f.read(buff, 3095)) > 0)
    {
        QMessageBox::information(this, "file", buff);
        asize += out;
        write(acceptedFd, buff, out);
        bzero(buff, 3096);
        ui->progressBar->setValue((asize * 100) / size);
    }
    ui->progressBar->hide();
    ui->pushButton_2->show();
    ui->pushButton_3->show();
    if (out == -1)
    {
        emit FtpError(strerror(errno));
        return;
    }
    ::close(acceptedFd);
    ::close(tmpFd);
    ret = receiveMessage();
    if (ret.left(3).toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
}

void FTP::on_send(const QString &cmd)
{
    char buff[3096] = {0};
    QString s;
    QString data(cmd + "\r\n");

    if (!cmd.isEmpty())
    {
        if (::write(fd, data.toStdString().data(), data.length()) == -1)
        {
            ui->label->setText(strerror(errno));
            ui->label->show();
            return;
        }
        else
        {
            ui->label->hide();
        }
    }

    while (read(fd, buff, 3096) > 0)
    {
        s += buff;
        if (s.contains("\r\n"))
            break;
        bzero(buff, 3096);
    }
}

void FTP::on_Pwd_returnPressed()
{
    QString ret;

    sendMessage(tr("CWD %1\r\n").arg(ui->Pwd->text()));
    ret = receiveMessage();
    if (ret.toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
    refreshDirectory();
}

void FTP::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int)
{
    QString ret;

    sendMessage(tr("CWD %1\r\n").arg(item->text(0)));
    ret = receiveMessage();
    if (ret.left(3).toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
    refreshDirectory();
}

void FTP::on_pushButton_clicked()
{
    QString ret;

    sendMessage("CDUP\r\n");
    ret = receiveMessage();
    if (ret.mid(0, 3).toInt() >= 300)
    {
        emit FtpError(ret);
        return;
    }
    refreshDirectory();
}

void FTP::on_pushButton_2_clicked()
{
    if (ui->treeWidget->currentItem())
    {
        QString fileName = QFileDialog::getSaveFileName(this, tr("Download location"), ui->treeWidget->currentItem()->text(0), tr("All Files (*)"));

        if (fileName.isEmpty() == false)
        {
            download(fileName);
        }
    }
}

void FTP::on_treeWidget_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if (current != 0)
    {
        if (current->icon(0).isNull() == true)
        {
            ui->pushButton_2->setDisabled(false);
        }
        else
        {
            ui->pushButton_2->setDisabled(true);
        }
    }
}

void FTP::on_pushButton_3_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("File to Upload"), "", tr("All Files (*)"));

    if (fileName.isEmpty() == false)
    {
        QMessageBox::information(this, "", fileName);
        upload(fileName);
    }
}
