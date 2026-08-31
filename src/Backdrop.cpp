#include "Backdrop.h"
#include "WallpaperData.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

Backdrop::Backdrop(QWidget *parent)
    : QWidget(parent)
{
}

void Backdrop::setTint(const QColor &tint)
{
    if (m_tint == tint)
        return;
    m_tint = tint;
    m_backdrop = QImage();      // пересоберётся при следующей отрисовке
    update();
}

void Backdrop::rebuild()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QSize target = screen->geometry().size();
    if (target.isEmpty())
        return;

    // Разворачиваем сетку цветов в изображение.
    QImage image(WallpaperData::kWidth, WallpaperData::kHeight, QImage::Format_RGB32);
    for (int y = 0; y < WallpaperData::kHeight; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < WallpaperData::kWidth; ++x)
            line[x] = 0xFF000000u | WallpaperData::kPixels[y * WallpaperData::kWidth + x];
    }

    // Растягиваем в несколько приёмов, а не одним прыжком: при увеличении
    // сразу во много раз линейная интерполяция даёт видимые грани между
    // пикселями, а несколько умеренных шагов сглаживают их до неразличимости.
    while (image.width() * 4 < target.width())
        image = image.scaled(image.size() * 4, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    image = image.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Подкрашиваем, только если тема просит. Картинка приходит уже готовой,
    // и в тёмной теме её ничем портить не нужно — там альфа нулевая.
    if (m_tint.alpha() > 0) {
        QPainter painter(&image);
        painter.fillRect(image.rect(), m_tint);
        painter.end();
    }

    m_backdrop = image;
}

void Backdrop::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);

    // Экран проверяем до всего остального. Раньше проверка стояла так, что
    // при уже построенной картинке и недоступном экране до обращения к нему
    // дело всё-таки доходило — по пустому указателю, с падением программы.
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        painter.fillRect(rect(), QColor(0x1B, 0x1D, 0x23));
        return;
    }

    if (m_backdrop.isNull() || m_backdrop.size() != screen->geometry().size())
        rebuild();

    if (m_backdrop.isNull()) {
        painter.fillRect(rect(), QColor(0x1B, 0x1D, 0x23));
        return;
    }

    // Берём тот участок картинки, который сейчас под нами. Отсюда и эффект
    // неподвижного фона: при движении окна меняется вырезаемый участок,
    // а сама картинка стоит на месте относительно рабочего стола.
    const QPoint origin = mapToGlobal(QPoint(0, 0)) - screen->geometry().topLeft();
    QRect source(origin, size());

    // Окно может уехать за пределы экрана — там картинки нет, поэтому
    // прижимаем вырезаемый участок к её границам.
    source.moveLeft(qBound(0, source.left(), qMax(0, m_backdrop.width() - source.width())));
    source.moveTop(qBound(0, source.top(), qMax(0, m_backdrop.height() - source.height())));

    painter.drawImage(rect(), m_backdrop, source);
}

void Backdrop::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    // Окно поехало — под ним другой участок картинки.
    update();
}
