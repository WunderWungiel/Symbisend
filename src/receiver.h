// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

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
class Sha256Hash;
class FavoritesModel;
class HistoryModel;
class NetworkManager;
class ReceiveFilesModel;
class Settings;
class WebShare;

// Receives files from other devices (LocalSend v2 upload API, our side):
//  1. POST /prepare-upload: the sender's info and file list. Answered once
//     our user accepts (a session id and a token per file), declines (403),
//     or right away: PIN missing/wrong (401), too many wrong PINs (429),
//     busy (409), quick save.
//     A lone text message goes to the history at once and is answered with
//     204 when the user closes it. It is never saved as a file.
//     Before accepting, the user may skip or rename files (offeredFiles()) and
//     change the folder and the gallery option for this session. Only the
//     files kept get a token, so the sender leaves out the others.
//  2. POST /upload?sessionId&fileId&token for each file, streamed into the
//     download folder. A file with a sha256 in the offer is checked while it
//     streams in: on a mismatch it is deleted and answered with 422, and the
//     sender may upload it again, 3 uploads in all. Otherwise a file that
//     fails is final: the others go on and the session ends in
//     "finishedWithErrors", as in the official app. A saved file gets the
//     modified and accessed times of the offer (only modified on Symbian).
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
    // State "cancelled": "sender" or "receiver" (we did); empty otherwise.
    Q_PROPERTY(QString cancelledBy READ cancelledBy NOTIFY stateChanged)

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
    // A lone text message (shown, not saved): isMessage, its text, and
    // whether that text is a link.
    Q_PROPERTY(bool isMessage READ isMessage NOTIFY stateChanged)
    Q_PROPERTY(QString message READ message NOTIFY stateChanged)
    Q_PROPERTY(bool messageIsLink READ messageIsLink NOTIFY stateChanged)

    // This session's options, from settings when the offer arrives; state
    // "request" can change them. hasFolders: the files come with folders, which
    // turns saveToGallery off at first, as in the official app.
    Q_PROPERTY(bool saveToGallery READ saveToGallery WRITE setSaveToGallery NOTIFY optionsChanged)
    Q_PROPERTY(bool hasFolders READ hasFolders NOTIFY stateChanged)

    // Receiving.
    // Where the files go: this session's folder, or the gallery folder if
    // every file saved so far went there. folders: every folder used (the
    // session's folder and/or settings' media folders), final once finished.
    Q_PROPERTY(QString folder READ folder NOTIFY optionsChanged)
    Q_PROPERTY(QStringList folders READ folders NOTIFY stateChanged)
    Q_PROPERTY(int filesDone READ filesDone NOTIFY progressChanged)
    Q_PROPERTY(int filesFailed READ filesFailed NOTIFY progressChanged)
    // Quick save got every file: state "finished", and the session closes by
    // itself and offers to open lastFilePath, as in the official app.
    Q_PROPERTY(bool quickSaved READ quickSaved NOTIFY stateChanged)
    // The file saved last, Qt separators; empty before the first one.
    Q_PROPERTY(QString lastFilePath READ lastFilePath NOTIFY stateChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY progressChanged)
    Q_PROPERTY(qreal bytesReceived READ bytesReceived NOTIFY progressChanged)
    Q_PROPERTY(qreal bytesTotal READ totalSize NOTIFY progressChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY progressChanged)

