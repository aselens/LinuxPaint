#include "ColorArea.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace {

// Стандартная палитра Paint: два ряда по десять цветов.
const QColor kPalette[20] = {
    QColor(0, 0, 0),       QColor(127, 127, 127), QColor(136, 0, 21),    QColor(237, 28, 36),
    QColor(255, 127, 39),  QColor(255, 242, 0),   QColor(34, 177, 76),   QColor(0, 162, 232),
    QColor(63, 72, 204),   QColor(163, 73, 164),
    QColor(255, 255, 255), QColor(195, 195, 195), QColor(185, 122, 87),  QColor(255, 174, 201),
    QColor(255, 201, 14),  QColor(239, 228, 176), QColor(181, 230, 29),  QColor(153, 217, 234),
    QColor(112, 146, 190), QColor(200, 191, 231)
};

const int kSwatchSize = 23;
const int kCustomSlots = 10;

} // namespace

// --- Swatch --------------------------------------------------------------

Swatch::Swatch(const QColor &colour, QWidget *parent)
    : QWidget(parent)
    , m_colour(colour)
{
    setFixedSize(kSwatchSize, kSwatchSize);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

void Swatch::setColour(const QColor &colour)
{
    m_colour = colour;
    update();
}

void Swatch::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    update();
}

void Swatch::setEmpty(bool empty)
{
    m_empty = empty;
    update();
}

void Swatch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Выбранный образец обведён кольцом с зазором, поэтому сама заливка
    // занимает не весь виджет — место под кольцо резервируется всегда,
    // иначе кружок «дёргался» бы при выборе.
    const double inset = 3.0;
    const QRectF box = QRectF(rect()).adjusted(inset, inset, -inset, -inset);

    if (m_empty) {
        p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(box);
        return;
    }

    p.setPen(QPen(palette().color(QPalette::Shadow), 1));
    p.setBrush(m_colour);
    p.drawEllipse(box);

    if (m_selected || m_hovered) {
        QPen pen(palette().color(QPalette::Highlight));
        pen.setWidthF(m_selected ? 2.0 : 1.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(rect()).adjusted(0.8, 0.8, -0.8, -0.8));
    }
}

void Swatch::mousePressEvent(QMouseEvent *event)
{
    if (!m_empty || event->button() == Qt::LeftButton)
        emit picked(m_colour, event->button());
}

void Swatch::mouseDoubleClickEvent(QMouseEvent *event)
{
    emit picked(m_colour, event->button());
}

void Swatch::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void Swatch::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

// --- ColorArea -----------------------------------------------------------

ColorArea::ColorArea(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    // Текущие цвета — крупными кружками слева, как в современном Paint.
    // Подписей нет: назначение объясняют подсказки.
    auto *currentBox = new QVBoxLayout;
    currentBox->setContentsMargins(0, 0, 0, 0);
    currentBox->setSpacing(1);

    m_primarySwatch = new Swatch(m_colour1, this);
    m_primarySwatch->setFixedSize(34, 34);
    m_primarySwatch->setToolTip(tr("Цвет 1 — рисование левой кнопкой"));

    m_secondarySwatch = new Swatch(m_colour2, this);
    m_secondarySwatch->setFixedSize(28, 28);
    m_secondarySwatch->setToolTip(tr("Цвет 2 — рисование правой кнопкой и фон"));

    currentBox->addWidget(m_primarySwatch, 0, Qt::AlignHCenter);
    currentBox->addWidget(m_secondarySwatch, 0, Qt::AlignHCenter);
    layout->addLayout(currentBox);

    // Палитра.
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(1);
    buildPalette(grid);
    layout->addLayout(grid);

    m_primarySwatch->setSelected(true);

    // Клик по «Цвет 1» / «Цвет 2» переключает, какой из них правится палитрой.
    connect(m_primarySwatch, &Swatch::picked, this, [this](const QColor &, Qt::MouseButton) {
        m_editingSecondary = false;
        m_primarySwatch->setSelected(true);
        m_secondarySwatch->setSelected(false);
    });
    connect(m_secondarySwatch, &Swatch::picked, this, [this](const QColor &, Qt::MouseButton) {
        m_editingSecondary = true;
        m_primarySwatch->setSelected(false);
        m_secondarySwatch->setSelected(true);
    });
}

void ColorArea::buildPalette(QGridLayout *grid)
{
    for (int i = 0; i < 20; ++i) {
        auto *swatch = new Swatch(kPalette[i], this);
        grid->addWidget(swatch, i / 10, i % 10);
        connect(swatch, &Swatch::picked, this, &ColorArea::onSwatchPicked);
    }

    // Ряд пользовательских цветов — заполняется из диалога выбора цвета.
    for (int i = 0; i < kCustomSlots; ++i) {
        auto *swatch = new Swatch(Qt::white, this);
        swatch->setEmpty(true);
        swatch->setToolTip(tr("Пользовательский цвет"));
        grid->addWidget(swatch, 2, i);
        m_customSwatches.append(swatch);
        connect(swatch, &Swatch::picked, this, &ColorArea::onSwatchPicked);
    }
}

void ColorArea::onSwatchPicked(const QColor &colour, Qt::MouseButton button)
{
    auto *source = qobject_cast<Swatch *>(sender());
    if (source && source->isEmpty())
        return;

    // ЛКМ правит активный цвет, ПКМ — противоположный.
    const bool secondary = (button == Qt::RightButton) ? !m_editingSecondary
                                                       : m_editingSecondary;
    if (secondary)
        setColour2(colour);
    else
        setColour1(colour);
}

void ColorArea::setColour1(const QColor &colour)
{
    if (m_colour1 == colour)
        return;
    m_colour1 = colour;
    m_primarySwatch->setColour(colour);
    emit colour1Changed(colour);
}

void ColorArea::setColour2(const QColor &colour)
{
    if (m_colour2 == colour)
        return;
    m_colour2 = colour;
    m_secondarySwatch->setColour(colour);
    emit colour2Changed(colour);
}

void ColorArea::addCustomColour(const QColor &colour)
{
    // Уже есть в списке — не дублируем.
    for (Swatch *swatch : m_customSwatches) {
        if (!swatch->isEmpty() && swatch->colour() == colour)
            return;
    }

    Swatch *slot = m_customSwatches[m_nextCustom % kCustomSlots];
    slot->setEmpty(false);
    slot->setColour(colour);
    slot->setToolTip(colour.name());
    m_nextCustom = (m_nextCustom + 1) % kCustomSlots;
}

QVector<QColor> ColorArea::customColours() const
{
    QVector<QColor> out;
    for (Swatch *swatch : m_customSwatches) {
        if (!swatch->isEmpty())
            out.append(swatch->colour());
    }
    return out;
}

void ColorArea::setCustomColours(const QVector<QColor> &colours)
{
    for (const QColor &colour : colours)
        addCustomColour(colour);
}
