#include "Canvas.h"
#include "Document.h"
#include "tools/PaintTools.h"
#include "tools/SelectTool.h"
#include "tools/ShapeTool.h"
#include "tools/TextTool.h"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QTransform>
#include <QVector>
#include <QWheelEvent>
#include <QtMath>

namespace {

const double kZoomSteps[] = {0.125, 0.25, 0.5, 0.75, 1.0, 2.0, 3.0, 4.0,
                             6.0, 8.0, 12.0, 16.0, 24.0, 32.0};
const int kZoomStepCount = int(sizeof(kZoomSteps) / sizeof(kZoomSteps[0]));

// Нечётный размер, чтобы маркер ровно центрировался на границе листа.
const int kHandleSize = 7;

} // namespace

Canvas::Canvas(Document *document, QWidget *parent)
    : QWidget(parent)
    , m_document(document)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, true);

    createTools();
    m_tool = m_tools.value(int(ToolId::Pencil));
    m_currentId = ToolId::Pencil;
    m_previousId = ToolId::Pencil;

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(30);
    connect(m_tickTimer, &QTimer::timeout, this, &Canvas::onTick);

    connect(m_document, &Document::changed, this, &Canvas::onDocumentChanged);
    connect(m_document, &Document::sizeChanged, this, &Canvas::onDocumentSizeChanged);

    updateGeometryForImage();
    setCursor(m_tool->cursor());
}

Canvas::~Canvas()
{
    qDeleteAll(m_tools);
    m_tools.clear();
}

void Canvas::createTools()
{
    m_tools.insert(int(ToolId::Pencil), new PencilTool(this));
    m_tools.insert(int(ToolId::Brush), new BrushTool(this));
    m_tools.insert(int(ToolId::Eraser), new EraserTool(this));
    m_tools.insert(int(ToolId::Fill), new FillTool(this));
    m_tools.insert(int(ToolId::ColorPicker), new ColorPickerTool(this));
    m_tools.insert(int(ToolId::Magnifier), new MagnifierTool(this));
    m_tools.insert(int(ToolId::Shape), new ShapeTool(this));
    m_tools.insert(int(ToolId::Text), new TextTool(this));
    m_tools.insert(int(ToolId::Select), new SelectTool(this, false));
    m_tools.insert(int(ToolId::FreeSelect), new SelectTool(this, true));
}

// --- инструменты ---------------------------------------------------------

void Canvas::setTool(ToolId id)
{
    if (m_tool && m_currentId == id)
        return;

    if (m_tool) {
        m_tool->commit();
        m_tickTimer->stop();
    }

    // Уходя с инструментов выделения, впечатываем то, что висело в воздухе.
    const bool wasSelect = (m_currentId == ToolId::Select || m_currentId == ToolId::FreeSelect);
    const bool willSelect = (id == ToolId::Select || id == ToolId::FreeSelect);
    if (wasSelect && !willSelect)
        finishSelection();

    m_previousId = m_currentId;
    m_currentId = id;
    m_tool = m_tools.value(int(id));
    setCursor(m_tool->cursor());
    emit toolChanged(id);
    update();
}

void Canvas::restorePreviousTool()
{
    setTool(m_previousId);
}

void Canvas::commitPendingTool()
{
    if (m_tool)
        m_tool->commit();
    update();
}

void Canvas::cancelPendingTool()
{
    if (m_tool)
        m_tool->cancel();
    // Незавершённый штрих выбрасываем вместе с его слоем.
    m_strokeLayerActive = false;
    m_strokeLayer = QImage();
    update();
}

void Canvas::applySettingsToTools()
{
    if (m_tool)
        m_tool->viewChanged();
}

// --- настройки -----------------------------------------------------------

void Canvas::setColor1(const QColor &color)
{
    if (m_settings.color1 == color)
        return;
    m_settings.color1 = color;
    applySettingsToTools();
    emit colorsChanged();
}

void Canvas::setColor2(const QColor &color)
{
    if (m_settings.color2 == color)
        return;
    m_settings.color2 = color;
    applySettingsToTools();
    emit colorsChanged();
}

void Canvas::swapColors()
{
    qSwap(m_settings.color1, m_settings.color2);
    applySettingsToTools();
    emit colorsChanged();
}

void Canvas::setStrokeSize(int size)
{
    m_settings.size = qBound(1, size, 50);
    update();
}

void Canvas::setBrush(BrushType brush)
{
    m_settings.brush = brush;
}

