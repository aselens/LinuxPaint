#pragma once

#include "tools/Tool.h"

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QSize>

// Мазок движком libmypaint.
//
// libmypaint — это кисти MyPaint, вынесенные в отдельную библиотеку; тот же
// движок стоит в Krita и в GIMP. Он умеет то, чего от рисования кодом
// добиться тяжело: сглаживание хода руки, зависимость следа от скорости,
// подхват цвета с холста, аккуратные отпечатки с дробным центром и
// собственным сглаживанием края.
//
// Библиотека необязательна. Если её нет в системе, класс остаётся пустышкой:
// isAvailable() возвращает false, и рисование идёт собственным движком
// (paintutil::Stroke). Сборка при этом не меняется вовсе — ни одного нового
// требования не появляется.
//
// Поверхность libmypaint используется как отдельный прозрачный слой, а не
// как сам холст. Поэтому после каждого движения мыши задетый кусок холста
// пересобирается заново из копии, снятой в начале мазка: сколько бы раз
// слой ни ложился поверх, темнее он не станет.
class MyPaintEngine
{
public:
    MyPaintEngine();
    ~MyPaintEngine();

    MyPaintEngine(const MyPaintEngine &) = delete;
    MyPaintEngine &operator=(const MyPaintEngine &) = delete;

    // Собрана ли программа с libmypaint и удалось ли её завести.
    static bool isAvailable();

    // Начинает мазок. target — то полотно, в которое пойдёт результат;
    // с него же снимается копия для пересборки.
    bool begin(const QImage &target, const QColor &colour, int width,
               StrokeStyle style, bool antialias);

    // Подаёт очередную точку пути. seconds — время с прошлой точки: движок
    // отличает быстрый росчерк от медленного ведения. Возвращает область
    // холста, которую нужно перерисовать.
    QRect motion(QImage &target, const QPointF &pos, double seconds);

    void end();
    bool isActive() const;

private:
    struct Private;
    Private *d = nullptr;
};
