#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QUrl>

enum class Provider { Gmail, Outlook, Yahoo, ICloud, Fastmail, Other };
enum class AuthMethod { Password, OAuth };
enum class SmtpSecurity { SslTls, StartTls };

// Everything needed to talk OAuth2 to one provider.
struct OAuthConfig {
    QUrl authorizationUrl;
    QUrl tokenUrl;
    QSet<QByteArray> scopes;
    QString clientId;
    QString clientSecret;   // Google "Desktop app" clients have a non-secret secret
    QString callbackHost;   // host used in the loopback redirect URI

    bool isConfigured() const { return !clientId.isEmpty(); }
};

// Default server settings for a mail provider.
struct ProviderPreset {
    Provider provider;
    QString label;
    QString imapHost;
    int imapPort = 993;
    QString smtpHost;
    int smtpPort = 465;
    SmtpSecurity smtpSecurity = SmtpSecurity::SslTls;
    bool supportsOAuth = false;
    bool supportsPassword = true;
    QString passwordHint;   // shown in the add-account dialog

    static QList<ProviderPreset> all();
    static ProviderPreset forProvider(Provider p);
};

struct Account {
    QString id;
    Provider provider = Provider::Other;
    AuthMethod auth = AuthMethod::Password;
    QString displayName;
    QString email;
    QString username;       // login name, usually the email address
    QString imapHost;
    int imapPort = 993;
    QString smtpHost;
    int smtpPort = 465;
    SmtpSecurity smtpSecurity = SmtpSecurity::SslTls;

    QString label() const { return displayName.isEmpty() ? email : displayName; }
};

QString providerKey(Provider p);
Provider providerFromKey(const QString &key);

// OAuth settings for Gmail / Outlook. Client IDs come from the environment
// first, then from the values baked in at build time.
OAuthConfig oauthConfigFor(Provider p);
