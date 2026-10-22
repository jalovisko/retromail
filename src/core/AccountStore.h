#pragma once

#include "core/Account.h"

#include <QList>
#include <QObject>

// Saves account settings (never passwords) with QSettings.
// Passwords and OAuth refresh tokens live in the OS keychain, see SecretStore.
class AccountStore : public QObject
{
    Q_OBJECT
public:
    explicit AccountStore(QObject *parent = nullptr);

    const QList<Account> &accounts() const { return m_accounts; }
    Account account(const QString &id) const;

    void add(const Account &account);
    void remove(const QString &id);

    static QString newId();

signals:
    void changed();

private:
    void load();
    void save();

    QList<Account> m_accounts;
};
