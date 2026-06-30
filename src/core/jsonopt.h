#ifndef JSONOPT_H
#define JSONOPT_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QFile>

class JsonOpt {
public:
    static QJsonDocument File2Json(const QString &path);
    static bool Json2File(const QString &path, const QJsonDocument &doc);
};

#endif // JSONOPT_H
