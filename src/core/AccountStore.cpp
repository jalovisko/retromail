#include "core/AccountStore.h"

#include <QSettings>
#include <QUuid>

AccountStore::AccountStore(QObject *parent)
    : QObject(parent)
{
    load();
}

Account AccountStore::account(const QString &id) const
{
    for (const auto &a : m_accounts) {
        if (a.id == id)
            return a;
    }
    return {};
}

void AccountStore::add(const Account &account)
{
    for (auto &a : m_accounts) {
        if (a.id == account.id) {
            a = account;
            save();
            return;
        }
    }
    m_accounts.append(account);
    save();
}

void AccountStore::remove(const QString &id)
{
    m_accounts.removeIf([&](const Account &a) { return a.id == id; });
    save();
}

QString AccountStore::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void AccountStore::load()
{
    QSettings settings;
    const int count = settings.beginReadArray(QStringLiteral("accounts"));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        Account a;
        a.id = settings.value("id").toString();
        a.provider = providerFromKey(settings.value("provider").toString());
        a.auth = settings.value("auth").toString() == QLatin1String("oauth")
                     ? AuthMethod::OAuth : AuthMethod::Password;
        a.displayName = settings.value("displayName").toString();
        a.email = settings.value("email").toString();
        a.username = settings.value("username").toString();
        a.imapHost = settings.value("imapHost").toString();
        a.imapPort = settings.value("imapPort", 993).toInt();
        a.smtpHost = settings.value("smtpHost").toString();
        a.smtpPort = settings.value("smtpPort", 465).toInt();
        a.smtpSecurity = settings.value("smtpSecurity").toString() == QLatin1String("starttls")
                             ? SmtpSecurity::StartTls : SmtpSecurity::SslTls;
        if (!a.id.isEmpty())
            m_accounts.append(a);
    }
    settings.endArray();
}

void AccountStore::save()
{
    QSettings settings;
    settings.remove(QStringLiteral("accounts"));
    settings.beginWriteArray(QStringLiteral("accounts"), int(m_accounts.size()));
    for (int i = 0; i < m_accounts.size(); ++i) {
        const Account &a = m_accounts.at(i);
        settings.setArrayIndex(i);
        settings.setValue("id", a.id);
        settings.setValue("provider", providerKey(a.provider));
        settings.setValue("auth", a.auth == AuthMethod::OAuth ? "oauth" : "password");
        settings.setValue("displayName", a.displayName);
        settings.setValue("email", a.email);
        settings.setValue("username", a.username);
        settings.setValue("imapHost", a.imapHost);
        settings.setValue("imapPort", a.imapPort);
        settings.setValue("smtpHost", a.smtpHost);
        settings.setValue("smtpPort", a.smtpPort);
        settings.setValue("smtpSecurity",
                          a.smtpSecurity == SmtpSecurity::StartTls ? "starttls" : "ssl");
    }
    settings.endArray();
    emit changed();
}
