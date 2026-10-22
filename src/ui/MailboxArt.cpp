#include "ui/MailboxArt.h"

#include "ui/Theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace MailboxArt {

namespace {

constexpr qreal kModel = 300.0;

QTransform modelTransform(const QRectF &target)
{
    const qreal s = qMin(target.width(), target.height()) / kModel;
    const qreal dx = target.left() + (target.width() - kModel * s) / 2;
    const qreal dy = target.top() + (target.height() - kModel * s) / 2;
    return QTransform(s, 0, 0, s, dx, dy);
}

QColor dim(const QColor &c, bool dimmed)
{
    return dimmed ? c.darker(170) : c;
}

const QRectF kPeekEnvelope(58, 104, 66, 44);

} // namespace

QRectF envelopeRect(const QRectF &target)
{
    return modelTransform(target).mapRect(kPeekEnvelope);
}

void paintMailbox(QPainter &p, const QRectF &target, const MailboxPose &pose)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);   // crisp 1998 edges
    p.setTransform(modelTransform(target), true);

    const QColor steel = dim(QColor(200, 208, 228), pose.dimmed);
    const QColor steelDark = dim(QColor(128, 140, 172), pose.dimmed);
    const QColor steelLight = dim(QColor(236, 240, 248), pose.dimmed);
    const QColor hole = dim(QColor(18, 34, 72), pose.dimmed);

    // Post
    QLinearGradient post(172, 0, 196, 0);
    post.setColorAt(0, steelDark);
    post.setColorAt(0.45, steelLight);
    post.setColorAt(1, steelDark);
    p.setPen(Qt::NoPen);
    p.setBrush(post);
    p.drawRoundedRect(QRectF(172, 140, 24, 152), 6, 6);

    // Body: a long box with a rounded back end
    QPainterPath body;
    body.moveTo(112, 66);
    body.lineTo(238, 60);
    body.cubicTo(262, 60, 268, 146, 244, 150);
    body.lineTo(112, 152);
    body.closeSubpath();
    QLinearGradient bodyGrad(0, 60, 0, 152);
    bodyGrad.setColorAt(0, steelLight);
    bodyGrad.setColorAt(0.6, steel);
    bodyGrad.setColorAt(1, steelDark);
    p.setBrush(bodyGrad);
    p.drawPath(body);

    // Front frame (arched opening)
    QPainterPath frame;
    frame.moveTo(86, 152);
    frame.lineTo(86, 82);
    frame.arcTo(QRectF(86, 62, 40, 40), 180, -180);
    frame.lineTo(126, 152);
    frame.closeSubpath();
    p.setBrush(steelDark);
    p.drawPath(frame);

    QPainterPath opening;
    opening.moveTo(92, 150);
    opening.lineTo(92, 86);
    opening.arcTo(QRectF(92, 68, 28, 34), 180, -180);
    opening.lineTo(120, 150);
    opening.closeSubpath();
    p.setBrush(hole);
    p.drawPath(opening);

    // Door, dropped open and hanging below the opening
    QPainterPath door;
    door.moveTo(90, 152);
    door.lineTo(124, 152);
    door.lineTo(104, 222);
    door.quadTo(96, 236, 80, 230);
    door.quadTo(68, 224, 72, 210);
    door.closeSubpath();
    QLinearGradient doorGrad(0, 152, 0, 232);
    doorGrad.setColorAt(0, steelDark);
    doorGrad.setColorAt(1, steel);
    p.setBrush(doorGrad);
    p.drawPath(door);

    // Envelope peeking out
    if (pose.hasMail)
        paintEnvelope(p, kPeekEnvelope, pose.dimmed ? 0.6 : 1.0);

    // Flag: rotates around a pivot on the side of the box.
    const QPointF pivot(150, 112);
    p.save();
    p.translate(pivot);
    p.rotate(108.0 * (1.0 - pose.flag));  // 0 deg = up, 108 deg = lying flat along the box
    p.translate(-pivot);
    const QColor red = dim(Theme::kRed, pose.dimmed);
    QPainterPath flag;
    flag.moveTo(150, 116);      // arm
    flag.lineTo(156, 110);
    flag.lineTo(134, 62);
    flag.lineTo(142, 30);       // flag head
    flag.lineTo(108, 52);
    flag.lineTo(126, 60);
    flag.lineTo(128, 64);
    flag.closeSubpath();
    p.setBrush(red);
    p.drawPath(flag);
    p.restore();

    p.restore();
}

void paintEnvelope(QPainter &p, const QRectF &r, qreal opacity, qreal open)
{
    p.save();
    p.setOpacity(p.opacity() * opacity);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor paper(214, 222, 238);
    const QColor edge(96, 112, 150);
    p.setPen(QPen(edge, qMax<qreal>(1.0, r.width() / 160.0)));
    p.setBrush(paper);
    p.drawRect(r);

    if (open > 0.0) {
        // A letter showing through, as in the reference "zoom" frame.
        p.save();
        p.setOpacity(p.opacity() * open * 0.7);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(236, 240, 248));
        p.drawRect(r.adjusted(r.width() * 0.15, r.height() * 0.06, -r.width() * 0.15, -r.height() * 0.12));
        p.restore();
    }

    QPen flap(edge, qMax<qreal>(1.0, r.width() / 110.0));
    p.setPen(flap);
    const QPointF tip(r.center().x(), r.top() + r.height() * 0.62);
    p.drawLine(r.topLeft(), tip);
    p.drawLine(r.topRight(), tip);
    p.restore();
}

void paintEnvelopeIcon(QPainter &p, const QRectF &r)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    const qreal w = qMax<qreal>(2.0, r.height() / 9.0);
    p.setPen(QPen(Theme::kInk, w, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);
    const QRectF box = r.adjusted(w / 2, w / 2, -w / 2, -w / 2);
    p.drawRect(box);
    const QPointF tip(box.center().x(), box.top() + box.height() * 0.58);
    p.drawLine(box.topLeft(), tip);
    p.drawLine(box.topRight(), tip);
    p.drawLine(box.bottomLeft(), QPointF(box.left() + box.width() * 0.38, box.top() + box.height() * 0.45));
    p.drawLine(box.bottomRight(), QPointF(box.right() - box.width() * 0.38, box.top() + box.height() * 0.45));
    p.restore();
}

void paintFlagIcon(QPainter &p, const QRectF &r)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::kRed);
    QPolygonF tri;
    tri << QPointF(r.left() + r.width() * 0.05, r.top() + r.height() * 0.42)
        << QPointF(r.left() + r.width() * 0.88, r.top())
        << QPointF(r.left() + r.width() * 0.62, r.top() + r.height() * 0.52)
        << QPointF(r.right(), r.bottom())
        << QPointF(r.left() + r.width() * 0.48, r.top() + r.height() * 0.62);
    p.drawPolygon(tri);
    p.restore();
}

} // namespace MailboxArt