void Canvas::setShape(ShapeType shape)
{
    // Незафиксированную фигуру нельзя превращать в другую на лету.
    commitPendingTool();
    m_settings.shape = shape;
}

void Canvas::setOutlineStyle(StrokeStyle style, bool enabled)
{
    m_settings.outline = style;
    m_settings.hasOutline = enabled;
    update();
}

void Canvas::setFillMode(FillMode fill)
{
    m_settings.fill = fill;
    update();
}

void Canvas::setTolerance(int percent)
{
    m_settings.tolerance = qBound(0, percent, 100);
}

void Canvas::setOpacity(int percent)
{
    m_settings.opacity = qBound(1, percent, 100);
}

void Canvas::setAntialias(bool on)
{
    m_settings.antialias = on;
}

// --- слой текущего штриха ------------------------------------------------

void Canvas::beginStrokeLayer()
{
    // При полной непрозрачности слой не нужен: рисуем прямо в холст
    // и не тратим память на копию размером с изображение.
    if (m_settings.opacity >= 100 || m_strokeLayerActive)
        return;

    m_strokeLayer = QImage(m_document->size(), QImage::Format_ARGB32);
    if (m_strokeLayer.isNull())
        return;
    m_strokeLayer.fill(Qt::transparent);
    m_strokeLayerActive = true;
}

void Canvas::commitStrokeLayer()
{
    if (!m_strokeLayerActive)
        return;

    m_strokeLayerActive = false;
    {
        QPainter p(&m_document->image());
        p.setOpacity(m_settings.opacity / 100.0);
        p.drawImage(0, 0, m_strokeLayer);
    }
    m_strokeLayer = QImage();
    update();
}

void Canvas::setFont(const QFont &font)
{
    m_settings.font = font;
    applySettingsToTools();
}

void Canvas::setTextOpaque(bool opaque)
{
    m_settings.textOpaque = opaque;
    update();
}

void Canvas::setTransparentSelection(bool on)
{
    m_settings.transparentSelection = on;
    update();
}

// --- масштаб -------------------------------------------------------------

void Canvas::setZoom(double zoom)
{
    zoom = qBound(0.05, zoom, 64.0);
    if (qFuzzyCompare(m_zoom, zoom))
        return;
    m_zoom = zoom;
    updateGeometryForImage();
    applySettingsToTools();
    emit zoomChanged(m_zoom);
    update();
}

void Canvas::zoomIn()
{
    for (int i = 0; i < kZoomStepCount; ++i) {
        if (kZoomSteps[i] > m_zoom + 1e-6) {
            setZoom(kZoomSteps[i]);
            return;
        }
    }
}

void Canvas::zoomOut()
{
    for (int i = kZoomStepCount - 1; i >= 0; --i) {
        if (kZoomSteps[i] < m_zoom - 1e-6) {
            setZoom(kZoomSteps[i]);
            return;
        }
    }
}

void Canvas::resetZoom()
{
    setZoom(1.0);
}

void Canvas::zoomToFit(const QSize &viewportSize)
{
    const QSize image = m_document->size();
    if (image.isEmpty() || viewportSize.isEmpty())
        return;
    const double sx = double(viewportSize.width() - m_margin * 2 - 12) / image.width();
    const double sy = double(viewportSize.height() - m_margin * 2 - 12) / image.height();
    setZoom(qMax(0.05, qMin(sx, sy)));
}

void Canvas::zoomAtImagePoint(int direction, const QPointF &anchor)
{
    if (direction > 0)
        zoomIn();
    else
        zoomOut();
    emit zoomAnchorRequested(anchor);
}

void Canvas::setGridVisible(bool visible)
{
    m_showGrid = visible;
    update();
}

// --- координаты ----------------------------------------------------------

QPoint Canvas::imageOrigin() const
{
    return QPoint(m_margin, m_margin);
}

QPointF Canvas::widgetToImage(const QPointF &p) const
{
    const QPoint origin = imageOrigin();
    return QPointF((p.x() - origin.x()) / m_zoom, (p.y() - origin.y()) / m_zoom);
}

QPoint Canvas::imageToWidget(const QPointF &p) const
{
    const QPoint origin = imageOrigin();
    return QPoint(qRound(origin.x() + p.x() * m_zoom),
                  qRound(origin.y() + p.y() * m_zoom));
}

