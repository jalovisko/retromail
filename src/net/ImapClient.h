#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSslSocket>
#include <QTimer>

#include <functional>
#include <optional>

// One server response line. Literals ({n}\r\n<n bytes>) are pulled out into
// `literals`; the `{n}` marker stays in `line` so the structure is visible.
struct ImapResponse {
    QByteArray line;
    QList<QByteArray> literals;
};

// Minimal asynchronous IMAP4rev1 client over implicit TLS (port 993).
// Commands are queued and sent one at a time.
class ImapClient : public QObject
{
    Q_OBJECT
public:
    using Callback = std::function<void(bool ok, const QByteArray &status,
                                        const QList<ImapResponse> &untagged)>;

    explicit ImapClient(QObject *parent = nullptr);
    ~ImapClient() override;

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    void abort();                       // drop the connection now, no callbacks
    bool isConnected() const;

    void command(const QByteArray &cmd, Callback callback);

    // Convenience wrappers
    void login(const QString &user, const QString &password, Callback callback);
    void authenticateXOAuth2(const QString &user, const QString &accessToken, Callback callback);

    static QByteArray quote(const QString &s);

signals:
    void ready();                       // greeting received, commands may be sent
    void errorOccurred(const QString &message);
    void disconnected();

private:
    struct Pending {
        QByteArray tag;
        QByteArray command;
        Callback callback;
        QList<ImapResponse> untagged;
    };

    void onReadyRead();
    void handleResponse(const ImapResponse &response);
    void pump();
    void failAll(const QString &message);

    QSslSocket m_socket;
    QTimer m_timeout;
    QByteArray m_buffer;
    ImapResponse m_current;
    qint64 m_literalRemaining = -1;
    bool m_greeted = false;
    int m_tagCounter = 0;
    QQueue<Pending> m_queue;
    std::optional<Pending> m_inflight;
};
