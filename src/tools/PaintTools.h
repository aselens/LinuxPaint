#pragma once

#include "tools/Tool.h"

// Карандаш. Толщиной в один пиксель всегда идёт чётким пером — сглаживать
// там нечего, а размытая точка вместо пикселя только мешает. Толще — идёт
// со сглаженным краем, как кисти, если сглаживание включено в настройках.
class PencilTool : public Tool
{
public:
    explicit PencilTool(Canvas *canvas) : Tool(canvas) {}
    ToolId id() const override { return ToolId::Pencil; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void move(const QPointF &pos, Qt::KeyboardModifiers mods) override;
    void release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

protected:
    virtual StrokeStyle style() const { return StrokeStyle::Solid; }
    // Слушается ли инструмент настройки сглаживания. Чёткость линии
    // в один пиксель обеспечивает сам движок мазка, не этот флаг.
    virtual bool smooth() const { return true; }
    virtual int strokeWidth() const;

    // Сообщает документу о задетой области, пропуская пустые.
    void touchStroke(const QRect &dirty);

    QPointF m_last;
    // Накопитель мазка: он и решает, как отпечатки складываются между
    // собой, поэтому живёт от нажатия до отпускания кнопки.
    paintutil::Stroke m_stroke;
};

// Кисти: тот же обход, но стиль берётся из выбранной кисти.
class BrushTool : public PencilTool
{
public:
    explicit BrushTool(Canvas *canvas) : PencilTool(canvas) {}
    ToolId id() const override { return ToolId::Brush; }

    void move(const QPointF &pos, Qt::KeyboardModifiers mods) override;
    bool needsTick() const override;
    void tick() override;

protected:
    StrokeStyle style() const override;
    int strokeWidth() const override;

private:
    QPointF m_hover;
};

// Ластик. ЛКМ закрашивает Цветом 2, ПКМ работает «цветным ластиком»:
// заменяет только пиксели Цвета 1, остальное не трогает.
class EraserTool : public Tool
{
public:
    explicit EraserTool(Canvas *canvas) : Tool(canvas) {}
    ToolId id() const override { return ToolId::Eraser; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void move(const QPointF &pos, Qt::KeyboardModifiers mods) override;
    void release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void paintOverlay(QPainter &painter) override;
    QCursor cursor() const override;

private:
    void eraseSegment(const QPointF &from, const QPointF &to);

    QPointF m_last;
    QPointF m_hover;
    bool m_hasHover = false;
};

// Заливка области.
class FillTool : public Tool
{
public:
    explicit FillTool(Canvas *canvas) : Tool(canvas) {}
    ToolId id() const override { return ToolId::Fill; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    QCursor cursor() const override;
};

// Пипетка: берёт цвет и возвращает предыдущий инструмент.
class ColorPickerTool : public Tool
{
public:
    explicit ColorPickerTool(Canvas *canvas) : Tool(canvas) {}
    ToolId id() const override { return ToolId::ColorPicker; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    QCursor cursor() const override;
};

// Лупа: ЛКМ приближает, ПКМ отдаляет.
class MagnifierTool : public Tool
{
public:
    explicit MagnifierTool(Canvas *canvas) : Tool(canvas) {}
    ToolId id() const override { return ToolId::Magnifier; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    QCursor cursor() const override;
};
