#include "core/DemoSession.h"

#include <QTimer>

namespace {

struct Canned {
    const char *from;
    const char *subject;
    const char *date;
    bool seen;
    const char *body;
};

const Canned kCanned[] = {
    {"Sysop <sysop@retromail.local>", "WELCOME TO RETROMAIL", "Mon, 12 Oct 1998 09:14:00 -0500", false,
     "Welcome, guest.\n\n"
     "This is a demonstration mailbox. Nothing here leaves your computer.\n\n"
     "Click a message to read it. Press WRITE to compose. The red flag takes you back.\n\n"
     "To read your own mail, sign off and choose REGISTERED USER."},
    {"Night Owl <nightowl@bbs.example>", "Meet on IRC tonight", "Sun, 11 Oct 1998 23:02:00 -0500", false,
     "Channel #retromail, 23:00 sharp.\n\n"
     "Bring your 28.8 modem and a sense of adventure. Don't let your mom pick up the phone."},
    {"Y2K Task Force <y2k@example.com>", "Is your computer ready for the year 2000?", "Fri, 09 Oct 1998 16:45:00 -0500", false,
     "Only 448 days remain.\n\n"
     "Check your BIOS. Back up to floppy. Stock up on canned goods.\n\n"
     "This message will self-destruct on 01/01/00."},
    {"Mom <mom@example.net>", "Did you get the floppy disk?", "Wed, 07 Oct 1998 12:30:00 -0500", true,
     "I mailed you the photos from the reunion on a 3.5 inch floppy.\n"
     "Your father says it holds 1.44 megabytes, which sounds like a lot.\n\nLove, Mom"},
    {"Webmaster <webmaster@homepage.example>", "Your hit counter reached 1,000!", "Mon, 05 Oct 1998 08:00:00 -0500", true,
     "Congratulations! Your homepage has been visited 1,000 times.\n\n"
     "Consider adding a guestbook and an animated UNDER CONSTRUCTION sign."},
};

} // namespace

DemoSession::DemoSession(QObject *parent)
    : MailSession(parent)
{
    quint32 uid = 100;
    for (const Canned &c : kCanned) {
        MimeMessage m;
        m.from = QString::fromUtf8(c.from);
        m.to = QStringLiteral("Guest <guest@retromail.local>");
        m.subject = QString::fromUtf8(c.subject);
        m.date = Mime::parseDate(c.date);
        m.body = QString::fromUtf8(c.body);
        m.messageId = "<demo" + QByteArray::number(uid) + "@retromail.local>";

        MessageHeader h;
        h.uid = uid;
        h.from = m.from;
        h.to = m.to;
        h.subject = m.subject;
        h.date = m.date;
        h.seen = c.seen;

        m_messages.insert(uid, m);
        m_box << h;
        --uid;
    }
}

Account DemoSession::account() const
{
    Account a;
    a.id = QStringLiteral("guest");
    a.displayName = QStringLiteral("GUEST");
    a.email = QStringLiteral("guest@retromail.local");
    return a;
}

void DemoSession::start()
{
    setState(State::Busy, tr("DIALING..."));
    QTimer::singleShot(900, this, [this] {
        setHeaders(m_box);
        setState(State::Ready);
    });
}

void DemoSession::refresh()
{
    setHeaders(m_box);
    setState(State::Ready);
}

void DemoSession::fetchMessage(quint32 uid)
{
    QTimer::singleShot(150, this, [this, uid] {
        if (!m_messages.contains(uid)) {
            emit messageFailed(uid, tr("Message is gone"));
            return;
        }
        for (auto &h : m_box) {
            if (h.uid == uid)
                h.seen = true;
        }
        markSeen(uid);
        emit messageLoaded(uid, m_messages.value(uid));
    });
}

void DemoSession::deleteMessage(quint32 uid)
{
    m_messages.remove(uid);
    m_box.removeIf([uid](const MessageHeader &h) { return h.uid == uid; });
    removeHeader(uid);
}

void DemoSession::sendMail(const OutgoingMail &)
{
    QTimer::singleShot(400, this, [this] {
        emit sendFinished(false, tr("GUESTS CANNOT SEND MAIL. PLEASE SIGN ON AS A REGISTERED USER."));
    });
}
