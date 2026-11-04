#ifndef RECEIVER_H
#define RECEIVER_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include "httpserver.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QSslError;
class FavoritesModel;
class HistoryModel;
class NetworkManager;
class Settings;
class WebShare;

// Receives files from other devices (LocalSend v2 upload API, our side):
//  1. POST /prepare-upload: the sender's info and file list. Answered once
//     our user accepts (a session id and a token per file), declines (403),
//     or right away: PIN missing/wrong (401), busy (409), quick save.
//     A lone text message is answered with 204 after accepting.
//  2. POST /upload?sessionId&fileId&token for each file, streamed into the
//     download folder. A file that fails is final: the others go on and the
//     session ends in "finishedWithErrors", as in the official app.
//  3. POST /cancel?sessionId when the sender gives up. It works both ways:
//     when our user cancels we POST /cancel to the sender, pinned to its
//     fingerprint. A /cancel that isn't about our session may be for a
//     transfer we are sending: cancelReceived().
// One session at a time. While "Receive via link" is on (WebShare), browsers
// send through the same endpoints; the link's PIN replaces the settings PIN
// and settings.receiveViaLinkAutoAccept works like quick save, as in the
// official app. See docs/qml-api.md.
class Receiver : public QObject, public HttpHandler
{
    Q_OBJECT
    // "idle", "request" (an offer waits for accept()/decline()), "receiving",
    // "finished", "finishedWithErrors" (every file arrived or failed, some
    // failed: errorText lists them), "failed", "cancelled" (by the sender or by us)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    // request or receiving
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

    // The sending device, from its prepare-upload request.
    Q_PROPERTY(QString peerAlias READ peerAlias NOTIFY stateChanged)
    Q_PROPERTY(QString peerDeviceModel READ peerDeviceModel NOTIFY stateChanged)
    Q_PROPERTY(QString peerDeviceType READ peerDeviceType NOTIFY stateChanged)
    Q_PROPERTY(QString peerFingerprint READ peerFingerprint NOTIFY stateChanged)
    Q_PROPERTY(QString peerIp READ peerIp NOTIFY stateChanged)
    Q_PROPERTY(QString peerVersion READ peerVersion NOTIFY stateChanged)
    // The sender announced https (its fingerprint is then its certificate's hash).
    Q_PROPERTY(bool peerHttps READ peerHttps NOTIFY stateChanged)

    // The offer.
    Q_PROPERTY(int fileCount READ fileCount NOTIFY stateChanged)
    Q_PROPERTY(qreal totalSize READ totalSize NOTIFY stateChanged)
    // [{ fileName, size, fileType }], in the sender's order
    Q_PROPERTY(QVariantList files READ files NOTIFY stateChanged)
    // A lone text message (shown, not saved): isMessage, and its text.
    Q_PROPERTY(bool isMessage READ isMessage NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)

    // Receiving.
    // Where the files go: settings.downloadFolder, or the gallery folder if
    // every file saved so far went there. folders: every folder used (the
    // download folder and/or settings' media folders), final once finished.
    Q_PROPERTY(QString folder READ folder NOTIFY stateChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY stateChanged)
    Q_PROPERTY(int filesDone READ filesDone NOTIFY progressChanged)
    Q_PROPERTY(int filesFailed READ filesFailed NOTIFY progressChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY progressChanged)
    Q_PROPERTY(qreal bytesReceived READ bytesReceived NOTIFY progressChanged)
    Q_PROPERTY(qreal bytesTotal READ totalSize NOTIFY progressChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY progressChanged)

public:
    // history (may be 0) gets every saved file and accepted message;
    // favorites (may be 0) is for settings.quickSaveFromFavorites.
    Receiver(HttpServer *server, Settings *settings, NetworkManager *network,
             WebShare *webShare, HistoryModel *history, FavoritesModel *favorites,
             QObject *parent = 0);
    ~Receiver();

