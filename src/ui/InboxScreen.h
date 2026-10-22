#pragma once

#include "core/MailSession.h"
#include "ui/RetroWidgets.h"

#include <QPointer>

class QLabel;
class QListWidget;

// List of envelopes in one account's INBOX.
class InboxScreen : public ScreenBackground
{
    Q_OBJECT
public:
    explicit InboxScreen(QWidget *parent = nullptr);

    void setSession(MailSession *session);
    MailSession *session() const { return m_session; }

signals:
    void messageOpened(quint32 uid);
    void composeRequested();
    void backRequested();

protected:
    void keyPressEvent(QKeyEvent *) override;

private:
    void reload();
    void deleteCurrent();

    QPointer<MailSession> m_session;
    QLabel *m_title;
    QLabel *m_status;
    QListWidget *m_list;
};
