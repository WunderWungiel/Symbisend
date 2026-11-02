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
#  include <unistd.h>
#  include <errno.h>
#  include <string.h>
#endif

static const int kPollIntervalMs = 250;
static const int kMaxDatagram = 8192;

MulticastReceiver::MulticastReceiver(QObject *parent)
    : QObject(parent), m_fd(-1)
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

    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
        qWarning() << "Multicast: could not set O_NONBLOCK, errno" << errno;

    m_fd = fd;
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
    for (int i = 0; i < 32; ++i) { // bounded per tick
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(m_fd, buffer, sizeof(buffer), 0,
                               reinterpret_cast<struct sockaddr *>(&from), &fromLen);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                qWarning() << "Multicast: recvfrom failed, errno" << errno;
            return;
        }
        QHostAddress sender(ntohl(from.sin_addr.s_addr));
        emit datagramReceived(QByteArray(buffer, int(n)), sender, ntohs(from.sin_port));
    }
}

#endif
