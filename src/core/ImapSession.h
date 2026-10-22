#pragma once

#include "core/MailSession.h"

#include <QDateTime>
#include <QTimer>

#include <functional>

class ImapClient;

// A real account: IMAP for reading, SMTP for sending. Keeps one IMAP
// connection open and polls the INBOX every few minutes.
class ImapSession : public MailSession
{
    Q_OBJECT
public:
    explicit ImapSession(const Account &account, QObject *parent = nullptr);

    Account account() const override { return m_account; }

    void start() override;
    void refresh() override;
    void fetchMessage(quint32 uid) override;
    void deleteMessage(quint32 uid) override;
    void sendMail(const OutgoingMail &mail) override;

    static constexpr int kHeaderLimit = 100;   // newest messages shown in the inbox

private:
    using CredentialCallback = std::function<void(bool ok, const QString &secretOrToken)>;

    void withCredential(CredentialCallback callback);
    void connectAndLogin();
    void onLoggedIn();
    void sync();
    void whenConnected(std::function<void()> action);
    void fail(const QString &message);

    Account m_account;
    ImapClient *m_imap = nullptr;
    QTimer m_poll;
    QString m_secret;           // password, or OAuth refresh token
    bool m_secretLoaded = false;
    QString m_accessToken;
    QDateTime m_accessTokenTime;
    bool m_connecting = false;
    bool m_loggedIn = false;
    bool m_retriedLogin = false;
    QList<std::function<void()>> m_afterLogin;
};
