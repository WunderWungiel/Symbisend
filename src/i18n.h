// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef I18N_H
#define I18N_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariant>

class Settings;

// The UI's strings, from the official LocalSend translations (i18n/*.json,
// copied unchanged from upstream, see i18n/README.md) plus our own
// (i18n/symbisend/*.json, looked up under the "symbisend." prefix).
//
// Only the language in use and English are loaded. A key missing in the
// language falls back to English, then to the key itself (logged once).
// Values follow upstream's slang format as far as these files use it:
// nested keys, key modifiers like "minutesUnit(plural, param=m)" (dropped),
// links "@:other.key" (also relative "@:.sibling"), "{name}" placeholders,
// plurals (zero/one/two/few/many/other, chosen like the official app) and
// lists (joined into paragraphs).
//
// settings.language is "system" (the phone's language, falling back to
// English) or one of languageCodes. A change applies at once: `rev` changes,
// so QML bindings written as `i18n.t("key") + i18n.rev` update.
class I18n : public QObject
{
    Q_OBJECT
    // Always "", but NOTIFY on every language change: append it to bindings.
    Q_PROPERTY(QString rev READ rev NOTIFY changed)
    // The language in use, e.g. "pl" or "pt-BR" (resolved from "system").
    Q_PROPERTY(QString language READ language NOTIFY changed)
    // For the Language setting: "system" first, then every language, with
    // the names the official app shows (each in its own language; "System"
    // translated).
    Q_PROPERTY(QStringList languageCodes READ languageCodes CONSTANT)
    Q_PROPERTY(QStringList languageNames READ languageNames NOTIFY changed)

public:
    explicit I18n(Settings *settings, QObject *parent = 0);
    ~I18n();

    // The instance, for C++ code; 0 before main() created it.
    static I18n *instance() { return s_instance; }
    // instance()->t() / plural(), or the key if there is no instance:
    // I18n::text("symbisend.errors.declined", I18n::args("name", alias)).
    static QString text(const char *key, const QVariantMap &args = QVariantMap());
    static QString pluralText(const char *key, int n, const QVariantMap &args = QVariantMap());
    static QVariantMap args(const char *k1, const QVariant &v1,
                            const char *k2 = 0, const QVariant &v2 = QVariant(),
                            const char *k3 = 0, const QVariant &v3 = QVariant());

    QString rev() const { return QString(); }
    QString language() const { return m_language; }
    QStringList languageCodes() const;
    QStringList languageNames() const;

    // The string for key, with "{name}" replaced by args["name"]. args is a
    // JS object from QML.
    Q_INVOKABLE QString t(const QString &key, const QVariant &args = QVariant()) const;
    // The plural form of key for n ({n} is n unless args says otherwise).
    Q_INVOKABLE QString plural(const QString &key, int n, const QVariant &args = QVariant()) const;
    // A language's name as the Language setting shows it ("system" too).
    Q_INVOKABLE QString languageName(const QString &code) const;

    // The JSON object the "Receive via link" page reads from /i18n.json,
    // filled like the official server does.
    QByteArray webStrings() const;

signals:
    void changed();

private slots:
    void onLanguageSetting();

private:
    typedef QHash<QString, QString> Table;

    static QString resolveSystemLanguage();
    static void load(const QString &code, Table *table);
    static void flatten(const QVariant &value, const QString &prefix, Table *table);
    // The raw value (links resolved) or a null string.
    QString lookup(const QString &key, int depth = 0) const;
    QString lookupIn(const Table &table, const QString &key, int depth) const;
    QString resolveLinks(const QString &value, const QString &key, int depth) const;
    static QString fill(QString text, const QVariantMap &args);
    QString pluralForm(int n) const;

    static I18n *s_instance;
    Settings *m_settings;
    QString m_language;
    Table m_strings;   // the language in use
    Table m_english;   // fallback (empty while the language is English)
    mutable QHash<QString, bool> m_missingLogged;
};

#endif // I18N_H