QRect Canvas::imageRectToWidget(const QRect &r) const
{
    const QPoint topLeft = imageToWidget(r.topLeft());
    const QPoint bottomRight = imageToWidget(QPointF(r.right() + 1, r.bottom() + 1));
    return QRect(topLeft, bottomRight - QPoint(1, 1));
}

void Canvas::updateImageRect(const QRect &imageRect)
{
    if (imageRect.isNull()) {
        update();
        return;
    }
    // Запас на толщину пера и рамку выделения после округления.
    update(imageRectToWidget(imageRect).adjusted(-4, -4, 4, 4));
}

void Canvas::updateGeometryForImage()
{
    const QSize image = m_document->size();
    const QSize widget(int(image.width() * m_zoom) + m_margin * 2 + kHandleSize + 2,
                       int(image.height() * m_zoom) + m_margin * 2 + kHandleSize + 2);
    setFixedSize(widget);
}

// --- отрисовка -----------------------------------------------------------

void Canvas::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());

    // Фон вокруг холста здесь не рисуем: под виджетом уже лежит слой
    // области просмотра, а под ним — подложка с градиентом. Своя заливка
    // легла бы поверх них вторым слоем, и по краю виджета вылез бы стык.

    const QRect canvasRect = imageRectToWidget(QRect(QPoint(0, 0), m_document->size()));

    // Лёгкая тень, чтобы «лист» отделялся от подложки.
    painter.fillRect(canvasRect.adjusted(2, 2, 3, 3), QColor(0, 0, 0, 40));

    // Шахматка под холстом: сквозь прозрачные места слоёв должна быть
    // видна именно она, а не подложка окна.
    {
        static const int kCell = 8;
        QPixmap tile(kCell * 2, kCell * 2);
        tile.fill(QColor(0xFF, 0xFF, 0xFF));
        QPainter tp(&tile);
        tp.fillRect(0, 0, kCell, kCell, QColor(0xCC, 0xCC, 0xCC));
        tp.fillRect(kCell, kCell, kCell, kCell, QColor(0xCC, 0xCC, 0xCC));
        tp.end();
        painter.fillRect(canvasRect, QBrush(tile));
    }

    painter.save();
    painter.translate(imageOrigin());
    painter.scale(m_zoom, m_zoom);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
    painter.drawImage(0, 0, m_document->composite());

    // Незавершённый полупрозрачный штрих показываем поверх холста с той же
    // прозрачностью, с какой он в него потом ляжет.
    if (m_strokeLayerActive) {
        painter.setOpacity(m_settings.opacity / 100.0);
        painter.drawImage(0, 0, m_strokeLayer);
        painter.setOpacity(1.0);
    }

    // Плавающее выделение поверх холста.
    if (m_selection.active && m_selection.floating && !m_selection.pixels.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(QRectF(m_selection.rect), selectionImage());
    }

    if (m_tool)
        m_tool->paintOverlay(painter);

    painter.restore();

    // Сетка попиксельного режима — как в Paint, только на крупном масштабе.
    if (m_showGrid && m_zoom >= 4.0) {
        painter.save();
        QPen pen(QColor(0, 0, 0, 60));
        pen.setWidth(1);
        painter.setPen(pen);
        const QPoint origin = imageOrigin();
        const QSize image = m_document->size();

        // Рисуем только те линии, что попали в обновляемую область: иначе
        // на большом холсте каждый штрих перерисовывал бы тысячи линий.
        const QRect visible = event->region().boundingRect();
        const int firstX = qBound(0, int((visible.left() - origin.x()) / m_zoom), image.width());
        const int lastX = qBound(0, int((visible.right() - origin.x()) / m_zoom) + 1, image.width());
        const int firstY = qBound(0, int((visible.top() - origin.y()) / m_zoom), image.height());
        const int lastY = qBound(0, int((visible.bottom() - origin.y()) / m_zoom) + 1, image.height());

        for (int x = firstX; x <= lastX; ++x) {
            const int px = origin.x() + int(x * m_zoom);
            painter.drawLine(px, origin.y(), px, origin.y() + int(image.height() * m_zoom));
        }
        for (int y = firstY; y <= lastY; ++y) {
            const int py = origin.y() + int(y * m_zoom);
            painter.drawLine(origin.x(), py, origin.x() + int(image.width() * m_zoom), py);
        }
        painter.restore();
    }

    // Граница листа.
    painter.setPen(QPen(palette().color(QPalette::Shadow), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(canvasRect.adjusted(-1, -1, 0, 0));

    drawSelectionOverlay(painter);
    drawCanvasHandles(painter);

    // Предпросмотр новых границ холста.
    if (m_resizingCanvas) {
        QPen pen(QColor(0, 120, 215));
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(imageRectToWidget(m_resizePreview));
    }
}

void Canvas::drawSelectionOverlay(QPainter &painter)
{
    QRect widgetRect;
    if (m_selection.active)
        widgetRect = imageRectToWidget(m_selection.rect);
    else if (m_hasPreviewSelection)
        widgetRect = imageRectToWidget(m_previewSelection);
    else
        return;

    painter.save();
    painter.setBrush(Qt::NoBrush);

    // «Муравьиная дорожка»: белая подложка плюс чёрный пунктир поверх —
    // так рамка видна и на светлом, и на тёмном рисунке.
    QPen white(Qt::white);
    white.setWidth(1);
    painter.setPen(white);
    painter.drawRect(widgetRect);

    QPen black(Qt::black);
    black.setWidth(1);
    black.setStyle(Qt::DashLine);
    painter.setPen(black);
    painter.drawRect(widgetRect);

    if (m_selection.active) {
        // Маркеры изменения размера выделения.
        painter.setPen(QPen(QColor(0, 120, 215), 1));
        painter.setBrush(Qt::white);
        const QPoint c = widgetRect.center();
        const QVector<QPoint> handles = {
            widgetRect.topLeft(),  QPoint(c.x(), widgetRect.top()),    widgetRect.topRight(),
            QPoint(widgetRect.right(), c.y()),
            widgetRect.bottomRight(), QPoint(c.x(), widgetRect.bottom()), widgetRect.bottomLeft(),
            QPoint(widgetRect.left(), c.y())
        };
        for (const QPoint &h : handles)
            painter.drawRect(QRect(h.x() - 3, h.y() - 3, 6, 6));
    }

    painter.restore();
}

void Canvas::drawCanvasHandles(QPainter &painter)
{
    const ResizeHandle handles[] = {
        ResizeHandle::TopLeft, ResizeHandle::Top, ResizeHandle::TopRight,
        ResizeHandle::Right, ResizeHandle::BottomRight, ResizeHandle::Bottom,
        ResizeHandle::BottomLeft, ResizeHandle::Left
    };

    painter.save();
    painter.setPen(QPen(QColor(120, 120, 120), 1));
    painter.setBrush(QColor(255, 255, 255));
    for (ResizeHandle handle : handles)
        painter.drawRect(handleRect(handle));
    painter.restore();
}

QRect Canvas::handleRect(ResizeHandle handle) const
{
    const QRect canvasRect = imageRectToWidget(m_document->image().rect());
    const int s = kHandleSize;

    // Маркеры сидят по центру границы листа, а не снаружи от неё.
    QPoint centre;
    switch (handle) {
    case ResizeHandle::TopLeft:     centre = canvasRect.topLeft();                              break;
    case ResizeHandle::Top:         centre = QPoint(canvasRect.center().x(), canvasRect.top()); break;
    case ResizeHandle::TopRight:    centre = canvasRect.topRight();                             break;
    case ResizeHandle::Right:       centre = QPoint(canvasRect.right(), canvasRect.center().y()); break;
    case ResizeHandle::BottomRight: centre = canvasRect.bottomRight();                          break;
    case ResizeHandle::Bottom:      centre = QPoint(canvasRect.center().x(), canvasRect.bottom()); break;
    case ResizeHandle::BottomLeft:  centre = canvasRect.bottomLeft();                           break;
    case ResizeHandle::Left:        centre = QPoint(canvasRect.left(), canvasRect.center().y()); break;
    default:                        return QRect();
    }

    return QRect(centre.x() - s / 2, centre.y() - s / 2, s, s);
}

Canvas::ResizeHandle Canvas::handleAtWidgetPos(const QPoint &pos) const
{
    // Углы проверяем первыми: на маленьком холсте они перекрываются
    // с серединами сторон, и приоритет должен быть у угла.
    const ResizeHandle order[] = {
        ResizeHandle::TopLeft, ResizeHandle::TopRight,
        ResizeHandle::BottomRight, ResizeHandle::BottomLeft,
        ResizeHandle::Top, ResizeHandle::Right,
        ResizeHandle::Bottom, ResizeHandle::Left
    };
    for (ResizeHandle h : order) {
        if (handleRect(h).adjusted(-2, -2, 2, 2).contains(pos))
            return h;
    }
    return ResizeHandle::None;
}

// --- события мыши --------------------------------------------------------

void Canvas::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    const QPoint widgetPos = event->position().toPoint();

    const ResizeHandle handle = handleAtWidgetPos(widgetPos);
    if (handle != ResizeHandle::None && event->button() == Qt::LeftButton) {
        m_activeHandle = handle;
        m_resizingCanvas = true;
        m_resizePreview = QRect(QPoint(0, 0), m_document->size());
        update();
        return;
    }

    if (!m_tool)
        return;

    m_toolPressed = true;
    m_tool->press(widgetToImage(event->position()), event->button(), event->modifiers());
    if (m_tool->needsTick())
        m_tickTimer->start();
}

