#pragma once

#include "core/MailSession.h"
#include "core/MimeMessage.h"
#include "ui/RetroWidgets.h"

#include <QList>
#include <QPointer>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class RetroButton;

// Write a message. Layout mirrors the reader: To / From slabs and a body.
class ComposeScreen : public ScreenBackground
{
    Q_OBJECT
public:
    explicit ComposeScreen(QWidget *parent = nullptr);

    void setSessions(const QList<MailSession *> &sessions);
    void startNew(MailSession *from);
    void startReply(MailSession *from, const MimeMessage &original);
    void refreshFonts();

signals:
    void closed();

private:
    void send();
    void discard();
    MailSession *selectedSession() const;

    QList<QPointer<MailSession>> m_sessions;
    QLineEdit *m_to;
    QComboBox *m_from;
    QLineEdit *m_subject;
    QPlainTextEdit *m_body;
    QLabel *m_status;
    RetroButton *m_send;
    QByteArray m_inReplyTo;
    QByteArray m_references;
};
