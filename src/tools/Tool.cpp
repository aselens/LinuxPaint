#include "tools/Tool.h"
#include "Canvas.h"
#include "Document.h"

#include <QBrush>
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>

StrokeStyle strokeStyleForBrush(BrushType brush)
{
    switch (brush) {
    case BrushType::Brush:            return StrokeStyle::Solid;
    case BrushType::Calligraphy1:     return StrokeStyle::Calligraphy1;
    case BrushType::Calligraphy2:     return StrokeStyle::Calligraphy2;
    case BrushType::Airbrush:         return StrokeStyle::Airbrush;
    case BrushType::OilBrush:         return StrokeStyle::Oil;
    case BrushType::Crayon:           return StrokeStyle::Crayon;
    case BrushType::Marker:           return StrokeStyle::Marker;
    case BrushType::NaturalPencil:    return StrokeStyle::NaturalPencil;
    case BrushType::WatercolourBrush: return StrokeStyle::Watercolour;
    }
    return StrokeStyle::Solid;
}

StrokeStyle strokeStyleForFill(FillMode fill)
{
    switch (fill) {
    case FillMode::None:          return StrokeStyle::Solid;
    case FillMode::Solid:         return StrokeStyle::Solid;
    case FillMode::Crayon:        return StrokeStyle::Crayon;
    case FillMode::Marker:        return StrokeStyle::Marker;
    case FillMode::Oil:           return StrokeStyle::Oil;
    case FillMode::NaturalPencil: return StrokeStyle::NaturalPencil;
    case FillMode::Watercolour:   return StrokeStyle::Watercolour;
    }
    return StrokeStyle::Solid;
}

namespace paintutil {
namespace {

inline double randomDouble()
{
    return QRandomGenerator::global()->generateDouble();
}

inline double randomSigned(double amplitude)
{
    return (randomDouble() * 2.0 - 1.0) * amplitude;
}

QColor withAlpha(const QColor &color, int alpha)
{
    QColor c = color;
    c.setAlpha(qBound(1, alpha, 255));
    return c;
}

// Ставит один отпечаток кисти в точке. Вся «фактура» кистей живёт здесь,
// а обход отрезка одинаков для всех стилей.
void stamp(QPainter &p, const QPointF &at, double angle, const QColor &color,
           int width, StrokeStyle style, quint32 phase)
{
    const double w = qMax(1, width);

    switch (style) {
    case StrokeStyle::Solid:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(at, w / 2.0, w / 2.0);
        break;

    case StrokeStyle::Calligraphy1:
    case StrokeStyle::Calligraphy2: {
        // Перо: узкий прямоугольник под фиксированным углом.
        const double nibAngle = (style == StrokeStyle::Calligraphy1) ? -45.0 : 45.0;
        const double nibLength = w * 1.2;
        const double nibWidth = qMax(1.0, w / 3.0);
        p.save();
        p.translate(at);
        p.rotate(nibAngle);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(-nibLength / 2.0, -nibWidth / 2.0, nibLength, nibWidth));
        p.restore();
        break;
    }

    case StrokeStyle::Airbrush: {
        // Облако точек: плотнее в центре, реже к краю.
        const double radius = w * 1.5;
        const int drops = qMax(6, int(w * 1.5));
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(color, 200));
        for (int i = 0; i < drops; ++i) {
            const double a = randomDouble() * 2.0 * M_PI;
            const double r = radius * qSqrt(randomDouble());
            p.drawEllipse(QPointF(at.x() + qCos(a) * r, at.y() + qSin(a) * r), 0.6, 0.6);
        }
        break;
    }

    case StrokeStyle::Oil: {
        // Несколько «щетинок» вдоль направления движения.
        const int bristles = qMax(3, int(w / 2));
        p.save();
        p.translate(at);
        p.rotate(angle);
        for (int i = 0; i < bristles; ++i) {
            const double offset = (double(i) / qMax(1, bristles - 1) - 0.5) * w;
            const int alpha = 160 + int(randomDouble() * 80);
            QPen pen(withAlpha(color, alpha));
            pen.setWidthF(qMax(1.0, w / bristles * 0.9));
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(QPointF(-w * 0.25, offset + randomSigned(0.6)),
                       QPointF(w * 0.25, offset + randomSigned(0.6)));
        }
        p.restore();
        break;
    }

    case StrokeStyle::Crayon: {
        // Зернистый след: часть пикселей внутри пятна просто не ложится.
        const double radius = w / 2.0 + 0.5;
        const int grains = qMax(8, int(w * w * 0.35));
        p.setPen(Qt::NoPen);
        for (int i = 0; i < grains; ++i) {
            const double a = randomDouble() * 2.0 * M_PI;
            const double r = radius * qSqrt(randomDouble());
            if (randomDouble() < 0.35)
                continue;
            p.setBrush(withAlpha(color, 90 + int(randomDouble() * 130)));
            p.drawEllipse(QPointF(at.x() + qCos(a) * r, at.y() + qSin(a) * r), 0.7, 0.7);
        }
        break;
    }

