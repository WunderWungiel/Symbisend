#include "multicastreceiver.h"

#include <QDebug>
#include <QSocketNotifier>

#if defined(Q_OS_WIN)
#  define SYMBISEND_NO_RAW_MULTICAST
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <sys/select.h>
#  include <sys/time.h>
#  include <unistd.h>
#  include <errno.h>
#  include <string.h>
#endif

// Only until the notifier has fired once (see the header).
static const int kFallbackPollMs = 5000;
// Reading pauses this long after empty reads or errors, which repeat without
// end while the link goes down (the descriptor keeps reporting "readable").
static const int kPauseMs = 1000;
static const int kMaxDatagram = 8192;
// Datagrams read per wake-up at most; the rest waits for the next one.
static const int kMaxReadsPerWakeUp = 32;
// Failed reads in a row before the socket is given up on. Same as the official
// app: a single one can be transient.
static const int kMaxReadFailures = 10;

MulticastReceiver::MulticastReceiver(QObject *parent)
    : QObject(parent), m_fd(-1), m_notifier(0), m_notifierFired(false), m_badReadLogged(false),
      m_oddAddressLogged(false), m_selectErrorLogged(false), m_fallbackLogged(false),
      m_readFailures(0)
{
    m_fallbackTimer.setInterval(kFallbackPollMs);
    connect(&m_fallbackTimer, SIGNAL(timeout()), this, SLOT(onFallbackPoll()));
    m_resumeTimer.setSingleShot(true);
    m_resumeTimer.setInterval(kPauseMs);
    connect(&m_resumeTimer, SIGNAL(timeout()), this, SLOT(resumeNotifier()));
}

MulticastReceiver::~MulticastReceiver()
{
    stop();
}

#ifdef SYMBISEND_NO_RAW_MULTICAST

bool MulticastReceiver::start(const QHostAddress &, quint16, const QHostAddress &)
{
    qWarning() << "Multicast: raw receiver not supported on this platform";
    return false;
}

void MulticastReceiver::stop() {}
void MulticastReceiver::onActivated() {}
void MulticastReceiver::onFallbackPoll() {}
void MulticastReceiver::resumeNotifier() {}
bool MulticastReceiver::readPending() { return false; }
void MulticastReceiver::pauseNotifier() {}
void MulticastReceiver::readFailed() {}

#else

bool MulticastReceiver::start(const QHostAddress &group, quint16 port, const QHostAddress &localInterface)
{
    stop();

    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        qWarning() << "Multicast: socket() failed, errno" << errno;
        return false;
    }

    int one = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
        qWarning() << "Multicast: SO_REUSEADDR failed, errno" << errno;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
        qWarning() << "Multicast: bind to port" << port << "failed, errno" << errno;
        ::close(fd);
        return false;
    }

    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = htonl(group.toIPv4Address());
    mreq.imr_interface.s_addr = localInterface.isNull()
            ? htonl(INADDR_ANY) : htonl(localInterface.toIPv4Address());
    if (::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
        int err = errno;
        // Retry with the default interface.
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (::setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
            qWarning() << "Multicast: IP_ADD_MEMBERSHIP failed, errno" << err << "/" << errno;
            ::close(fd);
            return false;
        }
    }

    // Non-blocking is only a second line of defence: readPending() checks
    // with select() before every read. On the 808 Open C refuses both ways
    // (fcntl errno 13, ioctl errno 78), so this is logged at debug level.
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        int err = errno;
        int nonBlocking = 1;
        if (::ioctl(fd, FIONBIO, &nonBlocking) != 0)
            qDebug() << "Multicast: socket stays blocking (errno" << err << "/" << errno
                     << "), reads are guarded by select()";
    }

    m_fd = fd;
    m_badReadLogged = false;
    m_oddAddressLogged = false;
    m_selectErrorLogged = false;
    m_fallbackLogged = false;
    m_readFailures = 0;
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), this, SLOT(onActivated()));
    if (!m_notifierFired)
        m_fallbackTimer.start();
    qDebug() << "Multicast: joined" << group.toString() << "port" << port
             << (m_notifierFired ? "" : "(fallback poll until the socket notifier fires)");
    return true;
}

