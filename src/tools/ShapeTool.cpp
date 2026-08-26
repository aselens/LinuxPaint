#include "tools/ShapeTool.h"
#include "Canvas.h"
#include "Document.h"

#include <QPainter>
#include <QtMath>

namespace {

// Правильный многоугольник/звезда, вписанные в прямоугольник.
QPainterPath starPath(const QRectF &rect, int points, double innerRatio)
{
    QPainterPath path;
    const QPointF center = rect.center();
    const double rx = rect.width() / 2.0;
    const double ry = rect.height() / 2.0;
    const int total = (innerRatio > 0.0) ? points * 2 : points;

    for (int i = 0; i < total; ++i) {
        const double ratio = (innerRatio > 0.0 && (i % 2)) ? innerRatio : 1.0;
        const double angle = -M_PI / 2.0 + (2.0 * M_PI * i) / total;
        const QPointF p(center.x() + qCos(angle) * rx * ratio,
                        center.y() + qSin(angle) * ry * ratio);
        if (i == 0)
            path.moveTo(p);
        else
            path.lineTo(p);
    }
    path.closeSubpath();
    return path;
}

QPainterPath polygonFromNormalized(const QRectF &rect, const QVector<QPointF> &pts)
{
    QPainterPath path;
    for (int i = 0; i < pts.size(); ++i) {
        const QPointF p(rect.left() + pts[i].x() * rect.width(),
                        rect.top() + pts[i].y() * rect.height());
        if (i == 0)
            path.moveTo(p);
        else
            path.lineTo(p);
    }
    path.closeSubpath();
    return path;
}

} // namespace

