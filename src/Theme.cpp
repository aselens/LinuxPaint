#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace Theme {
namespace {

Mode g_mode = Mode::System;
bool g_dark = false;

struct Colours {
    // Фон окна: ровная заливка плюс мягкое свечение из левого верхнего угла.
    // Сила свечения задана альфой в backdropGlow — оно должно лишь
    // угадываться, а не разливаться по всему окну.
    QColor backdropBase;
    QColor backdropGlow;
    QColor window;          // усреднённый цвет: его берёт палитра
    QColor ribbon;          // полоса инструментов, чуть темнее подложки
    QColor ribbonHover;
    QColor ribbonPressed;
    QColor ribbonTitle;     // подписи групп на серой ленте
    QColor surface;         // меню, строка состояния, диалоги
    QColor surfaceBorder;
    QColor text;
    QColor subtleText;
    QColor hover;
    QColor pressed;
    QColor checked;
    QColor checkedBorder;
    QColor canvasBackdrop;
    QColor accent;
    QColor separator;
    QColor base;            // поля ввода
    // Контрастный контур для того, что рисуется вручную: край холста,
    // черта линейки, рамки образцов цвета. Отдельно от границы панелей —
    // та намеренно еле заметна, а контур обязан быть виден.
    QColor outline;
};

Colours lightColours()
{
    Colours c;
    // Светлая тема Paint почти белая, с едва заметным холодным оттенком.
    // Подложка вокруг холста чуть плотнее ленты — так белый лист читается
    // на ней сам по себе, без тяжёлого серого фона.
    c.backdropBase   = QColor(0xF4, 0xF7, 0xFB);
    c.backdropGlow   = QColor(0x5A, 0x93, 0xE0, 40);
    c.window         = QColor(0xF5, 0xF8, 0xFC);
    c.ribbon         = QColor(0xEF, 0xF3, 0xF9);      // лента чуть темнее фона
    c.ribbonHover    = QColor(0xE1, 0xE7, 0xF0);
    c.ribbonPressed  = QColor(0xD2, 0xDA, 0xE5);
    c.ribbonTitle    = QColor(0x44, 0x4A, 0x52);
    c.canvasBackdrop = QColor(0xE7, 0xEC, 0xF4);
    c.surface        = QColor(0xFF, 0xFF, 0xFF);
    c.surfaceBorder  = QColor(0xDF, 0xE4, 0xEA);
    c.text           = QColor(0x1A, 0x1C, 0x1F);
    c.subtleText     = QColor(0x5A, 0x61, 0x69);
    c.hover          = QColor(0xE8, 0xEC, 0xF2);
    c.pressed        = QColor(0xD9, 0xDF, 0xE7);
    c.checked        = QColor(0xCC, 0xE4, 0xF7);
    c.checkedBorder  = QColor(0x00, 0x67, 0xC0);
    c.accent         = QColor(0x00, 0x67, 0xC0);
    c.separator      = QColor(0xDA, 0xDF, 0xE6);
    c.base           = QColor(0xFF, 0xFF, 0xFF);
    // Контур рисуется вручную вокруг холста и образцов цвета — на светлом
    // фоне он обязан быть заметно темнее его, иначе лист теряет края.
    c.outline        = QColor(0x9A, 0xA3, 0xAE);
    return c;
}

Colours darkColours()
{
    Colours c;
    // Синева заметна только вверху и постепенно сходит на нет к низу.
    // Остальные оттенки держим почти нейтральными: если подсинить всё
    // подряд, получится не «холодный тёмный», а «синий» — совсем другое.
    c.backdropBase   = QColor(0x1B, 0x1D, 0x23);
    c.backdropGlow   = QColor(0x46, 0x6E, 0xBE, 62);
    c.window         = QColor(0x1F, 0x21, 0x28);
    c.canvasBackdrop = QColor(0x1C, 0x1E, 0x25);
    c.ribbon         = QColor(0x17, 0x19, 0x1E);      // лента чуть темнее подложки
    c.ribbonHover    = QColor(0x24, 0x27, 0x2F);
    c.ribbonPressed  = QColor(0x2E, 0x32, 0x3C);
    c.ribbonTitle    = QColor(0xB4, 0xB8, 0xC0);
    c.surface        = QColor(0x24, 0x26, 0x2D);
    c.surfaceBorder  = QColor(0x32, 0x35, 0x3D);
    c.text           = QColor(0xED, 0xEF, 0xF2);
    c.subtleText     = QColor(0xA6, 0xAB, 0xB4);
    c.hover          = QColor(0x2F, 0x33, 0x3C);
    c.pressed        = QColor(0x3A, 0x3E, 0x49);
    c.checked        = QColor(0x2D, 0x44, 0x5C);
    c.checkedBorder  = QColor(0x4C, 0xC2, 0xFF);
    c.accent         = QColor(0x4C, 0xC2, 0xFF);
    c.separator      = QColor(0x32, 0x35, 0x3D);
    c.base           = QColor(0x19, 0x1B, 0x21);
    c.outline        = QColor(0x66, 0x6C, 0x76);
    return c;
}

QString css(const QColor &c)
{
    return QStringLiteral("rgb(%1,%2,%3)").arg(c.red()).arg(c.green()).arg(c.blue());
}

// Полупрозрачный вариант цвета. Слои поверх подложки обязаны быть
// прозрачными: иначе градиент окажется закрыт наглухо сплошной заливкой
// и весь смысл подложки пропадёт.
QString cssa(const QColor &c, double alpha)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(qBound(0.0, alpha, 1.0), 'f', 3));
}

