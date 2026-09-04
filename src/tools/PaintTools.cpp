#include "tools/PaintTools.h"
#include "Canvas.h"
#include "Document.h"

#include <QPainter>
#include <QtMath>

// --- карандаш ------------------------------------------------------------

int PencilTool::strokeWidth() const
{
    return qMax(1, settings().size);
}

void PencilTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;

    Tool::press(pos, button, mods);
    m_phase = 0;
    m_last = pos;

    doc()->beginEdit();
    // Слой заводится только при неполной непрозрачности; при 100 %
    // вызов ничего не делает и рисование идёт прямо в холст.
    m_canvas->beginStrokeLayer();

    // Мазок начинается здесь и живёт до отпускания кнопки: карта покрытия
    // общая на весь мазок, иначе перекрытия снова начали бы темнеть.
    m_stroke.begin(image().size(), strokeColor(), strokeWidth(), style(),
                   smooth() && settings().antialias);
    touchStroke(m_stroke.addSegment(image(), pos, pos));
}

void PencilTool::touchStroke(const QRect &dirty)
{
    // Пустая область значит, что ни один отпечаток не лёг: между двумя
    // событиями мыши не набралось и шага. Тревожить документ незачем —
    // иначе каждое такое движение перерисовывало бы весь холст.
    if (!dirty.isEmpty())
        doc()->touch(dirty);
}

void PencilTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    if (!m_active)
        return;

    QPointF target = pos;
    if (mods & Qt::ShiftModifier) {
        // Shift удерживает штрих строго по горизонтали или вертикали.
        const double dx = qAbs(pos.x() - m_last.x());
        const double dy = qAbs(pos.y() - m_last.y());
        if (dx > dy)
            target.setY(m_last.y());
        else
            target.setX(m_last.x());
    }

    touchStroke(m_stroke.addSegment(image(), m_last, target));
    m_last = target;
}

void PencilTool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    if (!m_active)
        return;
    Tool::release(pos, button, mods);
    m_stroke.end();
    m_canvas->commitStrokeLayer();
    doc()->endEdit();
}

// --- кисти ---------------------------------------------------------------

StrokeStyle BrushTool::style() const
{
    return strokeStyleForBrush(settings().brush);
}

int BrushTool::strokeWidth() const
{
    // Кисти заметно шире карандаша при том же значении «Размер».
    return qMax(1, settings().size * 2);
}

void BrushTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    m_hover = pos;
    PencilTool::move(pos, mods);
}

bool BrushTool::needsTick() const
{
    // Аэрограф продолжает распылять, пока кнопка нажата, даже если мышь стоит.
    return m_active && settings().brush == BrushType::Airbrush;
}

void BrushTool::tick()
{
    if (!needsTick())
        return;
    // Аэрограф копится: каждый тик добавляет краски в то же место, и пятно
    // постепенно набирает плотность — как у настоящего.
    touchStroke(m_stroke.addSegment(image(), m_hover, m_hover));
}

// --- ластик --------------------------------------------------------------

void EraserTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;

    Tool::press(pos, button, mods);
    m_last = pos;
    doc()->beginEdit();
    eraseSegment(pos, pos);
}

void EraserTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    m_hover = pos;
    m_hasHover = true;

    if (!m_active) {
        requestRepaint();
        return;
    }

    eraseSegment(m_last, pos);
    m_last = pos;
}

void EraserTool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    if (!m_active)
        return;
    Tool::release(pos, button, mods);
    doc()->endEdit();
}

void EraserTool::eraseSegment(const QPointF &from, const QPointF &to)
{
    const int size = qMax(1, settings().size * 3);
    const QColor target = settings().color2;

    if (m_button == Qt::RightButton) {
        // «Цветной ластик»: стираем только Цвет 1, всё остальное остаётся.
        QImage &img = image();
        const QRect area = paintutil::strokeBounds(from, to, size).intersected(img.rect());
        if (area.isEmpty())
            return;

        const QRgb from1 = settings().color1.rgb();
        const QRgb to2 = target.rgba();
        const double half = size / 2.0;

        const double dx = to.x() - from.x();
        const double dy = to.y() - from.y();
        const double lengthSquared = dx * dx + dy * dy;

        for (int y = area.top(); y <= area.bottom(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = area.left(); x <= area.right(); ++x) {
                if ((line[x] | 0xff000000u) != (from1 | 0xff000000u))
                    continue;

                // Проецируем пиксель на отрезок пути и смотрим, попадает ли он
                // в квадрат ластика в ближайшей точке. Это один проход по
                // площади вместо перебора всех промежуточных положений.
                const double px = x + 0.5;
                const double py = y + 0.5;
                double t = 0.0;
                if (lengthSquared > 0.0) {
                    t = ((px - from.x()) * dx + (py - from.y()) * dy) / lengthSquared;
                    t = qBound(0.0, t, 1.0);
                }
                const double cx = from.x() + dx * t;
                const double cy = from.y() + dy * t;

                if (qAbs(px - cx) <= half && qAbs(py - cy) <= half)
                    line[x] = to2;
            }
        }
        doc()->touch(area);
        return;
    }

    QPainter p(&image());
    QPen pen(target);
    pen.setWidth(size);
    pen.setCapStyle(Qt::SquareCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    if (from == to)
        p.drawPoint(from);
    else
        p.drawLine(from, to);
    p.end();

    doc()->touch(paintutil::strokeBounds(from, to, size));
}

void EraserTool::paintOverlay(QPainter &painter)
{
    if (!m_hasHover)
        return;
    const int size = qMax(1, settings().size * 3);
    const QRectF box(m_hover.x() - size / 2.0, m_hover.y() - size / 2.0, size, size);

    painter.save();
    painter.setBrush(Qt::NoBrush);
    QPen pen(QColor(0, 0, 0, 150));
    pen.setCosmetic(true);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(box);
    painter.restore();
}

QCursor EraserTool::cursor() const
{
    return QCursor(Qt::BlankCursor);
}

// --- заливка -------------------------------------------------------------

void FillTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;

    const QPoint p = pos.toPoint();
    if (!image().rect().contains(p))
        return;

    doc()->beginEdit();
    // Допуск задаётся в процентах, а сравниваем по каналам 0..255.
    const int tolerance = settings().tolerance * 255 / 100;
    const QRect filled = doc()->floodFill(p, colorForButton(button), tolerance);
    doc()->endEdit(filled);
}

QCursor FillTool::cursor() const
{
    return QCursor(Qt::PointingHandCursor);
}

// --- пипетка -------------------------------------------------------------

void ColorPickerTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    // Пипетка берёт то, что видно на экране, то есть цвет из склейки
    // слоёв, а не из одного лишь активного слоя.
    const QImage &visible = doc()->composite();
    const QPoint p = pos.toPoint();
    if (!visible.rect().contains(p))
        return;

    const QColor picked = QColor::fromRgba(visible.pixel(p));
    if (button == Qt::RightButton)
        m_canvas->setColor2(picked);
    else
        m_canvas->setColor1(picked);

    // Paint возвращает предыдущий инструмент сразу после взятия цвета.
    m_canvas->restorePreviousTool();
}

QCursor ColorPickerTool::cursor() const
{
    return QCursor(Qt::UpArrowCursor);
}

// --- лупа ----------------------------------------------------------------

void MagnifierTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (button == Qt::RightButton)
        m_canvas->zoomAtImagePoint(-1, pos);
    else
        m_canvas->zoomAtImagePoint(1, pos);
}

QCursor MagnifierTool::cursor() const
{
    return QCursor(Qt::PointingHandCursor);
}
