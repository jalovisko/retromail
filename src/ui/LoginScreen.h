#pragma once

#include "core/Account.h"
#include "ui/RetroWidgets.h"

class AccountStore;
class QLabel;
class QListWidget;
class QRadioButton;
class QStackedWidget;
class RetroButton;

// Sign-on screen: REGISTERED USER (pick accounts) or GUEST (demo mailbox).
class LoginScreen : public ScreenBackground
{
    Q_OBJECT
public:
    explicit LoginScreen(AccountStore *store, QWidget *parent = nullptr);

    void setStatus(const QString &text);
    void setBusy(bool busy);

signals:
    void signOnRequested(const QList<Account> &accounts);
    void guestRequested();
    void addAccountRequested();
    void removeAccountRequested(const QString &id);

private:
    void reloadAccounts();
    void updateMode();
    void submit();

    AccountStore *m_store;
    QRadioButton *m_registered;
    QRadioButton *m_guest;
    QStackedWidget *m_modeStack;
    QListWidget *m_list;
    RetroButton *m_add;
    RetroButton *m_remove;
    RetroButton *m_signOn;
    QLabel *m_status;
};
