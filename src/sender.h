#ifndef SENDER_H
#define SENDER_H

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QNetworkAccessManager>
#include <QStringList>
#include <QTimer>

#include "peermodel.h"
#include "selectionmodel.h"

class QNetworkReply;
class QSslError;
class DeviceInfo;
class Discovery;
class NetworkManager;

// Sends the selection to one peer (LocalSend v2 upload API):
//  1. POST /prepare-upload with the file list. The receiver answers once its
//     user accepted (or at once with quick save): a session id and a token
//     per accepted file, 204 (nothing to transfer, e.g. a text message),
//     401 (PIN), 403 (declined), 409 (busy), ...
//  2. POST /upload for every accepted file, one after another, streamed from
//     disk.
//     A file that fails is marked as failed and the next one goes on, as in
//     the official app: the transfer ends in "finishedWithErrors".
//  3. POST /cancel when the user cancels or the receiver stops responding. The
//     receiver can cancel too, with a /cancel to our server: onCancelReceived().
// One transfer at a time. See docs/qml-api.md.
class Sender : public QObject
{
    Q_OBJECT
    // "idle", "connecting" (sendToAddress(): finding out who is there),
    // "waiting" (for the receiver to accept), "pin" (receiver wants a PIN:
    // submitPin()), "sending", "finished", "finishedWithErrors" (some accepted
    // files were not sent: errorText lists them), "declined", "failed", "cancelled"
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    // connecting, waiting, pin or sending
    Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
    Q_PROPERTY(QString peerAlias READ peerAlias NOTIFY stateChanged)
    // Why it failed / was declined; also "Wrong PIN" in state "pin".
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    // Accepted files (a declined part of the selection is not counted).
    Q_PROPERTY(int fileCount READ fileCount NOTIFY stateChanged)
    // Accepted files that were sent.
    Q_PROPERTY(int filesDone READ filesDone NOTIFY progressChanged)
    // 1-based index of the file being uploaded, 0 before the first one.
    Q_PROPERTY(int fileIndex READ fileIndex NOTIFY progressChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY progressChanged)
    // Bytes of the accepted files; qreal for sizes over 2 GB.
    Q_PROPERTY(qreal bytesSent READ bytesSent NOTIFY progressChanged)
    Q_PROPERTY(qreal bytesTotal READ bytesTotal NOTIFY progressChanged)
    // 0..1 over all accepted files.
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    // Estimated seconds left, updated every second while sending; -1 unknown.
    Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY progressChanged)

public:
    // MIME type from the file name's extension, "application/octet-stream"
    // if unknown.
    static QString mimeType(const QString &fileName);
    // title, then the first few of lines ("a.jpg: why"), one per line.
    static QString failureText(const QString &title, const QStringList &lines);

    Sender(DeviceInfo *device, PeerModel *peers, SelectionModel *selection,
           NetworkManager *network, Discovery *discovery, QObject *parent = 0);

    QString state() const { return m_state; }
    bool isBusy() const;
    QString peerAlias() const { return m_peer.alias; }
    QString errorText() const { return m_errorText; }
    int fileCount() const { return m_queue.size(); }
    int filesDone() const;
    int fileIndex() const { return m_current + 1; }
    QString fileName() const;
    qreal bytesSent() const { return qreal(m_bytesDone + m_currentSent); }
    qreal bytesTotal() const { return qreal(m_bytesTotal); }
    qreal progress() const;
    int remainingSeconds() const { return m_remainingSeconds; }

    // Starts sending the current selection to the peer. Returns false (with
    // errorText set and state "failed") if that is not possible right now:
    // already busy, nothing selected, offline, unknown or v1 peer.
    Q_INVOKABLE bool send(const QString &fingerprint);
    // The same for a device that was not found, by its IP address on the
    // default port. False as above; a device that isn't there is reported
    // later, as state "failed".
    Q_INVOKABLE bool sendToAddress(const QString &ip);
    // State "pin": asks again with this PIN.
    Q_INVOKABLE void submitPin(const QString &pin);
    // Stops the transfer (and tells the receiver, if it accepted already).
    Q_INVOKABLE void cancel();

public slots:
    // A /cancel reached our server for a session that isn't one we receive
    // (Receiver::cancelReceived()). Ours if it comes from the receiver of the
    // transfer we are sending, as in the official app.
    void onCancelReceived(const QString &ip, const QString &sessionId);

signals:
    void stateChanged();
    void progressChanged();

private slots:
    void onPrepareFinished();
    void onUploadProgress(qint64 sent, qint64 total);
    void onUploadFinished();
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void onTick();
    void onOnlineChanged();
    void onLookUpFinished(const QString &ip, const QString &fingerprint, const QString &error);

private:
    QString certificateError() const;

    enum FileStatus { Queued, Done, Failed };
    struct OutFile {
        QString id;
        QString fileName;   // relative path for folder contents
        QString path;       // empty for text
        QString text;
        qint64 size;
        QString token;      // from prepare-upload
        FileStatus status;  // only used in m_queue
        QString error;      // why it failed
    };

    void reset();
    bool start(const QString &fingerprint);
    QUrl url(const QString &route) const;
    void postPrepare();
    void startNextUpload();
    bool postUpload(const OutFile &f);
    void fileFailed(const QString &error);
    void failRemaining(const QString &error);
    void finishUploads();
    void finish(const QString &state, const QString &error = QString());
    void setState(const QString &state, const QString &error = QString());
    void sendCancel();
    void dropReply();
    static QString isoTime(const QDateTime &t);

    DeviceInfo *m_device;
    PeerModel *m_peers;
    SelectionModel *m_selection;
    NetworkManager *m_network;
    Discovery *m_discovery;
    QNetworkAccessManager *m_nam;

    QString m_state;
    QString m_errorText;
    Peer m_peer;
    QList<OutFile> m_files;   // everything offered
    QList<OutFile> m_queue;   // the accepted ones
    QString m_pin;
    bool m_pinSent;
    // The receiver's certificate wasn't the one of the device we found.
    bool m_certRejected;
    QString m_sessionId;
    QNetworkReply *m_reply;   // the one request in flight (owns the upload's QFile)
    int m_current;            // index into m_queue, -1 before the first
    qint64 m_bytesTotal;
    qint64 m_bytesDone;       // completed files
    qint64 m_currentSent;     // of the current file
    QDateTime m_sendingSince;
    QDateTime m_lastActivity; // last upload progress (stall detection)
    int m_remainingSeconds;
    QTimer m_tick;
};

#endif // SENDER_H
