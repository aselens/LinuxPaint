#include "LayersPanel.h"
#include "Document.h"
#include "Icons.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const int kThumbWidth = 84;
const int kThumbHeight = 62;
const int kEyeSize = 18;

// Шахматка, сквозь которую видно прозрачные места слоя.
void drawCheckerboard(QPainter &p, const QRect &rect)
{
    const int cell = 6;
    p.save();
    p.setClipRect(rect);
    p.fillRect(rect, QColor(0xFF, 0xFF, 0xFF));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xCC, 0xCC, 0xCC));
    for (int y = rect.top(); y < rect.bottom(); y += cell) {
        for (int x = rect.left(); x < rect.right(); x += cell) {
            const bool odd = (((x - rect.left()) / cell) + ((y - rect.top()) / cell)) % 2;
            if (odd)
                p.drawRect(QRect(x, y, cell, cell));
        }
    }
    p.restore();
}

} // namespace

// --- LayerItem -----------------------------------------------------------

LayerItem::LayerItem(int index, const QImage &content, bool visible, bool selected,
                     QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_selected(selected)
{
    setFixedSize(kThumbWidth + 8, kThumbHeight + 8);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Слой %1").arg(index + 1));

    m_thumbnail = content.scaled(QSize(kThumbWidth, kThumbHeight),
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 6, 0);

    auto *eye = new QToolButton(this);
    eye->setObjectName(QStringLiteral("LayerEye"));
    eye->setAutoRaise(true);
    eye->setFixedSize(kEyeSize, kEyeSize);
    eye->setIconSize(QSize(13, 13));
    eye->setIcon(Icons::action(visible ? Icons::Action::LayerVisible
                                       : Icons::Action::LayerHidden));
    eye->setToolTip(visible ? tr("Скрыть слой") : tr("Показать слой"));
    layout->addWidget(eye, 0, Qt::AlignRight | Qt::AlignTop);
    layout->addStretch(1);

    connect(eye, &QToolButton::clicked, this, [this] {
        emit visibilityToggled(m_index);
    });
}

void LayerItem::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect frame = rect().adjusted(2, 2, -2, -2);
    drawCheckerboard(p, frame);

    if (!m_thumbnail.isNull()) {
        // Миниатюра может быть уже рамки, если пропорции холста другие —
        // центрируем её, а не растягиваем.
        QRect target(QPoint(0, 0), m_thumbnail.size());
        target.moveCenter(frame.center());
        p.drawImage(target, m_thumbnail);
    }

    p.setBrush(Qt::NoBrush);
    if (m_selected) {
        QPen pen(palette().color(QPalette::Highlight));
        pen.setWidth(2);
        p.setPen(pen);
        p.drawRect(rect().adjusted(1, 1, -1, -1));
    } else {
        p.setPen(QPen(palette().color(QPalette::Shadow), 1));
        p.drawRect(frame);
    }
}

void LayerItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit picked(m_index);
}

void LayerItem::contextMenuEvent(QContextMenuEvent *event)
{
    emit menuRequested(m_index, event->globalPos());
}

// --- LayersPanel ---------------------------------------------------------

LayersPanel::LayersPanel(Document *document, QWidget *parent)
    : QWidget(parent)
    , m_document(document)
{
    setObjectName(QStringLiteral("LayersPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kThumbWidth + 30);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    m_addButton = new QToolButton(this);
    m_addButton->setAutoRaise(true);
    m_addButton->setIconSize(QSize(20, 20));
    m_addButton->setFixedHeight(28);
    m_addButton->setToolTip(tr("Добавить слой"));
    m_addButton->setIcon(Icons::action(Icons::Action::AddLayer));
    root->addWidget(m_addButton, 0, Qt::AlignHCenter);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *host = new QWidget(scroll);
    m_itemsLayout = new QVBoxLayout(host);
    m_itemsLayout->setContentsMargins(0, 0, 0, 0);
    m_itemsLayout->setSpacing(6);
    m_itemsLayout->addStretch(1);
    scroll->setWidget(host);

    root->addWidget(scroll, 1);

    // Состав слоёв меняется редко — на него отвечаем сразу. А вот пиксели
    // меняются на каждое движение мыши, и пересобирать список так часто
    // нельзя: перезапускаемый таймер откладывает обновление миниатюр
    // до паузы в рисовании.
    m_thumbnailTimer = new QTimer(this);
    m_thumbnailTimer->setSingleShot(true);
    m_thumbnailTimer->setInterval(250);

    connect(m_addButton, &QToolButton::clicked, m_document, &Document::addLayer);
    connect(m_document, &Document::layersChanged, this, &LayersPanel::refresh);
    connect(m_thumbnailTimer, &QTimer::timeout, this, &LayersPanel::refresh);
    connect(m_document, &Document::changed, this, [this] {
        if (isVisible())
            m_thumbnailTimer->start();
    });

    refresh();
}

void LayersPanel::refresh()
{
    if (!m_itemsLayout)
        return;

    // Список короткий (единицы слоёв), поэтому пересобираем его целиком:
    // так не нужно следить за соответствием элементов и индексов.
    while (QLayoutItem *item = m_itemsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    // Сверху показываем верхний слой — то есть идём по стопке с конца.
    for (int i = m_document->layerCount() - 1; i >= 0; --i) {
        const Layer &layer = m_document->layer(i);
        auto *item = new LayerItem(i, layer.image, layer.visible,
                                   i == m_document->activeLayer());

        connect(item, &LayerItem::picked, m_document, &Document::setActiveLayer);
        connect(item, &LayerItem::visibilityToggled, this, [this](int index) {
            m_document->setLayerVisible(index, !m_document->layer(index).visible);
        });
        connect(item, &LayerItem::menuRequested, this, &LayersPanel::showItemMenu);

        m_itemsLayout->addWidget(item, 0, Qt::AlignHCenter);
    }

    m_itemsLayout->addStretch(1);
}

void LayersPanel::showItemMenu(int index, const QPoint &globalPos)
{
    QMenu menu(this);

    QAction *duplicate = menu.addAction(tr("Дублировать слой"));
    QAction *mergeDown = menu.addAction(tr("Объединить с нижним"));
    menu.addSeparator();
    QAction *moveUp = menu.addAction(tr("Переместить вверх"));
    QAction *moveDown = menu.addAction(tr("Переместить вниз"));
    menu.addSeparator();
    QAction *remove = menu.addAction(tr("Удалить слой"));

    mergeDown->setEnabled(index > 0);
    moveUp->setEnabled(index < m_document->layerCount() - 1);
    moveDown->setEnabled(index > 0);
    remove->setEnabled(m_document->layerCount() > 1);

    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;

    if (chosen == duplicate)
        m_document->duplicateLayer(index);
    else if (chosen == mergeDown)
        m_document->mergeLayerDown(index);
    else if (chosen == moveUp)
        m_document->moveLayer(index, 1);
    else if (chosen == moveDown)
        m_document->moveLayer(index, -1);
    else if (chosen == remove)
        m_document->removeLayer(index);
}
