#include "MainWindow.h"
#include "Canvas.h"
#include "ColorArea.h"
#include "Dialogs.h"
#include "Document.h"
#include "Backdrop.h"
#include "Icons.h"
#include "LayersPanel.h"
#include "Ribbon.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QUrl>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtMath>

namespace {

const int kRulerThickness = 22;
const double kMinZoom = 0.125;
const double kZoomRange = 256.0;      // от 12,5 % до 3200 %

double sliderToZoom(int value)
{
    return kMinZoom * qPow(kZoomRange, value / 1000.0);
}

int zoomToSlider(double zoom)
{
    return qRound(1000.0 * qLn(zoom / kMinZoom) / qLn(kZoomRange));
}

QToolButton *smallActionButton(QAction *action)
{
    auto *button = new QToolButton;
    button->setDefaultAction(action);
    button->setIconSize(QSize(20, 20));
    button->setAutoRaise(true);
    button->setFixedSize(30, 30);
    return button;
}

// Область прокрутки, показывающая полосу только под курсором. Постоянно
// висящая полоса в маленькой плашке с фигурами занимает заметную часть её
// ширины и лезет в глаза, а нужна лишь в момент прокрутки.
class HoverScrollArea : public QScrollArea
{
public:
    explicit HoverScrollArea(QWidget *parent = nullptr)
        : QScrollArea(parent)
    {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        QScrollArea::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        QScrollArea::leaveEvent(event);
    }
};

// Шаг делений линейки подбирается так, чтобы подписи не слипались.
int rulerStep(double zoom)
{
    const int candidates[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000};
    for (int step : candidates) {
        if (step * zoom >= 60.0)
            return step;
    }
    return 5000;
}

} // namespace

// --- Ruler ---------------------------------------------------------------

Ruler::Ruler(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent)
    , m_orientation(orientation)
{
    if (orientation == Qt::Horizontal)
        setFixedHeight(kRulerThickness);
    else
        setFixedWidth(kRulerThickness);
}

void Ruler::setZoom(double zoom)
{
    m_zoom = zoom;
    update();
}

void Ruler::setOffset(int offset)
{
    m_offset = offset;
    update();
}

void Ruler::setOrigin(int origin)
{
    m_origin = origin;
    update();
}

void Ruler::setCursorPosition(int position)
{
    if (m_cursor == position)
        return;
    m_cursor = position;
    update();
}

void Ruler::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), palette().color(QPalette::Base));

    const bool horizontal = (m_orientation == Qt::Horizontal);
    const int length = horizontal ? width() : height();
    const int step = rulerStep(m_zoom);

    QFont font = p.font();
    font.setPixelSize(9);
    p.setFont(font);

    const QColor line = palette().color(QPalette::WindowText);
    p.setPen(QPen(line, 1));

    // Идём по координатам изображения и переводим их в пиксели виджета.
    const int firstUnit = int((m_offset - m_origin) / m_zoom / step) * step;
    for (int unit = qMax(0, firstUnit);; unit += step) {
        const int pos = int(m_origin + unit * m_zoom) - m_offset;
        if (pos > length)
            break;
        if (pos < 0)
            continue;

        if (horizontal) {
            p.drawLine(pos, kRulerThickness - 7, pos, kRulerThickness - 1);
            p.drawText(QRect(pos + 2, 1, 40, 11), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(unit));
        } else {
            p.drawLine(kRulerThickness - 7, pos, kRulerThickness - 1, pos);
            p.save();
            p.translate(9, pos + 2);
            p.rotate(-90);
            p.drawText(QRect(-40, -10, 40, 11), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(unit));
            p.restore();
        }

        // Половинные деления.
        const int half = int(m_origin + (unit + step / 2.0) * m_zoom) - m_offset;
        if (half >= 0 && half <= length) {
            if (horizontal)
                p.drawLine(half, kRulerThickness - 4, half, kRulerThickness - 1);
            else
                p.drawLine(kRulerThickness - 4, half, kRulerThickness - 1, half);
        }
    }

    // Отметка текущего положения курсора.
    if (m_cursor >= 0) {
        const int pos = int(m_origin + m_cursor * m_zoom) - m_offset;
        p.setPen(QPen(palette().color(QPalette::Highlight), 1));
        if (horizontal)
            p.drawLine(pos, 0, pos, kRulerThickness);
        else
            p.drawLine(0, pos, kRulerThickness, pos);
    }

    p.setPen(QPen(palette().color(QPalette::Shadow), 1));
    if (horizontal)
        p.drawLine(0, height() - 1, width(), height() - 1);
    else
        p.drawLine(width() - 1, 0, width() - 1, height());
}

// --- MainWindow ----------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    loadSettings();

    Theme::apply(m_themeMode);
    Icons::setForeground(Theme::iconForeground());

    m_document = new Document(this);
    m_canvas = new Canvas(m_document, this);

    createActions();
    createTopBar();
    createRibbon();
    createCentralArea();
    createStatusBar();
    applyIcons();
    setupShortcuts();

    // --- проводка сигналов ---
    connect(m_document, &Document::historyChanged, this, &MainWindow::updateActionStates);
    connect(m_document, &Document::modifiedChanged, this, &MainWindow::updateWindowTitle);
    connect(m_document, &Document::filePathChanged, this, &MainWindow::updateWindowTitle);
    connect(m_document, &Document::sizeChanged, this, &MainWindow::updateCanvasSizeLabel);
    connect(m_document, &Document::sizeChanged, this, &MainWindow::syncRulers);

    connect(m_canvas, &Canvas::cursorMoved, this, &MainWindow::updateCursorLabel);
    connect(m_canvas, &Canvas::cursorLeft, this, &MainWindow::clearCursorLabel);
    connect(m_canvas, &Canvas::zoomChanged, this, &MainWindow::updateZoomControls);
    connect(m_canvas, &Canvas::selectionChanged, this, &MainWindow::updateActionStates);
    connect(m_canvas, &Canvas::selectionGeometryChanged, this, &MainWindow::updateSelectionLabel);
    connect(m_canvas, &Canvas::toolChanged, this, &MainWindow::onToolChanged);
    connect(m_canvas, &Canvas::colorsChanged, this, &MainWindow::onColoursChanged);
    connect(m_canvas, &Canvas::zoomAnchorRequested, this, &MainWindow::scrollToAnchor);
    connect(m_canvas, &Canvas::statusMessage, this, [this](const QString &text) {
        statusBar()->showMessage(text, 4000);
    });

    connect(m_colorArea, &ColorArea::colour1Changed, this, [this](const QColor &c) {
        m_canvas->setColor1(c);
    });
    connect(m_colorArea, &ColorArea::colour2Changed, this, [this](const QColor &c) {
        m_canvas->setColor2(c);
    });

    setAcceptDrops(true);
    setWindowIcon(Icons::application());
    resize(1320, 860);

    refreshBackdrop();

    // Настройки, для применения которых нужны уже созданные виджеты.
    m_canvas->setAntialias(m_antialias);
    m_canvas->setGridVisible(m_gridAction->isChecked());
    statusBar()->setVisible(m_statusBarAction->isChecked());

    // Canvas стартует с карандашом, поэтому setTool() ничего не переключит —
    // просто отмечаем соответствующую кнопку в ленте.
    onToolChanged(m_canvas->currentToolId());
    updateActionStates();
    updateWindowTitle();
    updateCanvasSizeLabel();
    updateZoomControls(m_canvas->zoom());
}

MainWindow::~MainWindow()
{
    saveSettings();
}

// --- действия ------------------------------------------------------------