void MulticastReceiver::stop()
{
    m_fallbackTimer.stop();
    m_resumeTimer.stop();
    if (m_notifier) {
        // Before close(): the dispatcher must stop watching the descriptor.
        // deleteLater(): stop() may run inside the notifier's own activated().
        m_notifier->setEnabled(false);
        m_notifier->disconnect(this);
        m_notifier->deleteLater();
        m_notifier = 0;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void MulticastReceiver::onActivated()
{
    if (!m_notifierFired) {
        m_notifierFired = true;
        m_fallbackTimer.stop();
        qDebug() << "Multicast: socket notifier works, fallback poll off";
    }
    if (!readPending())
        readFailed();
}

void MulticastReceiver::onFallbackPoll()
{
    if (m_notifierFired) {
        m_fallbackTimer.stop();
        return;
    }
    if (m_fd < 0 || m_resumeTimer.isActive())
        return;
    if (!readPending())
        readFailed();
}

// Pauses, or gives up for good if the socket doesn't recover. The usual cause,
// the link going down, ends in going offline before that.
void MulticastReceiver::readFailed()
{
    if (m_fd < 0)
        return; // stopped by a slot meanwhile
    if (++m_readFailures < kMaxReadFailures) {
        pauseNotifier();
        return;
    }
    qWarning() << "Multicast: giving up after" << m_readFailures << "failed reads";
    stop();
    emit failed();
}

void MulticastReceiver::pauseNotifier()
{
    if (m_notifier && m_notifier->isEnabled()) {
        m_notifier->setEnabled(false);
        m_resumeTimer.start();
    }
}

void MulticastReceiver::resumeNotifier()
{
    if (m_notifier)
        m_notifier->setEnabled(true);
}

bool MulticastReceiver::readPending()
{
    char buffer[kMaxDatagram];
    // Bounded per wake-up. m_fd is re-checked because a slot connected to
    // datagramReceived() may have called stop() (the notifier that may have
    // called us is only deleted later, see stop()).
    for (int i = 0; i < kMaxReadsPerWakeUp && m_fd >= 0; ++i) {
        // Open C refuses both O_NONBLOCK and FIONBIO (see start()), so the
        // socket may be blocking: only read when select() says a datagram
        // is waiting, or recvfrom() could freeze the UI thread. (A notifier
        // wake-up said so a moment ago; this is one cheap call per datagram.)
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_fd, &readable);
        struct timeval noWait;
        noWait.tv_sec = 0;
        noWait.tv_usec = 0;
        int ready = ::select(m_fd + 1, &readable, 0, 0, &noWait);
        if (ready < 0) {
            if (errno != EINTR && !m_selectErrorLogged) {
                m_selectErrorLogged = true;
                qWarning() << "Multicast: select failed, errno" << errno
                           << "- further ones not logged";
            }
            return false;
        }
        if (ready == 0 || !FD_ISSET(m_fd, &readable))
            return true; // nothing (more) waiting

        struct sockaddr_in from;
        memset(&from, 0, sizeof(from));
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(m_fd, buffer, sizeof(buffer), 0,
                               reinterpret_cast<struct sockaddr *>(&from), &fromLen);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;
            qWarning() << "Multicast: recvfrom failed, errno" << errno;
            return false;
        }
        // When the WLAN link goes down, Open C's recvfrom() "succeeds" with
        // no datagram behind it, again and again. Drop those and pause;
        // going offline stops the receiver shortly after.
        if (n == 0) {
            if (!m_badReadLogged) {
                m_badReadLogged = true;
                qWarning() << "Multicast: empty read - link down? Ignoring further ones";
            }
            return false;
        }
        if (!m_notifierFired && !m_fallbackLogged) {
            m_fallbackLogged = true;
            qWarning() << "Multicast: datagram found by the fallback poll - the socket notifier"
                       << "has not fired (yet)";
        }

        // Open C's sockaddr_in is 28 bytes (AF_INET is 0x0800 there), but
        // recvfrom() reports only the 8 it fills: family, port, address. So
        // don't compare the length with sizeof(from); it used to drop every
        // datagram. Never drop a datagram over its address either: Discovery
        // copes with a null (unknown) sender.
        QHostAddress sender;
        quint16 senderPort = 0;
        const socklen_t minLen = socklen_t(reinterpret_cast<const char *>(&from.sin_addr)
                                           - reinterpret_cast<const char *>(&from)
                                           + sizeof(from.sin_addr));
        if (from.sin_family == AF_INET && fromLen >= minLen && from.sin_addr.s_addr != 0) {
            sender = QHostAddress(ntohl(from.sin_addr.s_addr));
            senderPort = ntohs(from.sin_port);
        } else if (!m_oddAddressLogged) {
            m_oddAddressLogged = true;
            qWarning() << "Multicast: unusable sender address (family" << int(from.sin_family)
                       << ", length" << int(fromLen) << ") - further ones not logged";
        }
        m_readFailures = 0;
        emit datagramReceived(QByteArray(buffer, int(n)), sender, senderPort);
    }
    return true;
}

#endif
