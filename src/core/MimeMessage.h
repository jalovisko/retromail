#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMultiMap>
#include <QString>
#include <QStringList>

// Header block of an RFC 5322 message. Keys are lower-case.
class MailHeaders
{
public:
    static MailHeaders parse(const QByteArray &block);

    QString text(const QByteArray &name) const;     // decoded (RFC 2047)
    QByteArray raw(const QByteArray &name) const;   // undecoded, unfolded

private:
    QMultiMap<QByteArray, QByteArray> m_fields;
};

// A parsed message reduced to what a 1998 client shows: headers + plain text.
struct MimeMessage
{
    QString from;
    QString to;
    QString cc;
    QString subject;
    QDateTime date;
    QByteArray messageId;
    QByteArray references;
    QString body;
    QStringList attachments;

    static MimeMessage parse(const QByteArray &raw);
};

namespace Mime {
QString decodeWords(const QByteArray &value);            // RFC 2047 encoded-words
QByteArray encodeWord(const QString &text);              // =?UTF-8?B?...?= if needed
QDateTime parseDate(const QByteArray &value);
QString displayName(const QString &address);             // "Jane <j@x>" -> "Jane"
QString bareAddress(const QString &address);             // "Jane <j@x>" -> "j@x"
QStringList splitAddresses(const QString &list);
QString decodeCharset(const QByteArray &data, const QByteArray &charset);
}
