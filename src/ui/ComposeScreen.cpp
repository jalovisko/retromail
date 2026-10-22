#include "ui/ComposeScreen.h"

#include "core/MailSession.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

ComposeScreen::ComposeScreen(QWidget *parent)
    : ScreenBackground(parent)
{
    auto *toolbar = new RetroToolbar(this);
    auto *in = toolbar->addInButton();
    m_send = toolbar->addButton(tr("SEND"));
    auto *del = toolbar->addButton(tr("DELETE"));
    auto *back = toolbar->addBackFlag();

    m_to = new QLineEdit(this);
    m_to->setObjectName(QStringLiteral("field"));
    m_to->setFont(Theme::headline(20));
    m_to->setAlignment(Qt::AlignCenter);
    m_to->setPlaceholderText(tr("name@example.com"));

    m_from = new QComboBox(this);
    m_from->setObjectName(QStringLiteral("field"));
    m_from->setFont(Theme::headline(20));

    m_subject = new QLineEdit(this);
    m_subject->setObjectName(QStringLiteral("field"));
    m_subject->setFont(Theme::headline(20));
    m_subject->setAlignment(Qt::AlignCenter);

    m_body = new QPlainTextEdit(this);
    m_body->setObjectName(QStringLiteral("body"));

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setFont(Theme::plain(15));

    auto *fields = new QGridLayout;
    fields->setHorizontalSpacing(0);
    fields->setVerticalSpacing(10);
    fields->addWidget(makeSlab(tr("Message To:"), this), 0, 0);
    fields->addWidget(m_to, 0, 1);
    fields->addWidget(makeSlab(tr("Message From:"), this), 1, 0);
    fields->addWidget(m_from, 1, 1);
    fields->addWidget(makeSlab(tr("Subject:"), this), 2, 0);
    fields->addWidget(m_subject, 2, 1);
    fields->setColumnStretch(1, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    auto *content = new QVBoxLayout;
    content->setContentsMargins(20, 18, 20, 12);
    content->setSpacing(12);
    content->addLayout(fields);
    content->addWidget(m_body, 1);
    content->addWidget(m_status);
    layout->addLayout(content, 1);

    refreshFonts();

    connect(in, &QAbstractButton::clicked, this, &ComposeScreen::discard);
    connect(back, &QAbstractButton::clicked, this, &ComposeScreen::discard);
    connect(del, &QAbstractButton::clicked, this, &ComposeScreen::discard);
    connect(m_send, &QAbstractButton::clicked, this, &ComposeScreen::send);
}

void ComposeScreen::refreshFonts()
{
    m_body->setFont(Theme::body(28, Theme::capsDisplay()));
}

void ComposeScreen::setSessions(const QList<MailSession *> &sessions)
{
    m_sessions.clear();
    m_from->clear();
    for (MailSession *s : sessions) {
        m_sessions << s;
        m_from->addItem(s->account().email);
    }
}

MailSession *ComposeScreen::selectedSession() const
{
    const int i = m_from->currentIndex();
    return (i >= 0 && i < m_sessions.size()) ? m_sessions.at(i).data() : nullptr;
}

void ComposeScreen::startNew(MailSession *from)
{
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    m_status->clear();
    m_inReplyTo.clear();
    m_references.clear();
    m_send->setEnabled(true);
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i) == from)
            m_from->setCurrentIndex(i);
    }
    m_to->setFocus();
}

void ComposeScreen::startReply(MailSession *from, const MimeMessage &original)
{
    startNew(from);
    m_to->setText(Mime::bareAddress(original.from));
    QString subject = original.subject;
    if (!subject.startsWith(QLatin1String("re:"), Qt::CaseInsensitive))
        subject.prepend(QStringLiteral("Re: "));
    m_subject->setText(subject);

    QString quoted;
    for (const QString &line : original.body.split(QLatin1Char('\n')))
        quoted += QStringLiteral("> ") + line + QLatin1Char('\n');
    m_body->setPlainText(QStringLiteral("\n\n%1 wrote:\n%2").arg(Mime::displayName(original.from), quoted));
    m_inReplyTo = original.messageId;
    m_references = original.references;
    m_body->moveCursor(QTextCursor::Start);
    m_body->setFocus();
}

void ComposeScreen::send()
{
    MailSession *session = selectedSession();
    const QStringList to = Mime::splitAddresses(m_to->text());
    if (!session || to.isEmpty()) {
        QMessageBox::warning(this, tr("RetroMail"), tr("Who is this message for?\nPlease fill in Message To."));
        m_to->setFocus();
        return;
    }
    for (const QString &r : to) {
        if (!Mime::bareAddress(r).contains(QLatin1Char('@'))) {
            QMessageBox::warning(this, tr("RetroMail"), tr("\"%1\" is not an e-mail address.").arg(r));
            m_to->setFocus();
            return;
        }
    }

    OutgoingMail mail;
    const Account account = session->account();
    mail.fromName = account.displayName;
    mail.fromAddress = account.email;
    mail.to = to;
    mail.subject = m_subject->text();
    mail.body = m_body->toPlainText();
    mail.inReplyTo = m_inReplyTo;
    mail.references = m_references;

    m_send->setEnabled(false);
    m_status->setText(tr("SENDING...."));
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(session, &MailSession::sendFinished, this,
                          [this, connection](bool ok, const QString &message) {
        disconnect(*connection);
        m_send->setEnabled(true);
        if (ok) {
            m_status->clear();
            QMessageBox::information(this, tr("RetroMail"), tr("YOUR MESSAGE HAS BEEN SENT."));
            emit closed();
        } else {
            m_status->setText(tr("NOT SENT."));
            QMessageBox::warning(this, tr("RetroMail"), message);
        }
    });
    session->sendMail(mail);
}

void ComposeScreen::discard()
{
    const bool dirty = m_body->document()->isModified() || m_subject->isModified();
    if (dirty && QMessageBox::question(this, tr("RetroMail"), tr("Throw away this message?")) != QMessageBox::Yes)
        return;
    emit closed();
}
