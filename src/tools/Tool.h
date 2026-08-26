#pragma once

#include <QBrush>
#include <QColor>
#include <QCursor>
#include <QFont>
#include <QImage>
#include <QPainterPath>
#include <QPointF>
#include <QRect>
#include <QVector>

class QPainter;
class Canvas;
class Document;

// --- перечисления, общие для всего приложения ---------------------------

enum class ToolId {
    Select,          // прямоугольное выделение
    FreeSelect,      // произвольное выделение
    Pencil,
    Fill,
    Text,
    Eraser,
    ColorPicker,
    Magnifier,
    Brush,
    Shape
};

// Кисти из вкладки «Кисти» современного Paint.
enum class BrushType {
    Brush,
    Calligraphy1,
    Calligraphy2,
    Airbrush,
    OilBrush,
    Crayon,
    Marker,
    NaturalPencil,
    WatercolourBrush
};

enum class ShapeType {
    Line,
    Curve,
    Oval,
    Rectangle,
    RoundedRectangle,
    Polygon,
    Triangle,
    RightTriangle,
    Diamond,
    Pentagon,
    Hexagon,
    RightArrow,
    LeftArrow,
    UpArrow,
    DownArrow,
    FourPointStar,
    FivePointStar,
    SixPointStar,
    RoundedCallout,
    OvalCallout,
    CloudCallout,
    Heart,
    Lightning
};

// Способ нанесения краски. Один и тот же набор используется и кистями,
// и списками «Контур» / «Заливка» у фигур.
enum class StrokeStyle {
    Solid,
    Calligraphy1,
    Calligraphy2,
    Airbrush,
    Oil,
    Crayon,
    Marker,
    NaturalPencil,
    Watercolour
};

enum class FillMode {
    None,
    Solid,
    Crayon,
    Marker,
    Oil,
    NaturalPencil,
    Watercolour
};

StrokeStyle strokeStyleForBrush(BrushType brush);
StrokeStyle strokeStyleForFill(FillMode fill);

// --- текущие настройки инструментов -------------------------------------

struct ToolSettings {
    QColor color1 = QColor(0, 0, 0);
    QColor color2 = QColor(255, 255, 255);
    int size = 3;                                   // толщина 1..50
    int opacity = 100;                              // непрозрачность штриха, 1..100 %
    BrushType brush = BrushType::Brush;
    ShapeType shape = ShapeType::Line;
    StrokeStyle outline = StrokeStyle::Solid;
    bool hasOutline = true;
    FillMode fill = FillMode::None;
    int tolerance = 0;                              // допуск заливки, 0..100
    bool antialias = true;
    bool transparentSelection = false;
    QFont font = QFont(QStringLiteral("Sans Serif"), 12);
    bool textOpaque = false;                        // подложка под текстом Цветом 2
};

// --- утилиты рисования ---------------------------------------------------

namespace paintutil {

// Наносит отрезок выбранным стилем прямо в изображение.
// `phase` — сквозной счётчик шагов внутри одного штриха: текстурные кисти
// используют его, чтобы узор не «стоял на месте» при повторном проходе.
void drawStroke(QImage &target, const QPointF &from, const QPointF &to,
                const QColor &color, int width, StrokeStyle style,
                bool antialias, quint32 &phase);

// Обводит произвольный контур выбранным стилем.
void strokePath(QImage &target, const QPainterPath &path, const QColor &color,
                int width, StrokeStyle style, bool antialias);

// Кисть для заливки фигур (для текстурных стилей — тайл с шумом).
QBrush styledBrush(const QColor &color, FillMode fill);

// Прямоугольник, задетый отрезком с учётом толщины пера.
QRect strokeBounds(const QPointF &a, const QPointF &b, int width);

} // namespace paintutil

// --- базовый инструмент --------------------------------------------------

class Tool
{
public:
    explicit Tool(Canvas *canvas);
    virtual ~Tool();

    virtual ToolId id() const = 0;

    virtual void press(const QPointF &pos, Qt::MouseButton button,
                       Qt::KeyboardModifiers mods);
    virtual void move(const QPointF &pos, Qt::KeyboardModifiers mods);
    virtual void release(const QPointF &pos, Qt::MouseButton button,
                         Qt::KeyboardModifiers mods);
    virtual void doubleClick(const QPointF &pos, Qt::MouseButton button);

    // Рисование поверх холста в координатах изображения (превью фигур,
    // рамка выделения и т.п.).
    virtual void paintOverlay(QPainter &painter);

    // Инструменты с «продолжающимся» действием (аэрограф) возвращают true.
    virtual bool needsTick() const { return false; }
    virtual void tick();

    virtual QCursor cursor() const;

    // Масштаб или прокрутка изменились: инструментам с экранными
    // виджетами-накладками (текст) нужно переставить их на место.
    virtual void viewChanged();

    // Зафиксировать незавершённое построение (полигон, кривая, текст).
    virtual void commit();
    // Отменить незавершённое построение (Esc, смена инструмента).
    virtual void cancel();

    bool isBusy() const { return m_active; }

protected:
    Document *doc() const;
    ToolSettings &settings() const;
    QImage &image() const;

    // ЛКМ рисует Цветом 1, ПКМ — Цветом 2, как в Paint.
    QColor colorForButton(Qt::MouseButton button) const;
    QColor strokeColor() const { return colorForButton(m_button); }
    QColor alternateColor() const;

    void requestRepaint(const QRect &imageRect = QRect());

    Canvas *m_canvas = nullptr;
    bool m_active = false;
    Qt::MouseButton m_button = Qt::NoButton;
    quint32 m_phase = 0;
};