void MainWindow::createActions()
{
    auto make = [this](const QString &text, const QKeySequence &shortcut = QKeySequence()) {
        auto *action = new QAction(text, this);
        if (!shortcut.isEmpty())
            action->setShortcut(shortcut);
        return action;
    };

    // файл
    m_newAction = make(tr("Создать"), QKeySequence::New);
    m_openAction = make(tr("Открыть"), QKeySequence::Open);
    m_saveAction = make(tr("Сохранить"), QKeySequence::Save);
    m_saveAsAction = make(tr("Сохранить как"), QKeySequence::SaveAs);
    m_printAction = make(tr("Печать"), QKeySequence::Print);
    m_propertiesAction = make(tr("Свойства"), QKeySequence(QStringLiteral("Ctrl+E")));
    m_exitAction = make(tr("Выход"), QKeySequence::Quit);

    connect(m_newAction, &QAction::triggered, this, &MainWindow::newImage);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::open);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::save);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::saveAs);
    connect(m_printAction, &QAction::triggered, this, &MainWindow::print);
    connect(m_propertiesAction, &QAction::triggered, this, &MainWindow::showProperties);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::close);

    // правка
    m_undoAction = make(tr("Отменить"), QKeySequence::Undo);
    m_redoAction = make(tr("Вернуть"), QKeySequence::Redo);
    m_cutAction = make(tr("Вырезать"), QKeySequence::Cut);
    m_copyAction = make(tr("Копировать"), QKeySequence::Copy);
    m_pasteAction = make(tr("Вставить"), QKeySequence::Paste);
    m_pasteFromAction = make(tr("Вставить из файла"));
    m_selectAllAction = make(tr("Выделить всё"), QKeySequence::SelectAll);
    m_invertSelectionAction = make(tr("Обратить выделение"),
                                   QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    m_deleteSelectionAction = make(tr("Удалить выделенное"), QKeySequence::Delete);
    m_transparentSelectionAction = make(tr("Прозрачное выделение"));
    m_transparentSelectionAction->setCheckable(true);

    connect(m_undoAction, &QAction::triggered, this, [this] {
        // Незавершённая фигура и висящее выделение снимаются без следа —
        // отмена должна работать с историей документа, а не с ними.
        m_canvas->cancelPendingTool();
        m_canvas->discardSelection();
        m_document->undo();
    });
    connect(m_redoAction, &QAction::triggered, this, [this] {
        m_canvas->cancelPendingTool();
        m_canvas->discardSelection();
        m_document->redo();
    });
    connect(m_cutAction, &QAction::triggered, this, [this] { m_canvas->cutSelection(); });
    connect(m_copyAction, &QAction::triggered, this, [this] { m_canvas->copySelection(); });
    connect(m_pasteAction, &QAction::triggered, this, [this] { m_canvas->pasteFromClipboard(); });
    connect(m_pasteFromAction, &QAction::triggered, this, &MainWindow::pasteFrom);
    connect(m_selectAllAction, &QAction::triggered, this, [this] { m_canvas->selectAll(); });
    connect(m_invertSelectionAction, &QAction::triggered, this, [this] {
        m_canvas->invertSelection();
    });
    connect(m_deleteSelectionAction, &QAction::triggered, this, [this] {
        m_canvas->deleteSelection();
    });
    connect(m_transparentSelectionAction, &QAction::toggled, this, [this](bool on) {
        m_canvas->setTransparentSelection(on);
    });

    // изображение
    m_cropAction = make(tr("Обрезать"), QKeySequence(QStringLiteral("Ctrl+Shift+X")));
    m_resizeAction = make(tr("Изменить размер"), QKeySequence(QStringLiteral("Ctrl+W")));
    m_rotateRightAction = make(tr("Повернуть на 90° вправо"));
    m_rotateLeftAction = make(tr("Повернуть на 90° влево"));
    m_rotate180Action = make(tr("Повернуть на 180°"));
    m_flipHorizontalAction = make(tr("Отразить по горизонтали"));
    m_flipVerticalAction = make(tr("Отразить по вертикали"));
    m_invertColoursAction = make(tr("Обратить цвета"),
                                 QKeySequence(QStringLiteral("Ctrl+Shift+I")));
    m_clearImageAction = make(tr("Очистить"), QKeySequence(QStringLiteral("Ctrl+Shift+N")));

    connect(m_cropAction, &QAction::triggered, this, [this] { m_canvas->cropToSelection(); });
    connect(m_resizeAction, &QAction::triggered, this, &MainWindow::resizeAndSkew);
    connect(m_rotateRightAction, &QAction::triggered, this, [this] {
        m_canvas->rotateSelection(90);
    });
    connect(m_rotateLeftAction, &QAction::triggered, this, [this] {
        m_canvas->rotateSelection(270);
    });
    connect(m_rotate180Action, &QAction::triggered, this, [this] {
        m_canvas->rotateSelection(180);
    });
    connect(m_flipHorizontalAction, &QAction::triggered, this, [this] {
        m_canvas->flipSelection(Qt::Horizontal);
    });
    connect(m_flipVerticalAction, &QAction::triggered, this, [this] {
        m_canvas->flipSelection(Qt::Vertical);
    });
    connect(m_invertColoursAction, &QAction::triggered, this, [this] {
        m_canvas->invertColorsOfSelection();
    });
    connect(m_clearImageAction, &QAction::triggered, this, [this] {
        m_document->clearImage(m_canvas->settings().color2);
    });

    // вид
    m_zoomInAction = make(tr("Увеличить"), QKeySequence::ZoomIn);
    m_zoomOutAction = make(tr("Уменьшить"), QKeySequence::ZoomOut);
    m_zoomResetAction = make(tr("Масштаб 100 %"), QKeySequence(QStringLiteral("Ctrl+0")));
    m_zoomFitAction = make(tr("Вписать в окно"), QKeySequence(QStringLiteral("Ctrl+9")));
    m_gridAction = make(tr("Линии сетки"), QKeySequence(QStringLiteral("Ctrl+G")));
    m_rulersAction = make(tr("Линейки"), QKeySequence(QStringLiteral("Ctrl+R")));
    m_statusBarAction = make(tr("Строка состояния"));
    m_fullScreenAction = make(tr("Во весь экран"), QKeySequence(Qt::Key_F11));

    // Состояние берём из сохранённых настроек. Важно, что это происходит
    // до подключения обработчиков: иначе они сработают на виджетах,
    // которых ещё нет.
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(m_startGrid);
    m_rulersAction->setCheckable(true);
    m_rulersAction->setChecked(m_startRulers);
    m_statusBarAction->setCheckable(true);
    m_statusBarAction->setChecked(m_startStatusBar);
    m_fullScreenAction->setCheckable(true);

    connect(m_zoomInAction, &QAction::triggered, this, [this] { m_canvas->zoomIn(); });
    connect(m_zoomOutAction, &QAction::triggered, this, [this] { m_canvas->zoomOut(); });
    connect(m_zoomResetAction, &QAction::triggered, this, [this] { m_canvas->resetZoom(); });
    connect(m_zoomFitAction, &QAction::triggered, this, [this] {
        m_canvas->zoomToFit(m_scrollArea->viewport()->size());
    });
    connect(m_gridAction, &QAction::toggled, this, [this](bool on) {
        m_canvas->setGridVisible(on);
    });
    connect(m_rulersAction, &QAction::toggled, this, [this](bool on) {
        m_horizontalRuler->setVisible(on);
        m_verticalRuler->setVisible(on);
        m_rulerCorner->setVisible(on);
    });
    connect(m_statusBarAction, &QAction::toggled, this, [this](bool on) {
        statusBar()->setVisible(on);
    });
    connect(m_fullScreenAction, &QAction::toggled, this, &MainWindow::toggleFullScreen);

    // слои
    m_layersAction = make(tr("Слои"), QKeySequence(QStringLiteral("Ctrl+L")));
    m_layersAction->setCheckable(true);
    m_layersAction->setToolTip(tr("Показать панель слоёв"));
    m_addLayerAction = make(tr("Добавить слой"),
                            QKeySequence(QStringLiteral("Ctrl+Shift+L")));

    connect(m_layersAction, &QAction::toggled, this, [this](bool on) {
        if (m_layersPanel)
            m_layersPanel->setVisible(on);
    });
    connect(m_addLayerAction, &QAction::triggered, this, [this] {
        m_canvas->finishSelection();
        m_document->addLayer();
        // Новый слой без открытой панели незаметен — показываем её.
        if (!m_layersAction->isChecked())
            m_layersAction->setChecked(true);
    });

    // справка
    m_aboutAction = make(tr("О программе"));
    m_aboutQtAction = make(tr("О Qt"));
    connect(m_aboutAction, &QAction::triggered, this, [this] { showAboutDialog(this); });
    connect(m_aboutQtAction, &QAction::triggered, this, [this] { QMessageBox::aboutQt(this); });

    // инструменты
    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    struct ToolEntry { ToolId id; const char *title; const char *shortcut; };
    const ToolEntry entries[] = {
        {ToolId::Select,      QT_TR_NOOP("Прямоугольная область"), "S"},
        {ToolId::FreeSelect,  QT_TR_NOOP("Произвольная область"),  ""},
        {ToolId::Pencil,      QT_TR_NOOP("Карандаш"),              "P"},
        {ToolId::Fill,        QT_TR_NOOP("Заливка"),               "F"},
        {ToolId::Text,        QT_TR_NOOP("Текст"),                 "T"},
        {ToolId::Eraser,      QT_TR_NOOP("Ластик"),                "E"},
        {ToolId::ColorPicker, QT_TR_NOOP("Палитра"),               "K"},
        {ToolId::Magnifier,   QT_TR_NOOP("Лупа"),                  "M"},
        {ToolId::Brush,       QT_TR_NOOP("Кисти"),                 "B"},
        {ToolId::Shape,       QT_TR_NOOP("Фигуры"),                ""}
    };

    for (const ToolEntry &entry : entries) {
        auto *action = new QAction(tr(entry.title), this);
        action->setCheckable(true);
        if (entry.shortcut[0] != '\0')
            action->setShortcut(QKeySequence(QString::fromLatin1(entry.shortcut)));
        action->setToolTip(action->text());
        m_toolGroup->addAction(action);
        m_toolActions.insert(int(entry.id), action);

        const ToolId id = entry.id;
        connect(action, &QAction::triggered, this, [this, id] { m_canvas->setTool(id); });
    }
}

void MainWindow::setupShortcuts()
{
    // Строка меню собрана из кнопок с всплывающими меню, а не из QMenuBar,
    // поэтому Qt не регистрирует горячие клавиши автоматически: без этого
    // они работали бы только при открытом меню. Привязываем к окну все
    // действия, у которых сочетание задано.
    const QList<QAction *> actions = findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->shortcut().isEmpty())
            continue;
        action->setShortcutContext(Qt::WindowShortcut);
        addAction(action);
    }
}

