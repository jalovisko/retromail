#include "net/SmtpClient.h"

#include "BuildConfig.h"
#include "core/MimeMessage.h"

#include <QDateTime>
#include <QUuid>

namespace {

QByteArray formatAddress(const QString &name, const QString &address)
{
    if (name.isEmpty())
        return address.toUtf8();
    QByteArray encoded = Mime::encodeWord(name);
    if (!encoded.startsWith("=?")) {
        encoded.replace('\\', "\\\\");
        encoded.replace('"', "\\\"");
        encoded = '"' + encoded + '"';
    }
    return encoded + " <" + address.toUtf8() + '>';
}

} // namespace

QByteArray OutgoingMail::toRfc5322() const
{
    QByteArrayList toList;
    for (const QString &r : to)
        toList << formatAddress(Mime::displayName(r) == Mime::bareAddress(r) ? QString() : Mime::displayName(r),
                                Mime::bareAddress(r));

    const QString domain = fromAddress.section(QLatin1Char('@'), 1).isEmpty()
                               ? QStringLiteral("retromail.local")
                               : fromAddress.section(QLatin1Char('@'), 1);

    QByteArray out;
    out += "From: " + formatAddress(fromName, fromAddress) + "\r\n";
    out += "To: " + toList.join(", ") + "\r\n";
    out += "Subject: " + Mime::encodeWord(subject) + "\r\n";
    out += "Date: " + QDateTime::currentDateTime().toString(Qt::RFC2822Date).toLatin1() + "\r\n";
    out += "Message-ID: <" + QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1()
           + '@' + domain.toUtf8() + ">\r\n";
    if (!inReplyTo.isEmpty()) {
        out += "In-Reply-To: " + inReplyTo + "\r\n";
        out += "References: " + (references.isEmpty() ? inReplyTo : references + ' ' + inReplyTo) + "\r\n";
    }
    out += "MIME-Version: 1.0\r\n";
    out += "Content-Type: text/plain; charset=UTF-8\r\n";
    out += "Content-Transfer-Encoding: base64\r\n";
    out += "X-Mailer: RetroMail " RETROMAIL_VERSION "\r\n";
    out += "\r\n";

    QString text = body;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    const QByteArray b64 = text.toUtf8().toBase64();
    for (qsizetype i = 0; i < b64.size(); i += 76)
        out += b64.mid(i, 76) + "\r\n";
    return out;
}

// ---------------------------------------------------------------------------

SmtpClient::SmtpClient(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(60 * 1000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(tr("Mail server did not answer in time")); });
    connect(&m_socket, &QSslSocket::readyRead, this, &SmtpClient::onReadyRead);
    connect(&m_socket, &QSslSocket::encrypted, this, [this] {
        if (m_state == State::StartTls) {
            m_state = State::EhloAfterTls;
            sendLine("EHLO localhost");
        }
    });
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_state != State::Done && m_state != State::Quit)
            fail(m_socket.errorString());
    });
}

SmtpClient::~SmtpClient()
{
    m_socket.disconnect(this);
    m_socket.abort();
}

void SmtpClient::send(const QString &host, quint16 port, SmtpSecurity security,
                      const QString &user, const QString &secret, bool useOAuth,
                      const OutgoingMail &mail)
{
    m_security = security;
    m_user = user;
    m_secret = secret;
    m_oauth = useOAuth;
    m_from = mail.fromAddress.toUtf8();
    m_recipients.clear();
    for (const QString &r : mail.to)
        m_recipients << Mime::bareAddress(r);
    m_rcptIndex = 0;

    // Normalise line endings and dot-stuff (RFC 5321 4.5.2).
    QByteArray payload = mail.toRfc5322();
    QByteArray stuffed;
    for (const QByteArray &line : payload.split('\n')) {
        QByteArray l = line;
        if (l.endsWith('\r'))
            l.chop(1);
        if (l.startsWith('.'))
            l.prepend('.');
        stuffed += l + "\r\n";
    }
    m_payload = stuffed;

    m_buffer.clear();
    m_replyText.clear();
    m_state = State::Greeting;
    m_timeout.start();
    if (security == SmtpSecurity::SslTls)
        m_socket.connectToHostEncrypted(host, port);
    else
        m_socket.connectToHost(host, port);
}

void SmtpClient::sendLine(const QByteArray &line)
{
    m_socket.write(line + "\r\n");
    m_timeout.start();
}

