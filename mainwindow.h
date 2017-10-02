#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <QAuthenticator>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include <QDateTime>
#include <QTimeZone>
#include <QFile>

#include <QTimer>
#include <QStandardItemModel>

struct piDataValue {
    QDateTime time;
    double value;
};

struct piData {
    int id;
    QString tagName;
    QString pointType;
    QString webID;

    struct piDataValue val;
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void request(QString urls);
    void parsing(QByteArray data);
    QStringList readJSONFile(QString path);

    QNetworkAccessManager *manager;

    struct piData piServer;
    QStringList path;

    int id_sequence;
    int last_path;

    void writeToDB();
    void readFromDB();

private:
    Ui::MainWindow *ui;

public slots:
    void doWork();
    void replyFinished(QNetworkReply *reply);
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
};

#endif // MAINWINDOW_H
