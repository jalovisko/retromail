#pragma once

#include "core/Account.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSpinBox;

// "Add account" dialog: provider, address, and either browser sign-in
// (Gmail / Outlook) or a password / app password.
class AccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AccountDialog(QWidget *parent = nullptr);

    Account account() const;
    QString password() const;
    bool useOAuth() const;

    void accept() override;

private:
    void applyPreset();
    void updateAuthUi();

    QComboBox *m_provider;
    QLineEdit *m_name;
    QLineEdit *m_email;
    QRadioButton *m_oauth;
    QRadioButton *m_passwordMode;
    QLineEdit *m_password;
    QLabel *m_hint;
    QCheckBox *m_showServers;
    QGroupBox *m_servers;
    QLineEdit *m_username;
    QLineEdit *m_imapHost;
    QSpinBox *m_imapPort;
    QLineEdit *m_smtpHost;
    QSpinBox *m_smtpPort;
    QComboBox *m_smtpSecurity;
};
