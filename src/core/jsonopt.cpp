#include "jsonopt.h"
#include <QDebug>

QJsonDocument JsonOpt::File2Json(const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qDebug() << "can't open error!";
        return QJsonDocument();
    }

    QTextStream stream(&file);
    QString str = stream.readAll();
    file.close();

    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError && !doc.isNull()) {
        qDebug() << "Json format error!" << jsonError.error;
        return QJsonDocument();
    }
    return doc;
}

bool JsonOpt::Json2File(const QString &path, const QJsonDocument &doc)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "can't open error!";
        return false;
    }

    QTextStream stream(&file);
    stream << doc.toJson();
    file.close();
    return true;
}
