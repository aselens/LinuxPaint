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
//
// Рамку вокруг набранного текста можно тянуть за восемь маркеров и двигать
// за края — как в Paint. Маркеры и сама рамка нарочно вынесены за пределы
// редактора: он перехватывает мышь на всей своей площади, и попасть по
// маркеру, лежащему поверх него, было бы невозможно.
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
    // За что взялись: восемь маркеров рамки плюс перетаскивание целиком.
    enum class Grip {
        None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left, Move
    };

    void createEditor(const QRect &box);
    void destroyEditor();
    void syncEditorGeometry();
    void growToFit();                       // подрастить рамку под набранный текст

    QRect frameRect() const;                // рамка в координатах виджета
    QRect gripRect(const QPoint &centre) const;
    Grip gripAt(const QPoint &widgetPos) const;
    QRect boxForGrip(Grip grip, const QPointF &pos) const;

    QTextEdit *m_editor = nullptr;
    QRect m_box;
    QPointF m_origin;
    bool m_dragging = false;

    Grip m_grip = Grip::None;               // маркер, за который тянут сейчас
    Grip m_hoverGrip = Grip::None;          // маркер под курсором — для его вида
    QRect m_gripBox;                        // рамка на момент начала протяжки
    QPointF m_gripStart;                    // и точка, где мышь была прижата

    // Что уже применено к редактору — чтобы не трогать его лишний раз.
    QFont m_appliedFont;
    QColor m_appliedColor;
    double m_appliedScale = 0.0;
};
