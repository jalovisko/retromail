#include "ui/MainWindow.h"

#include "BuildConfig.h"
#include "core/AccountStore.h"
#include "core/DemoSession.h"
#include "core/ImapSession.h"
#include "core/SecretStore.h"
#include "net/OAuthLogin.h"
#include "ui/AccountDialog.h"
#include "ui/ComposeScreen.h"
#include "ui/InboxScreen.h"
#include "ui/LoginScreen.h"
#include "ui/MailboxScreen.h"
#include "ui/ReaderScreen.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QDir>
#include <QStackedWidget>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_store(new AccountStore(this))
{
    setWindowTitle(tr("RetroMail"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/retromail-256.png")));
    resize(800, 600);   // SVGA, naturally
    setMinimumSize(640, 480);

    m_stack = new QStackedWidget(this);
    m_login = new LoginScreen(m_store, m_stack);
    m_mailboxes = new MailboxScreen(m_stack);
    m_inbox = new InboxScreen(m_stack);
    m_reader = new ReaderScreen(m_stack);
    m_compose = new ComposeScreen(m_stack);
    for (QWidget *w : {static_cast<QWidget *>(m_login), static_cast<QWidget *>(m_mailboxes),
                       static_cast<QWidget *>(m_inbox), static_cast<QWidget *>(m_reader),
                       static_cast<QWidget *>(m_compose)})
        m_stack->addWidget(w);
    setCentralWidget(m_stack);

    buildMenus();

    connect(m_login, &LoginScreen::signOnRequested, this, &MainWindow::signOn);
    connect(m_login, &LoginScreen::guestRequested, this, &MainWindow::signOnAsGuest);
    connect(m_login, &LoginScreen::addAccountRequested, this, &MainWindow::addAccount);
    connect(m_login, &LoginScreen::removeAccountRequested, this, &MainWindow::removeAccount);

    connect(m_mailboxes, &MailboxScreen::mailboxOpened, this, &MainWindow::openInbox);
    connect(m_mailboxes, &MailboxScreen::composeRequested, this, [this] { compose(m_sessions.value(0)); });
    connect(m_mailboxes, &MailboxScreen::checkRequested, this, [this] {
        for (MailSession *s : std::as_const(m_sessions))
            s->refresh();
    });
    connect(m_mailboxes, &MailboxScreen::signOffRequested, this, &MainWindow::signOff);

    connect(m_inbox, &InboxScreen::messageOpened, this, &MainWindow::openMessage);
    connect(m_inbox, &InboxScreen::composeRequested, this, [this] { compose(m_current); });
    connect(m_inbox, &InboxScreen::backRequested, this, &MainWindow::showMailboxes);

    connect(m_reader, &ReaderScreen::inboxRequested, this, [this] { m_stack->setCurrentWidget(m_inbox); });
    connect(m_reader, &ReaderScreen::replyRequested, this, [this] {
        m_composeReturn = m_reader;
        m_compose->startReply(m_current, m_reader->message());
        m_stack->setCurrentWidget(m_compose);
    });
    connect(m_reader, &ReaderScreen::deleteRequested, this, [this](quint32 uid) {
        if (m_current)
            m_current->deleteMessage(uid);
        m_stack->setCurrentWidget(m_inbox);
    });

    connect(m_compose, &ComposeScreen::closed, this, [this] {
        m_stack->setCurrentWidget(m_composeReturn ? m_composeReturn : m_mailboxes);
    });

    m_stack->setCurrentWidget(m_login);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *write = file->addAction(tr("&New Message"), QKeySequence::New, this, [this] {
        if (!m_sessions.isEmpty())
            compose(m_current ? m_current.data() : m_sessions.first());
    });
    QAction *check = file->addAction(tr("&Check Mail"), QKeySequence(Qt::CTRL | Qt::Key_R), this, [this] {
        for (MailSession *s : std::as_const(m_sessions))
            s->refresh();
    });
    file->addSeparator();
    QAction *signOffAction = file->addAction(tr("Sign &Off"), this, &MainWindow::signOff);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, qApp, &QApplication::quit);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    QAction *crt = view->addAction(tr("CRT &Fonts (no smoothing)"));
    crt->setCheckable(true);
    crt->setChecked(Theme::crtFonts());
    connect(crt, &QAction::toggled, this, [this](bool on) {
        Theme::setCrtFonts(on);
        QMessageBox::information(this, tr("RetroMail"),
                                 tr("Restart RetroMail to apply the font change everywhere."));
    });
    QAction *caps = view->addAction(tr("&Capital Letters in Messages"));
    caps->setCheckable(true);
    caps->setChecked(Theme::capsDisplay());
    connect(caps, &QAction::toggled, this, [this](bool on) {
        Theme::setCapsDisplay(on);
        m_reader->refreshFonts();
        m_compose->refreshFonts();
    });

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About RetroMail..."), this, [this] {
        QMessageBox::about(this, tr("About RetroMail"),
                           tr("<b>RetroMail %1</b><br>An e-mail client from 1998, built today.<br><br>"
                              "Written in C++ with Qt.").arg(QStringLiteral(RETROMAIL_VERSION)));
    });

    // Only meaningful once signed on.
    const auto updateActions = [=, this] {
        const bool on = !m_sessions.isEmpty();
        write->setEnabled(on);
        check->setEnabled(on);
        signOffAction->setEnabled(on);
    };
    connect(m_stack, &QStackedWidget::currentChanged, this, updateActions);
    updateActions();
}

