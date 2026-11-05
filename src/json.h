#ifndef JSON_H
#define JSON_H

#include <QByteArray>
#include <QVariant>

// Minimal JSON helpers for Qt 4 (no QJsonDocument).
namespace Json {

// Parses a JSON object. Returns an empty map and sets *ok=false on failure,
// which includes nesting deeper than 32 levels.
QVariantMap parseObject(const QByteArray &data, bool *ok = 0);

// Serializes maps, lists, strings, numbers, bools and null.
QByteArray serialize(const QVariant &value);

}

#endif // JSON_H
