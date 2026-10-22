#include "ui/InboxScreen.h"

#include "core/MailSession.h"
#include "ui/MailboxArt.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

enum Role { UidRole = Qt::UserRole, FromRole, SubjectRole, DateRole, SeenRole };

// Each row is drawn like a line on a 1998 mail list: flag, sender, subject, date.
class EnvelopeDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return {200, 54};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        p->save();
        const QRect r = opt.rect.adjusted(0, 2, 0, -2);
        const bool selected = opt.state & QStyle::State_Selected;
        const bool seen = index.data(SeenRole).toBool();

        p->fillRect(r, selected ? Theme::kPanel : Theme::kSlab);
        if (!selected) {
            p->setPen(QColor(40, 56, 96));
            p->drawLine(r.bottomLeft(), r.bottomRight());
        }
        const QColor ink = selected ? Theme::kInk : Theme::kText;

        if (!seen)
            MailboxArt::paintFlagIcon(*p, QRectF(r.left() + 12, r.top() + 14, 22, 24));
        else
            MailboxArt::paintEnvelope(*p, QRectF(r.left() + 10, r.top() + 16, 28, 20), 0.6);

        const int dateW = 150;
        const QRect fromRect(r.left() + 52, r.top(), int(r.width() * 0.30), r.height());
        const QRect subjRect(fromRect.right() + 16, r.top(), r.right() - dateW - fromRect.right() - 28, r.height());
        const QRect dateRect(r.right() - dateW, r.top(), dateW - 12, r.height());

        QFont strong = Theme::headline(17);
        QFont normal = Theme::plain(17);
        strong.setCapitalization(QFont::AllUppercase);
        normal.setCapitalization(QFont::AllUppercase);

        p->setPen(ink);
        p->setFont(seen ? normal : strong);
        const QFontMetrics fm(p->font());
        p->drawText(fromRect, Qt::AlignVCenter | Qt::AlignLeft,
                    fm.elidedText(index.data(FromRole).toString(), Qt::ElideRight, fromRect.width()));
        p->drawText(subjRect, Qt::AlignVCenter | Qt::AlignLeft,
                    fm.elidedText(index.data(SubjectRole).toString(), Qt::ElideRight, subjRect.width()));
        p->setFont(Theme::plain(15));
        p->setPen(selected ? Theme::kInk : Theme::kGreen);
        p->drawText(dateRect, Qt::AlignVCenter | Qt::AlignRight, index.data(DateRole).toString());
        p->restore();
    }
};

QString shortDate(const QDateTime &dt)
{
    if (!dt.isValid())
        return {};
    const QDateTime local = dt.toLocalTime();
    if (local.date() == QDate::currentDate())
        return local.toString(QStringLiteral("hh:mm AP"));
    return local.toString(QStringLiteral("MM/dd/yy"));
}

} // namespace

InboxScreen::InboxScreen(QWidget *parent)
    : ScreenBackground(parent)
{
    auto *toolbar = new RetroToolbar(this);
    auto *in = toolbar->addInButton();
    in->setEnabled(false);   // we're already here
    auto *write = toolbar->addButton(tr("WRITE"));
    auto *check = toolbar->addButton(tr("CHECK"));
    auto *del = toolbar->addButton(tr("DELETE"));
    auto *back = toolbar->addBackFlag();

    m_title = makeSlab(QString(), this);
    m_title->setFont(Theme::headline(20));

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new EnvelopeDelegate(m_list));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setStyleSheet(QStringLiteral("QListWidget { background: %1; border: 3px solid %2; }")
                              .arg(Theme::kSlab.name(), Theme::kPanel.name()));
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setFont(Theme::plain(15));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    auto *content = new QVBoxLayout;
    content->setContentsMargins(20, 18, 20, 12);
    content->setSpacing(12);
    content->addWidget(m_title);
    content->addWidget(m_list, 1);
    content->addWidget(m_status);
    layout->addLayout(content, 1);

    connect(write, &QAbstractButton::clicked, this, &InboxScreen::composeRequested);
    connect(check, &QAbstractButton::clicked, this, [this] {
        if (m_session)
            m_session->refresh();
    });
    connect(del, &QAbstractButton::clicked, this, &InboxScreen::deleteCurrent);
    connect(back, &QAbstractButton::clicked, this, &InboxScreen::backRequested);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit messageOpened(item->data(UidRole).toUInt());
    });
}

void InboxScreen::setSession(MailSession *session)
{
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    m_session = session;
    if (session) {
        connect(session, &MailSession::headersChanged, this, &InboxScreen::reload);
        connect(session, &MailSession::stateChanged, this, &InboxScreen::reload);
    }
    reload();
    m_list->setFocus();
}

void InboxScreen::reload()
{
    if (!m_session)
        return;
    const quint32 current = m_list->currentItem() ? m_list->currentItem()->data(UidRole).toUInt() : 0;

    const Account account = m_session->account();
    const int unread = m_session->unreadCount();
    m_title->setText(tr("MAIL FOR: %1   (%2 NEW)").arg(account.label().toUpper()).arg(unread));

    const QSignalBlocker blocker(m_list);
    m_list->clear();
    for (const MessageHeader &h : m_session->headers()) {
        auto *item = new QListWidgetItem(m_list);
        item->setData(UidRole, h.uid);
        item->setData(FromRole, Mime::displayName(h.from));
        item->setData(SubjectRole, h.subject.isEmpty() ? tr("(NO SUBJECT)") : h.subject);
        item->setData(DateRole, shortDate(h.date));
        item->setData(SeenRole, h.seen);
        item->setToolTip(h.from);
        if (h.uid == current)
            m_list->setCurrentItem(item);
    }
    if (!m_list->currentItem() && m_list->count() > 0)
        m_list->setCurrentRow(0);

    QString status;
    switch (m_session->state()) {
    case MailSession::State::Busy: status = m_session->statusText(); break;
    case MailSession::State::Error: status = tr("ERROR: %1").arg(m_session->statusText()); break;
    default:
        status = m_session->headers().isEmpty() ? tr("YOUR MAILBOX IS EMPTY.")
                                                : tr("%1 MESSAGES. DOUBLE-CLICK TO READ.").arg(m_session->headers().size());
    }
    m_status->setText(status);
}

void InboxScreen::deleteCurrent()
{
    auto *item = m_list->currentItem();
    if (!item || !m_session)
        return;
    const auto answer = QMessageBox::question(
        this, tr("RetroMail"),
        tr("Delete \"%1\"?").arg(item->data(SubjectRole).toString()));
    if (answer == QMessageBox::Yes)
        m_session->deleteMessage(item->data(UidRole).toUInt());
}

void InboxScreen::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete) {
        deleteCurrent();
        return;
    }
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace) {
        emit backRequested();
        return;
    }
    ScreenBackground::keyPressEvent(e);
}