void MainWindow::createTopBar()
{
    // В современном Paint «Файл / Правка / Вид» — не системная строка меню,
    // а обычные кнопки, стоящие в одном ряду с быстрыми командами.
    // Поэтому QMenuBar здесь не используется: своя строка даёт нужный вид
    // и одинаково выглядит во всех окружениях рабочего стола.
    m_topBar = new QWidget(this);
    auto *layout = new QHBoxLayout(m_topBar);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(2);

    auto addMenuButton = [this, layout](const QString &title) {
        auto *button = new QToolButton(m_topBar);
        button->setObjectName(QStringLiteral("TopMenuButton"));
        button->setText(title);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setAutoRaise(true);
        auto *menu = new QMenu(button);
        button->setMenu(menu);
        layout->addWidget(button);
        return menu;
    };

    auto *fileMenu = addMenuButton(tr("Файл"));
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    m_recentMenu = fileMenu->addMenu(tr("Последние файлы"));
    refreshRecentMenu();
    fileMenu->addSeparator();
    fileMenu->addAction(m_printAction);
    fileMenu->addAction(m_propertiesAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    auto *editMenu = addMenuButton(tr("Правка"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_cutAction);
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);
    editMenu->addAction(m_pasteFromAction);
    editMenu->addSeparator();
    editMenu->addAction(m_selectAllAction);
    editMenu->addAction(m_invertSelectionAction);
    editMenu->addAction(m_deleteSelectionAction);

    auto *viewMenu = addMenuButton(tr("Вид"));
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
    viewMenu->addAction(m_zoomFitAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_rulersAction);
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_statusBarAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fullScreenAction);

    // Быстрые команды.
    layout->addSpacing(10);
    layout->addWidget(smallActionButton(m_saveAction));
    layout->addWidget(smallActionButton(m_printAction));
    layout->addSpacing(8);
    layout->addWidget(smallActionButton(m_undoAction));
    layout->addWidget(smallActionButton(m_redoAction));
    layout->addStretch(1);

    // Шестерёнка справа: оформление и справка.
    m_settingsButton = new QToolButton(m_topBar);
    m_settingsButton->setIconSize(QSize(20, 20));
    m_settingsButton->setAutoRaise(true);
    m_settingsButton->setFixedSize(30, 30);
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);
    m_settingsButton->setToolTip(tr("Параметры"));

    auto *settingsMenu = new QMenu(m_settingsButton);

    QAction *preferences = settingsMenu->addAction(tr("Параметры"));
    preferences->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    connect(preferences, &QAction::triggered, this, &MainWindow::openSettings);
    settingsMenu->addSeparator();

    // Быстрое переключение темы оставлено и здесь: это самая частая
    // настройка, ради неё не хочется открывать целое окно.
    auto *themeMenu = settingsMenu->addMenu(tr("Оформление"));
    m_themeGroup = new QActionGroup(this);
    const Theme::Mode modes[] = {Theme::Mode::System, Theme::Mode::Light, Theme::Mode::Dark};
    for (Theme::Mode mode : modes) {
        auto *action = new QAction(Theme::modeName(mode), this);
        action->setCheckable(true);
        action->setChecked(mode == m_themeMode);
        action->setData(int(mode));
        m_themeGroup->addAction(action);
        themeMenu->addAction(action);
        connect(action, &QAction::triggered, this, &MainWindow::applyTheme);
    }
    settingsMenu->addSeparator();
    settingsMenu->addAction(m_aboutAction);
    settingsMenu->addAction(m_aboutQtAction);
    m_settingsButton->setMenu(settingsMenu);

    layout->addWidget(m_settingsButton);
}

// --- лента ---------------------------------------------------------------

