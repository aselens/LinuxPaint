#pragma once

#include "Theme.h"
#include "tools/Tool.h"

#include <QHash>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

class Canvas;
class ColorArea;
class Document;
class GalleryPopup;
class LayersPanel;
class Backdrop;
class Ribbon;
class RibbonGroup;

class QAction;
class QActionGroup;
class QComboBox;
class QFontComboBox;
class QLabel;
class QMenu;
class QScrollArea;
class QSlider;
class QToolButton;

// Линейка вдоль края холста. Обычный QWidget без сигналов — ему нужно
// только знать масштаб, сдвиг прокрутки и положение курсора.
class Ruler : public QWidget
{
public:
    Ruler(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setZoom(double zoom);
    void setOffset(int offset);      // сдвиг прокрутки области просмотра
    void setOrigin(int origin);      // отступ холста внутри виджета Canvas
    void setCursorPosition(int position);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Qt::Orientation m_orientation;
    double m_zoom = 1.0;
    int m_offset = 0;
    int m_origin = 0;
    int m_cursor = -1;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    // файл
    void newImage();
    void open();
    bool save();
    bool saveAs();
    void print();
    void showProperties();
    void openRecent();

    // правка и изображение
    void pasteFrom();
    void resizeAndSkew();

    void openSettings();

    // вид
    void updateZoomControls(double zoom);
    void setZoomFromSlider(int value);
    void toggleFullScreen(bool on);
    void applyTheme();

    // состояние
    void updateWindowTitle();
    void updateActionStates();
    void updateCursorLabel(const QPoint &position);
    void clearCursorLabel();
    void updateSelectionLabel(const QSize &size);
    void updateCanvasSizeLabel();
    void onToolChanged(ToolId id);
    void onColoursChanged();
    void updateTextFont();
    void chooseCustomColour();
    void scrollToAnchor(const QPointF &imagePoint);
    void syncRulers();

private:
    void createActions();
    void refreshBackdrop();       // перекрасить подложку под текущую тему
    void createTopBar();          // строка Файл / Правка / Вид и быстрые кнопки
    void createRibbon();
    void createStatusBar();
    void createCentralArea();
    void applyIcons();
    void setupShortcuts();

    bool maybeSave();
    void loadSettings();
    void saveSettings();
    void addRecentFile(const QString &path);
    void refreshRecentMenu();
    QString imageFilters(bool forSaving) const;

    Document *m_document = nullptr;
    Canvas *m_canvas = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    Ribbon *m_ribbon = nullptr;
    ColorArea *m_colorArea = nullptr;
    Ruler *m_horizontalRuler = nullptr;
    Ruler *m_verticalRuler = nullptr;
    QWidget *m_rulerCorner = nullptr;
    LayersPanel *m_layersPanel = nullptr;
    // Обёртка панели слоёв: держит отступ снизу и прячется вместе с ней.
    QWidget *m_layersColumn = nullptr;

    // статусная строка: у каждого показателя свой значок, как в Paint
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_selectionLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QLabel *m_cursorIcon = nullptr;
    QLabel *m_selectionIcon = nullptr;
    QLabel *m_canvasSizeIcon = nullptr;
    QSlider *m_zoomSlider = nullptr;

    // вертикальный ползунок толщины у левого края рабочей области
    QWidget *m_sizePanel = nullptr;
    QWidget *m_sizeBox = nullptr;
    QWidget *m_opacityBox = nullptr;
    QWidget *m_toleranceBox = nullptr;
    QLabel *m_sizePanelIcon = nullptr;
    QLabel *m_opacityPanelIcon = nullptr;
    QLabel *m_tolerancePanelIcon = nullptr;

    // действия
    QAction *m_newAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_saveAsAction = nullptr;
    QAction *m_printAction = nullptr;
    QAction *m_propertiesAction = nullptr;
    QAction *m_exitAction = nullptr;

    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QAction *m_cutAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_pasteFromAction = nullptr;
    QAction *m_selectAllAction = nullptr;
    QAction *m_invertSelectionAction = nullptr;
    QAction *m_deleteSelectionAction = nullptr;
    QAction *m_transparentSelectionAction = nullptr;

    QAction *m_cropAction = nullptr;
    QAction *m_resizeAction = nullptr;
    QAction *m_rotateRightAction = nullptr;
    QAction *m_rotateLeftAction = nullptr;
    QAction *m_rotate180Action = nullptr;
    QAction *m_flipHorizontalAction = nullptr;
    QAction *m_flipVerticalAction = nullptr;
    QAction *m_invertColoursAction = nullptr;
    QAction *m_clearImageAction = nullptr;

    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
    QAction *m_zoomFitAction = nullptr;
    QAction *m_gridAction = nullptr;
    QAction *m_rulersAction = nullptr;
    QAction *m_statusBarAction = nullptr;
    QAction *m_fullScreenAction = nullptr;

    QAction *m_layersAction = nullptr;
    QAction *m_addLayerAction = nullptr;

    QAction *m_aboutAction = nullptr;
    QAction *m_aboutQtAction = nullptr;

    QActionGroup *m_themeGroup = nullptr;
    QHash<int, QAction *> m_toolActions;
    QActionGroup *m_toolGroup = nullptr;

    // элементы ленты, которым нужно обновлять значок
    QToolButton *m_brushButton = nullptr;
    QToolButton *m_outlineButton = nullptr;
    QToolButton *m_fillButton = nullptr;
    QToolButton *m_editColoursButton = nullptr;
    QToolButton *m_ribbonCollapseButton = nullptr;
    QToolButton *m_rotateButton = nullptr;
    QToolButton *m_flipButton = nullptr;

    GalleryPopup *m_brushGallery = nullptr;

    // Контекстная группа «Текст» — видна только при активном инструменте текста.
    RibbonGroup *m_textGroup = nullptr;
    QFontComboBox *m_fontCombo = nullptr;
    QComboBox *m_fontSizeCombo = nullptr;
    QToolButton *m_boldButton = nullptr;
    QToolButton *m_italicButton = nullptr;
    QToolButton *m_underlineButton = nullptr;
    QToolButton *m_opaqueTextButton = nullptr;

    Backdrop *m_backdrop = nullptr;
    Backdrop *m_statusBackdrop = nullptr;   // отдельная — под строкой состояния
    QWidget *m_topBar = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QVector<QToolButton *> m_shapeButtons;   // сетка фигур прямо в ленте

    QMenu *m_recentMenu = nullptr;
    QStringList m_recentFiles;

    // Настройки, переживающие перезапуск. Тема продублирована отдельным
    // полем, потому что применяется до создания виджетов.
    Theme::Mode m_themeMode = Theme::Mode::System;
    QSize m_defaultCanvas = QSize(1152, 648);
    bool m_antialias = true;
    // Прочитаны до создания действий, поэтому хранятся отдельно.
    bool m_startRulers = false;
    bool m_startGrid = false;
    bool m_startStatusBar = true;
};