    QString state() const { return m_state; }
    bool isBusy() const;
    QString errorText() const { return m_errorText; }
    QString peerAlias() const { return m_peerAlias; }
    QString peerDeviceModel() const { return m_peerDeviceModel; }
    QString peerDeviceType() const { return m_peerDeviceType; }
    QString peerFingerprint() const { return m_peerFingerprint; }
    QString peerIp() const { return m_peerIp.toString(); }
    QString peerVersion() const { return m_peerVersion; }
    bool peerHttps() const { return m_peerHttps; }
    int fileCount() const { return m_files.size(); }
    qreal totalSize() const { return qreal(m_totalSize); }
    QVariantList files() const;
    bool isMessage() const { return m_isMessage; }
    QString message() const;
    QString folder() const;
    QStringList folders() const;
    int filesDone() const;
    int filesFailed() const;
    QString fileName() const { return m_currentName; }
    qreal bytesReceived() const;
    qreal progress() const;
    int remainingSeconds() const { return m_remainingSeconds; }

    // State "request": take everything offered / refuse it.
    Q_INVOKABLE void accept();
    Q_INVOKABLE void decline();
    // Stops receiving (files not complete yet are deleted).
    Q_INVOKABLE void cancel();

    // HttpHandler
    bool handles(const HttpRequest &request) const;
    bool streamsBody(const HttpRequest &request) const;
    HttpResponse bodyData(const HttpRequest &request, const QByteArray &data);
    HttpResponse handleRequest(const HttpRequest &request);
    void requestAborted(const HttpRequest &request);

signals:
    void stateChanged();
    void progressChanged();
    // A /cancel for a session that isn't ours to receive: the device we are
    // sending to may be cancelling (see Sender::onCancelReceived()).
    void cancelReceived(const QString &ip, const QString &sessionId);

private slots:
    void onTick();
    void onOnlineChanged();
    void onCancelSent();
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void dropDraining();

private:
    enum FileStatus { Pending, Receiving, Done, Failed };
    struct InFile {
        QString id;
        QString fileName;   // as sent; may contain folders ("Trip/a.jpg")
        QString fileType;
        QString preview;
        qint64 size;
        QString token;
        FileStatus status;
        qint64 received;
        QString savedPath;  // while receiving and once done
        QString error;      // why it failed
    };
    struct Upload {
        int file;           // index into m_files
        QFile *out;
    };

    HttpResponse prepareUpload(const HttpRequest &request);
    HttpResponse cancelRequest(const HttpRequest &request);
    HttpResponse startUpload(const HttpRequest &request);
    HttpResponse writeUpload(const HttpRequest &request, const QByteArray &data);
    HttpResponse finishUpload(const HttpRequest &request);
    HttpResponse startSession();
    void closeUpload(quint32 requestId, bool keepFile, const QString &error = QString());
    void dropUploads();
    bool notifySender();
    void checkDone();
    void finish(const QString &state, const QString &error = QString());
    void setState(const QString &state, const QString &error = QString());
    void reset();
    static int mediaKind(const InFile &f);
    static QString safeRelativePath(const QString &fileName);
    static QString uniquePath(const QString &path);
    static QString randomToken();

    HttpServer *m_server;
    Settings *m_settings;
    NetworkManager *m_network;
    WebShare *m_webShare;
    HistoryModel *m_history;
    FavoritesModel *m_favorites;
    QNetworkAccessManager *m_nam;     // only for the /cancel to the sender

    QString m_state;
    QString m_errorText;
    QString m_peerAlias;
    QString m_peerDeviceModel;
    QString m_peerDeviceType;
    QString m_peerFingerprint;
    QString m_peerVersion;
    bool m_peerHttps;
    int m_peerPort;                   // of its server; 0 for browsers, which have none
    QHostAddress m_peerIp;
    QList<InFile> m_files;
    QHash<QString, int> m_fileIndex;  // file id -> index into m_files
    qint64 m_totalSize;
    bool m_isMessage;
    quint32 m_pendingRequest;         // the prepare-upload waiting for the user, 0 if none
    QString m_sessionId;
    QString m_folder;                 // Qt separators, set when accepted
    bool m_toGallery;                 // images/videos go to settings' media folders
    QStringList m_usedFolders;        // base folders files were saved to, Qt separators
    QHash<quint32, Upload> m_uploads; // running uploads by request id
    // Uploads we cancelled: their data is thrown away and their connections
    // stay open until the sender has our /cancel (see cancel()).
    QList<quint32> m_draining;
    QTimer m_drainTimer;
    QString m_currentName;
    QDateTime m_receivingSince;
    QDateTime m_lastActivity;
    int m_remainingSeconds;
    QHash<QString, int> m_wrongPins;  // sender IP -> wrong PINs in a row
    QTimer m_tick;
};

#endif // RECEIVER_H
