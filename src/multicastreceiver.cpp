#include "multicastreceiver.h"

#include <QDebug>

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

static const int kPollIntervalMs = 250;
static const int kMaxDatagram = 8192;

MulticastReceiver::MulticastReceiver(QObject *parent)
    : QObject(parent), m_fd(-1), m_badReadLogged(false), m_oddAddressLogged(false),
      m_selectErrorLogged(false)
{
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(poll()));
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
void MulticastReceiver::poll() {}

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

    // Non-blocking is only a second line of defence: poll() checks with
    // select() before every read. On the 808 Open C refuses both ways
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
    m_pollTimer.start();
    qDebug() << "Multicast: joined" << group.toString() << "port" << port;
    return true;
}

void MulticastReceiver::stop()
{
    m_pollTimer.stop();
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void MulticastReceiver::poll()
{
    if (m_fd < 0)
        return;
    char buffer[kMaxDatagram];
    // Bounded per tick. m_fd is re-checked because a slot connected to
    // datagramReceived() may have called stop().
    for (int i = 0; i < 32 && m_fd >= 0; ++i) {
        // Open C refuses both O_NONBLOCK and FIONBIO (see start()), so the
        // socket may be blocking: only read when select() says a datagram
        // is waiting, or recvfrom() could freeze the UI thread.
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
            return;
        }
        if (ready == 0 || !FD_ISSET(m_fd, &readable))
            return; // nothing waiting

        struct sockaddr_in from;
        memset(&from, 0, sizeof(from));
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(m_fd, buffer, sizeof(buffer), 0,
                               reinterpret_cast<struct sockaddr *>(&from), &fromLen);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                qWarning() << "Multicast: recvfrom failed, errno" << errno;
            return;
        }
        // When the WLAN link goes down, Open C's recvfrom() "succeeds" with
        // no datagram behind it. Drop those; going offline stops the
        // receiver shortly after.
        if (n == 0) {
            if (!m_badReadLogged) {
                m_badReadLogged = true;
                qWarning() << "Multicast: empty read - link down? Ignoring further ones";
            }
            return;
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
        emit datagramReceived(QByteArray(buffer, int(n)), sender, senderPort);
    }
}

#endif