void MainWindow::signOn(const QList<Account> &accounts)
{
    signOff();
    for (const Account &a : accounts) {
        auto *session = new ImapSession(a, this);
        connect(session, &MailSession::newMailArrived, this, [] { QApplication::beep(); });
        m_sessions << session;
    }
    showMailboxes();
    for (MailSession *s : std::as_const(m_sessions))
        s->start();
}

void MainWindow::signOnAsGuest()
{
    signOff();
    m_sessions << new DemoSession(this);
    showMailboxes();
    m_sessions.first()->start();
}

void MainWindow::signOff()
{
    m_inbox->setSession(nullptr);
    m_mailboxes->setSessions({});
    m_compose->setSessions({});
    qDeleteAll(m_sessions);
    m_sessions.clear();
    m_current = nullptr;
    m_stack->setCurrentWidget(m_login);
}

void MainWindow::showMailboxes()
{
    m_mailboxes->setSessions(m_sessions);
    m_compose->setSessions(m_sessions);
    m_stack->setCurrentWidget(m_mailboxes);
    m_mailboxes->setFocus();
}

void MainWindow::openInbox(MailSession *session)
{
    m_current = session;
    m_inbox->setSession(session);
    m_stack->setCurrentWidget(m_inbox);
}

void MainWindow::openMessage(quint32 uid)
{
    if (!m_current)
        return;
    m_reader->showLoading();
    m_stack->setCurrentWidget(m_reader);

    MailSession *session = m_current;
    auto loaded = std::make_shared<QMetaObject::Connection>();
    auto failed = std::make_shared<QMetaObject::Connection>();
    const auto cleanup = [loaded, failed] {
        QObject::disconnect(*loaded);
        QObject::disconnect(*failed);
    };
    *loaded = connect(session, &MailSession::messageLoaded, this,
                      [this, uid, cleanup](quint32 got, const MimeMessage &message) {
        if (got != uid)
            return;
        cleanup();
        m_reader->showMessage(uid, message);
    });
    *failed = connect(session, &MailSession::messageFailed, this,
                      [this, uid, cleanup](quint32 got, const QString &error) {
        if (got != uid)
            return;
        cleanup();
        m_reader->showError(error);
    });
    session->fetchMessage(uid);
}

void MainWindow::compose(MailSession *from)
{
    m_composeReturn = m_stack->currentWidget() == m_compose ? m_mailboxes : m_stack->currentWidget();
    m_compose->startNew(from);
    m_stack->setCurrentWidget(m_compose);
}

void MainWindow::addAccount()
{
    AccountDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const Account account = dialog.account();

    const auto save = [this, account](const QString &secret) {
        SecretStore::write(account.id, secret, this, [this, account](bool ok, const QString &error) {
            m_login->setBusy(false);
            if (!ok) {
                m_login->setStatus(tr("COULD NOT SAVE TO KEYCHAIN."));
                QMessageBox::warning(this, tr("RetroMail"),
                                     tr("Could not store the login in your system keychain:\n%1").arg(error));
                return;
            }
            m_store->add(account);
            m_login->setStatus(tr("ACCOUNT ADDED. PRESS SIGN ON."));
        });
    };

    if (!dialog.useOAuth()) {
        m_login->setBusy(true);
        save(dialog.password());
        return;
    }

    m_login->setBusy(true);
    m_login->setStatus(tr("WAITING FOR YOUR WEB BROWSER...."));
    auto *login = new OAuthLogin(account.provider, account.email, this);
    connect(login, &OAuthLogin::succeeded, this, [this, save](const QString &, const QString &refresh) {
        if (refresh.isEmpty()) {
            m_login->setBusy(false);
            m_login->setStatus(tr("SIGN-IN GAVE NO REFRESH TOKEN. TRY AGAIN."));
            return;
        }
        save(refresh);
    });
    connect(login, &OAuthLogin::failed, this, [this](const QString &message) {
        m_login->setBusy(false);
        m_login->setStatus(tr("SIGN-IN FAILED."));
        QMessageBox::warning(this, tr("RetroMail"), message);
    });
    login->authorize();
}

void MainWindow::removeAccount(const QString &id)
{
    const Account a = m_store->account(id);
    if (QMessageBox::question(this, tr("RetroMail"), tr("Remove %1 from RetroMail?\n\n"
                                                       "Your mail stays on the server.").arg(a.email))
        != QMessageBox::Yes)
        return;
    SecretStore::remove(id);
    m_store->remove(id);
}

void MainWindow::runScreenshotTour(const QString &dir)
{
    QDir().mkpath(dir);
    const auto shot = [this, dir](const QString &name) {
        grab().save(QDir(dir).filePath(name + QStringLiteral(".png")));
    };
    struct Step { int delay; std::function<void()> action; };
    const QList<Step> steps = {
        {600, [=] { shot(QStringLiteral("1-login")); signOnAsGuest(); }},
        {1600, [=] { shot(QStringLiteral("2-mailbox")); m_mailboxes->openMailbox(0); }},
        {420, [=] { shot(QStringLiteral("3-envelope")); }},
        {900, [=] { shot(QStringLiteral("4-inbox")); openMessage(m_current->headers().value(1).uid); }},
        {700, [=] { shot(QStringLiteral("5-reader")); emit m_reader->replyRequested(); }},
        {500, [=] { shot(QStringLiteral("6-compose")); qApp->quit(); }},
    };
    int at = 0;
    for (const Step &step : steps) {
        at += step.delay;
        QTimer::singleShot(at, this, step.action);
    }
}