void MainWindow::createRibbon()
{
    m_ribbon = new Ribbon(this);
    const QIcon chevron = Icons::action(Icons::Action::Chevron);

    // Подписей под кнопками нет — только названия групп внизу, как в Paint.
    // Что именно делает кнопка, объясняет всплывающая подсказка.

    // --- выделение ---
    auto *selection = m_ribbon->addGroup(tr("Выделение"));
    auto *selectMain = RibbonUi::iconButton(m_toolActions.value(int(ToolId::Select)), 40, 24);
    auto *selectMenu = new QMenu(this);
    selectMenu->addAction(m_toolActions.value(int(ToolId::Select)));
    selectMenu->addAction(m_toolActions.value(int(ToolId::FreeSelect)));
    selectMenu->addSeparator();
    selectMenu->addAction(m_selectAllAction);
    selectMenu->addAction(m_invertSelectionAction);
    selectMenu->addAction(m_deleteSelectionAction);
    selectMenu->addSeparator();
    selectMenu->addAction(m_cutAction);
    selectMenu->addAction(m_copyAction);
    selectMenu->addAction(m_pasteAction);
    selectMenu->addAction(m_pasteFromAction);
    selectMenu->addSeparator();
    selectMenu->addAction(m_transparentSelectionAction);
    selection->addItem(RibbonUi::stackedMenuButton(selectMain, selectMenu, chevron, 40));

    // --- изображение ---
    auto *image = m_ribbon->addGroup(tr("Изображение"));

    // Кнопки-меню делаем обычными кнопками с мгновенным раскрытием:
    // шеврон встроен в значок, поэтому ширина у них такая же, как у
    // остальных, и сетка не разъезжается.
    auto makeMenuButton = [](QMenu *menu, const QString &tip) {
        auto *button = new QToolButton;
        button->setToolTip(tip);
        button->setIconSize(QSize(20, 20));
        button->setAutoRaise(true);
        button->setFixedSize(32, 32);
        button->setPopupMode(QToolButton::InstantPopup);
        button->setMenu(menu);
        return button;
    };

    auto *rotateMenu = new QMenu(this);
    rotateMenu->addAction(m_rotateRightAction);
    rotateMenu->addAction(m_rotateLeftAction);
    rotateMenu->addAction(m_rotate180Action);
    m_rotateButton = makeMenuButton(rotateMenu, tr("Повернуть"));

    auto *flipMenu = new QMenu(this);
    flipMenu->addAction(m_flipHorizontalAction);
    flipMenu->addAction(m_flipVerticalAction);
    m_flipButton = makeMenuButton(flipMenu, tr("Отразить"));

    image->addItem(RibbonUi::buttonGrid({
        RibbonUi::iconButton(m_cropAction, 32, 20),
        RibbonUi::iconButton(m_resizeAction, 32, 20),
        m_rotateButton,
        m_flipButton,
        RibbonUi::iconButton(m_invertColoursAction, 32, 20),
        RibbonUi::iconButton(m_clearImageAction, 32, 20)
    }, 2));

    // --- инструменты ---
    auto *tools = m_ribbon->addGroup(tr("Инструменты"));

    // Допуск заливки переехал в панель ползунков слева: там он появляется
    // при выборе заливки и не заставляет её кнопку отличаться от прочих.
    tools->addItem(RibbonUi::buttonGrid({
        RibbonUi::iconButton(m_toolActions.value(int(ToolId::Pencil)), 32, 20),
        RibbonUi::iconButton(m_toolActions.value(int(ToolId::Fill)), 32, 20),
        RibbonUi::iconButton(m_toolActions.value(int(ToolId::Text)), 32, 20),
        RibbonUi::iconButton(m_toolActions.value(int(ToolId::Eraser)), 32, 20),
        RibbonUi::iconButton(m_toolActions.value(int(ToolId::ColorPicker)), 32, 20),
        RibbonUi::iconButton(m_toolActions.value(int(ToolId::Magnifier)), 32, 20)
    }, 2));

    // --- кисти ---
    auto *brushes = m_ribbon->addGroup(tr("Кисти"));
    m_brushGallery = new GalleryPopup(3, this);
    const struct { BrushType type; const char *name; } brushList[] = {
        {BrushType::Brush,            QT_TR_NOOP("Кисть")},
        {BrushType::Calligraphy1,     QT_TR_NOOP("Каллиграфическая кисть 1")},
        {BrushType::Calligraphy2,     QT_TR_NOOP("Каллиграфическая кисть 2")},
        {BrushType::Airbrush,         QT_TR_NOOP("Аэрограф")},
        {BrushType::OilBrush,         QT_TR_NOOP("Масляная кисть")},
        {BrushType::Crayon,           QT_TR_NOOP("Мелок")},
        {BrushType::Marker,           QT_TR_NOOP("Маркер")},
        {BrushType::NaturalPencil,    QT_TR_NOOP("Карандаш")},
        {BrushType::WatercolourBrush, QT_TR_NOOP("Акварель")}
    };
    for (const auto &entry : brushList)
        m_brushGallery->addEntry(int(entry.type), Icons::brush(entry.type), tr(entry.name));
    m_brushGallery->setCurrentEntry(int(BrushType::Brush));

    m_brushButton = RibbonUi::iconButton(m_toolActions.value(int(ToolId::Brush)), 40, 24);
    brushes->addItem(RibbonUi::stackedMenuButton(m_brushButton, m_brushGallery, chevron, 40));

    connect(m_brushGallery, &GalleryPopup::entrySelected, this, [this](int id) {
        m_canvas->setBrush(BrushType(id));
        m_canvas->setTool(ToolId::Brush);
        m_brushButton->setIcon(Icons::brush(BrushType(id)));
    });

    // --- фигуры ---
    auto *shapes = m_ribbon->addGroup(tr("Фигуры"));
    const struct { ShapeType type; const char *name; } shapeList[] = {
        {ShapeType::Line,             QT_TR_NOOP("Линия")},
        {ShapeType::Curve,            QT_TR_NOOP("Кривая")},
        {ShapeType::Oval,             QT_TR_NOOP("Овал")},
        {ShapeType::Rectangle,        QT_TR_NOOP("Прямоугольник")},
        {ShapeType::RoundedRectangle, QT_TR_NOOP("Скруглённый прямоугольник")},
        {ShapeType::Polygon,          QT_TR_NOOP("Многоугольник")},
        {ShapeType::Triangle,         QT_TR_NOOP("Треугольник")},
        {ShapeType::RightTriangle,    QT_TR_NOOP("Прямоугольный треугольник")},
        {ShapeType::Diamond,          QT_TR_NOOP("Ромб")},
        {ShapeType::Pentagon,         QT_TR_NOOP("Пятиугольник")},
        {ShapeType::Hexagon,          QT_TR_NOOP("Шестиугольник")},
        {ShapeType::RightArrow,       QT_TR_NOOP("Стрелка вправо")},
        {ShapeType::LeftArrow,        QT_TR_NOOP("Стрелка влево")},
        {ShapeType::UpArrow,          QT_TR_NOOP("Стрелка вверх")},
        {ShapeType::DownArrow,        QT_TR_NOOP("Стрелка вниз")},
        {ShapeType::FourPointStar,    QT_TR_NOOP("Четырёхконечная звезда")},
        {ShapeType::FivePointStar,    QT_TR_NOOP("Пятиконечная звезда")},
        {ShapeType::SixPointStar,     QT_TR_NOOP("Шестиконечная звезда")},
        {ShapeType::RoundedCallout,   QT_TR_NOOP("Прямоугольная выноска")},
        {ShapeType::OvalCallout,      QT_TR_NOOP("Овальная выноска")},
        {ShapeType::CloudCallout,     QT_TR_NOOP("Выноска-облако")},
        {ShapeType::Heart,            QT_TR_NOOP("Сердце")},
        {ShapeType::Lightning,        QT_TR_NOOP("Молния")}
    };
    // Контур и заливка — узкой колонкой слева от сетки, как в оригинале.
    m_outlineButton = new QToolButton;
    m_outlineButton->setToolTip(tr("Контур фигуры"));
    m_outlineButton->setPopupMode(QToolButton::InstantPopup);
    m_outlineButton->setAutoRaise(true);
    m_outlineButton->setIconSize(QSize(20, 20));
    m_outlineButton->setFixedSize(32, 32);

    auto *outlineMenu = new QMenu(m_outlineButton);
    auto *outlineGroup = new QActionGroup(this);
    const struct { StrokeStyle style; bool enabled; const char *name; } outlineList[] = {
        {StrokeStyle::Solid,          false, QT_TR_NOOP("Без контура")},
        {StrokeStyle::Solid,          true,  QT_TR_NOOP("Сплошной цвет")},
        {StrokeStyle::Crayon,         true,  QT_TR_NOOP("Мелок")},
        {StrokeStyle::Marker,         true,  QT_TR_NOOP("Маркер")},
        {StrokeStyle::Oil,            true,  QT_TR_NOOP("Масло")},
        {StrokeStyle::NaturalPencil,  true,  QT_TR_NOOP("Карандаш")},
        {StrokeStyle::Watercolour,    true,  QT_TR_NOOP("Акварель")}
    };
    for (const auto &entry : outlineList) {
        auto *action = new QAction(tr(entry.name), this);
        action->setCheckable(true);
        action->setChecked(entry.enabled && entry.style == StrokeStyle::Solid);
        outlineGroup->addAction(action);
        outlineMenu->addAction(action);
        const StrokeStyle style = entry.style;
        const bool enabled = entry.enabled;
        connect(action, &QAction::triggered, this, [this, style, enabled] {
            m_canvas->setOutlineStyle(style, enabled);
            m_outlineButton->setIcon(Icons::withChevron(
                enabled ? Icons::strokeStyle(style) : Icons::fillStyle(FillMode::None)));
        });
    }
    m_outlineButton->setMenu(outlineMenu);

    m_fillButton = new QToolButton;
    m_fillButton->setToolTip(tr("Заливка фигуры"));
    m_fillButton->setPopupMode(QToolButton::InstantPopup);
    m_fillButton->setAutoRaise(true);
    m_fillButton->setIconSize(QSize(20, 20));
    m_fillButton->setFixedSize(32, 32);

    auto *fillMenu = new QMenu(m_fillButton);
    auto *fillGroup = new QActionGroup(this);
    const struct { FillMode mode; const char *name; } fillList[] = {
        {FillMode::None,          QT_TR_NOOP("Без заливки")},
        {FillMode::Solid,         QT_TR_NOOP("Сплошной цвет")},
        {FillMode::Crayon,        QT_TR_NOOP("Мелок")},
        {FillMode::Marker,        QT_TR_NOOP("Маркер")},
        {FillMode::Oil,           QT_TR_NOOP("Масло")},
        {FillMode::NaturalPencil, QT_TR_NOOP("Карандаш")},
        {FillMode::Watercolour,   QT_TR_NOOP("Акварель")}
    };
    for (const auto &entry : fillList) {
        auto *action = new QAction(tr(entry.name), this);
        action->setCheckable(true);
        action->setChecked(entry.mode == FillMode::None);
        fillGroup->addAction(action);
        fillMenu->addAction(action);
        const FillMode mode = entry.mode;
        connect(action, &QAction::triggered, this, [this, mode] {
            m_canvas->setFillMode(mode);
            m_fillButton->setIcon(Icons::withChevron(Icons::fillStyle(mode)));
        });
    }
    m_fillButton->setMenu(fillMenu);

    auto *styleColumn = new QWidget;
    auto *styleLayout = new QVBoxLayout(styleColumn);
    styleLayout->setContentsMargins(0, 0, 0, 0);
    styleLayout->setSpacing(1);
    styleLayout->addWidget(m_outlineButton);
    styleLayout->addWidget(m_fillButton);

    // Сами фигуры выложены сеткой прямо в ленте — без выпадающей галереи,
    // как в современном Paint. Порядок в списке совпадает с порядком
    // значений ShapeType, поэтому индекс кнопки и есть номер фигуры.
    QVector<QToolButton *> shapeButtons;
    for (const auto &entry : shapeList) {
        auto *button = new QToolButton;
        button->setIcon(Icons::shape(entry.type));
        // Значок почти во всю кнопку: клетки менять нельзя, а фигуры должны
        // быть крупнее.
        button->setIconSize(QSize(24, 24));
        button->setFixedSize(27, 27);
        button->setAutoRaise(true);
        button->setCheckable(true);
        button->setToolTip(tr(entry.name));

        const ShapeType type = entry.type;
        connect(button, &QToolButton::clicked, this, [this, type] {
            m_canvas->setShape(type);
            m_canvas->setTool(ToolId::Shape);
            for (int i = 0; i < m_shapeButtons.size(); ++i)
                m_shapeButtons[i]->setChecked(i == int(type));
        });

        shapeButtons.append(button);
        m_shapeButtons.append(button);
    }
    // Сетка фигур по семь в ряд, обведённая рамкой, — как в Paint. Все они
    // в три ряда не помещаются, поэтому лишние уходят под прокрутку, и край
    // четвёртого ряда выглядывает снизу, подсказывая, что там ещё есть.
    const int shapesPerRow = 7;
    const int shapeCell = 27;
    const int shapeGap = 1;
    const int shapeMargin = 3;
    const int shapeRows = (int(shapeButtons.size()) + shapesPerRow - 1) / shapesPerRow;

    auto *shapesHost = new QWidget;
    auto *shapesGrid = new QGridLayout(shapesHost);
    shapesGrid->setContentsMargins(shapeMargin, shapeMargin, shapeMargin, shapeMargin);
    shapesGrid->setSpacing(shapeGap);
    for (int i = 0; i < shapeButtons.size(); ++i)
        shapesGrid->addWidget(shapeButtons[i], i / shapesPerRow, i % shapesPerRow);

    // Размер сетки задан жёстко, и она не растягивается под область
    // просмотра. Иначе при появлении полосы прокрутки виджет ужимался бы,
    // и все фигуры прыгали влево, а при уходе курсора — обратно.
    const int hostWidth = shapesPerRow * shapeCell + (shapesPerRow - 1) * shapeGap
                          + shapeMargin * 2;
    const int hostHeight = shapeRows * shapeCell + (shapeRows - 1) * shapeGap
                           + shapeMargin * 2;
    shapesHost->setFixedSize(hostWidth, hostHeight);

    auto *shapesBox = new HoverScrollArea;
    shapesBox->setObjectName(QStringLiteral("ShapesBox"));
    shapesBox->setWidget(shapesHost);
    shapesBox->setWidgetResizable(false);
    shapesBox->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    shapesBox->setFrameShape(QFrame::NoFrame);
    // По ширине — запас под полосу прокрутки, чтобы она никого не двигала.
    // По высоте — три ряда и полоска четвёртого: она и показывает, что
    // фигуры на этом не кончаются.
    shapesBox->setFixedSize(hostWidth + 12,
                            3 * shapeCell + 2 * shapeGap + shapeMargin * 2 + 9);

    shapes->addItem(shapesBox);
    shapes->addItem(styleColumn);

    // --- цвета ---
    auto *colours = m_ribbon->addGroup(tr("Цвета"));
    m_colorArea = new ColorArea(this);
    colours->addItem(m_colorArea);

    m_editColoursButton = new QToolButton;
    m_editColoursButton->setToolTip(tr("Изменение цветов"));
    m_editColoursButton->setIconSize(QSize(26, 26));
    m_editColoursButton->setAutoRaise(true);
    m_editColoursButton->setFixedSize(38, 38);
    connect(m_editColoursButton, &QToolButton::clicked, this, &MainWindow::chooseCustomColour);
    colours->addItem(m_editColoursButton);

    // --- слои ---
    auto *layers = m_ribbon->addGroup(tr("Слои"));
    auto *layersButton = RibbonUi::iconButton(m_layersAction, 40, 24);
    auto *layersMenu = new QMenu(this);
    layersMenu->addAction(m_addLayerAction);
    layersMenu->addSeparator();
    layersMenu->addAction(m_layersAction);
    layers->addItem(RibbonUi::stackedMenuButton(layersButton, layersMenu, chevron, 40));

    // --- текст (контекстная группа) ---
    m_textGroup = m_ribbon->addGroup(tr("Текст"));

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setFixedWidth(150);
    m_fontCombo->setCurrentFont(m_canvas->settings().font);
    m_textGroup->addItem(m_fontCombo);

    m_fontSizeCombo = new QComboBox;
    m_fontSizeCombo->setEditable(true);
    m_fontSizeCombo->setFixedWidth(60);
    const int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 36, 48, 72, 96};
    for (int size : sizes)
        m_fontSizeCombo->addItem(QString::number(size));
    m_fontSizeCombo->setCurrentText(QString::number(m_canvas->settings().font.pointSize()));
    m_textGroup->addItem(m_fontSizeCombo);

    auto makeStyleToggle = [](const QString &label, bool bold, bool italic, bool underline) {
        auto *button = new QToolButton;
        button->setText(label);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setFixedSize(28, 28);
        QFont font = button->font();
        font.setBold(bold);
        font.setItalic(italic);
        font.setUnderline(underline);
        button->setFont(font);
        return button;
    };

    m_boldButton = makeStyleToggle(tr("Ж"), true, false, false);
    m_italicButton = makeStyleToggle(tr("К"), false, true, false);
    m_underlineButton = makeStyleToggle(tr("Ч"), false, false, true);
    m_boldButton->setToolTip(tr("Полужирный"));
    m_italicButton->setToolTip(tr("Курсив"));
    m_underlineButton->setToolTip(tr("Подчёркнутый"));
    m_textGroup->addItem(RibbonUi::buttonGrid({m_boldButton, m_italicButton,
                                               m_underlineButton}, 1));

    m_opaqueTextButton = new QToolButton;
    m_opaqueTextButton->setText(tr("Фон"));
    m_opaqueTextButton->setCheckable(true);
    m_opaqueTextButton->setAutoRaise(true);
    m_opaqueTextButton->setToolTip(tr("Непрозрачная подложка под текстом (Цвет 2)"));
    m_textGroup->addItem(m_opaqueTextButton);

    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &MainWindow::updateTextFont);
    connect(m_fontSizeCombo, &QComboBox::currentTextChanged, this, &MainWindow::updateTextFont);
    connect(m_boldButton, &QToolButton::toggled, this, &MainWindow::updateTextFont);
    connect(m_italicButton, &QToolButton::toggled, this, &MainWindow::updateTextFont);
    connect(m_underlineButton, &QToolButton::toggled, this, &MainWindow::updateTextFont);
    connect(m_opaqueTextButton, &QToolButton::toggled, this, [this](bool on) {
        m_canvas->setTextOpaque(on);
    });

    m_textGroup->setVisible(false);

    m_ribbon->addStretch();

    // Кнопка сворачивания ленты в правом краю — как в Paint.
    m_ribbonCollapseButton = new QToolButton;
    m_ribbonCollapseButton->setObjectName(QStringLiteral("RibbonChevron"));
    m_ribbonCollapseButton->setIconSize(QSize(16, 16));
    m_ribbonCollapseButton->setAutoRaise(true);
    m_ribbonCollapseButton->setFixedSize(28, 24);
    m_ribbonCollapseButton->setToolTip(tr("Свернуть ленту"));
    connect(m_ribbonCollapseButton, &QToolButton::clicked, this, [this] {
        const bool collapse = !m_ribbon->isCollapsed();
        m_ribbon->setCollapsed(collapse);
        m_ribbonCollapseButton->setIcon(Icons::action(collapse ? Icons::Action::Chevron
                                                              : Icons::Action::ChevronUp));
        m_ribbonCollapseButton->setToolTip(collapse ? tr("Развернуть ленту")
                                                    : tr("Свернуть ленту"));
    });
    m_ribbon->addCornerWidget(m_ribbonCollapseButton);
}