    case StrokeStyle::Marker: {
        // Скошенный полупрозрачный наконечник.
        p.save();
        p.translate(at);
        p.rotate(-30.0);
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(color, 70));
        p.drawRect(QRectF(-w * 0.6, -w * 0.35, w * 1.2, w * 0.7));
        p.restore();
        break;
    }

    case StrokeStyle::NaturalPencil: {
        // Тонкий графитный след с лёгким дрожанием.
        const double radius = qMax(0.6, w / 2.5);
        const int grains = qMax(4, int(w * 1.5));
        p.setPen(Qt::NoPen);
        for (int i = 0; i < grains; ++i) {
            const double a = randomDouble() * 2.0 * M_PI;
            const double r = radius * randomDouble();
            p.setBrush(withAlpha(color, 70 + int(randomDouble() * 110)));
            p.drawEllipse(QPointF(at.x() + qCos(a) * r, at.y() + qSin(a) * r), 0.5, 0.5);
        }
        break;
    }

    case StrokeStyle::Watercolour: {
        // Несколько крупных полупрозрачных пятен — краска «растекается».
        const int blobs = 3;
        p.setPen(Qt::NoPen);
        for (int i = 0; i < blobs; ++i) {
            const double r = w * (0.5 + randomDouble() * 0.6);
            p.setBrush(withAlpha(color, 14 + int(randomDouble() * 18)));
            p.drawEllipse(QPointF(at.x() + randomSigned(w * 0.3),
                                  at.y() + randomSigned(w * 0.3)),
                          r, r);
        }
        break;
    }
    }

    Q_UNUSED(phase)
}

// Шаг между отпечатками. Плотные стили ставим чаще, «сыпучие» — реже,
// иначе они превращаются в сплошную заливку.
double stepForStyle(StrokeStyle style, int width)
{
    const double w = qMax(1, width);
    switch (style) {
    case StrokeStyle::Solid:          return qMax(0.5, w / 4.0);
    case StrokeStyle::Calligraphy1:
    case StrokeStyle::Calligraphy2:   return qMax(0.4, w / 6.0);
    case StrokeStyle::Airbrush:       return qMax(1.0, w / 3.0);
    case StrokeStyle::Oil:            return qMax(0.8, w / 4.0);
    case StrokeStyle::Crayon:         return qMax(1.0, w / 3.0);
    case StrokeStyle::Marker:         return qMax(0.6, w / 5.0);
    case StrokeStyle::NaturalPencil:  return qMax(0.7, w / 4.0);
    case StrokeStyle::Watercolour:    return qMax(1.2, w / 2.5);
    }
    return 1.0;
}

} // namespace

void drawStroke(QImage &target, const QPointF &from, const QPointF &to,
                const QColor &color, int width, StrokeStyle style,
                bool antialias, quint32 &phase)
{
    if (target.isNull())
        return;

    QPainter p(&target);
    p.setRenderHint(QPainter::Antialiasing, antialias);

    // Однопиксельная сплошная линия — это карандаш: ей нужны чёткие пиксели,
    // а не отпечатки кистью.
    if (style == StrokeStyle::Solid && (!antialias || width <= 1)) {
        QPen pen(color);
        pen.setWidth(qMax(1, width));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        if (from == to)
            p.drawPoint(from);
        else
            p.drawLine(from, to);
        ++phase;
        return;
    }

    const double dx = to.x() - from.x();
    const double dy = to.y() - from.y();
    const double distance = qSqrt(dx * dx + dy * dy);
    const double angle = qRadiansToDegrees(qAtan2(dy, dx));
    const double step = stepForStyle(style, width);
    const int steps = qMax(1, int(distance / step));

    for (int i = 0; i <= steps; ++i) {
        const double t = (steps == 0) ? 0.0 : double(i) / steps;
        const QPointF at(from.x() + dx * t, from.y() + dy * t);
        stamp(p, at, angle, color, width, style, phase++);
    }
}

void strokePath(QImage &target, const QPainterPath &path, const QColor &color,
                int width, StrokeStyle style, bool antialias)
{
    if (target.isNull() || path.isEmpty())
        return;

    if (style == StrokeStyle::Solid) {
        QPainter p(&target);
        p.setRenderHint(QPainter::Antialiasing, antialias);
        QPen pen(color);
        pen.setWidth(qMax(1, width));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        return;
    }

    // Текстурные стили наносим отпечатками, двигаясь по контуру.
    const double length = path.length();
    if (length <= 0.0)
        return;
    const double step = qMax(0.75, stepForStyle(style, width) * 0.75);
    const int steps = qMax(1, int(length / step));

    quint32 phase = 0;
    QPointF previous = path.pointAtPercent(0.0);
    for (int i = 1; i <= steps; ++i) {
        const QPointF current = path.pointAtPercent(double(i) / steps);
        drawStroke(target, previous, current, color, width, style, antialias, phase);
        previous = current;
    }
}

