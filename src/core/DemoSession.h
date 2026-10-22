#pragma once

#include "core/MailSession.h"

#include <QMap>

// Guest mode: an offline mailbox with a few canned messages, so the client
// can be tried (and screenshotted) without any account.
class DemoSession : public MailSession
{
    Q_OBJECT
public:
    explicit DemoSession(QObject *parent = nullptr);

    Account account() const override;
    bool isGuest() const override { return true; }

    void start() override;
    void refresh() override;
    void fetchMessage(quint32 uid) override;
    void deleteMessage(quint32 uid) override;
    void sendMail(const OutgoingMail &mail) override;

private:
    QMap<quint32, MimeMessage> m_messages;
    QList<MessageHeader> m_box;
};
