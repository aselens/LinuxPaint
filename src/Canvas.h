#pragma once

#include "tools/Tool.h"

#include <QHash>
#include <QImage>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QWidget>

class Document;
class QTimer;

// Плавающее выделение.
//
// Пока пользователь не сдвинул рамку, пиксели остаются в холсте
// (floating == false) — ровно как в Paint, где выделение до первого
// перетаскивания ничего не портит. Как только выделение «взлетает»,
// исходная область заливается Цветом 2, а содержимое живёт отдельно
// и впечатывается обратно при снятии выделения.
struct SelectionState {
    bool active = false;
    bool floating = false;
    bool fromClipboard = false;
    QRect rect;                 // текущее положение/размер на холсте
    QRect sourceRect;           // откуда вырезано
    QImage pixels;              // содержимое выделения
    bool masked = false;        // произвольная форма (в pixels есть альфа)
};

class Canvas : public QWidget
{
    Q_OBJECT

public:
    explicit Canvas(Document *document, QWidget *parent = nullptr);
    ~Canvas() override;

    Document *document() const { return m_document; }
    ToolSettings &settings() { return m_settings; }
    const ToolSettings &settings() const { return m_settings; }

    // --- инструменты ----------------------------------------------------
    void setTool(ToolId id);
    ToolId currentToolId() const { return m_currentId; }
    Tool *currentTool() const { return m_tool; }
    void restorePreviousTool();
    void commitPendingTool();          // зафиксировать фигуру/текст
    void cancelPendingTool();

    // --- настройки (меняются лентой) ------------------------------------
    void setColor1(const QColor &color);
    void setColor2(const QColor &color);
    void swapColors();
    void setStrokeSize(int size);
    void setBrush(BrushType brush);
    void setShape(ShapeType shape);
    void setOutlineStyle(StrokeStyle style, bool enabled);
    void setFillMode(FillMode fill);
    void setTolerance(int percent);
    void setOpacity(int percent);
    void setAntialias(bool on);

    // --- слой текущего штриха ------------------------------------------
    // При непрозрачности меньше 100 % инструмент рисует не прямо в холст,
    // а в отдельный прозрачный слой, который впечатывается разом.
    void beginStrokeLayer();
    void commitStrokeLayer();
    bool hasStrokeLayer() const { return m_strokeLayerActive; }
    QImage &strokeLayer() { return m_strokeLayer; }
    void setFont(const QFont &font);
    void setTextOpaque(bool opaque);
    void setTransparentSelection(bool on);

    // --- масштаб и вид --------------------------------------------------
    double zoom() const { return m_zoom; }
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void zoomToFit(const QSize &viewportSize);
    void zoomAtImagePoint(int direction, const QPointF &anchor);
    void setGridVisible(bool visible);
    bool isGridVisible() const { return m_showGrid; }

    // --- координаты -----------------------------------------------------
    QPointF widgetToImage(const QPointF &p) const;
    QPoint imageToWidget(const QPointF &p) const;
    QRect imageRectToWidget(const QRect &r) const;
    void updateImageRect(const QRect &imageRect = QRect());

    // --- выделение ------------------------------------------------------
    const SelectionState &selection() const { return m_selection; }
    bool hasSelection() const { return m_selection.active; }

    void beginRectSelection(const QRect &rect);
    void beginFreeSelection(const QPainterPath &path);
    void setPreviewSelectionRect(const QRect &rect);
    void clearSelectionShape();
    void floatSelection();
    void setSelectionRect(const QRect &rect);
    void finishSelection();                // впечатать и снять
    void discardSelection();               // снять, не впечатывая (перед отменой)
    void selectAll();
    void deleteSelection();
    void invertSelection();                // выделить всё, кроме текущего

    void cutSelection();
    void copySelection();
    void pasteFromClipboard();
    void pasteFromFile(const QString &path);
    bool canPaste() const;

    void cropToSelection();
    void rotateSelection(int degrees);      // при отсутствии выделения — весь холст
    void flipSelection(Qt::Orientation orientation);
    void invertColorsOfSelection();

    QImage selectionImage() const;          // с учётом прозрачного фона

signals:
    void cursorMoved(const QPoint &imagePos);
    void cursorLeft();
    void zoomChanged(double zoom);
    // Просьба прокрутить область просмотра к точке изображения
    // (лупа и Ctrl+колесо должны удерживать точку под курсором).
    void zoomAnchorRequested(const QPointF &imagePoint);
    void colorsChanged();
    void toolChanged(ToolId id);
    void selectionChanged();
    void selectionGeometryChanged(const QSize &size);
    void statusMessage(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onDocumentChanged(const QRect &dirty);
    void onDocumentSizeChanged(const QSize &size);
    void onTick();

private:
    // Маркеры изменения размера холста — по углам и серединам сторон,
    // как в Paint. Тянуть можно за любой из восьми.
    enum class ResizeHandle {
        None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
    };

    void createTools();
    void updateGeometryForImage();
    void applySettingsToTools();
    QPoint imageOrigin() const;
    ResizeHandle handleAtWidgetPos(const QPoint &pos) const;
    QRect handleRect(ResizeHandle handle) const;
    void drawSelectionOverlay(QPainter &painter);
    void drawCanvasHandles(QPainter &painter);
    QImage maskedPixels(const QImage &source, const QPainterPath &path,
                        const QRect &bounds) const;
    void eraseSelectionSource();
    void stampSelection();
    void beginFloatingImage(const QImage &image, const QPoint &at, bool fromClipboard);

    Document *m_document = nullptr;
    ToolSettings m_settings;

    QHash<int, Tool *> m_tools;
    Tool *m_tool = nullptr;
    ToolId m_currentId = ToolId::Pencil;
    ToolId m_previousId = ToolId::Pencil;

    double m_zoom = 1.0;
    bool m_showGrid = false;
    int m_margin = 8;

    QImage m_strokeLayer;
    bool m_strokeLayerActive = false;

    SelectionState m_selection;
    QRect m_previewSelection;
    bool m_hasPreviewSelection = false;

    ResizeHandle m_activeHandle = ResizeHandle::None;
    bool m_resizingCanvas = false;
    // Будущие границы холста в координатах текущего изображения:
    // при протяжке за верх или левый край угол уходит в отрицательные.
    QRect m_resizePreview;

    QTimer *m_tickTimer = nullptr;
    QPoint m_lastImagePos;
    bool m_toolPressed = false;
};
