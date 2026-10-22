#pragma once

#include <QRectF>

class QPainter;

// Vector drawings of the reference artwork: the rural mailbox with its red
// flag, and the envelope. All shapes are drawn in a 300x300 model space and
// scaled into the target rectangle.
namespace MailboxArt {

struct MailboxPose {
    qreal flag = 0.0;       // 0 = down, 1 = up
    bool hasMail = false;   // envelope peeking out
    bool dimmed = false;    // e.g. while connecting or on error
};

void paintMailbox(QPainter &p, const QRectF &target, const MailboxPose &pose);

// Where the peeking envelope sits, in target coordinates (for the zoom animation).
QRectF envelopeRect(const QRectF &target);

// Envelope with flap lines. `open` fades in a letter behind the flap.
void paintEnvelope(QPainter &p, const QRectF &rect, qreal opacity = 1.0, qreal open = 0.0);

// Small toolbar envelope icon (black outline on light background).
void paintEnvelopeIcon(QPainter &p, const QRectF &rect);

// Small red flag pointing up-left, used as the "back" button.
void paintFlagIcon(QPainter &p, const QRectF &rect);

} // namespace MailboxArt
