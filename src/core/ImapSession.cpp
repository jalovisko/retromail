#include "core/ImapSession.h"

#include "core/SecretStore.h"
#include "net/ImapClient.h"
#include "net/OAuthLogin.h"

#include <QRegularExpression>

#include <utility>

namespace {
constexpr int kPollMinutes = 3;
constexpr int kTokenLifetimeMinutes = 45;   // providers issue 60-minute tokens
}

ImapSession::ImapSession(const Account &account, QObject *parent)
    : MailSession(parent)
    , m_account(account)
    , m_imap(new ImapClient(this))
{
    m_poll.setInterval(kPollMinutes * 60 * 1000);
    connect(&m_poll, &QTimer::timeout, this, &ImapSession::refresh);

    connect(m_imap, &ImapClient::ready, this, [this] {
        setState(State::Busy, tr("SIGNING ON..."));
        auto onLogin = [this](bool ok, const QByteArray &status, const QList<ImapResponse> &) {
            if (ok) {
                m_retriedLogin = false;
                onLoggedIn();
                return;
            }
            if (m_account.auth == AuthMethod::OAuth && !m_retriedLogin) {
                // The cached token may have been revoked or expired early.
                m_retriedLogin = true;
                m_accessToken.clear();
                m_imap->abort();
                m_connecting = false;
                connectAndLogin();
                return;
            }
            fail(tr("Login failed: %1").arg(QString::fromUtf8(status)));
            m_imap->disconnectFromHost();
        };
        if (m_account.auth == AuthMethod::OAuth)
            m_imap->authenticateXOAuth2(m_account.username, m_accessToken, onLogin);
        else
            m_imap->login(m_account.username, m_secret, onLogin);
    });
    connect(m_imap, &ImapClient::errorOccurred, this, [this](const QString &message) {
        fail(message);
    });
    connect(m_imap, &ImapClient::disconnected, this, [this] { m_loggedIn = false; });
}

void ImapSession::fail(const QString &message)
{
    m_connecting = false;
    m_loggedIn = false;
    m_afterLogin.clear();
    setState(State::Error, message);
}

void ImapSession::start()
{
    m_poll.start();
    connectAndLogin();
}

void ImapSession::refresh()
{
    whenConnected([this] { sync(); });
}

void ImapSession::whenConnected(std::function<void()> action)
{
    if (m_loggedIn && m_imap->isConnected()) {
        action();
        return;
    }
    m_afterLogin.append(std::move(action));
    connectAndLogin();
}

void ImapSession::withCredential(CredentialCallback callback)
{
    if (!m_secretLoaded) {
        SecretStore::read(m_account.id, this, [this, callback](bool ok, const QString &secret, const QString &error) {
            if (!ok) {
                callback(false, tr("Keychain: %1").arg(error));
                return;
            }
            m_secret = secret;
            m_secretLoaded = true;
            withCredential(callback);
        });
        return;
    }

    if (m_account.auth == AuthMethod::Password) {
        callback(true, m_secret);
        return;
    }

    if (!m_accessToken.isEmpty()
        && m_accessTokenTime.secsTo(QDateTime::currentDateTimeUtc()) < kTokenLifetimeMinutes * 60) {
        callback(true, m_accessToken);
        return;
    }

    const auto store = [this, callback](const QString &access, const QString &refresh) {
        m_accessToken = access;
        m_accessTokenTime = QDateTime::currentDateTimeUtc();
        if (!refresh.isEmpty() && refresh != m_secret) {
            m_secret = refresh;
            SecretStore::write(m_account.id, refresh, this, [](bool, const QString &) {});
        }
        callback(true, access);
    };

    auto *login = new OAuthLogin(m_account.provider, m_account.email, this);
    connect(login, &OAuthLogin::succeeded, this, store);
    connect(login, &OAuthLogin::failed, this, [this, store, callback](const QString &) {
        // Refresh token rejected: fall back to signing in through the browser.
        setState(State::Busy, tr("PLEASE SIGN IN AGAIN IN YOUR BROWSER..."));
        auto *interactive = new OAuthLogin(m_account.provider, m_account.email, this);
        connect(interactive, &OAuthLogin::succeeded, this, store);
        connect(interactive, &OAuthLogin::failed, this, [callback](const QString &message) {
            callback(false, message);
        });
        interactive->authorize();
    });
    login->refresh(m_secret);
}

void ImapSession::connectAndLogin()
{
    if (m_connecting)
        return;
    m_connecting = true;
    setState(State::Busy, tr("DIALING %1...").arg(m_account.imapHost.toUpper()));
    withCredential([this](bool ok, const QString &secretOrToken) {
        if (!ok) {
            fail(secretOrToken);
            return;
        }
        if (m_account.auth == AuthMethod::OAuth)
            m_accessToken = secretOrToken;
        m_imap->connectToHost(m_account.imapHost, quint16(m_account.imapPort));
    });
}