void Canvas::mouseMoveEvent(QMouseEvent *event)
{
    const QPointF imagePos = widgetToImage(event->position());
    const QPoint rounded(int(qFloor(imagePos.x())), int(qFloor(imagePos.y())));
    if (rounded != m_lastImagePos) {
        m_lastImagePos = rounded;
        emit cursorMoved(rounded);
    }

    if (m_resizingCanvas) {
        QRect bounds(QPoint(0, 0), m_document->size());
        switch (m_activeHandle) {
        case ResizeHandle::TopLeft:     bounds.setTopLeft(rounded);     break;
        case ResizeHandle::Top:         bounds.setTop(rounded.y());     break;
        case ResizeHandle::TopRight:    bounds.setTopRight(rounded);    break;
        case ResizeHandle::Right:       bounds.setRight(rounded.x());   break;
        case ResizeHandle::BottomRight: bounds.setBottomRight(rounded); break;
        case ResizeHandle::Bottom:      bounds.setBottom(rounded.y());  break;
        case ResizeHandle::BottomLeft:  bounds.setBottomLeft(rounded);  break;
        case ResizeHandle::Left:        bounds.setLeft(rounded.x());    break;
        case ResizeHandle::None:                                        break;
        }
        // Схлопнуть холст в ничто нельзя.
        if (bounds.width() < 1)
            bounds.setWidth(1);
        if (bounds.height() < 1)
            bounds.setHeight(1);

        m_resizePreview = bounds;

        // Виджету нужно место под рамку предпросмотра, иначе при растягивании
        // она упрётся в его край. Уменьшать по ходу протяжки не даём — иначе
        // холст дёргался бы под курсором.
        const int needW = int(qMax(bounds.right() + 1, m_document->width()) * m_zoom)
                          + m_margin * 2 + kHandleSize;
        const int needH = int(qMax(bounds.bottom() + 1, m_document->height()) * m_zoom)
                          + m_margin * 2 + kHandleSize;
        setFixedSize(qMax(needW, width()), qMax(needH, height()));

        update();
        return;
    }

    // Курсор-подсказка над маркерами изменения размера холста.
    if (!m_toolPressed) {
        switch (handleAtWidgetPos(event->position().toPoint())) {
        case ResizeHandle::Top:
        case ResizeHandle::Bottom:
            setCursor(Qt::SizeVerCursor);
            break;
        case ResizeHandle::Left:
        case ResizeHandle::Right:
            setCursor(Qt::SizeHorCursor);
            break;
        case ResizeHandle::TopLeft:
        case ResizeHandle::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case ResizeHandle::TopRight:
        case ResizeHandle::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case ResizeHandle::None:
            setCursor(m_tool ? m_tool->cursor() : QCursor(Qt::ArrowCursor));
            break;
        }
    }

    if (m_tool)
        m_tool->move(imagePos, event->modifiers());
}

void Canvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizingCanvas) {
        m_resizingCanvas = false;
        m_activeHandle = ResizeHandle::None;

        const QRect bounds = m_resizePreview;
        if (bounds.size() != m_document->size() || !bounds.topLeft().isNull()) {
            // Начало новых границ уехало в минус — значит, старое содержимое
            // должно лечь с обратным сдвигом.
            m_document->resizeCanvas(bounds.size(), -bounds.topLeft(), m_settings.color2);
        } else {
            // Размер не изменился — возвращаем виджету обычные габариты,
            // раздутые под предпросмотр.
            updateGeometryForImage();
        }
        update();
        return;
    }

    m_tickTimer->stop();
    m_toolPressed = false;
    if (m_tool)
        m_tool->release(widgetToImage(event->position()), event->button(), event->modifiers());
}

void Canvas::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_tool)
        m_tool->doubleClick(widgetToImage(event->position()), event->button());
}

void Canvas::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    emit cursorLeft();
}

void Canvas::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const QPointF anchor = widgetToImage(event->position());
        zoomAtImagePoint(event->angleDelta().y() > 0 ? 1 : -1, anchor);
        event->accept();
        return;
    }
    // Обычную прокрутку отдаём области просмотра.
    event->ignore();
}

void Canvas::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_tool)
            m_tool->cancel();
        if (m_selection.active)
            finishSelection();
        update();
        return;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        commitPendingTool();
        if (m_selection.active)
            finishSelection();
        return;

    case Qt::Key_Delete:
        if (m_selection.active) {
            deleteSelection();
            return;
        }
        break;

    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (m_selection.active) {
            const int step = (event->modifiers() & Qt::ControlModifier) ? 10 : 1;
            QPoint delta;
            if (event->key() == Qt::Key_Left)  delta = QPoint(-step, 0);
            if (event->key() == Qt::Key_Right) delta = QPoint(step, 0);
            if (event->key() == Qt::Key_Up)    delta = QPoint(0, -step);
            if (event->key() == Qt::Key_Down)  delta = QPoint(0, step);
            floatSelection();
            setSelectionRect(m_selection.rect.translated(delta));
            return;
        }
        break;

    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void Canvas::onTick()
{
    if (m_tool)
        m_tool->tick();
}

