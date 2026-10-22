#pragma once

#include <QAbstractButton>
#include <QFrame>
#include <QWidget>

class QHBoxLayout;
class QLabel;

// Light steel button with a heavy dark bevel at the bottom/right edges.
class RetroButton : public QAbstractButton
{
    Q_OBJECT
public:
    enum class Icon { None, Envelope, Flag };

    explicit RetroButton(const QString &text, QWidget *parent = nullptr);
    RetroButton(Icon icon, const QString &text, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    Icon m_icon = Icon::None;
    bool m_hover = false;
};

// Light panel strip across the top of a screen: [IN] [A] [B] ...   [flag]
class RetroToolbar : public QFrame
{
    Q_OBJECT
public:
    explicit RetroToolbar(QWidget *parent = nullptr);

    RetroButton *addButton(const QString &text);
    RetroButton *addInButton();
    RetroButton *addBackFlag();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QHBoxLayout *m_layout;
    QHBoxLayout *m_right;
};

// Raised light panel, like the REGISTERED USER / GUEST box.
class LightPanel : public QFrame
{
    Q_OBJECT
public:
    explicit LightPanel(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

// Navy CRT backdrop with scanlines; base for every screen.
class ScreenBackground : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenBackground(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;
};

// "Message To:" style label slab.
QLabel *makeSlab(const QString &text, QWidget *parent = nullptr);
