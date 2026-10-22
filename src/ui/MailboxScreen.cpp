#include "ui/MailboxScreen.h"

#include "core/MailSession.h"
#include "ui/MailboxArt.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace {

QString numberWords(int n)
{
    static const char *const words[] = {
        "NO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN",
        "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN", "SEVENTEEN",
        "EIGHTEEN", "NINETEEN", "TWENTY"};
    if (n >= 0 && n <= 20)
        return QString::fromLatin1(words[n]);
    return QString::number(n);
}

} // namespace

MailboxScreen::MailboxScreen(QWidget *parent)
    : ScreenBackground(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_panel = new LightPanel(this);
    auto *layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(12, 10, 16, 14);
    layout->setSpacing(10);
    m_write = new RetroButton(tr("WRITE"), m_panel);
    m_check = new RetroButton(tr("CHECK MAIL"), m_panel);
    m_signOff = new RetroButton(tr("SIGN OFF"), m_panel);
    for (auto *b : {m_write, m_check, m_signOff}) {
        b->setFont(Theme::headline(15));
        layout->addWidget(b);
    }
    connect(m_write, &QAbstractButton::clicked, this, &MailboxScreen::composeRequested);
    connect(m_check, &QAbstractButton::clicked, this, &MailboxScreen::checkRequested);
    connect(m_signOff, &QAbstractButton::clicked, this, &MailboxScreen::signOffRequested);

    // Animated "...." while any mailbox is busy.
    m_blink.setInterval(450);
    connect(&m_blink, &QTimer::timeout, this, [this] {
        m_dots = (m_dots % 4) + 1;
        update();
    });
    m_blink.start();

    m_zoom.setDuration(700);
    m_zoom.setStartValue(0.0);
    m_zoom.setEndValue(1.0);
    m_zoom.setEasingCurve(QEasingCurve::InOutQuad);
    connect(&m_zoom, &QVariantAnimation::valueChanged, this, [this] { update(); });
    connect(&m_zoom, &QVariantAnimation::finished, this, [this] {
        const int index = m_zoomIndex;
        m_zoomIndex = -1;
        update();
        if (index >= 0 && index < m_sessions.size() && m_sessions.at(index))
            emit mailboxOpened(m_sessions.at(index));
    });

    // Flag swings up when mail arrives.
    m_flagAnim.setDuration(500);
    m_flagAnim.setStartValue(0.0);
    m_flagAnim.setEndValue(1.0);
    m_flagAnim.setEasingCurve(QEasingCurve::OutBack);
    connect(&m_flagAnim, &QVariantAnimation::valueChanged, this, [this] { update(); });
}

void MailboxScreen::setSessions(const QList<MailSession *> &sessions)
{
    for (const auto &s : std::as_const(m_sessions)) {
        if (s)
            disconnect(s, nullptr, this, nullptr);
    }
    m_sessions.clear();
    for (MailSession *s : sessions) {
        m_sessions << s;
        connect(s, &MailSession::stateChanged, this, qOverload<>(&QWidget::update));
        connect(s, &MailSession::headersChanged, this, qOverload<>(&QWidget::update));
        connect(s, &MailSession::newMailArrived, this, [this] { m_flagAnim.start(); });
    }
    m_flagAnim.start();
    update();
}

void MailboxScreen::resizeEvent(QResizeEvent *)
{
    const QSize hint = m_panel->sizeHint();
    m_panel->setGeometry(width() - hint.width() - 24, 20, hint.width(), hint.height());
}

QRectF MailboxScreen::headlineRect() const
{
    if (m_sessions.size() <= 1)
        return QRectF(width() * 0.07, height() * 0.24, width() * 0.38, height() * 0.52);
    return QRectF(width() * 0.06, height() * 0.06, width() * 0.62, height() * 0.24);
}

QRectF MailboxScreen::mailboxRect(int index) const
{
    const int n = int(m_sessions.size());
    if (n <= 1)
        return QRectF(width() * 0.40, height() * 0.14, width() * 0.58, height() * 0.84);

    // A row of mailboxes along the road, labels underneath.
    const int perRow = qMin(n, 4);
    const int rows = (n + perRow - 1) / perRow;
    const qreal top = height() * 0.30;
    const qreal cellW = width() / qreal(perRow);
    const qreal cellH = (height() - top - 10) / rows;
    const int row = index / perRow;
    const int col = index % perRow;
    return QRectF(col * cellW, top + row * cellH, cellW, cellH - 54);
}

int MailboxScreen::hitTest(const QPointF &pos) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        const QRectF r = mailboxRect(i);
        const qreal side = qMin(r.width(), r.height());
        const QRectF art(r.center().x() - side / 2, r.center().y() - side / 2, side, side);
        if (art.adjusted(side * 0.2, side * 0.08, -side * 0.08, -side * 0.02).contains(pos))
            return i;
    }
    return -1;
}