void Canvas::onDocumentChanged(const QRect &dirty)
{
    updateImageRect(dirty);
}

void Canvas::onDocumentSizeChanged(const QSize &size)
{
    Q_UNUSED(size)
    // Холст сменил размер — старое выделение к нему уже не относится.
    // Если оно было плавающим, правку документа нужно закрыть, иначе
    // следующий beginEdit() решит, что мы всё ещё внутри неё.
    if (m_selection.active) {
        if (m_selection.floating)
            m_document->endEdit();
        m_selection = SelectionState();
        emit selectionChanged();
    }
    updateGeometryForImage();
    update();
}

// --- выделение -----------------------------------------------------------

QImage Canvas::maskedPixels(const QImage &source, const QPainterPath &path,
                            const QRect &bounds) const
{
    QImage out(bounds.size(), QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.translate(-bounds.topLeft());
    p.setClipPath(path);
    p.drawImage(0, 0, source);
    p.end();
    return out;
}

void Canvas::beginRectSelection(const QRect &rect)
{
    finishSelection();

    const QRect r = rect.intersected(m_document->image().rect());
    m_hasPreviewSelection = false;
    if (r.isEmpty()) {
        m_selection = SelectionState();
        emit selectionChanged();
        update();
        return;
    }

    m_selection = SelectionState();
    m_selection.active = true;
    m_selection.rect = r;
    m_selection.sourceRect = r;
    m_selection.pixels = m_document->image().copy(r);
    m_selection.masked = false;

    emit selectionChanged();
    emit selectionGeometryChanged(r.size());
    update();
}

void Canvas::beginFreeSelection(const QPainterPath &path)
{
    finishSelection();
    m_hasPreviewSelection = false;

    const QRect bounds = path.boundingRect().toAlignedRect()
                             .intersected(m_document->image().rect());
    if (bounds.isEmpty()) {
        m_selection = SelectionState();
        emit selectionChanged();
        update();
        return;
    }

    m_selection = SelectionState();
    m_selection.active = true;
    m_selection.rect = bounds;
    m_selection.sourceRect = bounds;
    m_selection.pixels = maskedPixels(m_document->image(), path, bounds);
    m_selection.masked = true;

    emit selectionChanged();
    emit selectionGeometryChanged(bounds.size());
    update();
}

void Canvas::setPreviewSelectionRect(const QRect &rect)
{
    m_previewSelection = rect;
    m_hasPreviewSelection = true;
    emit selectionGeometryChanged(rect.size());
    update();
}

void Canvas::clearSelectionShape()
{
    m_hasPreviewSelection = false;
    if (m_selection.active) {
        m_selection = SelectionState();
        emit selectionChanged();
    }
    update();
}

void Canvas::eraseSelectionSource()
{
    QImage &img = m_document->image();

    if (m_selection.masked && !m_selection.pixels.isNull()) {
        // Форма произвольная: стираем ровно там, где в выделении есть пиксели.
        const QRect r = m_selection.sourceRect;
        const QColor fill = m_settings.color2;
        for (int y = 0; y < m_selection.pixels.height(); ++y) {
            const QRgb *src = reinterpret_cast<const QRgb *>(m_selection.pixels.constScanLine(y));
            const int targetY = r.top() + y;
            if (targetY < 0 || targetY >= img.height())
                continue;
            QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(targetY));
            for (int x = 0; x < m_selection.pixels.width(); ++x) {
                const int targetX = r.left() + x;
                if (targetX < 0 || targetX >= img.width())
                    continue;
                if (qAlpha(src[x]) > 0)
                    dst[targetX] = fill.rgba();
            }
        }
        return;
    }

    QPainter p(&img);
    p.fillRect(m_selection.sourceRect, m_settings.color2);
}