void SmtpClient::fail(const QString &message)
{
    if (m_state == State::Done)
        return;
    m_state = State::Done;
    m_timeout.stop();
    m_secret.clear();
    m_socket.abort();
    emit finished(false, message);
}

void SmtpClient::onReadyRead()
{
    m_buffer += m_socket.readAll();
    for (;;) {
        const qsizetype eol = m_buffer.indexOf("\r\n");
        if (eol < 0)
            return;
        const QByteArray line = m_buffer.left(eol);
        m_buffer.remove(0, eol + 2);
        if (line.size() < 3)
            continue;
        m_replyText += line.mid(4) + ' ';
        // "250-..." continues a multi-line reply, "250 ..." ends it.
        if (line.size() > 3 && line.at(3) == '-')
            continue;
        const int code = line.left(3).toInt();
        const QByteArray text = m_replyText.trimmed();
        m_replyText.clear();
        onReply(code, text);
        if (m_state == State::Done)
            return;
    }
}

void SmtpClient::sendAuth()
{
    m_state = State::Auth;
    if (m_oauth) {
        const QByteArray sasl = "user=" + m_user.toUtf8() + "\x01" "auth=Bearer " + m_secret.toUtf8() + "\x01\x01";
        sendLine("AUTH XOAUTH2 " + sasl.toBase64());
    } else {
        const QByteArray sasl = QByteArray(1, '\0') + m_user.toUtf8() + QByteArray(1, '\0') + m_secret.toUtf8();
        sendLine("AUTH PLAIN " + sasl.toBase64());
    }
}

void SmtpClient::onReply(int code, const QByteArray &text)
{
    const QString serverText = QString::fromUtf8(text);
    switch (m_state) {
    case State::Greeting:
        if (code != 220)
            return fail(tr("Mail server refused connection: %1").arg(serverText));
        m_state = State::Ehlo;
        sendLine("EHLO localhost");
        break;

    case State::Ehlo:
        if (code != 250)
            return fail(tr("EHLO rejected: %1").arg(serverText));
        if (m_security == SmtpSecurity::StartTls) {
            if (!text.toUpper().contains("STARTTLS"))
                return fail(tr("Server does not offer STARTTLS"));
            m_state = State::StartTls;
            sendLine("STARTTLS");
        } else {
            sendAuth();
        }
        break;

    case State::StartTls:
        if (code != 220)
            return fail(tr("STARTTLS rejected: %1").arg(serverText));
        m_socket.startClientEncryption();   // continues in the encrypted() handler
        break;

    case State::EhloAfterTls:
        if (code != 250)
            return fail(tr("EHLO rejected: %1").arg(serverText));
        sendAuth();
        break;

    case State::Auth:
        if (code == 334) {
            // XOAUTH2 error challenge: send an empty line, server then replies 5xx.
            m_state = State::AuthCancel;
            sendLine("");
            break;
        }
        if (code != 235)
            return fail(tr("Login failed: %1").arg(serverText));
        m_secret.clear();
        m_state = State::MailFrom;
        sendLine("MAIL FROM:<" + m_from + '>');
        break;

    case State::AuthCancel:
        return fail(tr("Login failed: %1").arg(serverText));

    case State::MailFrom:
        if (code != 250)
            return fail(tr("Sender rejected: %1").arg(serverText));
        m_state = State::RcptTo;
        sendLine("RCPT TO:<" + m_recipients.at(m_rcptIndex).toUtf8() + '>');
        break;

    case State::RcptTo:
        if (code != 250 && code != 251)
            return fail(tr("Recipient %1 rejected: %2").arg(m_recipients.at(m_rcptIndex), serverText));
        if (++m_rcptIndex < m_recipients.size()) {
            sendLine("RCPT TO:<" + m_recipients.at(m_rcptIndex).toUtf8() + '>');
        } else {
            m_state = State::Data;
            sendLine("DATA");
        }
        break;

    case State::Data:
        if (code != 354)
            return fail(tr("DATA rejected: %1").arg(serverText));
        m_state = State::Body;
        m_socket.write(m_payload);
        sendLine(".");
        break;

    case State::Body:
        if (code != 250)
            return fail(tr("Message rejected: %1").arg(serverText));
        m_state = State::Quit;
        sendLine("QUIT");
        m_timeout.stop();
        emit finished(true, tr("Message sent"));
        break;

    case State::Quit:
        m_state = State::Done;
        m_socket.disconnectFromHost();
        break;

    case State::Done:
        break;
    }
}
