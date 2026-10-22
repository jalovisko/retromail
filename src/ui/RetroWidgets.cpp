#include "ui/RetroWidgets.h"

#include "ui/MailboxArt.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

// ---------------------------------------------------------------------------
// RetroButton

RetroButton::RetroButton(const QString &text, QWidget *parent)
    : RetroButton(Icon::None, text, parent)
{
}

RetroButton::RetroButton(Icon icon, const QString &text, QWidget *parent)
    : QAbstractButton(parent)
    , m_icon(icon)
{
    setText(text);
    setFont(Theme::headline(18));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
}

QSize RetroButton::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textW = text().isEmpty() ? 0 : fm.horizontalAdvance(text());
    int w = textW + 40;
    if (m_icon == Icon::Envelope)
        w = textW + 64;
    else if (m_icon == Icon::Flag)
        w = 48;
    return {w, fm.height() + 18};
}

void RetroButton::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void RetroButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void RetroButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QRect r = rect();
    const bool down = isDown() || isChecked();
    const int bevel = 4;

    if (m_icon == Icon::Flag) {
        // The red flag is drawn bare on the panel, like the reference.
        QRectF icon = QRectF(r).adjusted(10, 6, -10, -6);
        if (down)
            icon.translate(1, 1);
        if (!isEnabled())
            p.setOpacity(0.4);
        MailboxArt::paintFlagIcon(p, icon);
        return;
    }

    p.fillRect(r, m_hover && isEnabled() ? Theme::kPanelLight : Theme::kPanel);
    p.setPen(Qt::NoPen);
    if (!down) {
        p.fillRect(QRect(r.left(), r.bottom() - bevel + 1, r.width(), bevel), Theme::kInk);
        p.fillRect(QRect(r.right() - bevel + 1, r.top() + r.height() / 3, bevel, r.height()), Theme::kInk);
    } else {
        p.fillRect(QRect(r.left(), r.top(), r.width(), 2), Theme::kInk);
        p.fillRect(QRect(r.left(), r.top(), 2, r.height()), Theme::kInk);
    }

    QRect content = r.adjusted(8, 2, -bevel - 6, -bevel);
    if (down)
        content.translate(1, 1);

    p.setPen(isEnabled() ? Theme::kInk : Theme::kPanelShadow);
    p.setFont(font());
    if (m_icon == Icon::Envelope) {
        const int h = qMin(content.height() - 8, 30);
        const QRectF env(content.left(), content.center().y() - h / 2.0, h * 1.45, h);
        MailboxArt::paintEnvelopeIcon(p, env);
        QFont small = Theme::headline(14);
        p.setFont(small);
        p.drawText(QRect(int(env.right()) + 6, content.top(), content.width(), content.height()),
                   Qt::AlignLeft | Qt::AlignTop, text());
    } else {
        p.drawText(content, Qt::AlignCenter, text());
    }

    if (hasFocus()) {
        p.setPen(QPen(Theme::kInk, 1, Qt::DotLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(content.adjusted(2, 2, -2, -2));
    }
}

// ---------------------------------------------------------------------------
// RetroToolbar

RetroToolbar::RetroToolbar(QWidget *parent)
    : QFrame(parent)
{
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(10, 8, 10, 14);
    outer->setSpacing(18);
    m_layout = new QHBoxLayout;
    m_layout->setSpacing(18);
    m_right = new QHBoxLayout;
    outer->addLayout(m_layout);
    outer->addStretch(1);
    outer->addLayout(m_right);
}

RetroButton *RetroToolbar::addButton(const QString &text)
{
    auto *b = new RetroButton(text, this);
    m_layout->addWidget(b);
    return b;
}

RetroButton *RetroToolbar::addInButton()
{
    auto *b = new RetroButton(RetroButton::Icon::Envelope, tr("IN"), this);
    b->setToolTip(tr("Inbox"));
    m_layout->addWidget(b);
    return b;
}

RetroButton *RetroToolbar::addBackFlag()
{
    auto *b = new RetroButton(RetroButton::Icon::Flag, QString(), this);
    b->setToolTip(tr("Back"));
    m_right->addWidget(b);
    return b;
}

void RetroToolbar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::kPanel);
    // Double rule under the toolbar, like the reference.
    p.fillRect(QRect(0, height() - 9, width(), 3), Theme::kInk);
    p.fillRect(QRect(0, height() - 4, width(), 4), Theme::kPanelShadow);
}

// ---------------------------------------------------------------------------
// LightPanel / ScreenBackground

LightPanel::LightPanel(QWidget *parent)
    : QFrame(parent)
{
    setAutoFillBackground(false);
}

void LightPanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect r = rect();
    p.fillRect(r, Theme::kPanel);
    p.setPen(Theme::kPanelLight);
    p.drawLine(r.topLeft(), r.topRight());
    p.drawLine(r.topLeft(), r.bottomLeft());
    p.setPen(QPen(Theme::kPanelShadow, 2));
    p.drawLine(r.bottomLeft() + QPoint(1, 0), r.bottomRight());
    p.drawLine(r.topRight() + QPoint(0, 1), r.bottomRight());
}

ScreenBackground::ScreenBackground(QWidget *parent)
    : QWidget(parent)
{
}

void ScreenBackground::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    Theme::paintBackground(p, rect());
}

QLabel *makeSlab(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("slab"));
    label->setFont(Theme::headline(20));
    label->setMinimumWidth(220);
    return label;
}
