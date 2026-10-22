#include "ui/LoginScreen.h"

#include "core/AccountStore.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

LoginScreen::LoginScreen(AccountStore *store, QWidget *parent)
    : ScreenBackground(parent)
    , m_store(store)
{
    // Top-right REGISTERED USER / GUEST box
    auto *modeBox = new LightPanel(this);
    auto *modeLayout = new QVBoxLayout(modeBox);
    modeLayout->setContentsMargins(16, 10, 24, 10);
    m_registered = new QRadioButton(tr("REGISTERED USER"), modeBox);
    m_guest = new QRadioButton(tr("GUEST"), modeBox);
    for (auto *rb : {m_registered, m_guest}) {
        rb->setFont(Theme::headline(18));
        modeLayout->addWidget(rb);
    }
    auto *group = new QButtonGroup(this);
    group->addButton(m_registered);
    group->addButton(m_guest);

    // Title
    auto *title = new QLabel(tr("RETROMAIL"), this);
    title->setFont(Theme::headline(54));
    title->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::kText.name()));
    auto *subtitle = new QLabel(tr("PLEASE SIGN ON...."), this);
    subtitle->setFont(Theme::plain(26));
    subtitle->setObjectName(QStringLiteral("status"));

    // Middle panel: either the account list or the guest notice
    m_modeStack = new QStackedWidget(this);

    auto *accountsPanel = new LightPanel;
    auto *accLayout = new QVBoxLayout(accountsPanel);
    accLayout->setContentsMargins(14, 12, 14, 14);
    auto *pick = new QLabel(tr("SCREEN NAMES:"), accountsPanel);
    pick->setFont(Theme::headline(16));
    m_list = new QListWidget(accountsPanel);
    m_list->setFont(Theme::headline(16));
    m_list->setMinimumHeight(120);
    auto *accButtons = new QHBoxLayout;
    m_add = new RetroButton(tr("ADD ACCOUNT"), accountsPanel);
    m_remove = new RetroButton(tr("REMOVE"), accountsPanel);
    m_add->setFont(Theme::headline(14));
    m_remove->setFont(Theme::headline(14));
    accButtons->addWidget(m_add);
    accButtons->addWidget(m_remove);
    accButtons->addStretch(1);
    accLayout->addWidget(pick);
    accLayout->addWidget(m_list, 1);
    accLayout->addLayout(accButtons);

    auto *guestPanel = new LightPanel;
    auto *guestLayout = new QVBoxLayout(guestPanel);
    guestLayout->setContentsMargins(18, 18, 18, 18);
    auto *guestText = new QLabel(tr(
        "GUEST ACCESS\n\nOpens a demonstration mailbox.\nNo account needed, nothing is sent."), guestPanel);
    guestText->setFont(Theme::headline(16));
    guestText->setWordWrap(true);
    guestLayout->addWidget(guestText);
    guestLayout->addStretch(1);

    m_modeStack->addWidget(accountsPanel);
    m_modeStack->addWidget(guestPanel);
    m_modeStack->setFixedWidth(420);
    m_modeStack->setMinimumHeight(240);

    m_signOn = new RetroButton(tr("SIGN ON"), this);
    m_signOn->setFont(Theme::headline(22));
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setFont(Theme::plain(16));
    m_status->setWordWrap(true);

    // Layout
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(36, 24, 24, 24);
    grid->addWidget(modeBox, 0, 1, Qt::AlignRight | Qt::AlignTop);
    auto *left = new QVBoxLayout;
    left->addStretch(1);
    left->addWidget(title);
    left->addWidget(subtitle);
    left->addSpacing(24);
    left->addWidget(m_modeStack, 0, Qt::AlignLeft);
    left->addSpacing(16);
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(m_signOn);
    bottom->addSpacing(16);
    bottom->addWidget(m_status, 1);
    left->addLayout(bottom);
    left->addStretch(2);
    grid->addLayout(left, 1, 0, 1, 2);
    grid->setRowStretch(1, 1);

    connect(m_registered, &QRadioButton::toggled, this, &LoginScreen::updateMode);
    connect(m_add, &QAbstractButton::clicked, this, &LoginScreen::addAccountRequested);
    connect(m_remove, &QAbstractButton::clicked, this, [this] {
        if (auto *item = m_list->currentItem())
            emit removeAccountRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_signOn, &QAbstractButton::clicked, this, &LoginScreen::submit);
    connect(m_list, &QListWidget::itemChanged, this, &LoginScreen::updateMode);
    connect(m_list, &QListWidget::currentItemChanged, this, &LoginScreen::updateMode);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &LoginScreen::submit);
    connect(store, &AccountStore::changed, this, &LoginScreen::reloadAccounts);

    reloadAccounts();
    (m_store->accounts().isEmpty() ? m_guest : m_registered)->setChecked(true);
    updateMode();
}

void LoginScreen::reloadAccounts()
{
    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const Account &a : m_store->accounts()) {
        const QString provider = ProviderPreset::forProvider(a.provider).label;
        auto *item = new QListWidgetItem(QStringLiteral("%1   [%2]").arg(a.email.toUpper(), provider), m_list);
        item->setData(Qt::UserRole, a.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    updateMode();
}

void LoginScreen::updateMode()
{
    const bool registered = m_registered->isChecked();
    m_modeStack->setCurrentIndex(registered ? 0 : 1);
    m_remove->setEnabled(m_list->currentItem() != nullptr);

    bool anyChecked = false;
    for (int i = 0; i < m_list->count(); ++i)
        anyChecked |= m_list->item(i)->checkState() == Qt::Checked;
    m_signOn->setEnabled(!registered || anyChecked);
}

void LoginScreen::submit()
{
    if (!m_signOn->isEnabled())
        return;
    if (m_guest->isChecked()) {
        emit guestRequested();
        return;
    }
    QList<Account> chosen;
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->checkState() == Qt::Checked)
            chosen << m_store->account(m_list->item(i)->data(Qt::UserRole).toString());
    }
    emit signOnRequested(chosen);
}

void LoginScreen::setStatus(const QString &text)
{
    m_status->setText(text);
}

void LoginScreen::setBusy(bool busy)
{
    m_signOn->setEnabled(!busy);
    m_add->setEnabled(!busy);
    if (!busy)
        updateMode();
}
