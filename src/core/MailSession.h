#pragma once

#include "core/Account.h"
#include "core/MimeMessage.h"
#include "net/SmtpClient.h"

#include <QDateTime>
#include <QList>
#include <QObject>

struct MessageHeader {
    quint32 uid = 0;
    QString from;
    QString to;
    QString subject;
    QDateTime date;
    bool seen = false;
};

// One mailbox (the INBOX of one account). The UI only talks to this
// interface; ImapSession does real mail, DemoSession powers guest mode.
class MailSession : public QObject
{
    Q_OBJECT
public:
    enum class State { Idle, Busy, Ready, Error };

    explicit MailSession(QObject *parent = nullptr) : QObject(parent) {}

    virtual Account account() const = 0;
    virtual bool isGuest() const { return false; }

    State state() const { return m_state; }
    QString statusText() const { return m_statusText; }
    const QList<MessageHeader> &headers() const { return m_headers; }
    int unreadCount() const;

    virtual void start() = 0;
    virtual void refresh() = 0;
    virtual void fetchMessage(quint32 uid) = 0;
    virtual void deleteMessage(quint32 uid) = 0;
    virtual void sendMail(const OutgoingMail &mail) = 0;

signals:
    void stateChanged();
    void headersChanged();
    void newMailArrived(int newUnread);
    void messageLoaded(quint32 uid, const MimeMessage &message);
    void messageFailed(quint32 uid, const QString &error);
    void sendFinished(bool ok, const QString &message);

protected:
    void setState(State state, const QString &text = {});
    void setHeaders(QList<MessageHeader> headers);
    void markSeen(quint32 uid);
    void removeHeader(quint32 uid);

private:
    State m_state = State::Idle;
    QString m_statusText;
    QList<MessageHeader> m_headers;
    bool m_loadedOnce = false;
};
