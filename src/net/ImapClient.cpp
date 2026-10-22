#include "net/ImapClient.h"

#include <QRegularExpression>

ImapClient::ImapClient(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(60 * 1000);

    connect(&m_socket, &QSslSocket::readyRead, this, &ImapClient::onReadyRead);
    connect(&m_socket, &QSslSocket::disconnected, this, [this] {
        m_greeted = false;
        failAll(tr("Connection closed by server"));
        emit disconnected();
    });
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        // Servers drop idle connections; that is not worth an error message.
        if (error == QAbstractSocket::RemoteHostClosedError && !m_inflight && m_queue.isEmpty())
            return;
        const QString message = m_socket.errorString();
        failAll(message);
        emit errorOccurred(message);
    });
    connect(&m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        QStringList texts;
        for (const auto &e : errors)
            texts << e.errorString();
        emit errorOccurred(tr("TLS error: %1").arg(texts.join(QStringLiteral("; "))));
    });
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        emit errorOccurred(tr("Server did not answer in time"));
        m_socket.abort();
    });
}

ImapClient::~ImapClient()
{
    // Closing the socket emits signals; the owner may already be half
    // destroyed, so make sure none of them reach callbacks.
    m_socket.disconnect(this);
    m_queue.clear();
    m_inflight.reset();
    m_socket.abort();
}

void ImapClient::abort()
{
    m_queue.clear();
    m_inflight.reset();
    m_greeted = false;
    m_timeout.stop();
    m_socket.abort();
}

void ImapClient::connectToHost(const QString &host, quint16 port)
{
    m_buffer.clear();
    m_current = {};
    m_literalRemaining = -1;
    m_greeted = false;
    m_timeout.start();
    m_socket.connectToHostEncrypted(host, port);
}

void ImapClient::disconnectFromHost()
{
    m_queue.clear();
    if (m_socket.state() == QAbstractSocket::ConnectedState && m_greeted)
        m_socket.write("Z LOGOUT\r\n");
    m_socket.disconnectFromHost();
}

bool ImapClient::isConnected() const
{
    return m_greeted && m_socket.state() == QAbstractSocket::ConnectedState;
}

void ImapClient::command(const QByteArray &cmd, Callback callback)
{
    Pending p;
    p.tag = "A" + QByteArray::number(++m_tagCounter);
    p.command = cmd;
    p.callback = std::move(callback);
    m_queue.enqueue(std::move(p));
    pump();
}

QByteArray ImapClient::quote(const QString &s)
{
    QByteArray out = s.toUtf8();
    out.replace('\\', "\\\\");
    out.replace('"', "\\\"");
    return '"' + out + '"';
}

void ImapClient::login(const QString &user, const QString &password, Callback callback)
{
    command("LOGIN " + quote(user) + ' ' + quote(password), std::move(callback));
}

void ImapClient::authenticateXOAuth2(const QString &user, const QString &accessToken, Callback callback)
{
    // SASL XOAUTH2, sent as an initial response (RFC 4959). Gmail and Outlook
    // both support it. On failure the server sends a "+ <json>" challenge,
    // which onReadyRead answers with an empty line.
    const QByteArray sasl = "user=" + user.toUtf8() + "\x01" "auth=Bearer " + accessToken.toUtf8() + "\x01\x01";
    command("AUTHENTICATE XOAUTH2 " + sasl.toBase64(), std::move(callback));
}

void ImapClient::pump()
{
    if (m_inflight || !m_greeted || m_queue.isEmpty())
        return;
    m_inflight = m_queue.dequeue();
    m_socket.write(m_inflight->tag + ' ' + m_inflight->command + "\r\n");
    m_timeout.start();
}

void ImapClient::failAll(const QString &message)
{
    m_timeout.stop();
    const QByteArray status = message.toUtf8();
    std::optional<Pending> inflight;
    inflight.swap(m_inflight);
    QQueue<Pending> queue;
    queue.swap(m_queue);
    if (inflight && inflight->callback)
        inflight->callback(false, status, {});
    for (auto &p : queue) {
        if (p.callback)
            p.callback(false, status, {});
    }
}

void ImapClient::onReadyRead()
{
    m_buffer += m_socket.readAll();
    static const QRegularExpression literalMarker(QStringLiteral("\\{(\\d+)\\+?\\}$"));

    for (;;) {
        if (m_literalRemaining >= 0) {
            if (m_buffer.size() < m_literalRemaining)
                break;
            m_current.literals.append(m_buffer.left(m_literalRemaining));
            m_buffer.remove(0, m_literalRemaining);
            m_literalRemaining = -1;
            continue;
        }

        const qsizetype eol = m_buffer.indexOf("\r\n");
        if (eol < 0)
            break;
        const QByteArray part = m_buffer.left(eol);
        m_buffer.remove(0, eol + 2);
        m_current.line += part;

        const auto match = literalMarker.match(QString::fromLatin1(part));
        if (match.hasMatch()) {
            m_literalRemaining = match.captured(1).toLongLong();
            continue;
        }

        ImapResponse done;
        std::swap(done, m_current);
        handleResponse(done);
    }

    if (m_inflight)
        m_timeout.start();   // data is flowing, push the deadline out
}

void ImapClient::handleResponse(const ImapResponse &response)
{
    const QByteArray &line = response.line;

    if (!m_greeted) {
        if (line.startsWith("* OK") || line.startsWith("* PREAUTH")) {
            m_greeted = true;
            m_timeout.stop();
            emit ready();
            pump();
        } else {
            emit errorOccurred(tr("Unexpected greeting: %1").arg(QString::fromUtf8(line)));
            m_socket.abort();
        }
        return;
    }

    if (line.startsWith("+")) {
        // Continuation request. We never send literals ourselves, so this only
        // happens on a failed AUTHENTICATE: cancel with an empty line.
        m_socket.write("\r\n");
        return;
    }

    if (line.startsWith("* ")) {
        if (line.startsWith("* BYE") && !m_inflight)
            emit errorOccurred(QString::fromUtf8(line.mid(6)));
        if (m_inflight)
            m_inflight->untagged.append(response);
        return;
    }

    if (m_inflight && line.startsWith(m_inflight->tag + ' ')) {
        m_timeout.stop();
        const QByteArray rest = line.mid(m_inflight->tag.size() + 1);
        const bool ok = rest.startsWith("OK");
        Pending finished = std::move(*m_inflight);
        m_inflight.reset();
        if (finished.callback)
            finished.callback(ok, rest, finished.untagged);
        pump();
    }
}