void MailboxScreen::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    Theme::paintBackground(p, rect());

    int total = 0;
    bool busy = false;
    QString status;
    for (const auto &s : m_sessions) {
        if (!s)
            continue;
        total += s->unreadCount();
        busy |= s->state() == MailSession::State::Busy;
        if (m_sessions.size() == 1)
            status = s->state() == MailSession::State::Error ? tr("ERROR: %1").arg(s->statusText())
                                                             : s->statusText();
    }

    // Headline
    const QString dots = QString(busy ? m_dots : 4, QLatin1Char('.'));
    QString headline;
    if (busy && total == 0)
        headline = tr("CHECKING FOR\nNEW MAIL") + dots;
    else if (total == 0)
        headline = tr("YOU HAVE\nNO NEW\nMAIL.");
    else if (total == 1)
        headline = tr("YOU HAVE\nONE PIECE\nOF MAIL") + dots;
    else
        headline = tr("YOU HAVE\n%1 PIECES\nOF MAIL").arg(numberWords(total)) + dots;
    if (m_sessions.size() > 1)
        headline.replace(QLatin1Char('\n'), QLatin1Char(' '));

    const QRectF hr = headlineRect();
    int size = m_sessions.size() > 1 ? int(qBound(22.0, height() / 16.0, 46.0))
                                     : int(qBound(24.0, height() / 9.5, 72.0));
    // Shrink until the text fits ("SEVENTEEN PIECES" is long).
    const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
    for (; size > 14; size -= 2) {
        const QFontMetrics fm(Theme::plain(size));
        const QRect need = fm.boundingRect(hr.toRect(), flags, headline);
        if (need.height() <= hr.height() && need.width() <= hr.width())
            break;
    }
    p.setFont(Theme::plain(size));
    p.setPen(Theme::kGreen);
    p.drawText(hr, flags, headline);

    // Mailboxes
    const qreal flagT = m_flagAnim.currentValue().toReal();
    for (int i = 0; i < m_sessions.size(); ++i) {
        MailSession *s = m_sessions.at(i);
        if (!s)
            continue;
        const QRectF r = mailboxRect(i);
        MailboxArt::MailboxPose pose;
        const int unread = s->unreadCount();
        pose.hasMail = unread > 0 && i != m_zoomIndex;
        pose.flag = unread > 0 ? flagT : 0.0;
        pose.dimmed = s->state() == MailSession::State::Error;
        MailboxArt::paintMailbox(p, r, pose);

        if (m_sessions.size() > 1) {
            const QRectF label(r.left() + 8, r.bottom() - 6, r.width() - 16, 60);
            p.setFont(Theme::headline(15));
            p.setPen(Theme::kText);
            const QString name = s->account().label().toUpper();
            p.drawText(label, Qt::AlignHCenter | Qt::AlignTop,
                       p.fontMetrics().elidedText(name, Qt::ElideMiddle, int(label.width())));
            p.setFont(Theme::plain(14));
            p.setPen(Theme::kGreen);
            QString line;
            if (s->state() == MailSession::State::Error)
                line = tr("ERROR");
            else if (s->state() == MailSession::State::Busy)
                line = s->statusText();
            else
                line = unread == 0 ? tr("NO NEW MAIL") : tr("%1 NEW").arg(unread);
            p.drawText(label.adjusted(0, 22, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                       p.fontMetrics().elidedText(line, Qt::ElideRight, int(label.width())));
        }
    }

    // Single-account status line
    if (!status.isEmpty()) {
        p.setFont(Theme::plain(15));
        p.setPen(Theme::kGreen);
        const QRect line(24, height() - 34, width() - 48, 24);
        p.drawText(line, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(status, Qt::ElideRight, line.width()));
    }

    // Envelope zoom: pulled out of the box and enlarged, as in the reference.
    if (m_zoomIndex >= 0) {
        const qreal t = m_zoom.currentValue().toReal();
        const QRectF from = MailboxArt::envelopeRect(mailboxRect(m_zoomIndex));
        const qreal w = width() * 0.46;
        const QRectF to(width() * 0.10, height() * 0.42, w, w * 0.66);
        const QRectF cur(from.left() + (to.left() - from.left()) * t,
                         from.top() + (to.top() - from.top()) * t,
                         from.width() + (to.width() - from.width()) * t,
                         from.height() + (to.height() - from.height()) * t);
        MailboxArt::paintEnvelope(p, cur, 0.92, t);
    }
}

void MailboxScreen::open(int index)
{
    if (index < 0 || index >= m_sessions.size() || m_zoom.state() == QAbstractAnimation::Running)
        return;
    m_zoomIndex = index;
    m_zoom.start();
}

void MailboxScreen::mousePressEvent(QMouseEvent *e)
{
    open(hitTest(e->position()));
}

void MailboxScreen::mouseMoveEvent(QMouseEvent *e)
{
    setCursor(hitTest(e->position()) >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void MailboxScreen::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
        open(0);
        return;
    }
    if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_9) {
        open(e->key() - Qt::Key_1);
        return;
    }
    ScreenBackground::keyPressEvent(e);
}
