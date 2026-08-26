#pragma once

#include "Theme.h"

#include <QDialog>
#include <QSize>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QSpinBox;

// Настройки приложения, которые переживают перезапуск.
struct AppSettings {
    Theme::Mode theme = Theme::Mode::System;
    QSize defaultCanvas = QSize(1152, 648);
    bool antialias = true;
    bool rulers = false;
    bool grid = false;
    bool statusBar = true;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings &current, QWidget *parent = nullptr);

    AppSettings result() const;

private:
    QComboBox *m_theme = nullptr;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QCheckBox *m_antialias = nullptr;
    QCheckBox *m_rulers = nullptr;
    QCheckBox *m_grid = nullptr;
    QCheckBox *m_statusBar = nullptr;
};

// «Изменение размеров и наклона» — точный аналог одноимённого окна Paint.
class ResizeSkewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ResizeSkewDialog(const QSize &current, QWidget *parent = nullptr);

    bool resizeByPercent() const;
    QSize resultSize() const;
    double skewHorizontal() const;
    double skewVertical() const;
    bool keepAspectRatio() const;

private:
    void onPercentToggled(bool percent);
    void onHorizontalChanged();
    void onVerticalChanged();

    QSize m_original;
    bool m_updating = false;

    QRadioButton *m_percentButton = nullptr;
    QRadioButton *m_pixelButton = nullptr;
    QSpinBox *m_horizontal = nullptr;
    QSpinBox *m_vertical = nullptr;
    QCheckBox *m_keepAspect = nullptr;
    QSpinBox *m_skewH = nullptr;
    QSpinBox *m_skewV = nullptr;
};

// «Свойства изображения»: размер холста в пикселях, сантиметрах или дюймах.
class AttributesDialog : public QDialog
{
    Q_OBJECT

public:
    AttributesDialog(const QSize &current, const QString &fileInfo,
                     QWidget *parent = nullptr);

    QSize resultSize() const;

private:
    void onUnitChanged(int index);

    QSize m_original;
    int m_unitIndex = 0;
    QDoubleSpinBox *m_width = nullptr;
    QDoubleSpinBox *m_height = nullptr;
    QComboBox *m_units = nullptr;
};

void showAboutDialog(QWidget *parent);