void ImapSession::onLoggedIn()
{
    m_connecting = false;
    m_loggedIn = true;
    const auto actions = std::exchange(m_afterLogin, {});
    if (actions.isEmpty()) {
        sync();
        return;
    }
    for (const auto &action : actions)
        action();
}

void ImapSession::sync()
{
    setState(State::Busy, tr("CHECKING MAIL..."));
    m_imap->command("SELECT INBOX", [this](bool ok, const QByteArray &status, const QList<ImapResponse> &untagged) {
        if (!ok) {
            fail(tr("Cannot open INBOX: %1").arg(QString::fromUtf8(status)));
            return;
        }
        static const QRegularExpression existsRe(QStringLiteral("^\\* (\\d+) EXISTS"));
        quint32 exists = 0;
        for (const auto &r : untagged) {
            const auto m = existsRe.match(QString::fromLatin1(r.line));
            if (m.hasMatch())
                exists = m.captured(1).toUInt();
        }
        if (exists == 0) {
            setHeaders({});
            setState(State::Ready);
            return;
        }

        const quint32 first = exists > quint32(kHeaderLimit) ? exists - kHeaderLimit + 1 : 1;
        const QByteArray cmd = "FETCH " + QByteArray::number(first) + ':' + QByteArray::number(exists)
            + " (UID FLAGS BODY.PEEK[HEADER.FIELDS (FROM TO SUBJECT DATE)])";
        m_imap->command(cmd, [this](bool ok, const QByteArray &status, const QList<ImapResponse> &untagged) {
            if (!ok) {
                fail(tr("Cannot read INBOX: %1").arg(QString::fromUtf8(status)));
                return;
            }
            static const QRegularExpression uidRe(QStringLiteral("UID (\\d+)"));
            static const QRegularExpression flagsRe(QStringLiteral("FLAGS \\(([^)]*)\\)"));
            QList<MessageHeader> list;
            for (const auto &r : untagged) {
                if (!r.line.contains(" FETCH "))
                    continue;
                const QString line = QString::fromLatin1(r.line);
                const auto uid = uidRe.match(line);
                if (!uid.hasMatch())
                    continue;
                MessageHeader h;
                h.uid = uid.captured(1).toUInt();
                h.seen = flagsRe.match(line).captured(1).contains(QLatin1String("\\Seen"), Qt::CaseInsensitive);
                if (!r.literals.isEmpty()) {
                    const MailHeaders headers = MailHeaders::parse(r.literals.first());
                    h.from = headers.text("from");
                    h.to = headers.text("to");
                    h.subject = headers.text("subject");
                    h.date = Mime::parseDate(headers.raw("date"));
                }
                list << h;
            }
            setHeaders(list);
            setState(State::Ready);
        });
    });
}

void ImapSession::fetchMessage(quint32 uid)
{
    whenConnected([this, uid] {
        setState(State::Busy, tr("OPENING ENVELOPE..."));
        m_imap->command("UID FETCH " + QByteArray::number(uid) + " (BODY[])",
                        [this, uid](bool ok, const QByteArray &status, const QList<ImapResponse> &untagged) {
            setState(State::Ready);
            for (const auto &r : untagged) {
                if (r.line.contains(" FETCH ") && !r.literals.isEmpty()) {
                    markSeen(uid);
                    emit messageLoaded(uid, MimeMessage::parse(r.literals.first()));
                    return;
                }
            }
            emit messageFailed(uid, ok ? tr("Message is gone") : QString::fromUtf8(status));
        });
    });
}

void ImapSession::deleteMessage(quint32 uid)
{
    whenConnected([this, uid] {
        m_imap->command("UID STORE " + QByteArray::number(uid) + " +FLAGS.SILENT (\\Deleted)",
                        [this, uid](bool ok, const QByteArray &status, const QList<ImapResponse> &) {
            if (!ok) {
                setState(State::Error, tr("Delete failed: %1").arg(QString::fromUtf8(status)));
                return;
            }
            removeHeader(uid);
            m_imap->command("EXPUNGE", [](bool, const QByteArray &, const QList<ImapResponse> &) {});
        });
    });
}

void ImapSession::sendMail(const OutgoingMail &mail)
{
    withCredential([this, mail](bool ok, const QString &secretOrToken) {
        if (!ok) {
            emit sendFinished(false, secretOrToken);
            return;
        }
        auto *smtp = new SmtpClient(this);
        connect(smtp, &SmtpClient::finished, this, [this, smtp](bool ok, const QString &message) {
            smtp->deleteLater();
            emit sendFinished(ok, message);
        });
        smtp->send(m_account.smtpHost, quint16(m_account.smtpPort), m_account.smtpSecurity,
                   m_account.username, secretOrToken, m_account.auth == AuthMethod::OAuth, mail);
    });
}
