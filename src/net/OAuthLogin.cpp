#include "net/OAuthLogin.h"

#include <QDesktopServices>
#include <QHostAddress>
#include <QOAuthHttpServerReplyHandler>
#include <QOAuthOobReplyHandler>

OAuthLogin::OAuthLogin(Provider provider, const QString &loginHint, QObject *parent)
    : QObject(parent)
    , m_config(oauthConfigFor(provider))
    , m_loginHint(loginHint)
{
    m_flow.setAuthorizationUrl(m_config.authorizationUrl);
    m_flow.setTokenUrl(m_config.tokenUrl);
    m_flow.setClientIdentifier(m_config.clientId);
    if (!m_config.clientSecret.isEmpty())
        m_flow.setClientIdentifierSharedKey(m_config.clientSecret);
    m_flow.setRequestedScopeTokens(m_config.scopes);

    const bool isGoogle = provider == Provider::Gmail;
    m_flow.setModifyParametersFunction(
        [this, isGoogle](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant> *params) {
            if (stage != QAbstractOAuth::Stage::RequestingAuthorization)
                return;
            if (!m_loginHint.isEmpty())
                params->replace(QStringLiteral("login_hint"), m_loginHint);
            if (isGoogle) {
                // Ask for a refresh token every time, so re-adding works.
                params->replace(QStringLiteral("access_type"), QStringLiteral("offline"));
                params->replace(QStringLiteral("prompt"), QStringLiteral("consent"));
            }
        });

    connect(&m_flow, &QAbstractOAuth::authorizeWithBrowser, this, &QDesktopServices::openUrl);
    connect(&m_flow, &QAbstractOAuth::granted, this, [this] { finish(true); });
    connect(&m_flow, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error error) {
        finish(false, tr("Sign-in request failed (error %1)").arg(int(error)));
    });
    connect(&m_flow, &QAbstractOAuth2::serverReportedErrorOccurred, this,
            [this](const QString &error, const QString &description, const QUrl &) {
                finish(false, description.isEmpty() ? error : description);
            });

    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish(false, tr("Sign-in timed out")); });
}

void OAuthLogin::authorize()
{
    if (!m_config.isConfigured()) {
        finish(false, tr("No OAuth client ID configured for this provider. See README."));
        return;
    }
    auto *handler = new QOAuthHttpServerReplyHandler(this);
    handler->setCallbackHost(m_config.callbackHost);
    handler->setCallbackText(tr(
        "<html><body style='background:#0a1a3a;color:#d8deea;font-family:Helvetica,Arial;"
        "text-align:center;padding-top:80px'><h1>SIGNED ON.</h1>"
        "<p>You may close this window and return to RetroMail.</p></body></html>"));
    if (!handler->listen(QHostAddress::LocalHost)) {
        finish(false, tr("Could not open a local port for the sign-in reply"));
        return;
    }
    m_handler = handler;
    m_flow.setReplyHandler(handler);
    m_timeout.start(5 * 60 * 1000);
    m_flow.grant();
}

void OAuthLogin::refresh(const QString &refreshToken)
{
    if (!m_config.isConfigured()) {
        finish(false, tr("No OAuth client ID configured for this provider. See README."));
        return;
    }
    auto *handler = new QOAuthOobReplyHandler(this);
    m_handler = handler;
    m_flow.setReplyHandler(handler);
    m_flow.setRefreshToken(refreshToken);
    // Depending on the Qt version, a refresh may only report through
    // tokenChanged; let the event loop settle so refreshToken() is current too.
    connect(&m_flow, &QAbstractOAuth::tokenChanged, this, [this](const QString &token) {
        if (!token.isEmpty())
            QTimer::singleShot(0, this, [this] { finish(true); });
    });
    m_timeout.start(60 * 1000);
    m_flow.refreshTokens();
}

void OAuthLogin::finish(bool ok, const QString &message)
{
    if (m_done)
        return;
    m_done = true;
    m_timeout.stop();
    if (auto *server = qobject_cast<QOAuthHttpServerReplyHandler *>(m_handler))
        server->close();

    if (ok && !m_flow.token().isEmpty())
        emit succeeded(m_flow.token(), m_flow.refreshToken());
    else
        emit failed(message.isEmpty() ? tr("Sign-in failed") : message);
    deleteLater();
}
