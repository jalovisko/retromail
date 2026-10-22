#pragma once

#include "core/MailSession.h"
#include "ui/RetroWidgets.h"

#include <QList>
#include <QPointer>
#include <QTimer>
#include <QVariantAnimation>

class RetroButton;

// "YOU HAVE ONE PIECE OF MAIL...." – one mailbox per signed-on account.
// Clicking a mailbox pulls the envelope out and opens that inbox.
class MailboxScreen : public ScreenBackground
{
    Q_OBJECT
public:
    explicit MailboxScreen(QWidget *parent = nullptr);

    void setSessions(const QList<MailSession *> &sessions);
    void openMailbox(int index) { open(index); }

signals:
    void mailboxOpened(MailSession *session);
    void composeRequested();
    void checkRequested();
    void signOffRequested();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QRectF mailboxRect(int index) const;
    QRectF headlineRect() const;
    int hitTest(const QPointF &pos) const;
    void open(int index);

    QList<QPointer<MailSession>> m_sessions;
    LightPanel *m_panel;
    RetroButton *m_write;
    RetroButton *m_check;
    RetroButton *m_signOff;
    QTimer m_blink;
    int m_dots = 4;
    QVariantAnimation m_zoom;
    int m_zoomIndex = -1;
    QVariantAnimation m_flagAnim;
};
