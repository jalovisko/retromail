#include "core/MimeMessage.h"
#include "net/SmtpClient.h"

#include <QTest>

class TestMime : public QObject
{
    Q_OBJECT

private slots:
    void encodedWords()
    {
        QCOMPARE(Mime::decodeWords("=?UTF-8?B?w4lsw6huZQ==?= <e@x.org>"), QStringLiteral("Élène <e@x.org>"));
        QCOMPARE(Mime::decodeWords("=?iso-8859-1?Q?Caf=E9_cr=E8me?="), QStringLiteral("Café crème"));
        // Whitespace between adjacent encoded words is dropped (RFC 2047 6.2).
        QCOMPARE(Mime::decodeWords("=?UTF-8?Q?Hello?= =?UTF-8?Q?_World?="), QStringLiteral("Hello World"));
        QCOMPARE(Mime::decodeWords("Plain subject"), QStringLiteral("Plain subject"));
    }

    void addresses()
    {
        QCOMPARE(Mime::bareAddress(QStringLiteral("\"Doe, Jane\" <jane@x.org>")), QStringLiteral("jane@x.org"));
        QCOMPARE(Mime::displayName(QStringLiteral("\"Doe, Jane\" <jane@x.org>")), QStringLiteral("Doe, Jane"));
        QCOMPARE(Mime::splitAddresses(QStringLiteral("\"Doe, Jane\" <jane@x.org>, bob@y.org; c@z.org")).size(), 3);
    }

    void dates()
    {
        const QDateTime dt = Mime::parseDate("Mon, 12 Oct 1998 09:14:00 -0500 (EST)");
        QVERIFY(dt.isValid());
        QCOMPARE(dt.toUTC().time().hour(), 14);
    }

    void multipartAlternativePrefersPlain()
    {
        const QByteArray raw =
            "From: A <a@x.org>\r\n"
            "Subject: =?UTF-8?B?VGVzdCDinJM=?=\r\n"
            "Content-Type: multipart/alternative; boundary=\"b1\"\r\n"
            "\r\n"
            "--b1\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n"
            "Content-Transfer-Encoding: quoted-printable\r\n"
            "\r\n"
            "Caf=C3=A9 at nine=\r\n"
            " sharp.\r\n"
            "--b1\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<p>HTML version</p>\r\n"
            "--b1--\r\n";
        const MimeMessage m = MimeMessage::parse(raw);
        QCOMPARE(m.subject, QStringLiteral("Test ✓"));
        QCOMPARE(m.body.trimmed(), QStringLiteral("Café at nine sharp."));
    }

    void htmlOnlyAndAttachments()
    {
        const QByteArray raw =
            "From: a@x.org\n"
            "Content-Type: multipart/mixed; boundary=XX\n"
            "\n"
            "--XX\n"
            "Content-Type: text/html; charset=us-ascii\n"
            "\n"
            "<html><head><style>p{color:red}</style></head><body><p>Hi <b>there</b></p></body></html>\n"
            "--XX\n"
            "Content-Type: application/pdf; name=\"report.pdf\"\n"
            "Content-Disposition: attachment; filename=\"report.pdf\"\n"
            "Content-Transfer-Encoding: base64\n"
            "\n"
            "JVBERi0=\n"
            "--XX--\n";
        const MimeMessage m = MimeMessage::parse(raw);
        QCOMPARE(m.body, QStringLiteral("Hi there"));
        QCOMPARE(m.attachments, QStringList{QStringLiteral("report.pdf")});
    }

    void base64Body()
    {
        const QByteArray raw =
            "Subject: x\r\n"
            "Content-Type: text/plain; charset=UTF-8\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "\r\n"
            + QByteArray("Привет, 1998!").toBase64() + "\r\n";
        QCOMPARE(MimeMessage::parse(raw).body.trimmed(), QStringLiteral("Привет, 1998!"));
    }

    void outgoingRoundTrip()
    {
        OutgoingMail mail;
        mail.fromName = QStringLiteral("Zoë");
        mail.fromAddress = QStringLiteral("zoe@example.org");
        mail.to = {QStringLiteral("Bob <bob@example.com>")};
        mail.subject = QStringLiteral("Grüße");
        mail.body = QStringLiteral("Line one\n.dot line\nLine three");
        const QByteArray wire = mail.toRfc5322();

        QVERIFY(wire.contains("Content-Transfer-Encoding: base64\r\n"));
        const MimeMessage parsed = MimeMessage::parse(wire);
        QCOMPARE(parsed.subject, mail.subject);
        QCOMPARE(Mime::displayName(parsed.from), mail.fromName);
        QCOMPARE(Mime::bareAddress(parsed.to), QStringLiteral("bob@example.com"));
        QCOMPARE(parsed.body.trimmed(), mail.body);
    }
};

QTEST_MAIN(TestMime)
#include "tst_mime.moc"
