#include "core/MimeMessage.h"

#include <QRegularExpression>
#include <QStringDecoder>
#include <QTextDocumentFragment>

namespace {

// Splits "headers<blank line>body" and accepts both CRLF and LF.
void splitEntity(const QByteArray &raw, QByteArray *headers, QByteArray *body)
{
    qsizetype crlf = raw.indexOf("\r\n\r\n");
    qsizetype lf = raw.indexOf("\n\n");
    if (crlf >= 0 && (lf < 0 || crlf < lf)) {
        *headers = raw.left(crlf);
        *body = raw.mid(crlf + 4);
    } else if (lf >= 0) {
        *headers = raw.left(lf);
        *body = raw.mid(lf + 2);
    } else {
        *headers = raw;
        body->clear();
    }
}

struct ContentType {
    QByteArray type = "text/plain";
    QMap<QByteArray, QByteArray> params;
};

// Parses: text/plain; charset="utf-8"; format=flowed
ContentType parseContentType(const QByteArray &value)
{
    ContentType ct;
    if (value.trimmed().isEmpty())
        return ct;

    QList<QByteArray> parts;
    QByteArray current;
    bool quoted = false;
    for (char c : value) {
        if (c == '"')
            quoted = !quoted;
        if (c == ';' && !quoted) {
            parts << current;
            current.clear();
        } else {
            current += c;
        }
    }
    parts << current;

    ct.type = parts.takeFirst().trimmed().toLower();
    for (const QByteArray &p : parts) {
        const qsizetype eq = p.indexOf('=');
        if (eq < 0)
            continue;
        QByteArray key = p.left(eq).trimmed().toLower();
        QByteArray val = p.mid(eq + 1).trimmed();
        if (val.size() >= 2 && val.startsWith('"') && val.endsWith('"'))
            val = val.mid(1, val.size() - 2);
        // RFC 2231 "filename*=utf-8''name" – keep the simple case readable.
        if (key.endsWith('*')) {
            key.chop(1);
            const qsizetype q = val.indexOf("''");
            if (q >= 0)
                val = QByteArray::fromPercentEncoding(val.mid(q + 2));
        }
        ct.params.insert(key, val);
    }
    return ct;
}

QByteArray decodeQuotedPrintable(const QByteArray &in, bool headerMode = false)
{
    QByteArray out;
    out.reserve(in.size());
    for (qsizetype i = 0; i < in.size(); ++i) {
        const char c = in.at(i);
        if (headerMode && c == '_') {
            out += ' ';
        } else if (c == '=') {
            if (i + 1 < in.size() && in.at(i + 1) == '\n') {          // soft break
                i += 1;
            } else if (i + 2 < in.size() && in.at(i + 1) == '\r' && in.at(i + 2) == '\n') {
                i += 2;
            } else if (i + 2 < in.size()) {
                bool ok = false;
                const int v = in.mid(i + 1, 2).toInt(&ok, 16);
                if (ok) {
                    out += char(v);
                    i += 2;
                } else {
                    out += c;
                }
            }
        } else {
            out += c;
        }
    }
    return out;
}

QByteArray decodeTransfer(const QByteArray &body, const QByteArray &encoding)
{
    const QByteArray enc = encoding.trimmed().toLower();
    if (enc == "base64")
        return QByteArray::fromBase64(body);
    if (enc == "quoted-printable")
        return decodeQuotedPrintable(body);
    return body;
}

QString htmlToText(const QString &html)
{
    // QTextDocumentFragment ignores <style>/<script> content poorly, strip them.
    QString cleaned = html;
    static const QRegularExpression styles(
        QStringLiteral("<(style|script|head)[^>]*>.*?</\\1>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    cleaned.remove(styles);
    QString text = QTextDocumentFragment::fromHtml(cleaned).toPlainText();
    static const QRegularExpression blankRuns(QStringLiteral("\\n{3,}"));
    text.replace(blankRuns, QStringLiteral("\n\n"));
    return text.trimmed();
}

struct Extracted {
    QString plain;
    QString html;
    QStringList attachments;
};

void extract(const QByteArray &raw, Extracted *out, int depth = 0)
{
    if (depth > 20)
        return;

    QByteArray headerBlock, body;
    splitEntity(raw, &headerBlock, &body);
    const MailHeaders headers = MailHeaders::parse(headerBlock);
    const ContentType ct = parseContentType(headers.raw("content-type"));
    const QByteArray disposition = headers.raw("content-disposition").trimmed().toLower();

    if (ct.type.startsWith("multipart/")) {
        const QByteArray boundary = ct.params.value("boundary");
        if (boundary.isEmpty())
            return;
        const QByteArray delimiter = "--" + boundary;
        QList<QByteArray> parts;
        qsizetype pos = body.indexOf(delimiter);
        while (pos >= 0) {
            qsizetype start = body.indexOf('\n', pos);
            if (start < 0)
                break;
            ++start;
            const qsizetype next = body.indexOf(delimiter, start);
            if (next < 0)
                break;
            QByteArray part = body.mid(start, next - start);
            if (part.endsWith("\r\n"))
                part.chop(2);
            else if (part.endsWith('\n'))
                part.chop(1);
            parts << part;
            if (body.mid(next + delimiter.size(), 2) == "--")
                break;
            pos = next;
        }

        if (ct.type == "multipart/alternative") {
            // Prefer the plain-text alternative when there is one.
            Extracted best;
            for (const QByteArray &p : parts) {
                Extracted e;
                extract(p, &e, depth + 1);
                if (best.plain.isEmpty())
                    best.plain = e.plain;
                if (best.html.isEmpty())
                    best.html = e.html;
                best.attachments << e.attachments;
            }
            if (!best.plain.isEmpty())
                out->plain += best.plain;
            else
                out->html += best.html;
            out->attachments << best.attachments;
        } else {
            for (const QByteArray &p : parts)
                extract(p, out, depth + 1);
        }
        return;
    }

    if (ct.type == "message/rfc822") {
        const MimeMessage inner = MimeMessage::parse(decodeTransfer(body, headers.raw("content-transfer-encoding")));
        out->plain += QStringLiteral("\n---------- FORWARDED MESSAGE ----------\nFROM: %1\nSUBJECT: %2\n\n%3\n")
                          .arg(inner.from, inner.subject, inner.body);
        return;
    }

    const bool isAttachment = disposition.startsWith("attachment")
        || !(ct.type == "text/plain" || ct.type == "text/html");
    if (isAttachment) {
        QString name = Mime::decodeWords(ct.params.value("name"));
        const ContentType disp = parseContentType("x/x;" + headers.raw("content-disposition").mid(
            headers.raw("content-disposition").indexOf(';') + 1));
        if (disp.params.contains("filename"))
            name = Mime::decodeWords(disp.params.value("filename"));
        out->attachments << (name.isEmpty() ? QString::fromLatin1(ct.type) : name);
        return;
    }

    const QByteArray decoded = decodeTransfer(body, headers.raw("content-transfer-encoding"));
    const QString text = Mime::decodeCharset(decoded, ct.params.value("charset", "us-ascii"));
    if (ct.type == "text/html") {
        out->html += text;
    } else {
        if (!out->plain.isEmpty())
            out->plain += QLatin1Char('\n');
        out->plain += text;
    }
}

} // namespace

// ---------------------------------------------------------------------------

MailHeaders MailHeaders::parse(const QByteArray &block)
{
    MailHeaders h;
    QByteArray current;
    const auto flush = [&] {
        const qsizetype colon = current.indexOf(':');
        if (colon > 0)
            h.m_fields.insert(current.left(colon).trimmed().toLower(), current.mid(colon + 1).trimmed());
        current.clear();
    };
    for (QByteArray line : block.split('\n')) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (!line.isEmpty() && (line.at(0) == ' ' || line.at(0) == '\t')) {
            current += ' ' + line.trimmed();    // folded continuation
        } else {
            flush();
            current = line;
        }
    }
    flush();
    return h;
}

QByteArray MailHeaders::raw(const QByteArray &name) const
{
    return m_fields.value(name.toLower());
}

QString MailHeaders::text(const QByteArray &name) const
{
    return Mime::decodeWords(raw(name));
}

MimeMessage MimeMessage::parse(const QByteArray &raw)
{
    QByteArray headerBlock, body;
    splitEntity(raw, &headerBlock, &body);
    const MailHeaders headers = MailHeaders::parse(headerBlock);

    MimeMessage m;
    m.from = headers.text("from");
    m.to = headers.text("to");
    m.cc = headers.text("cc");
    m.subject = headers.text("subject");
    m.date = Mime::parseDate(headers.raw("date"));
    m.messageId = headers.raw("message-id");
    m.references = headers.raw("references");

    Extracted e;
    extract(raw, &e);
    m.body = !e.plain.isEmpty() ? e.plain : htmlToText(e.html);
    m.body.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    m.attachments = e.attachments;
    return m;
}

// ---------------------------------------------------------------------------

namespace Mime {

QString decodeCharset(const QByteArray &data, const QByteArray &charset)
{
    QByteArray cs = charset.trimmed().toLower();
    if (cs.isEmpty() || cs == "us-ascii" || cs == "ascii")
        cs = "utf-8";   // ASCII is a subset; tolerate mislabelled UTF-8
    QStringDecoder decoder(cs.constData());
    if (!decoder.isValid())
        decoder = QStringDecoder(QStringDecoder::Latin1);
    QString text = decoder.decode(data);
    if (decoder.hasError() && cs == "utf-8")
        text = QString::fromLatin1(data);
    return text;
}

QString decodeWords(const QByteArray &value)
{
    static const QRegularExpression word(
        QStringLiteral("=\\?([^?]+)\\?([bBqQ])\\?([^?]*)\\?="));
    static const QRegularExpression gap(QStringLiteral("(\\?=)\\s+(=\\?)"));

    QString input = QString::fromLatin1(value);
    input.replace(gap, QStringLiteral("\\1\\2"));   // whitespace between words is dropped

    QString out;
    qsizetype last = 0;
    auto it = word.globalMatch(input);
    bool any = false;
    while (it.hasNext()) {
        const auto m = it.next();
        any = true;
        out += input.mid(last, m.capturedStart() - last);
        QByteArray charset = m.captured(1).toLatin1();
        const qsizetype star = charset.indexOf('*');   // RFC 2231 language tag
        if (star >= 0)
            charset.truncate(star);
        const QByteArray payload = m.captured(3).toLatin1();
        const QByteArray bytes = m.captured(2).compare(QLatin1String("b"), Qt::CaseInsensitive) == 0
                                     ? QByteArray::fromBase64(payload)
                                     : decodeQuotedPrintable(payload, true);
        out += decodeCharset(bytes, charset);
        last = m.capturedEnd();
    }
    if (!any)
        return decodeCharset(value, "utf-8");   // raw 8-bit headers are usually UTF-8
    out += input.mid(last);
    return out;
}

QByteArray encodeWord(const QString &text)
{
    bool ascii = true;
    for (QChar c : text) {
        if (c.unicode() > 126 || (c.unicode() < 32 && c != QLatin1Char('\t'))) {
            ascii = false;
            break;
        }
    }
    if (ascii)
        return text.toLatin1();
    return "=?UTF-8?B?" + text.toUtf8().toBase64() + "?=";
}

QDateTime parseDate(const QByteArray &value)
{
    QString s = QString::fromLatin1(value).trimmed();
    static const QRegularExpression comment(QStringLiteral("\\s*\\([^)]*\\)\\s*$"));
    s.remove(comment);
    QDateTime dt = QDateTime::fromString(s, Qt::RFC2822Date);
    if (!dt.isValid()) {
        // Some servers omit the weekday or use odd spacing.
        static const QRegularExpression spaces(QStringLiteral("\\s+"));
        s.replace(spaces, QStringLiteral(" "));
        dt = QDateTime::fromString(s, Qt::RFC2822Date);
    }
    return dt;
}

QString bareAddress(const QString &address)
{
    static const QRegularExpression angle(QStringLiteral("<([^>]+)>"));
    const auto m = angle.match(address);
    return (m.hasMatch() ? m.captured(1) : address).trimmed();
}

QString displayName(const QString &address)
{
    const qsizetype lt = address.indexOf(QLatin1Char('<'));
    if (lt <= 0)
        return address.trimmed();
    QString name = address.left(lt).trimmed();
    if (name.size() >= 2 && name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"')))
        name = name.mid(1, name.size() - 2);
    return name.isEmpty() ? bareAddress(address) : name;
}

QStringList splitAddresses(const QString &list)
{
    QStringList out;
    QString current;
    bool quoted = false;
    int angle = 0;
    for (QChar c : list) {
        if (c == QLatin1Char('"'))
            quoted = !quoted;
        else if (c == QLatin1Char('<'))
            ++angle;
        else if (c == QLatin1Char('>'))
            --angle;
        if ((c == QLatin1Char(',') || c == QLatin1Char(';')) && !quoted && angle <= 0) {
            if (!current.trimmed().isEmpty())
                out << current.trimmed();
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.trimmed().isEmpty())
        out << current.trimmed();
    return out;
}

} // namespace Mime
