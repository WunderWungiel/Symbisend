#include "json.h"

#include <QScriptEngine>
#include <QScriptValue>
#include <QStringList>

namespace Json {

static QScriptEngine *engine()
{
    static QScriptEngine *s_engine = 0;
    if (!s_engine)
        s_engine = new QScriptEngine;
    return s_engine;
}

QVariantMap parseObject(const QByteArray &data, bool *ok)
{
    if (ok)
        *ok = false;
    QScriptEngine *e = engine();
    QScriptValue parse = e->globalObject().property(QLatin1String("JSON"))
            .property(QLatin1String("parse"));
    QScriptValue result = parse.call(QScriptValue(),
                                     QScriptValueList() << QScriptValue(QString::fromUtf8(data)));
    if (e->hasUncaughtException()) {
        e->clearExceptions();
        return QVariantMap();
    }
    if (!result.isObject() || result.isArray())
        return QVariantMap();
    if (ok)
        *ok = true;
    return result.toVariant().toMap();
}

static void escapeString(const QString &s, QByteArray &out)
{
    out.append('"');
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        const ushort u = c.unicode();
        switch (u) {
        case '"':  out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\b': out.append("\\b"); break;
        case '\f': out.append("\\f"); break;
        case '\n': out.append("\\n"); break;
        case '\r': out.append("\\r"); break;
        case '\t': out.append("\\t"); break;
        default:
            if (u < 0x20) {
                out.append(QString::fromLatin1("\\u%1").arg(u, 4, 16, QLatin1Char('0')).toLatin1());
            } else {
                out.append(QString(c).toUtf8());
            }
        }
    }
    out.append('"');
}

static void serializeInto(const QVariant &v, QByteArray &out)
{
    switch (v.type()) {
    case QVariant::Invalid:
        out.append("null");
        break;
    case QVariant::Bool:
        out.append(v.toBool() ? "true" : "false");
        break;
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::LongLong:
    case QVariant::ULongLong:
        out.append(QByteArray::number(v.toLongLong()));
        break;
    case QVariant::Double:
        out.append(QByteArray::number(v.toDouble(), 'g', 15));
        break;
    case QVariant::Map: {
        out.append('{');
        const QVariantMap map = v.toMap();
        bool first = true;
        for (QVariantMap::const_iterator it = map.constBegin(); it != map.constEnd(); ++it) {
            if (!first)
                out.append(',');
            first = false;
            escapeString(it.key(), out);
            out.append(':');
            serializeInto(it.value(), out);
        }
        out.append('}');
        break;
    }
    case QVariant::List:
    case QVariant::StringList: {
        out.append('[');
        const QVariantList list = v.toList();
        for (int i = 0; i < list.size(); ++i) {
            if (i)
                out.append(',');
            serializeInto(list.at(i), out);
        }
        out.append(']');
        break;
    }
    default:
        escapeString(v.toString(), out);
    }
}

QByteArray serialize(const QVariant &value)
{
    QByteArray out;
    serializeInto(value, out);
    return out;
}

}