// --- центральная область и статусная строка ------------------------------

void MainWindow::createCentralArea()
{
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_canvas);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    // Фон области просмотра задан таблицей стилей и полупрозрачен, поэтому
    // штатную непрозрачную заливку по роли палитры отключаем.
    m_scrollArea->viewport()->setAutoFillBackground(false);

    m_horizontalRuler = new Ruler(Qt::Horizontal, this);
    m_verticalRuler = new Ruler(Qt::Vertical, this);
    m_rulerCorner = new QWidget(this);
    m_rulerCorner->setFixedSize(kRulerThickness, kRulerThickness);
    // Уголок между линейками должен быть того же цвета, что и они сами,
    // иначе на стыке видна ступенька.
    m_rulerCorner->setAutoFillBackground(true);
    m_rulerCorner->setBackgroundRole(QPalette::Base);

    // Толщина и прозрачность — двумя отдельными блоками у левого края
    // рабочей области, как в современном Paint.
    struct SliderBox {
        QWidget *box = nullptr;
        QLabel *icon = nullptr;
        QSlider *slider = nullptr;
        QLabel *value = nullptr;
    };

    auto makeSliderBox = [](int minimum, int maximum, int value, const QString &tip) {
        SliderBox parts;
        parts.box = new QWidget;
        parts.box->setObjectName(QStringLiteral("SliderBox"));
        parts.box->setFixedWidth(46);
        parts.box->setAttribute(Qt::WA_StyledBackground, true);

        auto *layout = new QVBoxLayout(parts.box);
        layout->setContentsMargins(4, 10, 4, 10);
        layout->setSpacing(8);

        // Значку и подписи задан точный размер. Без него у метки с картинкой
        // минимальная высота нулевая: когда места в колонке не хватает,
        // раскладка ужимает её до нуля, и картинка обрезается сверху.
        parts.icon = new QLabel(parts.box);
        parts.icon->setAlignment(Qt::AlignCenter);
        parts.icon->setFixedHeight(24);

        parts.slider = new QSlider(Qt::Vertical, parts.box);
        parts.slider->setRange(minimum, maximum);
        parts.slider->setValue(value);
        parts.slider->setMinimumHeight(60);
        parts.slider->setToolTip(tip);
        // Ручка шире дорожки за счёт отрицательных полей, а в расчётную
        // ширину виджета они не входят — задаём её сами, иначе ручку
        // срежет по бокам.
        parts.slider->setFixedWidth(22);

        parts.value = new QLabel(parts.box);
        parts.value->setAlignment(Qt::AlignCenter);
        parts.value->setFixedHeight(16);

        layout->addWidget(parts.icon);
        layout->addWidget(parts.slider, 1, Qt::AlignHCenter);
        layout->addWidget(parts.value);

        // Блок тянется за высотой окна, но не ниже этой границы — иначе
        // раскладка при нехватке места начнёт резать содержимое.
        parts.box->setMinimumHeight(150);
        return parts;
    };

    const SliderBox sizeBox = makeSliderBox(1, 50, m_canvas->settings().size,
                                            tr("Толщина линии"));
    const SliderBox opacityBox = makeSliderBox(1, 100, m_canvas->settings().opacity,
                                               tr("Прозрачность"));
    const SliderBox toleranceBox = makeSliderBox(0, 100, m_canvas->settings().tolerance,
                                                 tr("Допуск заливки: насколько близкие\n"
                                                    "цвета считаются одной областью"));

    m_sizeBox = sizeBox.box;
    m_opacityBox = opacityBox.box;
    m_toleranceBox = toleranceBox.box;
    m_sizePanelIcon = sizeBox.icon;
    m_opacityPanelIcon = opacityBox.icon;
    m_tolerancePanelIcon = toleranceBox.icon;

    sizeBox.value->setText(QString::number(m_canvas->settings().size));
    opacityBox.value->setText(QStringLiteral("%1%").arg(m_canvas->settings().opacity));
    toleranceBox.value->setText(QStringLiteral("%1%").arg(m_canvas->settings().tolerance));

    QLabel *sizeValue = sizeBox.value;
    connect(sizeBox.slider, &QSlider::valueChanged, this, [this, sizeValue](int value) {
        m_canvas->setStrokeSize(value);
        sizeValue->setText(QString::number(value));
    });

    QLabel *opacityValue = opacityBox.value;
    connect(opacityBox.slider, &QSlider::valueChanged, this, [this, opacityValue](int value) {
        m_canvas->setOpacity(value);
        opacityValue->setText(QStringLiteral("%1%").arg(value));
    });

    QLabel *toleranceValue = toleranceBox.value;
    connect(toleranceBox.slider, &QSlider::valueChanged, this, [this, toleranceValue](int value) {
        m_canvas->setTolerance(value);
        toleranceValue->setText(QStringLiteral("%1%").arg(value));
    });

    // Колонка-контейнер сама фона не имеет. Отступы сверху и снизу
    // отодвигают блоки от ленты и от строки состояния.
    m_sizePanel = new QWidget(this);
    m_sizePanel->setFixedWidth(46);
    auto *panelColumn = new QVBoxLayout(m_sizePanel);
    // Отступ сверху опускает блоки под ленту, снизу — не даёт упереться
    // в строку состояния. Оба уменьшены: вместе с полями рабочей области
    // они съедали высоту, которой блокам не хватало, и содержимое резалось.
    panelColumn->setContentsMargins(0, 24, 0, 24);
    panelColumn->setSpacing(8);
    panelColumn->addWidget(sizeBox.box, 1);
    panelColumn->addWidget(opacityBox.box, 1);
    panelColumn->addWidget(toleranceBox.box, 1);

    m_layersPanel = new LayersPanel(m_document, this);
    m_layersPanel->setVisible(false);

    auto *canvasArea = new QWidget(this);
    auto *grid = new QGridLayout(canvasArea);
    grid->setContentsMargins(6, 6, 6, 0);
    grid->setSpacing(4);
    grid->addWidget(m_sizePanel, 0, 0, 2, 1);
    grid->addWidget(m_rulerCorner, 0, 1);
    grid->addWidget(m_horizontalRuler, 0, 2);
    grid->addWidget(m_verticalRuler, 1, 1);
    grid->addWidget(m_scrollArea, 1, 2);
    grid->addWidget(m_layersPanel, 0, 3, 2, 1);

    // Линейки выключены по умолчанию — их состояние задаёт действие в меню.
    const bool showRulers = m_rulersAction->isChecked();
    m_horizontalRuler->setVisible(showRulers);
    m_verticalRuler->setVisible(showRulers);
    m_rulerCorner->setVisible(showRulers);

    // Общих полей у раскладки нет: лента должна доходить до краёв окна,
    // поэтому отступы заданы отдельно верхней строке и рабочей области.
    // В самом низу окна — подложка с градиентом, всё остальное поверх неё.
    m_backdrop = new Backdrop(this);
    QWidget *central = m_backdrop;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_topBar);
    layout->addWidget(m_ribbon);
    layout->addWidget(canvasArea, 1);
    setCentralWidget(central);

    connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::syncRulers);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::syncRulers);

    syncRulers();
}

