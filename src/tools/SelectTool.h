#pragma once

#include "tools/Tool.h"

#include <QPolygonF>
#include <QRect>

// Прямоугольное и произвольное выделение.
//
// Сами пиксели и рамка живут в Canvas — ими пользуются буфер обмена,
// обрезка и повороты. Инструмент отвечает только за работу мышью:
// создать рамку, подвинуть, потянуть за маркер.
class SelectTool : public Tool
{
public:
    SelectTool(Canvas *canvas, bool freeForm)
        : Tool(canvas), m_freeForm(freeForm) {}

    ToolId id() const override { return m_freeForm ? ToolId::FreeSelect : ToolId::Select; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void move(const QPointF &pos, Qt::KeyboardModifiers mods) override;
    void release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    void paintOverlay(QPainter &painter) override;
    QCursor cursor() const override;

    void commit() override;
    void cancel() override;

private:
    enum class State { Idle, Creating, Moving, Resizing };

    QVector<QPointF> handlePositions(const QRect &rect) const;
    int handleAt(const QPointF &pos) const;
    double handleSize() const;
    QRect resizedRect(const QRect &original, int handle, const QPointF &pos) const;

    bool m_freeForm = false;
    State m_state = State::Idle;

    QPointF m_origin;
    QPointF m_dragOrigin;
    QRect m_rectOrigin;
    int m_activeHandle = -1;

    QPolygonF m_lasso;
};
