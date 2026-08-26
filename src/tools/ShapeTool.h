#pragma once

#include "tools/Tool.h"

#include <QPointF>
#include <QVector>

// Инструмент «Фигуры».
//
// Повторяет поведение современного Paint: после отпускания мыши фигура
// остаётся «живой» — её можно двигать и тянуть за маркеры, пока она не
// зафиксирована (Enter, клик мимо, смена инструмента или цвета).
// Отдельные сценарии — кривая (два изгиба) и многоугольник (клики по вершинам).
class ShapeTool : public Tool
{
public:
    explicit ShapeTool(Canvas *canvas) : Tool(canvas) {}

    ToolId id() const override { return ToolId::Shape; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void move(const QPointF &pos, Qt::KeyboardModifiers mods) override;
    void release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void doubleClick(const QPointF &pos, Qt::MouseButton button) override;

    void paintOverlay(QPainter &painter) override;
    QCursor cursor() const override;

    void commit() override;
    void cancel() override;

    bool hasPendingShape() const { return m_state != State::Idle; }

    // Строит контур фигуры по описывающему прямоугольнику.
    static QPainterPath pathForShape(ShapeType type, const QRectF &rect);

private:
    enum class State {
        Idle,
        Dragging,        // тянем новую фигуру
        Pending,         // фигура построена, но ещё редактируется
        MovingPending,   // тащим готовую фигуру целиком
        ResizingPending, // тянем за маркер
        CurveBend,       // задаём изгибы кривой
        PolygonBuilding  // набираем вершины многоугольника
    };

    QPainterPath currentPath() const;
    QRectF boundingRect() const;
    void applyConstraint(QPointF &end, const QPointF &start, Qt::KeyboardModifiers mods) const;

    QVector<QPointF> handlePositions() const;
    int handleAt(const QPointF &pos) const;
    double handleSize() const;
    void resizeBy(int handle, const QPointF &pos);

    State m_state = State::Idle;
    QPointF m_start;
    QPointF m_end;

    // Кривая Безье: две контрольные точки, задаются последовательно.
    QPointF m_control1;
    QPointF m_control2;
    int m_curveStage = 0;

    QVector<QPointF> m_polygon;
    QPointF m_polygonPreview;

    QPointF m_dragOrigin;
    QPointF m_shapeOriginStart;
    QPointF m_shapeOriginEnd;
    int m_activeHandle = -1;

    QColor m_outlineColor;
    QColor m_fillColor;
};