QPainterPath ShapeTool::pathForShape(ShapeType type, const QRectF &rect)
{
    QPainterPath path;
    auto P = [&rect](double u, double v) {
        return QPointF(rect.left() + u * rect.width(), rect.top() + v * rect.height());
    };

    switch (type) {
    case ShapeType::Line:
    case ShapeType::Curve:
    case ShapeType::Polygon:
        // Эти три строятся не по описывающему прямоугольнику.
        break;

    case ShapeType::Oval:
        path.addEllipse(rect);
        break;

    case ShapeType::Rectangle:
        path.addRect(rect);
        break;

    case ShapeType::RoundedRectangle: {
        const double radius = qMin(rect.width(), rect.height()) * 0.18;
        path.addRoundedRect(rect, radius, radius);
        break;
    }

    case ShapeType::Triangle:
        path = polygonFromNormalized(rect, {{0.5, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
        break;

    case ShapeType::RightTriangle:
        path = polygonFromNormalized(rect, {{0.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
        break;

    case ShapeType::Diamond:
        path = polygonFromNormalized(rect, {{0.5, 0.0}, {1.0, 0.5}, {0.5, 1.0}, {0.0, 0.5}});
        break;

    case ShapeType::Pentagon:
        path = starPath(rect, 5, 0.0);
        break;

    case ShapeType::Hexagon:
        path = polygonFromNormalized(rect, {{0.25, 0.0}, {0.75, 0.0}, {1.0, 0.5},
                                            {0.75, 1.0}, {0.25, 1.0}, {0.0, 0.5}});
        break;

    case ShapeType::RightArrow:
        path = polygonFromNormalized(rect, {{0.0, 0.3}, {0.6, 0.3}, {0.6, 0.05},
                                            {1.0, 0.5}, {0.6, 0.95}, {0.6, 0.7}, {0.0, 0.7}});
        break;

    case ShapeType::LeftArrow:
        path = polygonFromNormalized(rect, {{1.0, 0.3}, {0.4, 0.3}, {0.4, 0.05},
                                            {0.0, 0.5}, {0.4, 0.95}, {0.4, 0.7}, {1.0, 0.7}});
        break;

    case ShapeType::UpArrow:
        path = polygonFromNormalized(rect, {{0.3, 1.0}, {0.3, 0.4}, {0.05, 0.4},
                                            {0.5, 0.0}, {0.95, 0.4}, {0.7, 0.4}, {0.7, 1.0}});
        break;

    case ShapeType::DownArrow:
        path = polygonFromNormalized(rect, {{0.3, 0.0}, {0.3, 0.6}, {0.05, 0.6},
                                            {0.5, 1.0}, {0.95, 0.6}, {0.7, 0.6}, {0.7, 0.0}});
        break;

    case ShapeType::FourPointStar:
        path = starPath(rect, 4, 0.32);
        break;

    case ShapeType::FivePointStar:
        path = starPath(rect, 5, 0.38);
        break;

    case ShapeType::SixPointStar:
        path = starPath(rect, 6, 0.55);
        break;

    case ShapeType::RoundedCallout: {
        // Тело выноски занимает верхние 3/4, снизу слева — хвостик.
        QRectF body(rect.left(), rect.top(), rect.width(), rect.height() * 0.75);
        const double radius = qMin(body.width(), body.height()) * 0.2;
        path.addRoundedRect(body, radius, radius);
        QPainterPath tail;
        tail.moveTo(P(0.22, 0.7));
        tail.lineTo(P(0.12, 1.0));
        tail.lineTo(P(0.42, 0.7));
        tail.closeSubpath();
        path = path.united(tail);
        break;
    }

    case ShapeType::OvalCallout: {
        QRectF body(rect.left(), rect.top(), rect.width(), rect.height() * 0.75);
        path.addEllipse(body);
        QPainterPath tail;
        tail.moveTo(P(0.25, 0.68));
        tail.lineTo(P(0.12, 1.0));
        tail.lineTo(P(0.45, 0.72));
        tail.closeSubpath();
        path = path.united(tail);
        break;
    }

    case ShapeType::CloudCallout: {
        // Облако собираем из перекрывающихся эллипсов и склеиваем в один контур.
        const QVector<QPointF> bubbles = {
            {0.30, 0.36}, {0.50, 0.26}, {0.70, 0.34}, {0.82, 0.48},
            {0.66, 0.60}, {0.44, 0.62}, {0.24, 0.54}
        };
        const QVector<QPointF> radii = {
            {0.16, 0.16}, {0.20, 0.19}, {0.18, 0.17}, {0.15, 0.14},
            {0.17, 0.15}, {0.19, 0.16}, {0.16, 0.15}
        };
        for (int i = 0; i < bubbles.size(); ++i) {
            QPainterPath bubble;
            const QPointF c = P(bubbles[i].x(), bubbles[i].y());
            bubble.addEllipse(c, radii[i].x() * rect.width(), radii[i].y() * rect.height());
            path = path.isEmpty() ? bubble : path.united(bubble);
        }
        // Два «пузырька» к говорящему.
        QPainterPath small1;
        small1.addEllipse(P(0.28, 0.80), 0.06 * rect.width(), 0.055 * rect.height());
        QPainterPath small2;
        small2.addEllipse(P(0.18, 0.93), 0.035 * rect.width(), 0.035 * rect.height());
        path = path.united(small1).united(small2);
        break;
    }

    case ShapeType::Heart:
        path.moveTo(P(0.5, 1.0));
        path.cubicTo(P(-0.10, 0.44), P(0.14, -0.12), P(0.5, 0.28));
        path.cubicTo(P(0.86, -0.12), P(1.10, 0.44), P(0.5, 1.0));
        path.closeSubpath();
        break;

    case ShapeType::Lightning:
        path = polygonFromNormalized(rect, {{0.52, 0.0}, {1.0, 0.0}, {0.63, 0.38},
                                            {0.88, 0.38}, {0.28, 1.0}, {0.44, 0.55},
                                            {0.13, 0.55}});
        break;
    }

    return path;
}

// --- построение текущего контура ----------------------------------------

QPainterPath ShapeTool::currentPath() const
{
    const ShapeType type = settings().shape;
    QPainterPath path;

    if (type == ShapeType::Line) {
        path.moveTo(m_start);
        path.lineTo(m_end);
        return path;
    }

    if (type == ShapeType::Curve) {
        path.moveTo(m_start);
        if (m_curveStage == 0)
            path.lineTo(m_end);
        else
            path.cubicTo(m_control1, m_control2, m_end);
        return path;
    }

    if (type == ShapeType::Polygon) {
        if (m_polygon.isEmpty())
            return path;
        path.moveTo(m_polygon.first());
        for (int i = 1; i < m_polygon.size(); ++i)
            path.lineTo(m_polygon[i]);
        if (m_state == State::PolygonBuilding)
            path.lineTo(m_polygonPreview);
        else
            path.closeSubpath();
        return path;
    }

    return pathForShape(type, boundingRect());
}

QRectF ShapeTool::boundingRect() const
{
    return QRectF(m_start, m_end).normalized();
}

void ShapeTool::applyConstraint(QPointF &end, const QPointF &start,
                                Qt::KeyboardModifiers mods) const
{
    if (!(mods & Qt::ShiftModifier))
        return;

    const double dx = end.x() - start.x();
    const double dy = end.y() - start.y();

    if (settings().shape == ShapeType::Line || settings().shape == ShapeType::Curve) {
        // Линию Shift кладёт на ближайший угол, кратный 45°.
        const double length = qSqrt(dx * dx + dy * dy);
        const double angle = qAtan2(dy, dx);
        const double snapped = qRound(angle / (M_PI / 4.0)) * (M_PI / 4.0);
        end = QPointF(start.x() + qCos(snapped) * length,
                      start.y() + qSin(snapped) * length);
        return;
    }

    // Остальные фигуры Shift делает «квадратными».
    const double side = qMax(qAbs(dx), qAbs(dy));
    end = QPointF(start.x() + (dx < 0 ? -side : side),
                  start.y() + (dy < 0 ? -side : side));
}

// --- маркеры -------------------------------------------------------------

double ShapeTool::handleSize() const
{
    // Маркеры должны быть одного размера на экране при любом масштабе.
    return 8.0 / qMax(0.05, m_canvas->zoom());
}

QVector<QPointF> ShapeTool::handlePositions() const
{
    const ShapeType type = settings().shape;
    if (type == ShapeType::Line || type == ShapeType::Curve)
        return {m_start, m_end};
    if (type == ShapeType::Polygon)
        return m_polygon;

    const QRectF r = boundingRect();
    return {
        r.topLeft(),    QPointF(r.center().x(), r.top()),    r.topRight(),
        QPointF(r.right(), r.center().y()),
        r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(),
        QPointF(r.left(), r.center().y())
    };
}

int ShapeTool::handleAt(const QPointF &pos) const
{
    const QVector<QPointF> handles = handlePositions();
    const double reach = handleSize();
    for (int i = 0; i < handles.size(); ++i) {
        if (qAbs(handles[i].x() - pos.x()) <= reach
            && qAbs(handles[i].y() - pos.y()) <= reach)
            return i;
    }
    return -1;
}

void ShapeTool::resizeBy(int handle, const QPointF &pos)
{
    const ShapeType type = settings().shape;

    if (type == ShapeType::Line || type == ShapeType::Curve) {
        if (handle == 0)
            m_start = pos;
        else
            m_end = pos;
        return;
    }

    if (type == ShapeType::Polygon) {
        if (handle >= 0 && handle < m_polygon.size())
            m_polygon[handle] = pos;
        return;
    }

    QRectF r = boundingRect();
    switch (handle) {
    case 0: r.setTopLeft(pos);                    break;
    case 1: r.setTop(pos.y());                    break;
    case 2: r.setTopRight(pos);                   break;
    case 3: r.setRight(pos.x());                  break;
    case 4: r.setBottomRight(pos);                break;
    case 5: r.setBottom(pos.y());                 break;
    case 6: r.setBottomLeft(pos);                 break;
    case 7: r.setLeft(pos.x());                   break;
    default: break;
    }
    r = r.normalized();
    m_start = r.topLeft();
    m_end = r.bottomRight();
}

// --- обработка мыши ------------------------------------------------------

void ShapeTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    if (button != Qt::LeftButton && button != Qt::RightButton)
        return;

    const ShapeType type = settings().shape;

    // 1. Редактирование уже построенной фигуры.
    if (m_state == State::Pending) {
        const int handle = handleAt(pos);
        if (handle >= 0) {
            m_activeHandle = handle;
            m_state = State::ResizingPending;
            m_active = true;
            return;
        }
        if (currentPath().boundingRect().adjusted(-2, -2, 2, 2).contains(pos)) {
            m_dragOrigin = pos;
            m_shapeOriginStart = m_start;
            m_shapeOriginEnd = m_end;
            m_state = State::MovingPending;
            m_active = true;
            return;
        }
        // Клик мимо фигуры фиксирует её и начинает новую.
        commit();
    }

    // 2. Изгиб кривой.
    if (m_state == State::CurveBend) {
        m_active = true;
        return;
    }

    // 3. Многоугольник: каждый клик — новая вершина.
    if (type == ShapeType::Polygon) {
        if (m_state != State::PolygonBuilding) {
            m_outlineColor = colorForButton(button);
            m_fillColor = (button == Qt::RightButton) ? settings().color1 : settings().color2;
            m_button = button;
            m_polygon.clear();
            m_polygon.append(pos);
            m_polygonPreview = pos;
            m_state = State::PolygonBuilding;
            m_active = true;
            requestRepaint();
            return;
        }

        // Клик рядом с первой вершиной замыкает контур.
        const double reach = handleSize();
        if (m_polygon.size() >= 3
            && qAbs(m_polygon.first().x() - pos.x()) <= reach
            && qAbs(m_polygon.first().y() - pos.y()) <= reach) {
            commit();
            return;
        }

        m_polygon.append(pos);
        m_polygonPreview = pos;
        requestRepaint();
        return;
    }

    // 4. Новая фигура.
    m_outlineColor = colorForButton(button);
    m_fillColor = (button == Qt::RightButton) ? settings().color1 : settings().color2;
    m_button = button;
    m_start = pos;
    m_end = pos;
    m_curveStage = 0;
    m_state = State::Dragging;
    m_active = true;
    requestRepaint();
}

void ShapeTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    switch (m_state) {
    case State::Dragging: {
        QPointF end = pos;
        applyConstraint(end, m_start, mods);
        m_end = end;
        requestRepaint();
        break;
    }

    case State::MovingPending: {
        const QPointF delta = pos - m_dragOrigin;
        m_start = m_shapeOriginStart + delta;
        m_end = m_shapeOriginEnd + delta;
        requestRepaint();
        break;
    }

    case State::ResizingPending:
        resizeBy(m_activeHandle, pos);
        requestRepaint();
        break;

    case State::CurveBend:
        if (m_active) {
            if (m_curveStage == 1) {
                m_control1 = pos;
                m_control2 = pos;
            } else {
                m_control2 = pos;
            }
            requestRepaint();
        }
        break;

    case State::PolygonBuilding:
        m_polygonPreview = pos;
        requestRepaint();
        break;

    default:
        break;
    }
}

void ShapeTool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(button)

    switch (m_state) {
    case State::Dragging: {
        QPointF end = pos;
        applyConstraint(end, m_start, mods);
        m_end = end;

        if (settings().shape == ShapeType::Curve) {
            // Прямая проведена — дальше два перетаскивания задают изгибы.
            m_control1 = m_start + (m_end - m_start) / 3.0;
            m_control2 = m_start + (m_end - m_start) * 2.0 / 3.0;
            m_curveStage = 1;
            m_state = State::CurveBend;
            m_active = false;
        } else {
            m_state = State::Pending;
            m_active = false;
        }
        requestRepaint();
        break;
    }

    case State::MovingPending:
    case State::ResizingPending:
        m_state = State::Pending;
        m_activeHandle = -1;
        m_active = false;
        requestRepaint();
        break;

    case State::CurveBend:
        if (m_active) {
            m_active = false;
            if (m_curveStage >= 2)
                commit();
            else
                ++m_curveStage;
        }
        break;

    default:
        m_active = false;
        break;
    }
}

void ShapeTool::doubleClick(const QPointF &pos, Qt::MouseButton button)
{
    Q_UNUSED(pos) Q_UNUSED(button)
    if (m_state == State::PolygonBuilding && m_polygon.size() >= 2)
        commit();
    else if (m_state == State::Pending || m_state == State::CurveBend)
        commit();
}

// --- отрисовка -----------------------------------------------------------

void ShapeTool::paintOverlay(QPainter &painter)
{
    if (m_state == State::Idle)
        return;

    const QPainterPath path = currentPath();
    if (path.isEmpty())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, settings().antialias);

    const bool closed = settings().shape != ShapeType::Line
                        && settings().shape != ShapeType::Curve;

    if (closed && settings().fill != FillMode::None) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(paintutil::styledBrush(m_fillColor, settings().fill));
        painter.drawPath(path);
    }

    if (settings().hasOutline) {
        QPen pen(m_outlineColor);
        pen.setWidthF(qMax(1, settings().size));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    // Маркеры показываем только когда фигуру ещё можно править.
    if (m_state == State::Pending || m_state == State::MovingPending
        || m_state == State::ResizingPending) {
        const double s = handleSize();
        QPen framePen(QColor(0, 120, 215));
        framePen.setCosmetic(true);
        framePen.setStyle(Qt::DashLine);
        painter.setPen(framePen);
        painter.setBrush(Qt::NoBrush);
        if (settings().shape != ShapeType::Line && settings().shape != ShapeType::Curve)
            painter.drawRect(boundingRect());

        painter.setPen(QPen(QColor(0, 120, 215), 0));
        painter.setBrush(QColor(255, 255, 255));
        const QVector<QPointF> handles = handlePositions();
        for (const QPointF &h : handles)
            painter.drawRect(QRectF(h.x() - s / 2.0, h.y() - s / 2.0, s, s));
    }

    painter.restore();
}

QCursor ShapeTool::cursor() const
{
    return QCursor(Qt::CrossCursor);
}

// --- фиксация ------------------------------------------------------------

void ShapeTool::commit()
{
    if (m_state == State::Idle)
        return;

    const QPainterPath path = currentPath();
    const ShapeType type = settings().shape;
    const bool closed = type != ShapeType::Line && type != ShapeType::Curve;

    // Многоугольник из одной точки рисовать нечего.
    if (path.isEmpty() || (type == ShapeType::Polygon && m_polygon.size() < 2)) {
        cancel();
        return;
    }

    doc()->beginEdit();
    // Контур и заливка должны лечь одной полупрозрачной фигурой, а не
    // наложиться друг на друга — поэтому оба идут в общий слой штриха.
    m_canvas->beginStrokeLayer();

    if (closed && settings().fill != FillMode::None) {
        QPainter p(&image());
        p.setRenderHint(QPainter::Antialiasing, settings().antialias);
        p.setPen(Qt::NoPen);
        p.setBrush(paintutil::styledBrush(m_fillColor, settings().fill));
        p.drawPath(path);
    }

    if (settings().hasOutline) {
        QPainterPath outline = path;
        if (type == ShapeType::Polygon)
            outline.closeSubpath();
        paintutil::strokePath(image(), outline, m_outlineColor,
                              qMax(1, settings().size), settings().outline,
                              settings().antialias);
    }

    const int pad = qMax(4, settings().size * 2);
    const QRect dirty = path.boundingRect().toAlignedRect().adjusted(-pad, -pad, pad, pad);

    m_state = State::Idle;
    m_curveStage = 0;
    m_polygon.clear();
    m_active = false;
    m_activeHandle = -1;

    m_canvas->commitStrokeLayer();
    doc()->endEdit(dirty);
    requestRepaint();
}

void ShapeTool::cancel()
{
    m_state = State::Idle;
    m_curveStage = 0;
    m_polygon.clear();
    m_active = false;
    m_activeHandle = -1;
    m_button = Qt::NoButton;
    requestRepaint();
}
