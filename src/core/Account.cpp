#include "core/Account.h"

#include "BuildConfig.h"

#include <QtGlobal>

QList<ProviderPreset> ProviderPreset::all()
{
    QList<ProviderPreset> list;

    ProviderPreset gmail{Provider::Gmail, QStringLiteral("GMAIL")};
    gmail.imapHost = QStringLiteral("imap.gmail.com");
    gmail.smtpHost = QStringLiteral("smtp.gmail.com");
    gmail.smtpPort = 465;
    gmail.supportsOAuth = true;
    gmail.passwordHint = QStringLiteral(
        "Use a Google app password (needs 2-step verification).");
    list << gmail;

    ProviderPreset outlook{Provider::Outlook, QStringLiteral("OUTLOOK / HOTMAIL / MICROSOFT 365")};
    outlook.imapHost = QStringLiteral("outlook.office365.com");
    outlook.smtpHost = QStringLiteral("smtp-mail.outlook.com");
    outlook.smtpPort = 587;
    outlook.smtpSecurity = SmtpSecurity::StartTls;
    outlook.supportsOAuth = true;
    outlook.supportsPassword = false;   // Microsoft turned off password logins
    list << outlook;

    ProviderPreset yahoo{Provider::Yahoo, QStringLiteral("YAHOO! MAIL")};
    yahoo.imapHost = QStringLiteral("imap.mail.yahoo.com");
    yahoo.smtpHost = QStringLiteral("smtp.mail.yahoo.com");
    yahoo.passwordHint = QStringLiteral("Use a Yahoo app password.");
    list << yahoo;

    ProviderPreset icloud{Provider::ICloud, QStringLiteral("ICLOUD")};
    icloud.imapHost = QStringLiteral("imap.mail.me.com");
    icloud.smtpHost = QStringLiteral("smtp.mail.me.com");
    icloud.smtpPort = 587;
    icloud.smtpSecurity = SmtpSecurity::StartTls;
    icloud.passwordHint = QStringLiteral("Use an app-specific password from appleid.apple.com.");
    list << icloud;

    ProviderPreset fastmail{Provider::Fastmail, QStringLiteral("FASTMAIL")};
    fastmail.imapHost = QStringLiteral("imap.fastmail.com");
    fastmail.smtpHost = QStringLiteral("smtp.fastmail.com");
    fastmail.passwordHint = QStringLiteral("Use a Fastmail app password.");
    list << fastmail;

    ProviderPreset other{Provider::Other, QStringLiteral("OTHER IMAP SERVER")};
    list << other;

    return list;
}

ProviderPreset ProviderPreset::forProvider(Provider p)
{
    const auto presets = all();
    for (const auto &preset : presets) {
        if (preset.provider == p)
            return preset;
    }
    return presets.last();
}

QString providerKey(Provider p)
{
    switch (p) {
    case Provider::Gmail: return QStringLiteral("gmail");
    case Provider::Outlook: return QStringLiteral("outlook");
    case Provider::Yahoo: return QStringLiteral("yahoo");
    case Provider::ICloud: return QStringLiteral("icloud");
    case Provider::Fastmail: return QStringLiteral("fastmail");
    case Provider::Other: break;
    }
    return QStringLiteral("other");
}

Provider providerFromKey(const QString &key)
{
    for (const auto &preset : ProviderPreset::all()) {
        if (providerKey(preset.provider) == key)
            return preset.provider;
    }
    return Provider::Other;
}

static QString envOr(const char *name, const char *fallback)
{
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? QString::fromUtf8(fallback) : value;
}

OAuthConfig oauthConfigFor(Provider p)
{
    OAuthConfig cfg;
    if (p == Provider::Gmail) {
        cfg.authorizationUrl = QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
        cfg.tokenUrl = QUrl(QStringLiteral("https://oauth2.googleapis.com/token"));
        cfg.scopes = {QByteArrayLiteral("https://mail.google.com/")};
        cfg.clientId = envOr("RETROMAIL_GOOGLE_CLIENT_ID", RETROMAIL_GOOGLE_CLIENT_ID);
        cfg.clientSecret = envOr("RETROMAIL_GOOGLE_CLIENT_SECRET", RETROMAIL_GOOGLE_CLIENT_SECRET);
        cfg.callbackHost = QStringLiteral("127.0.0.1");
    } else if (p == Provider::Outlook) {
        cfg.authorizationUrl = QUrl(QStringLiteral(
            "https://login.microsoftonline.com/common/oauth2/v2.0/authorize"));
        cfg.tokenUrl = QUrl(QStringLiteral(
            "https://login.microsoftonline.com/common/oauth2/v2.0/token"));
        cfg.scopes = {QByteArrayLiteral("https://outlook.office.com/IMAP.AccessAsUser.All"),
                      QByteArrayLiteral("https://outlook.office.com/SMTP.Send"),
                      QByteArrayLiteral("offline_access")};
        cfg.clientId = envOr("RETROMAIL_MICROSOFT_CLIENT_ID", RETROMAIL_MICROSOFT_CLIENT_ID);
        cfg.callbackHost = QStringLiteral("localhost");
    }
    return cfg;
}