void MainWindow::createStatusBar()
{
    // Каждый показатель — значок плюс короткий текст, без поясняющих слов.
    auto makeIcon = [this]() {
        auto *label = new QLabel(this);
        label->setFixedWidth(18);
        label->setAlignment(Qt::AlignCenter);
        return label;
    };

    // Раз фона у панели больше нет, блоки показателей разделяем волосяной
    // чертой — как в Paint. Ширина 13 — это сама черта в 1 пиксель плюс по
    // 6 пикселей воздуха с боков: отступы в стилях отсчитываются внутрь
    // виджета, поэтому место под них нужно заложить в его размер.
    auto makeDivider = [this]() {
        auto *line = new QFrame(this);
        line->setObjectName(QStringLiteral("StatusDivider"));
        line->setFrameShape(QFrame::NoFrame);
        line->setAttribute(Qt::WA_StyledBackground, true);
        line->setFixedWidth(13);
        return line;
    };

    m_cursorIcon = makeIcon();
    m_selectionIcon = makeIcon();
    m_canvasSizeIcon = makeIcon();

    m_cursorLabel = new QLabel(this);
    m_selectionLabel = new QLabel(this);
    m_sizeLabel = new QLabel(this);
    m_zoomLabel = new QLabel(this);

    m_cursorLabel->setMinimumWidth(90);
    m_selectionLabel->setMinimumWidth(90);
    m_sizeLabel->setMinimumWidth(90);
    m_zoomLabel->setMinimumWidth(52);
    m_zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(0, 1000);
    m_zoomSlider->setFixedWidth(140);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &MainWindow::setZoomFromSlider);

    statusBar()->addWidget(m_cursorIcon);
    statusBar()->addWidget(m_cursorLabel);
    statusBar()->addWidget(makeDivider());
    statusBar()->addWidget(m_selectionIcon);
    statusBar()->addWidget(m_selectionLabel);
    statusBar()->addWidget(makeDivider());
    statusBar()->addWidget(m_canvasSizeIcon);
    statusBar()->addWidget(m_sizeLabel);

    statusBar()->addPermanentWidget(makeDivider());
    statusBar()->addPermanentWidget(smallActionButton(m_zoomFitAction));
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->addPermanentWidget(smallActionButton(m_zoomOutAction));
    statusBar()->addPermanentWidget(m_zoomSlider);
    statusBar()->addPermanentWidget(smallActionButton(m_zoomInAction));

    clearCursorLabel();
    updateSelectionLabel(QSize());
}

void MainWindow::refreshBackdrop()
{
    if (!m_backdrop)
        return;

    m_backdrop->setTint(Theme::backdropTint());
}

void MainWindow::applyIcons()
{
    Icons::setForeground(Theme::iconForeground());

    m_newAction->setIcon(Icons::action(Icons::Action::New));
    m_openAction->setIcon(Icons::action(Icons::Action::Open));
    m_saveAction->setIcon(Icons::action(Icons::Action::Save));
    m_saveAsAction->setIcon(Icons::action(Icons::Action::SaveAs));
    m_printAction->setIcon(Icons::action(Icons::Action::Print));
    m_propertiesAction->setIcon(Icons::action(Icons::Action::Properties));
    m_exitAction->setIcon(Icons::action(Icons::Action::Exit));

    m_undoAction->setIcon(Icons::action(Icons::Action::Undo));
    m_redoAction->setIcon(Icons::action(Icons::Action::Redo));
    m_cutAction->setIcon(Icons::action(Icons::Action::Cut));
    m_copyAction->setIcon(Icons::action(Icons::Action::Copy));
    m_pasteAction->setIcon(Icons::action(Icons::Action::Paste));
    m_pasteFromAction->setIcon(Icons::action(Icons::Action::Paste));
    m_selectAllAction->setIcon(Icons::action(Icons::Action::SelectAll));
    m_invertSelectionAction->setIcon(Icons::action(Icons::Action::InvertSelection));
    m_deleteSelectionAction->setIcon(Icons::action(Icons::Action::DeleteSelection));

    m_cropAction->setIcon(Icons::action(Icons::Action::Crop));
    m_resizeAction->setIcon(Icons::action(Icons::Action::ResizeImage));
    m_rotateRightAction->setIcon(Icons::action(Icons::Action::RotateRight));
    m_rotateLeftAction->setIcon(Icons::action(Icons::Action::RotateLeft));
    m_rotate180Action->setIcon(Icons::action(Icons::Action::RotateRight));
    m_flipHorizontalAction->setIcon(Icons::action(Icons::Action::FlipHorizontal));
    m_flipVerticalAction->setIcon(Icons::action(Icons::Action::FlipVertical));
    m_invertColoursAction->setIcon(Icons::action(Icons::Action::InvertColours));
    m_clearImageAction->setIcon(Icons::action(Icons::Action::ClearImage));

    m_zoomInAction->setIcon(Icons::action(Icons::Action::ZoomIn));
    m_zoomOutAction->setIcon(Icons::action(Icons::Action::ZoomOut));
    m_zoomResetAction->setIcon(Icons::action(Icons::Action::ZoomReset));
    m_zoomFitAction->setIcon(Icons::action(Icons::Action::FitToWindow));
    m_gridAction->setIcon(Icons::action(Icons::Action::Grid));
    m_rulersAction->setIcon(Icons::action(Icons::Action::Rulers));
    m_fullScreenAction->setIcon(Icons::action(Icons::Action::Fullscreen));
    m_aboutAction->setIcon(Icons::action(Icons::Action::About));
    m_layersAction->setIcon(Icons::action(Icons::Action::Layers));
    m_addLayerAction->setIcon(Icons::action(Icons::Action::AddLayer));

    for (auto it = m_toolActions.constBegin(); it != m_toolActions.constEnd(); ++it)
        it.value()->setIcon(Icons::tool(ToolId(it.key())));

    // Содержимое галерей и сетки фигур тоже нарисовано цветом темы.
    if (m_brushGallery) {
        for (int i = int(BrushType::Brush); i <= int(BrushType::WatercolourBrush); ++i)
            m_brushGallery->updateEntryIcon(i, Icons::brush(BrushType(i)));
    }
    // Порядок кнопок совпадает с порядком значений ShapeType.
    for (int i = 0; i < m_shapeButtons.size(); ++i)
        m_shapeButtons[i]->setIcon(Icons::shape(ShapeType(i)));

    if (m_brushButton)
        m_brushButton->setIcon(Icons::brush(m_canvas->settings().brush));

    // Кнопки, раскрывающие меню, носят шеврон прямо в значке.
    if (m_rotateButton)
        m_rotateButton->setIcon(Icons::withChevron(Icons::action(Icons::Action::RotateRight)));
    if (m_flipButton)
        m_flipButton->setIcon(Icons::withChevron(Icons::action(Icons::Action::FlipHorizontal)));
    if (m_outlineButton) {
        m_outlineButton->setIcon(Icons::withChevron(
            m_canvas->settings().hasOutline ? Icons::strokeStyle(m_canvas->settings().outline)
                                            : Icons::fillStyle(FillMode::None)));
    }
    if (m_fillButton)
        m_fillButton->setIcon(Icons::withChevron(Icons::fillStyle(m_canvas->settings().fill)));
    if (m_editColoursButton)
        m_editColoursButton->setIcon(Icons::action(Icons::Action::EditColours));
    if (m_settingsButton)
        m_settingsButton->setIcon(Icons::action(Icons::Action::Settings));
    if (m_ribbonCollapseButton) {
        m_ribbonCollapseButton->setIcon(Icons::action(m_ribbon->isCollapsed()
                                                          ? Icons::Action::Chevron
                                                          : Icons::Action::ChevronUp));
    }

    // Значки, вставленные в метки как готовые картинки, сами не обновятся —
    // их нужно перерисовать при смене темы вручную.
    if (m_sizePanelIcon)
        m_sizePanelIcon->setPixmap(Icons::action(Icons::Action::Size).pixmap(20, 20));
    if (m_opacityPanelIcon)
        m_opacityPanelIcon->setPixmap(Icons::action(Icons::Action::Opacity).pixmap(20, 20));
    if (m_tolerancePanelIcon)
        m_tolerancePanelIcon->setPixmap(Icons::tool(ToolId::Fill).pixmap(20, 20));
    if (m_cursorIcon)
        m_cursorIcon->setPixmap(Icons::action(Icons::Action::CursorPosition).pixmap(14, 14));
    if (m_selectionIcon)
        m_selectionIcon->setPixmap(Icons::action(Icons::Action::SelectAll).pixmap(14, 14));
    if (m_canvasSizeIcon)
        m_canvasSizeIcon->setPixmap(Icons::action(Icons::Action::CanvasSize).pixmap(14, 14));
}

