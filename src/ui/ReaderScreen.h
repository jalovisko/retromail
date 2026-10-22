#pragma once

#include "core/MimeMessage.h"
#include "ui/RetroWidgets.h"

class QLineEdit;
class QLabel;
class QTextEdit;

// Shows one message: Message To / Message From slabs and the body.
class ReaderScreen : public ScreenBackground
{
    Q_OBJECT
public:
    explicit ReaderScreen(QWidget *parent = nullptr);

    void showLoading();
    void showMessage(quint32 uid, const MimeMessage &message);
    void showError(const QString &error);
    void refreshFonts();

    quint32 uid() const { return m_uid; }
    const MimeMessage &message() const { return m_message; }

signals:
    void inboxRequested();
    void replyRequested();
    void deleteRequested(quint32 uid);

protected:
    void keyPressEvent(QKeyEvent *) override;

private:
    quint32 m_uid = 0;
    MimeMessage m_message;
    QLineEdit *m_to;
    QLineEdit *m_from;
    QLineEdit *m_subject;
    QTextEdit *m_body;
    QLabel *m_footer;
};