bool systemPrefersDark()
{
    // Совместимо и с Qt 6.2, где ещё нет QStyleHints::colorScheme().
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

void applyPalette(const Colours &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.surface);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.surface);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, g_dark ? QColor(0x10, 0x10, 0x10)
                                                 : QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::ToolTipBase, c.surface);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.subtleText);
    // Ссылки в тексте (окно «О программе») — акцентным цветом темы.
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::LinkVisited, c.accent);
    // Mid используется холстом как цвет подложки вокруг листа.
    p.setColor(QPalette::Mid, c.canvasBackdrop);
    p.setColor(QPalette::Shadow, c.outline);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.subtleText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.subtleText);
    p.setColor(QPalette::Disabled, QPalette::Text, c.subtleText);
    QApplication::setPalette(p);
}

QString buildStyleSheet(const Colours &c)
{
    return QStringLiteral(R"(
QWidget {
    font-size: 13px;
}

/* Градиент рисует виджет-подложка (Backdrop), он занимает всю площадь окна.
   Здесь достаточно ровной заливки — она видна лишь по самым краям. */
QMainWindow {
    background: %WINDOW%;
}
QDialog {
    background: %SURFACE%;
}

/* --- верхняя строка меню --- */
QMenuBar {
    background: transparent;
    padding: 2px 4px;
}
QMenuBar::item {
    background: transparent;
    padding: 5px 12px;
    border-radius: 5px;
    color: %TEXT%;
}
QMenuBar::item:selected, QMenuBar::item:pressed {
    background: %HOVER%;
}

QMenu {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 5px;
}
QMenu::item {
    padding: 7px 30px 7px 30px;
    border-radius: 5px;
    color: %TEXT%;
}
QMenu::item:selected {
    background: %HOVER%;
}
QMenu::item:disabled {
    color: %SUBTLE%;
}
QMenu::separator {
    height: 1px;
    background: %SEPARATOR%;
    margin: 5px 8px;
}
QMenu::icon {
    padding-left: 8px;
}

/* --- лента --- */
/* Лента идёт от края до края окна, поэтому ни скругления, ни рамки по
   периметру у неё нет — только черта, отделяющая её от рабочей области. */
QWidget#Ribbon {
    background: %RIBBONLAYER%;
    border: none;
    border-bottom: 1px solid %SEPARATOR%;
    border-radius: 0;
}
QLabel#RibbonGroupTitle {
    color: %SUBTLE%;
    font-size: 11px;
}
QWidget#Ribbon QLabel#RibbonGroupTitle {
    color: %RIBBONTITLE%;
}
/* Вертикальная черта между группами инструментов. */
QFrame#RibbonSeparator {
    background: %SEPARATOR%;
    max-width: 1px;
    border: none;
    margin-top: 4px;
    margin-bottom: 4px;
}

/* Подсветку кнопок в ленте задаём отдельно: на сером фоне нужны свои
   оттенки, иначе наведение выглядело бы вспышкой. */
