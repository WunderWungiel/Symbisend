// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "i18n.h"

#include "json.h"
#include "settings.h"

#include <QDebug>
#include <QFile>
#include <QLocale>
#include <QRegExp>
#include <QTime>

I18n *I18n::s_instance = 0;

static const char *kOwnPrefix = "symbisend.";
static const int kMaxLinkDepth = 8;

namespace {

struct Language {
    QString code;
    QString name;
};

// i18n/symbisend/languages.json: [[code, name], ...] in the official app's
// order (getLocaleName() in app/lib/util/i18n.dart). English first.
const QList<Language> &languages()
{
    static QList<Language> s_list;
    if (!s_list.isEmpty())
        return s_list;
    QFile f(QLatin1String(":/i18n/symbisend/languages.json"));
    if (f.open(QIODevice::ReadOnly)) {
        bool ok = false;
        const QVariantMap root = Json::parseObject(f.readAll(), &ok);
        const QVariantList list = root.value(QLatin1String("languages")).toList();
        for (int i = 0; i < list.size(); ++i) {
            const QVariantList pair = list.at(i).toList();
            if (pair.size() != 2)
                continue;
            Language l;
            l.code = pair.at(0).toString();
            l.name = pair.at(1).toString();
            s_list.append(l);
        }
        if (!ok)
            qWarning() << "I18n: languages.json is invalid";
        qDebug() << "I18n:" << s_list.size() << "languages";
    } else {
        qWarning() << "I18n: languages.json is missing";
    }
    if (s_list.isEmpty()) {
        Language en;
        en.code = QLatin1String("en");
        en.name = QLatin1String("English");
        s_list.append(en);
    }
    return s_list;
}

bool isLanguage(const QString &code)
{
    const QList<Language> &list = languages();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).code == code)
            return true;
    }
    return false;
}

}

I18n::I18n(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    s_instance = this;
    onLanguageSetting();
    connect(settings, SIGNAL(languageChanged()), this, SLOT(onLanguageSetting()));
}

I18n::~I18n()
{
    if (s_instance == this)
        s_instance = 0;
}

QString I18n::text(const char *key, const QVariantMap &args)
{
    if (!s_instance)
        return QLatin1String(key);
    return s_instance->t(QLatin1String(key), args);
}

QString I18n::pluralText(const char *key, int n, const QVariantMap &args)
{
    if (!s_instance)
        return QLatin1String(key);
    return s_instance->plural(QLatin1String(key), n, args);
}

QVariantMap I18n::args(const char *k1, const QVariant &v1, const char *k2, const QVariant &v2,
                       const char *k3, const QVariant &v3)
{
    QVariantMap map;
    map.insert(QLatin1String(k1), v1);
    if (k2)
        map.insert(QLatin1String(k2), v2);
    if (k3)
        map.insert(QLatin1String(k3), v3);
    return map;
}

QStringList I18n::languageCodes() const
{
    QStringList codes;
    codes << QLatin1String("system");
    const QList<Language> &list = languages();
    for (int i = 0; i < list.size(); ++i)
        codes << list.at(i).code;
    return codes;
}

QStringList I18n::languageNames() const
{
    QStringList names;
    names << languageName(QLatin1String("system"));
    const QList<Language> &list = languages();
    for (int i = 0; i < list.size(); ++i)
        names << list.at(i).name;
    return names;
}

QString I18n::languageName(const QString &code) const
{
    if (code == QLatin1String("system"))
        return t(QLatin1String("settingsTab.general.languageOptions.system"));
    const QList<Language> &list = languages();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).code == code)
            return list.at(i).name;
    }
    return code;
}

