#include "ui/ReaderScreen.h"

#include "ui/Theme.h"

#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QLineEdit *makeField(QWidget *parent)
{
    auto *field = new QLineEdit(parent);
    field->setObjectName(QStringLiteral("field"));
    field->setReadOnly(true);
    field->setFont(Theme::headline(20));
    field->setAlignment(Qt::AlignCenter);
    return field;
}

} // namespace

ReaderScreen::ReaderScreen(QWidget *parent)
    : ScreenBackground(parent)
{
    auto *toolbar = new RetroToolbar(this);
    auto *in = toolbar->addInButton();
    auto *reply = toolbar->addButton(tr("REPLY"));
    auto *del = toolbar->addButton(tr("DELETE"));
    auto *back = toolbar->addBackFlag();

    m_to = makeField(this);
    m_from = makeField(this);
    m_subject = makeField(this);

    m_body = new QTextEdit(this);
    m_body->setObjectName(QStringLiteral("body"));
    m_body->setReadOnly(true);
    m_body->setAcceptRichText(false);
    m_body->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    m_footer = new QLabel(this);
    m_footer->setObjectName(QStringLiteral("status"));
    m_footer->setFont(Theme::plain(14));
    m_footer->setWordWrap(true);

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
    content->addWidget(m_footer);
    layout->addLayout(content, 1);

    refreshFonts();

    connect(in, &QAbstractButton::clicked, this, &ReaderScreen::inboxRequested);
    connect(back, &QAbstractButton::clicked, this, &ReaderScreen::inboxRequested);
    connect(reply, &QAbstractButton::clicked, this, &ReaderScreen::replyRequested);
    connect(del, &QAbstractButton::clicked, this, [this] {
        if (!m_uid)
            return;
        if (QMessageBox::question(this, tr("RetroMail"), tr("Delete this message?")) == QMessageBox::Yes)
            emit deleteRequested(m_uid);
    });
}

void ReaderScreen::refreshFonts()
{
    m_body->setFont(Theme::body(30, Theme::capsDisplay()));
}

void ReaderScreen::showLoading()
{
    m_uid = 0;
    m_message = {};
    m_to->clear();
    m_from->clear();
    m_subject->clear();
    m_body->setPlainText(tr("OPENING ENVELOPE...."));
    m_footer->clear();
}

void ReaderScreen::showMessage(quint32 uid, const MimeMessage &message)
{
    m_uid = uid;
    m_message = message;
    m_to->setText(Mime::bareAddress(Mime::splitAddresses(message.to).value(0)));
    m_to->setToolTip(message.to + (message.cc.isEmpty() ? QString() : QStringLiteral("\nCC: ") + message.cc));
    m_from->setText(Mime::bareAddress(message.from));
    m_from->setToolTip(message.from);
    m_subject->setText(message.subject);
    m_subject->setCursorPosition(0);
    m_body->setPlainText(message.body.trimmed());

    QStringList footer;
    if (message.date.isValid())
        footer << tr("RECEIVED %1").arg(message.date.toLocalTime().toString(QStringLiteral("MM/dd/yyyy hh:mm AP")));
    if (!message.attachments.isEmpty())
        footer << tr("ATTACHMENTS (NOT SHOWN): %1").arg(message.attachments.join(QStringLiteral(", ")));
    m_footer->setText(footer.join(QStringLiteral("   ·   ")));
    m_body->setFocus();
}

void ReaderScreen::showError(const QString &error)
{
    m_body->setPlainText(tr("COULD NOT OPEN MESSAGE.\n\n%1").arg(error));
}

void ReaderScreen::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace) {
        emit inboxRequested();
        return;
    }
    if (e->key() == Qt::Key_Delete && m_uid) {
        if (QMessageBox::question(this, tr("RetroMail"), tr("Delete this message?")) == QMessageBox::Yes)
            emit deleteRequested(m_uid);
        return;
    }
    ScreenBackground::keyPressEvent(e);
}