QWidget#Ribbon QToolButton:hover {
    background: %RIBBONHOVER%;
}
QWidget#Ribbon QToolButton:pressed {
    background: %RIBBONPRESSED%;
}
/* Идёт после наведения намеренно: при равной специфичности побеждает
   последнее правило, и отметка выбранного инструмента не пропадает
   под курсором. */
QWidget#Ribbon QToolButton:checked {
    background: %CHECKED%;
    border: 1px solid %CHECKEDBORDER%;
}

QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px;
    color: %TEXT%;
}
QToolButton:hover {
    background: %HOVER%;
}
QToolButton:pressed {
    background: %PRESSED%;
}
QToolButton:checked {
    background: %CHECKED%;
    border: 1px solid %CHECKEDBORDER%;
}
QToolButton:disabled {
    color: %SUBTLE%;
}
QToolButton::menu-indicator {
    subcontrol-origin: padding;
    subcontrol-position: bottom right;
    width: 10px;
}

/* --- верхняя строка: Файл / Правка / Вид --- */
QToolButton#TopMenuButton {
    padding: 5px 12px;
    border-radius: 5px;
    color: %TEXT%;
}
QToolButton#TopMenuButton::menu-indicator {
    image: none;
    width: 0;
    height: 0;
}

/* Штатную стрелку меню в ленте не рисуем нигде: у кнопок с меню шеврон
   уже встроен в сам значок, а стрелка Qt добавляла бы к нему вторую
   и растягивала кнопку по ширине. */
QToolButton#RibbonChevron::menu-indicator,
QWidget#Ribbon QToolButton::menu-indicator {
    image: none;
    width: 0;
    height: 0;
}

QPushButton {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 6px 16px;
    color: %TEXT%;
}
QPushButton:hover   { background: %HOVER%; }
QPushButton:pressed { background: %PRESSED%; }
QPushButton:default { border: 1px solid %ACCENT%; }

QComboBox, QSpinBox, QLineEdit, QDoubleSpinBox {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 4px 8px;
    color: %TEXT%;
    selection-background-color: %ACCENT%;
}
QComboBox:hover, QSpinBox:hover, QLineEdit:hover { border: 1px solid %ACCENT%; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    selection-background-color: %HOVER%;
    selection-color: %TEXT%;
    outline: none;
}

QCheckBox, QRadioButton, QLabel { color: %TEXT%; }

/* Поля по краям дорожки — место под ручку. Без них в крайних положениях
   ручка вылезает за границу виджета и срезается наполовину. */
QSlider::groove:horizontal {
    height: 4px;
    background: %BORDER%;
    border-radius: 2px;
    margin: 0 8px;
}
QSlider::sub-page:horizontal {
    background: %ACCENT%;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: %ACCENT%;
    width: 14px;
    height: 14px;
    margin: -6px 0;
    border-radius: 7px;
}

/* Вертикальный ползунок толщины у левого края рабочей области.
   У вертикального QSlider минимум внизу, поэтому закрашенной должна быть
   add-page — часть под ручкой. */
/* Дорожка нарочно уже ручки, а ручка выходит за неё вбок отрицательными
   полями. Ширину виджета под это задаём явно (см. MainWindow): если
   положиться на расчётную, отрицательные поля в неё не попадают, и ручка
   срезается по бокам. Поля сверху и снизу — место под ручку в крайних
   положениях. */
QSlider::groove:vertical {
    width: 6px;
    background: %BORDER%;
    border-radius: 3px;
    margin: 9px 0;
}
QSlider::add-page:vertical {
    background: %ACCENT%;
    border-radius: 3px;
}
QSlider::handle:vertical {
    background: %ACCENT%;
    width: 16px;
    height: 16px;
    margin: 0 -5px;
    border-radius: 8px;
}

/* Блоки ползунков не затемняем: темнее общего фона должна быть только
   лента инструментов. Эти — наоборот, светлые карточки поверх подложки. */
QWidget#SliderBox {
    background: %SURFACE%;
    border: 1px solid %SEPARATOR%;
    border-radius: 8px;
}
QWidget#SliderBox QLabel {
    color: %TEXT%;
    font-size: 11px;
    border: none;
}

/* --- панель слоёв --- */
QWidget#LayersPanel {
    background: %SURFACE%;
    border: 1px solid %SEPARATOR%;
    border-radius: 8px;
}
QToolButton#LayerEye {
    background: rgba(0, 0, 0, 90);
    border-radius: 9px;
    padding: 0;
}
QToolButton#LayerEye:hover {
    background: %ACCENT%;
}

