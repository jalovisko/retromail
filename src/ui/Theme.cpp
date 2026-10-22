#include "ui/Theme.h"

#include <QApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

namespace Theme {

namespace {
const QStringList kSansFamilies = {
    QStringLiteral("Helvetica"), QStringLiteral("Arial"), QStringLiteral("Liberation Sans"),
    QStringLiteral("Nimbus Sans"), QStringLiteral("DejaVu Sans"), QStringLiteral("sans-serif")};

QFont makeFont(int pixelSize, int weight)
{
    QFont f;
    f.setFamilies(kSansFamilies);
    f.setPixelSize(pixelSize);
    f.setWeight(QFont::Weight(weight));
    f.setStyleHint(QFont::Helvetica);
    if (crtFonts())
        f.setStyleStrategy(QFont::StyleStrategy(QFont::NoAntialias | QFont::PreferMatch));
    return f;
}
} // namespace

bool crtFonts()
{
    return QSettings().value(QStringLiteral("view/crtFonts"), true).toBool();
}

void setCrtFonts(bool on)
{
    QSettings().setValue(QStringLiteral("view/crtFonts"), on);
    QApplication::setFont(plain(13));
}

bool capsDisplay()
{
    return QSettings().value(QStringLiteral("view/caps"), true).toBool();
}

void setCapsDisplay(bool on)
{
    QSettings().setValue(QStringLiteral("view/caps"), on);
}

QFont headline(int pixelSize)
{
    return makeFont(pixelSize, QFont::Bold);
}

QFont plain(int pixelSize)
{
    return makeFont(pixelSize, QFont::Normal);
}

QFont body(int pixelSize, bool caps)
{
    QFont f = makeFont(pixelSize, QFont::DemiBold);
    if (caps)
        f.setCapitalization(QFont::AllUppercase);
    return f;
}

void apply(QApplication &app)
{
    // Qt still ships the classic Windows 95/98 widget style on every platform.
    if (auto *style = QStyleFactory::create(QStringLiteral("Windows")))
        app.setStyle(style);

    QPalette pal;
    pal.setColor(QPalette::Window, kPanel);
    pal.setColor(QPalette::WindowText, kInk);
    pal.setColor(QPalette::Base, kPanelLight);
    pal.setColor(QPalette::AlternateBase, kPanel);
    pal.setColor(QPalette::Text, kInk);
    pal.setColor(QPalette::Button, kPanel);
    pal.setColor(QPalette::ButtonText, kInk);
    pal.setColor(QPalette::Light, kPanelLight);
    pal.setColor(QPalette::Midlight, kPanel.lighter(108));
    pal.setColor(QPalette::Mid, kPanelShadow.lighter(130));
    pal.setColor(QPalette::Dark, kPanelShadow);
    pal.setColor(QPalette::Shadow, kSlab);
    pal.setColor(QPalette::Highlight, QColor(10, 36, 106));   // Windows 98 title-bar blue
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 225));
    pal.setColor(QPalette::ToolTipText, Qt::black);
    pal.setColor(QPalette::PlaceholderText, kPanelShadow);
    app.setPalette(pal);
    app.setFont(plain(13));

    app.setStyleSheet(QStringLiteral(R"(
        QLabel#slab {
            background: %1; color: %2;
            border: 3px solid %3; padding: 4px 12px;
        }
        QLineEdit#field, QComboBox#field {
            background: %3; color: %4;
            border: 3px solid %1; padding: 4px 10px;
            selection-background-color: %1; selection-color: %2;
        }
        QTextEdit#body, QPlainTextEdit#body {
            background: %1; color: %2;
            border: 3px solid %3; padding: 6px;
            selection-background-color: %2; selection-color: %1;
        }
        QLabel#status { color: %5; }
    )").arg(kSlab.name(), kText.name(), kPanel.name(), kInk.name(), kGreen.name()));
}

void paintBackground(QPainter &p, const QRect &rect)
{
    QLinearGradient g(rect.topLeft(), rect.bottomLeft());
    g.setColorAt(0.0, kNavyTop);
    g.setColorAt(1.0, kNavyBottom);
    p.fillRect(rect, g);

    // Faint scanlines for that cathode-ray glow.
    p.save();
    p.setPen(QColor(0, 0, 0, 38));
    for (int y = rect.top(); y < rect.bottom(); y += 3)
        p.drawLine(rect.left(), y, rect.right(), y);
    p.restore();
}

} // namespace Theme
