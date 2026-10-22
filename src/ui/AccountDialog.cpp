#include "ui/AccountDialog.h"

#include "core/AccountStore.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

AccountDialog::AccountDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Account"));
    setFont(Theme::plain(13));

    m_provider = new QComboBox(this);
    for (const auto &preset : ProviderPreset::all())
        m_provider->addItem(preset.label, int(preset.provider));

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("Your name, as recipients see it"));
    m_email = new QLineEdit(this);
    m_email->setPlaceholderText(tr("you@example.com"));

    m_oauth = new QRadioButton(tr("Sign in with my web browser (recommended)"), this);
    m_passwordMode = new QRadioButton(tr("Password / app password"), this);
    m_password = new QLineEdit(this);
    m_password->setEchoMode(QLineEdit::Password);
    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::kPanelShadow.darker(140).name()));

    m_showServers = new QCheckBox(tr("Show server settings"), this);
    m_servers = new QGroupBox(tr("Servers"), this);
    m_username = new QLineEdit(m_servers);
    m_username->setPlaceholderText(tr("Same as e-mail address"));
    m_imapHost = new QLineEdit(m_servers);
    m_imapPort = new QSpinBox(m_servers);
    m_imapPort->setRange(1, 65535);
    m_smtpHost = new QLineEdit(m_servers);
    m_smtpPort = new QSpinBox(m_servers);
    m_smtpPort->setRange(1, 65535);
    m_smtpSecurity = new QComboBox(m_servers);
    m_smtpSecurity->addItem(tr("SSL/TLS"), int(SmtpSecurity::SslTls));
    m_smtpSecurity->addItem(tr("STARTTLS"), int(SmtpSecurity::StartTls));

    auto *serverForm = new QFormLayout(m_servers);
    serverForm->addRow(tr("User name:"), m_username);
    serverForm->addRow(tr("IMAP server (SSL):"), m_imapHost);
    serverForm->addRow(tr("IMAP port:"), m_imapPort);
    serverForm->addRow(tr("SMTP server:"), m_smtpHost);
    serverForm->addRow(tr("SMTP port:"), m_smtpPort);
    serverForm->addRow(tr("SMTP security:"), m_smtpSecurity);

    auto *form = new QFormLayout;
    form->addRow(tr("Service:"), m_provider);
    form->addRow(tr("Your name:"), m_name);
    form->addRow(tr("E-mail address:"), m_email);
    form->addRow(tr("Sign in:"), m_oauth);
    form->addRow(QString(), m_passwordMode);
    form->addRow(tr("Password:"), m_password);
    form->addRow(QString(), m_hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Add"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_showServers);
    layout->addWidget(m_servers);
    layout->addStretch(1);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &AccountDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_provider, &QComboBox::currentIndexChanged, this, &AccountDialog::applyPreset);
    connect(m_oauth, &QRadioButton::toggled, this, &AccountDialog::updateAuthUi);
    connect(m_showServers, &QCheckBox::toggled, this, &AccountDialog::updateAuthUi);

    applyPreset();
    resize(520, sizeHint().height());
}

void AccountDialog::applyPreset()
{
    const auto preset = ProviderPreset::forProvider(Provider(m_provider->currentData().toInt()));
    m_imapHost->setText(preset.imapHost);
    m_imapPort->setValue(preset.imapPort);
    m_smtpHost->setText(preset.smtpHost);
    m_smtpPort->setValue(preset.smtpPort);
    m_smtpSecurity->setCurrentIndex(preset.smtpSecurity == SmtpSecurity::SslTls ? 0 : 1);

    const bool oauthReady = preset.supportsOAuth && oauthConfigFor(preset.provider).isConfigured();
    m_oauth->setEnabled(oauthReady);
    m_passwordMode->setEnabled(preset.supportsPassword);
    if (oauthReady)
        m_oauth->setChecked(true);
    else
        m_passwordMode->setChecked(true);
    m_showServers->setChecked(preset.provider == Provider::Other);
    updateAuthUi();
}

void AccountDialog::updateAuthUi()
{
    const auto preset = ProviderPreset::forProvider(Provider(m_provider->currentData().toInt()));
    const bool oauth = m_oauth->isChecked() && m_oauth->isEnabled();
    m_password->setEnabled(!oauth);

    QString hint;
    if (preset.supportsOAuth && !oauthConfigFor(preset.provider).isConfigured()) {
        hint = tr("Browser sign-in is not set up in this build (no OAuth client ID). ");
        if (!preset.supportsPassword)
            hint += tr("Outlook needs it; see the README.");
    }
    if (oauth)
        hint += tr("Your browser will open so you can sign in. RetroMail never sees your password.");
    else
        hint += preset.passwordHint;
    m_hint->setText(hint.trimmed());
    m_hint->setVisible(!m_hint->text().isEmpty());
    m_servers->setVisible(m_showServers->isChecked());
}

Account AccountDialog::account() const
{
    Account a;
    a.id = AccountStore::newId();
    a.provider = Provider(m_provider->currentData().toInt());
    a.auth = useOAuth() ? AuthMethod::OAuth : AuthMethod::Password;
    a.displayName = m_name->text().trimmed();
    a.email = m_email->text().trimmed();
    a.username = m_username->text().trimmed().isEmpty() ? a.email : m_username->text().trimmed();
    a.imapHost = m_imapHost->text().trimmed();
    a.imapPort = m_imapPort->value();
    a.smtpHost = m_smtpHost->text().trimmed();
    a.smtpPort = m_smtpPort->value();
    a.smtpSecurity = SmtpSecurity(m_smtpSecurity->currentData().toInt());
    return a;
}

QString AccountDialog::password() const
{
    return m_password->text();
}

bool AccountDialog::useOAuth() const
{
    return m_oauth->isChecked() && m_oauth->isEnabled();
}

void AccountDialog::accept()
{
    const Account a = account();
    QString problem;
    if (!a.email.contains(QLatin1Char('@')))
        problem = tr("Please enter your e-mail address.");
    else if (!useOAuth() && !m_passwordMode->isEnabled())
        problem = tr("This service needs browser sign-in, which is not set up in this build. See the README.");
    else if (!useOAuth() && password().isEmpty())
        problem = tr("Please enter your password.");
    else if (a.imapHost.isEmpty() || a.smtpHost.isEmpty())
        problem = tr("Please fill in the IMAP and SMTP servers.");
    if (!problem.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), problem);
        return;
    }
    QDialog::accept();
}
