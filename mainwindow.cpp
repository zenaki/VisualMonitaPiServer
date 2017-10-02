#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Visual Monita - Monitoring Pi Server");

    manager = new QNetworkAccessManager(this);
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply *)));
    connect(manager, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(authenticationRequired(QNetworkReply*,QAuthenticator*)));
#ifndef QT_NO_SSL
    connect(manager, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
#endif

    path = this->readJSONFile("C:/Users/Administrator/Desktop/MonitaPiService/tag_list.json");
    id_sequence = 0;
    last_path = 0;

    QTimer *t = new QTimer(this);
    connect(t, SIGNAL(timeout()), this, SLOT(doWork()));
    t->start(60000);

    if (path.length() > 0) {
        last_path++;
        doWork();
    } else {
        qDebug() << "Path is empty !!!!";
        t->stop();
        this->close();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::doWork() {
    this->request("https://tmspremier/piwebapi/points?path="+path.at(last_path-1));
}

void MainWindow::request(QString urls)
{
    QNetworkRequest request;
    qDebug() << "url = " << urls;
    QUrl url =  QUrl::fromEncoded(urls.toLocal8Bit().data());

    qDebug() << "Request URL";
    request.setUrl(url);
    manager->get(request);
}

void MainWindow::parsing(QByteArray data)
{
    QJsonDocument JsonDoc = QJsonDocument::fromJson(data);
    QJsonObject object = JsonDoc.object();

    if (id_sequence == 1) {
        piServer.webID = object.value("WebId").toString();
        piServer.id = object.value("Id").toInt();
        piServer.tagName = object.value("Name").toString();
        piServer.pointType = object.value("PointType").toString();

        QJsonObject link = object.value("Links").toObject();
        if (!link.isEmpty()) {
//            this->readJSONFile("C:/Users/Administrator/Desktop/MonitaPiService/sample 2.json");
            this->request(link.value("Value").toString());
            return;
        }
    }

    if (id_sequence == 2) {
        piServer.val.value = object.value("Value").toDouble();
        QString time = object.value("Timestamp").toString();
        if (time.length() > 20) {
            time = time.mid(0, 19) + "Z";
        }
        piServer.val.time = QDateTime::fromString(time, "yyyy-MM-ddTHH:mm:ssZ");

        id_sequence = 0;
        writeToDB();
        if (path.length() > last_path) {
            last_path++;
            if (last_path == 5) {
                qDebug() << "debug";
            }
            doWork();
            return;
        } else {
            last_path = 1;
            readFromDB();
        }
    }

    qDebug() << "Finish";
}

void MainWindow::replyFinished(QNetworkReply *reply)
{
    qDebug() << "Get Reply";
    QByteArray data;
    data = reply->readAll();
    qDebug() << data;
    id_sequence++;
    this->parsing(data);
}

void MainWindow::authenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    qDebug() << "Authentication";
    authenticator->setUser("administrator");
    authenticator->setPassword("P@$$w0rd");
}

#ifndef QT_NO_SSL
void MainWindow::sslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    qDebug() << "SSL Error";
    reply->ignoreSslErrors();
}
#endif

QStringList MainWindow::readJSONFile(QString path) {
    QStringList result;
    QFile json_file(path);
    if (json_file.exists()) {
        if (json_file.open(QIODevice::ReadWrite)) {
            QByteArray readFile = json_file.readAll();
            QJsonDocument JsonDoc = QJsonDocument::fromJson(readFile);
            QJsonObject object = JsonDoc.object();

            QJsonArray array = object.value("path").toArray();
            foreach (const QJsonValue & v, array) {
                result.append(v.toObject().value("location").toString().replace(" ", "%20"));
            }
        }
    }
    return result;
}

void MainWindow::writeToDB() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("C:/Users/Administrator/Desktop/MonitaPiService/database.db");
    if(db.open()) {
        qDebug() << "DB Connected";
    } else {
        qDebug() << "DB Not Connected";
    }
    QSqlQuery query;
    QString str = "SELECT count(*) FROM list_titik_ukur WHERE "
            " tag = '" + piServer.tagName + "' and "
            " webid = '" + piServer.webID + "'";
    if (query.exec(str)) {
        while (query.next()) {
            if (query.value(0).toInt() == 0) {
                str = "INSERT INTO list_titik_ukur (id_tu, tag, webid) values (" +
                        QString::number(piServer.id) + ", '" +
                        piServer.tagName + "', '" +
                        piServer.webID + "')";
                if (query.exec(str)) {
                    qDebug() << "Berhasil Insert Data Baru di table list_titik_ukur";
                } else {
                    qDebug() << "Gagal Insert Data Baru di table list_titik_ukur";
                    qDebug() << query.lastError().text();
                }
            }
        }
    }

    str = "SELECT count(*) FROM data_history WHERE "
            " id_tu = " + QString::number(piServer.id) + " and "
            " timestamp = '" + QString::number(piServer.val.time.toUTC().toMSecsSinceEpoch()) + "'";
    if (query.exec(str)) {
        while (query.next()) {
            if (query.value(0).toInt() == 0) {
                str = "INSERT INTO data_history (id_tu, timestamp, value) values (" +
                        QString::number(piServer.id) + ", " +
                        QString::number(piServer.val.time.toUTC().toMSecsSinceEpoch()) + ", " +
                        QString::number(piServer.val.value) + ")";
                if (query.exec(str)) {
                    qDebug() << "Berhasil Insert Data Baru di table data_history";
                } else {
                    qDebug() << "Gagal Insert Data Baru di table data_history";
                    qDebug() << query.lastError().text();
                }
            }
        }
    }
    db.close();
}

void MainWindow::readFromDB() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("C:/Users/Administrator/Desktop/MonitaPiService/database.db");
    if(db.open()) {
        qDebug() << "DB Connected";
    } else {
        qDebug() << "DB Not Connected";
    }
    QSqlQuery query;
    QString str = "SELECT "
            "a.tag, "
            "b.timestamp, "
            "b.value "
        "FROM "
            "list_titik_ukur a, "
            "data_history b "
        "WHERE "
            "a.id_tu = b.id_tu "
        "ORDER BY b.timestamp desc";
//            "b.timestamp = (SELECT max(timestamp) FROM data_history WHERE id_tu = b.id_tu)";
    if (query.exec(str)) {
        qDebug() << "Berhasil Dapat Data Monitoring";
        QStringList result;
        while (query.next()) {
            result.append(query.value(0).toString()+";"+query.value(1).toString()+";"+query.value(2).toString());
        }

        QStandardItemModel *model_data = new QStandardItemModel(result.length(), 3, this);
        model_data->setHorizontalHeaderItem(0, new QStandardItem(QString("Tag Name")));
        model_data->setHorizontalHeaderItem(1, new QStandardItem(QString("Time")));
        model_data->setHorizontalHeaderItem(2, new QStandardItem(QString("Value")));

        ui->tableView->setModel(model_data);

        for (int i = 0; i < result.length(); i++) {
            QStringList listData = result.at(i).split(";");

            model_data->setItem(i, 0, new QStandardItem(listData.at(0)));
            model_data->setItem(i, 1, new QStandardItem(QDateTime::fromTime_t(listData.at(1).mid(0,10).toInt()).toString("yyyy-MM-dd HH:mm:ss")));
            model_data->setItem(i, 2, new QStandardItem(listData.at(2)));
        }
    } else {
        qDebug() << "Database Error : " << query.lastError();
    }
    db.close();
}
