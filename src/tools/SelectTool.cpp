#include "tools/SelectTool.h"
#include "Canvas.h"
#include "Document.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

double SelectTool::handleSize() const
{
    return 8.0 / qMax(0.05, m_canvas->zoom());
}

QVector<QPointF> SelectTool::handlePositions(const QRect &r) const
{
    const QRectF f(r);
    return {
        f.topLeft(),  QPointF(f.center().x(), f.top()),    f.topRight(),
        QPointF(f.right(), f.center().y()),
        f.bottomRight(), QPointF(f.center().x(), f.bottom()), f.bottomLeft(),
        QPointF(f.left(), f.center().y())
    };
}

int SelectTool::handleAt(const QPointF &pos) const
{
    const SelectionState &sel = m_canvas->selection();
    if (!sel.active)
        return -1;

    const QVector<QPointF> handles = handlePositions(sel.rect);
    const double reach = handleSize();
    for (int i = 0; i < handles.size(); ++i) {
        if (qAbs(handles[i].x() - pos.x()) <= reach
            && qAbs(handles[i].y() - pos.y()) <= reach)
            return i;
    }
    return -1;
}

QRect SelectTool::resizedRect(const QRect &original, int handle, const QPointF &pos) const
{
    QRectF r(original);
    switch (handle) {
    case 0: r.setTopLeft(pos);     break;
    case 1: r.setTop(pos.y());     break;
    case 2: r.setTopRight(pos);    break;
    case 3: r.setRight(pos.x());   break;
    case 4: r.setBottomRight(pos); break;
    case 5: r.setBottom(pos.y());  break;
    case 6: r.setBottomLeft(pos);  break;
    case 7: r.setLeft(pos.x());    break;
    default: break;
    }
    QRect out = r.normalized().toAlignedRect();
    if (out.width() < 1)
        out.setWidth(1);
    if (out.height() < 1)
        out.setHeight(1);
    return out;
}

void SelectTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (button != Qt::LeftButton)
        return;

    m_button = button;
    const SelectionState &sel = m_canvas->selection();

    if (sel.active) {
        const int handle = handleAt(pos);
        if (handle >= 0) {
            m_canvas->floatSelection();
            m_activeHandle = handle;
            m_rectOrigin = m_canvas->selection().rect;
            m_state = State::Resizing;
            m_active = true;
            return;
        }

        if (sel.rect.contains(pos.toPoint())) {
            // Перетаскивание содержимого: пиксели «отрываются» от холста.
            m_canvas->floatSelection();
            m_dragOrigin = pos;
            m_rectOrigin = m_canvas->selection().rect;
            m_state = State::Moving;
            m_active = true;
            return;
        }

        // Клик вне рамки — старое выделение впечатывается в холст.
        m_canvas->finishSelection();
    }

    m_origin = pos;
    m_lasso.clear();
    if (m_freeForm)
        m_lasso.append(pos);
    m_state = State::Creating;
    m_active = true;
    m_canvas->clearSelectionShape();
    requestRepaint();
}

void SelectTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (!m_active)
        return;

    switch (m_state) {
    case State::Creating:
        if (m_freeForm) {
            m_lasso.append(pos);
        } else {
            QRect r = QRectF(m_origin, pos).normalized().toAlignedRect();
            r = r.intersected(doc()->image().rect());
            m_canvas->setPreviewSelectionRect(r);
        }
        requestRepaint();
        break;

    case State::Moving: {
        const QPoint delta = (pos - m_dragOrigin).toPoint();
        m_canvas->setSelectionRect(m_rectOrigin.translated(delta));
        break;
    }

    case State::Resizing:
        m_canvas->setSelectionRect(resizedRect(m_rectOrigin, m_activeHandle, pos));
        break;

    default:
        break;
    }
}

void SelectTool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(button) Q_UNUSED(mods)
    if (!m_active)
        return;

    if (m_state == State::Creating) {
        if (m_freeForm) {
            m_lasso.append(pos);
            if (m_lasso.size() > 2) {
                QPainterPath path;
                path.addPolygon(m_lasso);
                path.closeSubpath();
                m_canvas->beginFreeSelection(path);
            } else {
                m_canvas->clearSelectionShape();
            }
            m_lasso.clear();
        } else {
            QRect r = QRectF(m_origin, pos).normalized().toAlignedRect();
            r = r.intersected(doc()->image().rect());
            if (r.width() > 0 && r.height() > 0)
                m_canvas->beginRectSelection(r);
            else
                m_canvas->clearSelectionShape();
        }
    }

    m_state = State::Idle;
    m_active = false;
    m_activeHandle = -1;
    requestRepaint();
}

void SelectTool::paintOverlay(QPainter &painter)
{
    // Пунктир вокруг лассо, пока его ведут. Готовое выделение рисует Canvas.
    if (m_state == State::Creating && m_freeForm && m_lasso.size() > 1) {
        painter.save();
        QPen pen(Qt::black);
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(m_lasso);
        painter.drawLine(m_lasso.last(), m_lasso.first());
        painter.restore();
    }
}

QCursor SelectTool::cursor() const
{
    return QCursor(Qt::CrossCursor);
}

void SelectTool::commit()
{
    m_canvas->finishSelection();
    m_state = State::Idle;
    m_active = false;
}

void SelectTool::cancel()
{
    m_lasso.clear();
    m_state = State::Idle;
    m_active = false;
    m_canvas->finishSelection();
}
