#pragma once

#include <QColor>
#include <QString>

// Оформление в духе Windows 11: скруглённые панели, подсветка при
// наведении, акцентный синий. Стиль задаётся кодом, а не системной темой
// рабочего стола, — иначе одно и то же окно выглядело бы по-разному
// в KDE, GNOME и XFCE.
namespace Theme {

enum class Mode { System, Light, Dark };

void apply(Mode mode);
Mode currentMode();
bool isDark();

QColor accent();
QColor iconForeground();
QString modeName(Mode mode);

// Подложка окна: ровный фон и цвет свечения в углу (сила — в его альфе).
QColor backdropBase();
QColor backdropGlow();

} // namespace Theme