QStatusBar {
    background: %SURFACE%;
    border-top: 1px solid %BORDER%;
    color: %TEXT%;
}
QStatusBar QLabel { color: %TEXT%; }
QStatusBar::item { border: none; }

/* Область вокруг холста — полупрозрачный слой, а не сплошная заливка:
   градиент должен просвечивать и здесь, это самая большая площадь в окне. */
QScrollArea,
QScrollArea > QWidget > QWidget {
    border: none;
    background: %BACKDROPLAYER%;
}

QScrollBar:vertical {
    background: transparent;
    width: 12px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: %SCROLL%;
    border-radius: 5px;
    min-height: 30px;
    margin: 2px;
}
QScrollBar:horizontal {
    background: transparent;
    height: 12px;
    margin: 0;
}
QScrollBar::handle:horizontal {
    background: %SCROLL%;
    border-radius: 5px;
    min-width: 30px;
    margin: 2px;
}
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QToolTip {
    background: %SURFACE%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 6px;
    padding: 4px 8px;
}

QTabWidget::pane { border: none; }
)")
        .replace(QStringLiteral("%WINDOW%"), css(c.window))
        .replace(QStringLiteral("%RIBBONHOVER%"), css(c.ribbonHover))
        .replace(QStringLiteral("%RIBBONPRESSED%"), css(c.ribbonPressed))
        .replace(QStringLiteral("%RIBBONTITLE%"), css(c.ribbonTitle))
        // Лента — полупрозрачный слой: сквозь неё виден градиент подложки,
        // но сама она остаётся темнее фона.
        .replace(QStringLiteral("%RIBBONLAYER%"), cssa(c.ribbon, 0.72))
        .replace(QStringLiteral("%RIBBON%"), css(c.ribbon))
        .replace(QStringLiteral("%SURFACE%"), css(c.surface))
        .replace(QStringLiteral("%BORDER%"), css(c.surfaceBorder))
        .replace(QStringLiteral("%TEXT%"), css(c.text))
        .replace(QStringLiteral("%SUBTLE%"), css(c.subtleText))
        .replace(QStringLiteral("%HOVER%"), css(c.hover))
        .replace(QStringLiteral("%PRESSED%"), css(c.pressed))
        .replace(QStringLiteral("%CHECKEDBORDER%"), css(c.checkedBorder))
        .replace(QStringLiteral("%CHECKED%"), css(c.checked))
        .replace(QStringLiteral("%BACKDROPLAYER%"), cssa(c.canvasBackdrop, 0.55))
        .replace(QStringLiteral("%BACKDROP%"), css(c.canvasBackdrop))
        .replace(QStringLiteral("%ACCENT%"), css(c.accent))
        .replace(QStringLiteral("%SEPARATOR%"), css(c.separator))
        .replace(QStringLiteral("%SCROLL%"), css(c.subtleText))
        .replace(QStringLiteral("%BASE%"), css(c.base));
}

} // namespace

void apply(Mode mode)
{
    g_mode = mode;

    if (mode == Mode::System)
        g_dark = systemPrefersDark();
    else
        g_dark = (mode == Mode::Dark);

    // Fusion — единственный стиль, одинаково доступный во всех окружениях;
    // на нём таблица стилей ведёт себя предсказуемо.
    if (QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const Colours colours = g_dark ? darkColours() : lightColours();
    applyPalette(colours);
    qApp->setStyleSheet(buildStyleSheet(colours));
}

Mode currentMode()
{
    return g_mode;
}

bool isDark()
{
    return g_dark;
}

QColor accent()
{
    return g_dark ? darkColours().accent : lightColours().accent;
}

QColor iconForeground()
{
    return g_dark ? QColor(0xE0, 0xE0, 0xE0) : QColor(0x3C, 0x3C, 0x3C);
}

QColor backdropBase()
{
    return g_dark ? darkColours().backdropBase : lightColours().backdropBase;
}

QColor backdropGlow()
{
    return g_dark ? darkColours().backdropGlow : lightColours().backdropGlow;
}

QString modeName(Mode mode)
{
    switch (mode) {
    case Mode::System: return QStringLiteral("Как в системе");
    case Mode::Light:  return QStringLiteral("Светлая");
    case Mode::Dark:   return QStringLiteral("Тёмная");
    }
    return QString();
}

} // namespace Theme