public:
    // history (may be 0) gets every saved file and incoming message;
    // favorites (may be 0) is for settings.quickSaveFromFavorites.
    Receiver(HttpServer *server, Settings *settings, NetworkManager *network,
             WebShare *webShare, HistoryModel *history, FavoritesModel *favorites,
             QObject *parent = 0);
    ~Receiver();

    QString state() const { return m_state; }
    bool isBusy() const;
    QString errorText() const { return m_errorText; }
    QString cancelledBy() const { return m_cancelledBy; }
    QString peerAlias() const { return m_peerAlias; }
    QString peerDeviceModel() const { return m_peerDeviceModel; }
    QString peerDeviceType() const { return m_peerDeviceType; }
    QString peerFingerprint() const { return m_peerFingerprint; }
    QString peerIp() const { return m_peerIp.toString(); }
    QString peerVersion() const { return m_peerVersion; }
    bool peerHttps() const { return m_peerHttps; }
    int fileCount() const { return m_files.size(); }
    qreal totalSize() const { return qreal(m_totalSize); }
    // For the Options page, a QML context property: what to take of this offer.
    ReceiveFilesModel *offeredFiles() const { return m_offeredFiles; }
    bool saveToGallery() const { return m_toGallery; }
    void setSaveToGallery(bool on);
    bool hasFolders() const { return m_hasFolders; }
    bool isMessage() const { return m_isMessage; }
    QString message() const;
    bool messageIsLink() const;
    QString folder() const;
    QStringList folders() const;
    int filesDone() const;
    int filesFailed() const;
    bool quickSaved() const;
    QString lastFilePath() const { return m_lastFilePath; }
    QString fileName() const { return m_currentName; }
    qreal bytesReceived() const;
    qreal progress() const;
    int remainingSeconds() const { return m_remainingSeconds; }

    // State "request": take the files selected in offeredFiles(), at least
    // one, under their names there / refuse it.
    Q_INVOKABLE void accept();
    Q_INVOKABLE void decline();
    // State "request": the native folder picker, for this session's folder
    // (returns at once, `folder` changes when the user confirms one).
    Q_INVOKABLE void pickFolder();
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
    // Also on every stateChanged().
    void optionsChanged();
    // A /cancel for a session that isn't ours to receive: the device we are
    // sending to may be cancelling (see Sender::onCancelReceived()).
    void cancelReceived(const QString &ip, const QString &sessionId);

private slots:
    void onTick();
    void onOnlineChanged();
    void onCancelSent();
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void dropDraining();
    void clearPinAttempts();
    void runFolderPicker();

private:
    enum FileStatus { Pending, Receiving, Done, Failed };
    struct InFile {
        QString id;
        QString fileName;   // as sent or as renamed; may contain folders ("Trip/a.jpg")
        QString fileType;
        QString preview;
        QString sha256;     // from the offer, may be empty
        QDateTime modified; // from the offer, UTC, invalid if missing
        QDateTime accessed; // the same
        qint64 size;
        QString token;
        FileStatus status;
        int attempts;       // uploads started
        qint64 received;
        QString savedPath;  // while receiving and once done
        QString error;      // why it failed
    };
    struct Upload {
        int file;           // index into m_files
        QFile *out;
        Sha256Hash *hash;   // 0 if the offer had no sha256
    };

    HttpResponse prepareUpload(const HttpRequest &request);
    HttpResponse cancelRequest(const HttpRequest &request);
    HttpResponse startUpload(const HttpRequest &request);
    HttpResponse writeUpload(const HttpRequest &request, const QByteArray &data);
    HttpResponse finishUpload(const HttpRequest &request);
    HttpResponse startSession();
    bool quickSaves() const;
    void applySelection();
    void closeUpload(quint32 requestId, bool keepFile, const QString &error = QString());
    void applyFileTimes(const InFile &f);
    void dropUploads();
    bool notifySender();
    void checkDone();
    void finish(const QString &state, const QString &error = QString());
    // finish("cancelled"), by "sender" or "receiver".
    void finishCancelled(const char *by, const QString &error = QString());
    void setState(const QString &state, const QString &error = QString());
    void reset();
    static QDateTime parseTime(const QString &text);
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
    ReceiveFilesModel *m_offeredFiles;
    QNetworkAccessManager *m_nam;     // only for the /cancel to the sender

    QString m_state;
    QString m_errorText;
    QString m_cancelledBy;
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
    QString m_folder;                 // Qt separators, set when the offer arrives
    bool m_toGallery;                 // images/videos go to settings' media folders
    bool m_hasFolders;                // some file name contains a folder
    QStringList m_usedFolders;        // base folders files were saved to, Qt separators
    QString m_lastFilePath;
    QHash<quint32, Upload> m_uploads; // running uploads by request id
    // Uploads we cancelled: their data is thrown away and their connections
    // stay open until the sender has our /cancel (see cancel()).
    QList<quint32> m_draining;
    QTimer m_drainTimer;
    QString m_currentName;
    QDateTime m_receivingSince;
    QDateTime m_lastActivity;
    int m_remainingSeconds;
    QHash<QString, int> m_wrongPins;  // sender IP -> wrong PINs since the last right one
    QTimer m_tick;
};

#endif // RECEIVER_H