// --- файловые операции ---------------------------------------------------

QString MainWindow::imageFilters(bool forSaving) const
{
    if (forSaving) {
        return tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;"
                  "GIF (*.gif);;TIFF (*.tif *.tiff);;WebP (*.webp);;Все файлы (*)");
    }

    QStringList patterns;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QByteArray &format : formats)
        patterns << QStringLiteral("*.%1").arg(QString::fromLatin1(format));

    return tr("Все изображения (%1);;Все файлы (*)").arg(patterns.join(QLatin1Char(' ')));
}

void MainWindow::newImage()
{
    if (!maybeSave())
        return;
    m_canvas->cancelPendingTool();
    m_document->newImage(m_defaultCanvas, m_canvas->settings().color2);
    m_canvas->resetZoom();
}

void MainWindow::open()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Открыть"), QString(),
                                                      imageFilters(false));
    if (path.isEmpty())
        return;
    openFile(path);
}

bool MainWindow::openFile(const QString &path)
{
    m_canvas->cancelPendingTool();
    if (!m_document->load(path)) {
        QMessageBox::warning(this, tr("Открытие файла"),
                             tr("Не удалось открыть «%1».").arg(QFileInfo(path).fileName()));
        return false;
    }
    addRecentFile(path);
    m_canvas->resetZoom();
    updateCanvasSizeLabel();
    return true;
}

bool MainWindow::save()
{
    m_canvas->commitPendingTool();
    m_canvas->finishSelection();

    if (m_document->filePath().isEmpty())
        return saveAs();

    if (!m_document->save(m_document->filePath())) {
        QMessageBox::warning(this, tr("Сохранение"), tr("Не удалось сохранить файл."));
        return false;
    }
    statusBar()->showMessage(tr("Сохранено"), 2000);
    return true;
}

bool MainWindow::saveAs()
{
    m_canvas->commitPendingTool();
    m_canvas->finishSelection();

    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("Сохранить как"),
                                                m_document->filePath(),
                                                imageFilters(true), &selectedFilter);
    if (path.isEmpty())
        return false;

    // Если расширение не указано, берём его из выбранного фильтра.
    if (QFileInfo(path).suffix().isEmpty()) {
        if (selectedFilter.contains(QLatin1String("*.png")))       path += QStringLiteral(".png");
        else if (selectedFilter.contains(QLatin1String("*.jpg")))  path += QStringLiteral(".jpg");
        else if (selectedFilter.contains(QLatin1String("*.bmp")))  path += QStringLiteral(".bmp");
        else if (selectedFilter.contains(QLatin1String("*.gif")))  path += QStringLiteral(".gif");
        else if (selectedFilter.contains(QLatin1String("*.tif")))  path += QStringLiteral(".tif");
        else if (selectedFilter.contains(QLatin1String("*.webp"))) path += QStringLiteral(".webp");
        else path += QStringLiteral(".png");
    }

    if (!m_document->save(path)) {
        QMessageBox::warning(this, tr("Сохранение"),
                             tr("Не удалось сохранить «%1».").arg(QFileInfo(path).fileName()));
        return false;
    }

    addRecentFile(path);
    statusBar()->showMessage(tr("Сохранено"), 2000);
    return true;
}

void MainWindow::print()
{
    m_canvas->commitPendingTool();
    m_canvas->finishSelection();

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Печать"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    QPainter painter(&printer);
    const QRect target = painter.viewport();
    QSize scaled = m_document->size();
    scaled.scale(target.size(), Qt::KeepAspectRatio);

    // Печатаем склейку слоёв — то же, что видно на экране.
    painter.setViewport(target.x(), target.y(), scaled.width(), scaled.height());
    painter.setWindow(QRect(QPoint(0, 0), m_document->size()));
    painter.drawImage(0, 0, m_document->composite());
}

void MainWindow::showProperties()
{
    const QString info = m_document->filePath().isEmpty()
                             ? tr("Файл: не сохранён")
                             : tr("Файл: %1").arg(m_document->filePath());

    AttributesDialog dialog(m_document->size(), info, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_canvas->finishSelection();
    m_document->resizeCanvas(dialog.resultSize(), m_canvas->settings().color2);
}

void MainWindow::pasteFrom()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Вставить из файла"),
                                                      QString(), imageFilters(false));
    if (path.isEmpty())
        return;
    m_canvas->pasteFromFile(path);
}

