#include "core/MailSession.h"

#include <algorithm>

int MailSession::unreadCount() const
{
    return int(std::count_if(m_headers.cbegin(), m_headers.cend(),
                             [](const MessageHeader &h) { return !h.seen; }));
}

void MailSession::setState(State state, const QString &text)
{
    m_state = state;
    m_statusText = text;
    emit stateChanged();
}

void MailSession::setHeaders(QList<MessageHeader> headers)
{
    std::sort(headers.begin(), headers.end(), [](const MessageHeader &a, const MessageHeader &b) {
        return a.uid > b.uid;   // newest first
    });

    // Count unread messages we haven't seen in the previous list.
    int fresh = 0;
    if (m_loadedOnce) {
        for (const auto &h : headers) {
            if (h.seen)
                continue;
            const bool known = std::any_of(m_headers.cbegin(), m_headers.cend(),
                                           [&](const MessageHeader &old) { return old.uid == h.uid; });
            if (!known)
                ++fresh;
        }
    }
    m_headers = std::move(headers);
    m_loadedOnce = true;
    emit headersChanged();
    if (fresh > 0)
        emit newMailArrived(fresh);
}

void MailSession::markSeen(quint32 uid)
{
    for (auto &h : m_headers) {
        if (h.uid == uid && !h.seen) {
            h.seen = true;
            emit headersChanged();
            return;
        }
    }
}

void MailSession::removeHeader(quint32 uid)
{
    m_headers.removeIf([uid](const MessageHeader &h) { return h.uid == uid; });
    emit headersChanged();
}
