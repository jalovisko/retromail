#pragma once

#include "core/Account.h"

#include <QObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QTimer>

class QAbstractOAuthReplyHandler;

// One OAuth2 exchange with Google or Microsoft: either an interactive browser
// sign-in, or a silent refresh with a stored refresh token. Deletes itself
// after emitting succeeded() or failed().
class OAuthLogin : public QObject
{
    Q_OBJECT
public:
    OAuthLogin(Provider provider, const QString &loginHint, QObject *parent = nullptr);

    void authorize();                               // opens the system browser
    void refresh(const QString &refreshToken);      // no user interaction

signals:
    void succeeded(const QString &accessToken, const QString &refreshToken);
    void failed(const QString &message);

private:
    void finish(bool ok, const QString &message = {});

    OAuthConfig m_config;
    QString m_loginHint;
    QOAuth2AuthorizationCodeFlow m_flow;
    QAbstractOAuthReplyHandler *m_handler = nullptr;
    QTimer m_timeout;
    bool m_done = false;
};