void MainWindow::resizeAndSkew()
{
    ResizeSkewDialog dialog(m_document->size(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_canvas->finishSelection();

    const QSize target = dialog.resultSize();
    if (target != m_document->size())
        m_document->scaleImage(target);

    if (!qFuzzyIsNull(dialog.skewHorizontal()) || !qFuzzyIsNull(dialog.skewVertical()))
        m_document->skew(dialog.skewHorizontal(), dialog.skewVertical());
}

void MainWindow::openRecent()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    if (!maybeSave())
        return;
    openFile(action->data().toString());
}

// --- вид -----------------------------------------------------------------

void MainWindow::updateZoomControls(double zoom)
{
    m_zoomLabel->setText(QStringLiteral("%1 %").arg(qRound(zoom * 100)));

    QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(qBound(0, zoomToSlider(zoom), 1000));

    syncRulers();
}

void MainWindow::setZoomFromSlider(int value)
{
    m_canvas->setZoom(sliderToZoom(value));
}

void MainWindow::toggleFullScreen(bool on)
{
    if (on)
        showFullScreen();
    else
        showNormal();
}

void MainWindow::openSettings()
{
    AppSettings current;
    current.theme = m_themeMode;
    current.defaultCanvas = m_defaultCanvas;
    current.antialias = m_antialias;
    current.rulers = m_rulersAction->isChecked();
    current.grid = m_gridAction->isChecked();
    current.statusBar = m_statusBarAction->isChecked();

    SettingsDialog dialog(current, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const AppSettings chosen = dialog.result();

    m_defaultCanvas = chosen.defaultCanvas;
    m_antialias = chosen.antialias;
    m_canvas->setAntialias(m_antialias);

    // Действия сами перерисуют линейки, сетку и строку состояния.
    m_rulersAction->setChecked(chosen.rulers);
    m_gridAction->setChecked(chosen.grid);
    m_statusBarAction->setChecked(chosen.statusBar);

    if (chosen.theme != m_themeMode) {
        m_themeMode = chosen.theme;
        Theme::apply(m_themeMode);
        applyIcons();
        refreshBackdrop();
        // Отметка в быстром меню тоже должна догнать выбор.
        const QList<QAction *> themeActions = m_themeGroup->actions();
        for (QAction *action : themeActions)
            action->setChecked(Theme::Mode(action->data().toInt()) == m_themeMode);
        update();
    }

    saveSettings();
}

void MainWindow::applyTheme()
{
    auto *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    m_themeMode = Theme::Mode(action->data().toInt());
    Theme::apply(m_themeMode);
    applyIcons();
    refreshBackdrop();
    update();
}

void MainWindow::scrollToAnchor(const QPointF &imagePoint)
{
    // Геометрия холста меняется после setZoom, поэтому прокручиваем
    // на следующем проходе цикла событий.
    QTimer::singleShot(0, this, [this, imagePoint]() {
        const QPoint widgetPoint = m_canvas->imageToWidget(imagePoint);
        m_scrollArea->ensureVisible(widgetPoint.x(), widgetPoint.y(),
                                    m_scrollArea->viewport()->width() / 2,
                                    m_scrollArea->viewport()->height() / 2);
        syncRulers();
    });
}

void MainWindow::syncRulers()
{
    if (!m_horizontalRuler || !m_canvas)
        return;

    const double zoom = m_canvas->zoom();
    const QSize viewport = m_scrollArea->viewport()->size();

    // Считаем от значений полос прокрутки, а не от текущей позиции холста:
    // на момент сигнала valueChanged виджет может быть ещё не сдвинут.
    // Когда холст меньше окна, QScrollArea центрует его — это отдельное
    // слагаемое, полосы прокрутки в таком случае стоят на нуле.
    const int centreX = qMax(0, (viewport.width() - m_canvas->width()) / 2);
    const int centreY = qMax(0, (viewport.height() - m_canvas->height()) / 2);
    const int marginInsideCanvas = 8;   // отступ листа внутри виджета Canvas

    m_horizontalRuler->setZoom(zoom);
    m_verticalRuler->setZoom(zoom);
    m_horizontalRuler->setOrigin(marginInsideCanvas + centreX);
    m_verticalRuler->setOrigin(marginInsideCanvas + centreY);
    m_horizontalRuler->setOffset(m_scrollArea->horizontalScrollBar()->value());
    m_verticalRuler->setOffset(m_scrollArea->verticalScrollBar()->value());
}

// --- состояние -----------------------------------------------------------

void MainWindow::updateWindowTitle()
{
    const QString name = m_document->displayName();
    const QString marker = m_document->isModified() ? QStringLiteral("*") : QString();
    setWindowTitle(tr("%1%2 — LinuxPaint").arg(name, marker));
}

void MainWindow::updateActionStates()
{
    m_undoAction->setEnabled(m_document->canUndo());
    m_redoAction->setEnabled(m_document->canRedo());

    const bool hasSelection = m_canvas->hasSelection();
    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_cropAction->setEnabled(hasSelection);
    m_deleteSelectionAction->setEnabled(hasSelection);
    m_pasteAction->setEnabled(m_canvas->canPaste());

    if (!hasSelection)
        updateSelectionLabel(QSize());
}

void MainWindow::updateCursorLabel(const QPoint &position)
{
    m_cursorLabel->setText(QStringLiteral("%1, %2 px").arg(position.x()).arg(position.y()));
    m_cursorIcon->setVisible(true);
    m_horizontalRuler->setCursorPosition(position.x());
    m_verticalRuler->setCursorPosition(position.y());
}

void MainWindow::clearCursorLabel()
{
    m_cursorLabel->setText(QString());
    m_cursorIcon->setVisible(false);
    m_horizontalRuler->setCursorPosition(-1);
    m_verticalRuler->setCursorPosition(-1);
}

void MainWindow::updateSelectionLabel(const QSize &size)
{
    // Пока выделения нет, значок тоже прячем — иначе он висит без числа.
    const bool visible = !size.isEmpty() && m_canvas->hasSelection();
    m_selectionIcon->setVisible(visible);
    m_selectionLabel->setText(visible
                                  ? QStringLiteral("%1 × %2 px")
                                        .arg(size.width()).arg(size.height())
                                  : QString());
}

void MainWindow::updateCanvasSizeLabel()
{
    const QSize size = m_document->size();
    m_sizeLabel->setText(QStringLiteral("%1 × %2 px").arg(size.width()).arg(size.height()));
}

void MainWindow::onToolChanged(ToolId id)
{
    QAction *action = m_toolActions.value(int(id));
    if (action)
        action->setChecked(true);

    // Настройки шрифта нужны только инструменту «Текст».
    if (m_textGroup)
        m_textGroup->setVisible(id == ToolId::Text);

    // Подсветка в сетке фигур должна гаснуть, когда выбран другой инструмент,
    // иначе кажется, что фигуры всё ещё активны.
    const int current = int(m_canvas->settings().shape);
    for (int i = 0; i < m_shapeButtons.size(); ++i)
        m_shapeButtons[i]->setChecked(id == ToolId::Shape && i == current);

    // В панели слева показываем только то, что относится к текущему
    // инструменту: толщину с прозрачностью — рисующим, допуск — заливке.
    if (m_sizePanel) {
        const bool usesThickness = (id == ToolId::Pencil || id == ToolId::Brush
                                    || id == ToolId::Eraser || id == ToolId::Shape);
        const bool usesTolerance = (id == ToolId::Fill);

        m_sizeBox->setVisible(usesThickness);
        m_opacityBox->setVisible(usesThickness);
        m_toleranceBox->setVisible(usesTolerance);
        m_sizePanel->setVisible(usesThickness || usesTolerance);
    }
}

void MainWindow::updateTextFont()
{
    if (!m_fontCombo || !m_fontSizeCombo)
        return;

    QFont font = m_fontCombo->currentFont();
    bool ok = false;
    const int size = m_fontSizeCombo->currentText().toInt(&ok);
    font.setPointSize((ok && size > 0) ? size : 12);
    font.setBold(m_boldButton->isChecked());
    font.setItalic(m_italicButton->isChecked());
    font.setUnderline(m_underlineButton->isChecked());

    m_canvas->setFont(font);
}

void MainWindow::onColoursChanged()
{
    m_colorArea->setColour1(m_canvas->settings().color1);
    m_colorArea->setColour2(m_canvas->settings().color2);
}

void MainWindow::chooseCustomColour()
{
    const bool secondary = m_colorArea->isEditingSecondary();
    const QColor initial = secondary ? m_canvas->settings().color2
                                     : m_canvas->settings().color1;

    const QColor chosen = QColorDialog::getColor(initial, this, tr("Изменение палитры"),
                                                 QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid())
        return;

    m_colorArea->addCustomColour(chosen);
    if (secondary)
        m_colorArea->setColour2(chosen);
    else
        m_colorArea->setColour1(chosen);
}

// --- служебное -----------------------------------------------------------

bool MainWindow::maybeSave()
{
    if (!m_document->isModified())
        return true;

    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("Paint"),
        tr("Сохранить изменения в «%1»?").arg(m_document->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        saveSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();

    if (mime->hasUrls()) {
        const QList<QUrl> urls = mime->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            if (maybeSave())
                openFile(urls.first().toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }

    if (mime->hasImage()) {
        m_canvas->pasteFromClipboard();
        event->acceptProposedAction();
    }
}

void MainWindow::loadSettings()
{
    QSettings settings(QStringLiteral("linux-paint"), QStringLiteral("linux-paint"));
    m_themeMode = Theme::Mode(settings.value(QStringLiteral("theme"),
                                             int(Theme::Mode::System)).toInt());
    m_recentFiles = settings.value(QStringLiteral("recentFiles")).toStringList();
    m_defaultCanvas = settings.value(QStringLiteral("defaultCanvas"),
                                     QSize(1152, 648)).toSize();
    if (m_defaultCanvas.isEmpty())
        m_defaultCanvas = QSize(1152, 648);
    m_antialias = settings.value(QStringLiteral("antialias"), true).toBool();
    m_startRulers = settings.value(QStringLiteral("rulers"), false).toBool();
    m_startGrid = settings.value(QStringLiteral("grid"), false).toBool();
    m_startStatusBar = settings.value(QStringLiteral("statusBar"), true).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings(QStringLiteral("linux-paint"), QStringLiteral("linux-paint"));
    settings.setValue(QStringLiteral("theme"), int(m_themeMode));
    settings.setValue(QStringLiteral("recentFiles"), m_recentFiles);
    settings.setValue(QStringLiteral("defaultCanvas"), m_defaultCanvas);
    settings.setValue(QStringLiteral("antialias"), m_antialias);
    settings.setValue(QStringLiteral("rulers"), m_rulersAction->isChecked());
    settings.setValue(QStringLiteral("grid"), m_gridAction->isChecked());
    settings.setValue(QStringLiteral("statusBar"), m_statusBarAction->isChecked());
}

void MainWindow::addRecentFile(const QString &path)
{
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 8)
        m_recentFiles.removeLast();
    refreshRecentMenu();
}

void MainWindow::refreshRecentMenu()
{
    if (!m_recentMenu)
        return;

    m_recentMenu->clear();
    if (m_recentFiles.isEmpty()) {
        QAction *empty = m_recentMenu->addAction(tr("Пусто"));
        empty->setEnabled(false);
        return;
    }

    for (const QString &path : m_recentFiles) {
        QAction *action = m_recentMenu->addAction(QFileInfo(path).fileName());
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, &MainWindow::openRecent);
    }
}