// The phone's language as one of ours: "pt_BR" -> "pt-BR", else the bare
// language ("de_AT" -> "de"), else its first regional variant ("es" ->
// "es-ES"), else English.
QString I18n::resolveSystemLanguage()
{
    const QString name = QLocale::system().name(); // "pt_BR"
    const QString full = QString(name).replace(QLatin1Char('_'), QLatin1Char('-'));
    const QString lang = name.section(QLatin1Char('_'), 0, 0);
    if (isLanguage(full))
        return full;
    if (isLanguage(lang))
        return lang;
    const QList<Language> &list = languages();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).code.startsWith(lang + QLatin1Char('-')))
            return list.at(i).code;
    }
    return QLatin1String("en");
}

void I18n::onLanguageSetting()
{
    QString wanted = m_settings->language();
    QString code = wanted;
    if (code != QLatin1String("system") && !isLanguage(code)) {
        qWarning() << "I18n: unknown language" << code << "- following the system";
        code = QLatin1String("system");
    }
    if (code == QLatin1String("system")) {
        code = resolveSystemLanguage();
        qDebug() << "I18n: system locale" << QLocale::system().name() << "->" << code;
    }
    if (code == m_language && !m_strings.isEmpty())
        return;

    QTime timer;
    timer.start();
    m_language = code;
    m_strings.clear();
    m_english.clear();
    m_missingLogged.clear();
    load(code, &m_strings);
    if (code != QLatin1String("en"))
        load(QLatin1String("en"), &m_english);
    qDebug() << "I18n: language" << code << "(setting" << wanted << ")," << m_strings.size()
             << "strings, loaded in" << timer.elapsed() << "ms";
    emit changed();
}

// Upstream's file and ours (under "symbisend."), flattened.
void I18n::load(const QString &code, Table *table)
{
    const QString files[2] = {
        QString::fromLatin1(":/i18n/%1.json").arg(code),
        QString::fromLatin1(":/i18n/symbisend/%1.json").arg(code)
    };
    for (int i = 0; i < 2; ++i) {
        QFile f(files[i]);
        if (!f.open(QIODevice::ReadOnly)) {
            if (i == 0)
                qWarning() << "I18n: missing" << files[i];
            continue;
        }
        bool ok = false;
        const QVariantMap root = Json::parseObject(f.readAll(), &ok);
        if (!ok) {
            qWarning() << "I18n: invalid JSON in" << files[i];
            continue;
        }
        flatten(root, i == 0 ? QString() : QLatin1String(kOwnPrefix), table);
    }
}

// {"a": {"b(plural)": {"one": "x"}}} -> "a.b.one" = "x". Lists become
// paragraphs; "@..." keys are upstream's comments for translators.
void I18n::flatten(const QVariant &value, const QString &prefix, Table *table)
{
    if (value.type() == QVariant::Map) {
        const QVariantMap map = value.toMap();
        for (QVariantMap::const_iterator it = map.constBegin(); it != map.constEnd(); ++it) {
            QString key = it.key();
            if (key.startsWith(QLatin1Char('@')))
                continue;
            const int paren = key.indexOf(QLatin1Char('('));
            if (paren >= 0)
                key.truncate(paren);
            flatten(it.value(), prefix + key + QLatin1Char('.'), table);
        }
        return;
    }
    const QString key = prefix.left(prefix.size() - 1);
    if (value.type() == QVariant::List || value.type() == QVariant::StringList)
        table->insert(key, value.toStringList().join(QLatin1String("\n\n")));
    else
        table->insert(key, value.toString());
}

QString I18n::lookupIn(const Table &table, const QString &key, int depth) const
{
    Table::const_iterator it = table.constFind(key);
    if (it == table.constEnd())
        return QString();
    return resolveLinks(it.value(), key, depth);
}

QString I18n::lookup(const QString &key, int depth) const
{
    QString value = lookupIn(m_strings, key, depth);
    if (value.isNull() && !m_english.isEmpty())
        value = lookupIn(m_english, key, depth);
    return value;
}

