#pragma once

#include "tools/Tool.h"

#include <QRect>

class QTextEdit;

// Инструмент «Текст».
//
// Пока текст набирается, поверх холста висит настоящий QTextEdit — это даёт
// курсор, выделение, перенос строк и системный ввод (в том числе IME)
// без единой строчки собственного кода. При фиксации содержимое редактора
// отрисовывается в изображение, а виджет удаляется.
class TextTool : public Tool
{
public:
    explicit TextTool(Canvas *canvas) : Tool(canvas) {}
    ~TextTool() override;

    ToolId id() const override { return ToolId::Text; }

    void press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;
    void move(const QPointF &pos, Qt::KeyboardModifiers mods) override;
    void release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods) override;

    void paintOverlay(QPainter &painter) override;
    QCursor cursor() const override;

    // Вызывается и при смене масштаба/прокрутки, и при смене шрифта или цвета.
    void viewChanged() override;

    void commit() override;
    void cancel() override;

    bool isEditing() const { return m_editor != nullptr; }

private:
    void createEditor(const QRect &box);
    void destroyEditor();
    void syncEditorGeometry();

    QTextEdit *m_editor = nullptr;
    QRect m_box;
    QPointF m_origin;
    bool m_dragging = false;

    // Что уже применено к редактору — чтобы не трогать его лишний раз.
    QFont m_appliedFont;
    QColor m_appliedColor;
    double m_appliedScale = 0.0;
};
