#pragma once

#include <QColor>
#include <QFont>

class QApplication;
class QPainter;
class QRect;

namespace Theme {

// Palette taken from the reference screens: deep navy CRT, steel-blue
// panels, phosphor green status text, one red flag.
inline const QColor kNavyTop{3, 8, 26};
inline const QColor kNavyBottom{14, 34, 70};
inline const QColor kSlab{11, 21, 48};
inline const QColor kPanel{196, 204, 222};
inline const QColor kPanelLight{232, 236, 244};
inline const QColor kPanelShadow{86, 98, 130};
inline const QColor kInk{16, 26, 56};
inline const QColor kText{216, 222, 234};
inline const QColor kGreen{58, 184, 138};
inline const QColor kRed{214, 36, 36};

void apply(QApplication &app);

// "CRT mode": fonts drawn without anti-aliasing, like a 1998 screen.
bool crtFonts();
void setCrtFonts(bool on);

// Show message text in capitals, like the reference screens.
bool capsDisplay();
void setCapsDisplay(bool on);

QFont headline(int pixelSize);              // bold sans, labels and buttons
QFont plain(int pixelSize);                 // regular sans, status text
QFont body(int pixelSize, bool caps);       // message text

void paintBackground(QPainter &p, const QRect &rect);

} // namespace Theme