QBrush styledBrush(const QColor &color, FillMode fill)
{
    if (fill == FillMode::Solid || fill == FillMode::None)
        return QBrush(color);

    // Текстурная заливка — тайл 48×48, который Qt размножит по фигуре.
    const int tileSize = 48;
    QImage tile(tileSize, tileSize, QImage::Format_ARGB32);
    tile.fill(Qt::transparent);

    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    int passes = 0;
    int alphaLow = 0;
    int alphaHigh = 0;
    double radius = 1.0;

    switch (fill) {
    case FillMode::Crayon:        passes = 1400; alphaLow =  90; alphaHigh = 210; radius = 0.9; break;
    case FillMode::Marker:        passes =  900; alphaLow =  50; alphaHigh =  90; radius = 2.2; break;
    case FillMode::Oil:           passes = 1100; alphaLow = 170; alphaHigh = 250; radius = 1.8; break;
    case FillMode::NaturalPencil: passes =  900; alphaLow =  60; alphaHigh = 150; radius = 0.7; break;
    case FillMode::Watercolour:   passes =  600; alphaLow =  25; alphaHigh =  60; radius = 3.0; break;
    default:                      passes =  800; alphaLow = 200; alphaHigh = 255; radius = 1.0; break;
    }

    for (int i = 0; i < passes; ++i) {
        QColor c = color;
        c.setAlpha(alphaLow + int(randomDouble() * (alphaHigh - alphaLow)));
        p.setBrush(c);
        const double x = randomDouble() * tileSize;
        const double y = randomDouble() * tileSize;
        const double r = radius * (0.6 + randomDouble() * 0.8);
        p.drawEllipse(QPointF(x, y), r, r);
        // Дубли по краям, чтобы стык тайлов не бросался в глаза.
        if (x < r) p.drawEllipse(QPointF(x + tileSize, y), r, r);
        if (y < r) p.drawEllipse(QPointF(x, y + tileSize), r, r);
    }
    p.end();

    return QBrush(tile);
}

QRect strokeBounds(const QPointF &a, const QPointF &b, int width)
{
    QRectF r(a, b);
    r = r.normalized();
    const double pad = qMax(2, width * 2);
    return r.adjusted(-pad, -pad, pad, pad).toAlignedRect();
}

} // namespace paintutil

// --- Tool ----------------------------------------------------------------

Tool::Tool(Canvas *canvas)
    : m_canvas(canvas)
{
}

Tool::~Tool() = default;

Document *Tool::doc() const
{
    return m_canvas->document();
}

ToolSettings &Tool::settings() const
{
    return m_canvas->settings();
}

QImage &Tool::image() const
{
    // Полупрозрачный штрих сначала копится в отдельном слое и лишь потом
    // ложится на холст целиком. Иначе перекрывающиеся отпечатки кисти
    // складывали бы прозрачность сами с собой, и штрих выходил бы
    // плотным в местах наложения вместо ровной полупрозрачной линии.
    if (m_canvas->hasStrokeLayer())
        return m_canvas->strokeLayer();
    return m_canvas->document()->image();
}

QColor Tool::colorForButton(Qt::MouseButton button) const
{
    return (button == Qt::RightButton) ? settings().color2 : settings().color1;
}

QColor Tool::alternateColor() const
{
    return (m_button == Qt::RightButton) ? settings().color1 : settings().color2;
}

void Tool::requestRepaint(const QRect &imageRect)
{
    m_canvas->updateImageRect(imageRect);
}

void Tool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos) Q_UNUSED(mods)
    m_button = button;
    m_active = true;
}

void Tool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos) Q_UNUSED(mods)
}

void Tool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos) Q_UNUSED(button) Q_UNUSED(mods)
    m_active = false;
    m_button = Qt::NoButton;
}

void Tool::doubleClick(const QPointF &pos, Qt::MouseButton button)
{
    Q_UNUSED(pos) Q_UNUSED(button)
}

void Tool::paintOverlay(QPainter &painter)
{
    Q_UNUSED(painter)
}

void Tool::tick()
{
}

void Tool::viewChanged()
{
}

QCursor Tool::cursor() const
{
    return QCursor(Qt::CrossCursor);
}

void Tool::commit()
{
}

void Tool::cancel()
{
    m_active = false;
    m_button = Qt::NoButton;
}
