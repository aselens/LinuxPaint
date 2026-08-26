#pragma once

#include <QColor>
#include <QVector>
#include <QWidget>

class QGridLayout;

// Одна ячейка палитры. ЛКМ задаёт Цвет 1, ПКМ — Цвет 2 (как в Paint).
class Swatch : public QWidget
{
    Q_OBJECT

public:
    explicit Swatch(const QColor &colour, QWidget *parent = nullptr);

    QColor colour() const { return m_colour; }
    void setColour(const QColor &colour);
    void setSelected(bool selected);
    void setEmpty(bool empty);
    bool isEmpty() const { return m_empty; }

signals:
    void picked(const QColor &colour, Qt::MouseButton button);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QColor m_colour;
    bool m_selected = false;
    bool m_hovered = false;
    bool m_empty = false;
};

// Блок «Цвета» в ленте: два текущих цвета, стандартная палитра,
// ряд пользовательских цветов и кнопка вызова диалога.
class ColorArea : public QWidget
{
    Q_OBJECT

public:
    explicit ColorArea(QWidget *parent = nullptr);

    QColor colour1() const { return m_colour1; }
    QColor colour2() const { return m_colour2; }

    void setColour1(const QColor &colour);
    void setColour2(const QColor &colour);
    void addCustomColour(const QColor &colour);

    QVector<QColor> customColours() const;
    void setCustomColours(const QVector<QColor> &colours);

    bool isEditingSecondary() const { return m_editingSecondary; }

signals:
    void colour1Changed(const QColor &colour);
    void colour2Changed(const QColor &colour);

private:
    void buildPalette(QGridLayout *grid);
    void onSwatchPicked(const QColor &colour, Qt::MouseButton button);

    QColor m_colour1 = Qt::black;
    QColor m_colour2 = Qt::white;
    bool m_editingSecondary = false;

    Swatch *m_primarySwatch = nullptr;
    Swatch *m_secondarySwatch = nullptr;
    QVector<Swatch *> m_customSwatches;
    int m_nextCustom = 0;
};