void Canvas::floatSelection()
{
    if (!m_selection.active || m_selection.floating)
        return;

    m_selection.floating = true;
    m_document->beginEdit();
    eraseSelectionSource();
    m_document->touch(m_selection.sourceRect);
    update();
}

void Canvas::setSelectionRect(const QRect &rect)
{
    if (!m_selection.active)
        return;
    const QRect old = m_selection.rect;
    m_selection.rect = rect;
    updateImageRect(old.united(rect).adjusted(-8, -8, 8, 8));
    emit selectionGeometryChanged(rect.size());
}

QImage Canvas::selectionImage() const
{
    if (m_selection.pixels.isNull())
        return QImage();

    if (!m_settings.transparentSelection)
        return m_selection.pixels;

    // «Прозрачное выделение»: пиксели Цвета 2 выпадают из содержимого.
    QImage out = m_selection.pixels.convertToFormat(QImage::Format_ARGB32);
    const QRgb key = m_settings.color2.rgb() | 0xff000000u;
    for (int y = 0; y < out.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            if ((line[x] | 0xff000000u) == key)
                line[x] = qRgba(0, 0, 0, 0);
        }
    }
    return out;
}

void Canvas::stampSelection()
{
    if (m_selection.pixels.isNull())
        return;
    QPainter p(&m_document->image());
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(QRectF(m_selection.rect), selectionImage());
}

void Canvas::finishSelection()
{
    if (!m_selection.active)
        return;

    if (m_selection.floating) {
        stampSelection();
        m_document->endEdit(m_selection.rect.united(m_selection.sourceRect)
                                .adjusted(-2, -2, 2, 2));
    }

    m_selection = SelectionState();
    m_hasPreviewSelection = false;
    emit selectionChanged();
    update();
}

// Снять выделение, не оставив следов: плавающие пиксели возвращаются
// на место. Нужно перед отменой — иначе незакрытая правка документа
// перемешалась бы с шагом истории.
void Canvas::discardSelection()
{
    if (!m_selection.active)
        return;

    if (m_selection.floating)
        m_document->abortEdit();

    m_selection = SelectionState();
    m_hasPreviewSelection = false;
    emit selectionChanged();
    update();
}

void Canvas::selectAll()
{
    if (m_currentId != ToolId::Select && m_currentId != ToolId::FreeSelect)
        setTool(ToolId::Select);
    beginRectSelection(m_document->image().rect());
}

void Canvas::deleteSelection()
{
    if (!m_selection.active)
        return;

    if (m_selection.floating) {
        // Пиксели уже сняты с холста — достаточно их не возвращать.
        m_document->endEdit(m_selection.sourceRect);
    } else {
        m_document->beginEdit();
        eraseSelectionSource();
        m_document->endEdit(m_selection.sourceRect);
    }

    m_selection = SelectionState();
    emit selectionChanged();
    update();
}

void Canvas::invertSelection()
{
    if (!m_selection.active) {
        selectAll();
        return;
    }

    // Собираем «всё, кроме выделенного»: копия холста с дыркой на месте рамки.
    const QRect hole = m_selection.sourceRect;
    const QImage holeMask = m_selection.pixels;
    const bool masked = m_selection.masked;

    finishSelection();

    const QRect bounds = m_document->image().rect();
    QImage pixels = m_document->image().copy(bounds).convertToFormat(QImage::Format_ARGB32);

    const QRect punch = hole.intersected(bounds);
    for (int y = punch.top(); y <= punch.bottom(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(pixels.scanLine(y));
        for (int x = punch.left(); x <= punch.right(); ++x) {
            if (masked && !holeMask.isNull()) {
                const int mx = x - hole.left();
                const int my = y - hole.top();
                if (mx < 0 || my < 0 || mx >= holeMask.width() || my >= holeMask.height())
                    continue;
                if (qAlpha(holeMask.pixel(mx, my)) == 0)
                    continue;
            }
            line[x] = qRgba(0, 0, 0, 0);
        }
    }

    m_selection = SelectionState();
    m_selection.active = true;
    m_selection.rect = bounds;
    m_selection.sourceRect = bounds;
    m_selection.pixels = pixels;
    m_selection.masked = true;

    emit selectionChanged();
    emit selectionGeometryChanged(bounds.size());
    update();
}

void Canvas::cutSelection()
{
    if (!m_selection.active)
        return;
    copySelection();
    deleteSelection();
}

void Canvas::copySelection()
{
    if (!m_selection.active)
        return;
    QApplication::clipboard()->setImage(selectionImage());
}

bool Canvas::canPaste() const
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    return mime && mime->hasImage();
}

