#pragma once

#include "tools/Tool.h"

#include <QColor>
#include <QIcon>

// Иконки рисуются кодом: приложение остаётся одним бинарником без
// файлов ресурсов, а тема (светлая/тёмная) меняет их цвет на лету.
// Кисти и фигуры рисуют себя теми же функциями, что и на холсте, —
// значок всегда совпадает с реальным результатом.
namespace Icons {

enum class Action {
    New, Open, Save, SaveAs, Print, Properties, Exit,
    Undo, Redo,
    Cut, Copy, Paste,
    Crop, ResizeImage, RotateRight, RotateLeft, FlipHorizontal, FlipVertical,
    SelectAll, InvertSelection, DeleteSelection, InvertColours, ClearImage,
    ZoomIn, ZoomOut, ZoomReset, Grid, Rulers, Fullscreen,
    EditColours, SwapColours, Size, About,
    Share, Settings, Chevron, ChevronUp,
    CursorPosition, CanvasSize, FitToWindow, Opacity,
    Layers, AddLayer, LayerVisible, LayerHidden
};

void setForeground(const QColor &colour);
QColor foreground();

QIcon action(Action id, int size = 24);

// Тот же значок, но с маленьким шевроном в правом нижнем углу — признак
// кнопки, раскрывающей меню. Рисуется в сам значок, чтобы не включать
// у QToolButton режим с отдельной половинкой под стрелку: он добавляет
// разделитель и лишнюю ширину, ломая сетку кнопок.
QIcon withChevron(const QIcon &base, int size = 24);
QIcon tool(ToolId id, int size = 24);
QIcon brush(BrushType id, int size = 24);
// Широкий образец мазка для галереи кистей: одна плавная волна во всю
// ширину, нанесённая той самой кистью, — как в Paint.
QIcon brushSample(BrushType id, const QSize &size);
QIcon shape(ShapeType id, int size = 24);
QIcon colourSwatch(const QColor &colour, int size = 24);
QIcon strokeStyle(StrokeStyle style, int size = 24);
QIcon fillStyle(FillMode fill, int size = 24);
QIcon lineWidth(int width, int size = 24);
QIcon application();

} // namespace Icons