// "@:general.off" -> that string; "@:.x" is a sibling of key. A link to a
// plural takes its "other" form.
QString I18n::resolveLinks(const QString &value, const QString &key, int depth) const
{
    if (!value.contains(QLatin1String("@:")))
        return value;
    if (depth >= kMaxLinkDepth) {
        qWarning() << "I18n: links nested too deeply at" << key;
        return value;
    }
    static const QRegExp link(QLatin1String("@:(\\.?[A-Za-z0-9_]+(?:\\.[A-Za-z0-9_]+)*)"));
    QRegExp rx(link);
    QString out;
    int pos = 0;
    int at;
    while ((at = rx.indexIn(value, pos)) >= 0) {
        out += value.mid(pos, at - pos);
        QString target = rx.cap(1);
        if (target.startsWith(QLatin1Char('.')))
            target = key.section(QLatin1Char('.'), 0, -2) + target;
        QString resolved = lookup(target, depth + 1);
        if (resolved.isNull())
            resolved = lookup(target + QLatin1String(".other"), depth + 1);
        out += resolved.isNull() ? rx.cap(0) : resolved;
        pos = at + rx.matchedLength();
    }
    out += value.mid(pos);
    return out;
}

QString I18n::fill(QString text, const QVariantMap &args)
{
    for (QVariantMap::const_iterator it = args.constBegin(); it != args.constEnd(); ++it)
        text.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value().toString());
    return text;
}

QString I18n::t(const QString &key, const QVariant &args) const
{
    QString value = lookup(key);
    if (value.isNull()) {
        if (!m_missingLogged.contains(key)) {
            m_missingLogged.insert(key, true);
            qWarning() << "I18n: missing string" << key;
        }
        return key;
    }
    return fill(value, args.toMap());
}

// The official app's rule (initI18n() in app/lib/util/i18n.dart), the same
// for every language: 0 zero, 1 one, 2 two (Hebrew only), else other; a
// missing form falls back to other.
QString I18n::pluralForm(int n) const
{
    if (n == 0)
        return QLatin1String("zero");
    if (n == 1)
        return QLatin1String("one");
    if (n == 2 && m_language == QLatin1String("he"))
        return QLatin1String("two");
    return QLatin1String("other");
}

QString I18n::plural(const QString &key, int n, const QVariant &args) const
{
    QVariantMap map = args.toMap();
    if (!map.contains(QLatin1String("n")))
        map.insert(QLatin1String("n"), n);
    const QString form = key + QLatin1Char('.') + pluralForm(n);
    const QString other = key + QLatin1String(".other");
    // The language's own forms first, then English's.
    QString value = lookupIn(m_strings, form, 0);
    if (value.isNull())
        value = lookupIn(m_strings, other, 0);
    if (value.isNull() && !m_english.isEmpty()) {
        value = lookupIn(m_english, key + (n == 1 ? QLatin1String(".one") : QLatin1String(".other")), 0);
        if (value.isNull())
            value = lookupIn(m_english, other, 0);
    }
    if (value.isNull())
        return t(other, map); // logs it
    return fill(value, map);
}

// WebI18n as the official app fills it (server_provider.dart).
QByteArray I18n::webStrings() const
{
    QVariantMap m;
    m.insert(QLatin1String("waiting"), t(QLatin1String("web.waiting")));
    m.insert(QLatin1String("enterPin"), t(QLatin1String("web.enterPin")));
    m.insert(QLatin1String("invalidPin"), t(QLatin1String("web.invalidPin")));
    m.insert(QLatin1String("tooManyAttempts"), t(QLatin1String("web.tooManyAttempts")));
    m.insert(QLatin1String("rejected"), t(QLatin1String("web.rejected")));
    m.insert(QLatin1String("uploadRejected"), t(QLatin1String("sendPage.rejected")));
    m.insert(QLatin1String("busy"), t(QLatin1String("sendPage.busy")));
    m.insert(QLatin1String("files"), t(QLatin1String("web.files")));
    m.insert(QLatin1String("fileName"), t(QLatin1String("web.fileName")));
    m.insert(QLatin1String("size"), t(QLatin1String("web.size")));
    m.insert(QLatin1String("dropHint"), t(QLatin1String("sendTab.placeItems")));
    return Json::serialize(m);
}