void Canvas::beginFloatingImage(const QImage &image, const QPoint &at, bool fromClipboard)
{
    if (image.isNull())
        return;

    finishSelection();
    if (m_currentId != ToolId::Select && m_currentId != ToolId::FreeSelect)
        setTool(ToolId::Select);

    m_selection = SelectionState();
    m_selection.active = true;
    m_selection.floating = true;
    m_selection.fromClipboard = fromClipboard;
    m_selection.pixels = image.convertToFormat(QImage::Format_ARGB32);
    m_selection.rect = QRect(at, image.size());
    m_selection.sourceRect = m_selection.rect;
    m_selection.masked = image.hasAlphaChannel();

    // Вставка вместе с последующим перемещением — один шаг отмены.
    m_document->beginEdit();

    emit selectionChanged();
    emit selectionGeometryChanged(image.size());
    update();
}

void Canvas::pasteFromClipboard()
{
    const QImage img = QApplication::clipboard()->image();
    if (img.isNull()) {
        emit statusMessage(tr("В буфере обмена нет изображения"));
        return;
    }

    // Если вставляемое больше холста, Paint предлагает его расширить.
    if (img.width() > m_document->width() || img.height() > m_document->height()) {
        m_document->resizeCanvas(QSize(qMax(img.width(), m_document->width()),
                                       qMax(img.height(), m_document->height())),
                                 m_settings.color2);
    }

    beginFloatingImage(img, QPoint(0, 0), true);
}

void Canvas::pasteFromFile(const QString &path)
{
    QImage img(path);
    if (img.isNull()) {
        emit statusMessage(tr("Не удалось открыть файл"));
        return;
    }
    beginFloatingImage(img, QPoint(0, 0), false);
}

void Canvas::cropToSelection()
{
    if (!m_selection.active)
        return;

    // Плавающее выделение сначала возвращаем на холст: обрезка — операция
    // над всеми слоями, и содержимое выделения должно быть уже на месте.
    const QRect bounds = m_selection.rect;
    finishSelection();
    m_document->crop(bounds);
}

void Canvas::rotateSelection(int degrees)
{
    if (!m_selection.active) {
        m_document->rotate(degrees);
        return;
    }

    floatSelection();
    QTransform t;
    t.rotate(degrees);
    m_selection.pixels = m_selection.pixels.transformed(t, Qt::SmoothTransformation);

    // Поворачиваем вокруг центра рамки, как это делает Paint.
    const QPoint center = m_selection.rect.center();
    QRect r(QPoint(0, 0), m_selection.pixels.size());
    r.moveCenter(center);
    m_selection.rect = r;

    emit selectionGeometryChanged(r.size());
    update();
}

void Canvas::flipSelection(Qt::Orientation orientation)
{
    if (!m_selection.active) {
        m_document->flip(orientation);
        return;
    }

    floatSelection();
    m_selection.pixels = m_selection.pixels.mirrored(orientation == Qt::Horizontal,
                                                     orientation == Qt::Vertical);
    update();
}

void Canvas::invertColorsOfSelection()
{
    if (!m_selection.active) {
        m_document->invertColors();
        return;
    }

    if (!m_selection.floating && !m_selection.masked) {
        m_document->invertColors(m_selection.rect);
        m_selection.pixels = m_document->image().copy(m_selection.rect);
        update();
        return;
    }

    floatSelection();
    QImage &px = m_selection.pixels;
    for (int y = 0; y < px.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(px.scanLine(y));
        for (int x = 0; x < px.width(); ++x) {
            const QRgb c = line[x];
            if (qAlpha(c) == 0)
                continue;
            line[x] = qRgba(255 - qRed(c), 255 - qGreen(c), 255 - qBlue(c), qAlpha(c));
        }
    }
    update();
}
