#pragma once

#include "core/MailSession.h"
#include "core/Account.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>

class AccountStore;
class ComposeScreen;
class InboxScreen;
class LoginScreen;
class MailboxScreen;
class QStackedWidget;
class ReaderScreen;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Used by --guest and the screenshot helper.
    void signOnAsGuest();

    // Walks through the guest screens and saves a PNG of each (for docs).
    void runScreenshotTour(const QString &dir);

private:
    void buildMenus();
    void signOn(const QList<Account> &accounts);
    void signOff();
    void addAccount();
    void removeAccount(const QString &id);
    void openInbox(MailSession *session);
    void openMessage(quint32 uid);
    void compose(MailSession *from);
    void showMailboxes();

    AccountStore *m_store;
    QStackedWidget *m_stack;
    LoginScreen *m_login;
    MailboxScreen *m_mailboxes;
    InboxScreen *m_inbox;
    ReaderScreen *m_reader;
    ComposeScreen *m_compose;

    QList<MailSession *> m_sessions;
    QPointer<MailSession> m_current;
    QWidget *m_composeReturn = nullptr;
};
