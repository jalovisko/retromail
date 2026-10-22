#pragma once

#include "core/Account.h"

#include <QObject>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>

// Outgoing message, plain text only (it's 1998).
struct OutgoingMail {
    QString fromName;
    QString fromAddress;
    QStringList to;          // "Name <addr>" or bare addresses
    QString subject;
    QString body;
    QByteArray inReplyTo;
    QByteArray references;

    QByteArray toRfc5322() const;
};

// Sends one message, then disconnects. Supports implicit TLS (465) and
// STARTTLS (587), with AUTH PLAIN or AUTH XOAUTH2.
class SmtpClient : public QObject
{
    Q_OBJECT
public:
    explicit SmtpClient(QObject *parent = nullptr);
    ~SmtpClient() override;

    void send(const QString &host, quint16 port, SmtpSecurity security,
              const QString &user, const QString &secret, bool useOAuth,
              const OutgoingMail &mail);

signals:
    void finished(bool ok, const QString &message);

private:
    enum class State { Greeting, Ehlo, StartTls, EhloAfterTls, Auth, AuthCancel, MailFrom,
                       RcptTo, Data, Body, Quit, Done };

    void onReadyRead();
    void onReply(int code, const QByteArray &text);
    void sendLine(const QByteArray &line);
    void sendAuth();
    void fail(const QString &message);

    QSslSocket m_socket;
    QTimer m_timeout;
    QByteArray m_buffer;
    QByteArray m_replyText;
    State m_state = State::Done;
    SmtpSecurity m_security = SmtpSecurity::SslTls;
    QString m_user;
    QString m_secret;
    bool m_oauth = false;
    QByteArray m_from;
    QStringList m_recipients;
    int m_rcptIndex = 0;
    QByteArray m_payload;
};
