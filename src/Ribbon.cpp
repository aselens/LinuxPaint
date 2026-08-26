#include "Ribbon.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

// --- RibbonGroup ---------------------------------------------------------

RibbonGroup::RibbonGroup(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 4, 6, 2);
    outer->setSpacing(2);

    m_content = new QHBoxLayout;
    m_content->setContentsMargins(0, 0, 0, 0);
    m_content->setSpacing(2);

    m_title = new QLabel(title, this);
    m_title->setObjectName(QStringLiteral("RibbonGroupTitle"));
    m_title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    outer->addLayout(m_content, 1);
    outer->addWidget(m_title);
}

void RibbonGroup::addItem(QWidget *widget)
{
    m_content->addWidget(widget, 0, Qt::AlignVCenter);
}

void RibbonGroup::addSpacing(int px)
{
    m_content->addSpacing(px);
}

void RibbonGroup::setVisible(bool visible)
{
    m_wanted = visible;
    applyVisibility();
}

void RibbonGroup::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    applyVisibility();
}

void RibbonGroup::applyVisibility()
{
    const bool shown = m_wanted && !m_collapsed;
    QWidget::setVisible(shown);
    if (m_separator)
        m_separator->setVisible(shown);
}

// --- Ribbon --------------------------------------------------------------

Ribbon::Ribbon(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("Ribbon"));
    // Подкласс QWidget сам по себе фон и рамку из таблицы стилей не рисует —
    // ему нужен либо собственный paintEvent, либо этот атрибут. Без него
    // правило QWidget#Ribbon молча не действует, и лента показывает фон окна.
    setAttribute(Qt::WA_StyledBackground, true);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 3, 6, 3);
    m_layout->setSpacing(2);
}

RibbonGroup *Ribbon::addGroup(const QString &title)
{
    // Черта перед группой принадлежит самой группе: контекстная «Текст»
    // скрывается целиком, не оставляя висящего разделителя.
    QWidget *separator = nullptr;
    if (m_layout->count() > 0) {
        addSeparator();
        separator = m_layout->itemAt(m_layout->count() - 1)->widget();
    }

    auto *group = new RibbonGroup(title, this);
    group->setLeadingSeparator(separator);
    m_layout->addWidget(group);
    return group;
}

void Ribbon::addSeparator()
{
    auto *line = new QFrame(this);
    line->setObjectName(QStringLiteral("RibbonSeparator"));
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedWidth(1);
    m_layout->addWidget(line);
}

void Ribbon::addStretch()
{
    m_layout->addStretch(1);
}

void Ribbon::addCornerWidget(QWidget *widget)
{
    m_layout->addWidget(widget, 0, Qt::AlignBottom);
}

void Ribbon::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;

    const QList<RibbonGroup *> groups = findChildren<RibbonGroup *>();
    for (RibbonGroup *group : groups)
        group->setCollapsed(collapsed);
}

// --- GalleryPopup --------------------------------------------------------

GalleryPopup::GalleryPopup(int columns, QWidget *parent)
    : QMenu(parent)
    , m_columns(qMax(1, columns))
{
    auto *host = new QWidget(this);
    m_grid = new QGridLayout(host);
    m_grid->setContentsMargins(6, 6, 6, 6);
    m_grid->setSpacing(2);

    auto *action = new QWidgetAction(this);
    action->setDefaultWidget(host);
    addAction(action);
}

void GalleryPopup::addEntry(int id, const QIcon &icon, const QString &tooltip)
{
    auto *button = new QToolButton;
    button->setIcon(icon);
    button->setIconSize(QSize(30, 30));
    button->setToolTip(tooltip);
    button->setAutoRaise(true);
    button->setCheckable(true);
    button->setFixedSize(38, 38);

    const int index = m_count++;
    m_grid->addWidget(button, index / m_columns, index % m_columns);
    m_buttons.append(button);
    m_ids.append(id);

    connect(button, &QToolButton::clicked, this, [this, id]() {
        setCurrentEntry(id);
        emit entrySelected(id);
        close();
    });
}

void GalleryPopup::setCurrentEntry(int id)
{
    m_current = id;
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->setChecked(m_ids[i] == id);
}

void GalleryPopup::updateEntryIcon(int id, const QIcon &icon)
{
    const int index = m_ids.indexOf(id);
    if (index >= 0)
        m_buttons[index]->setIcon(icon);
}

QIcon GalleryPopup::entryIcon(int id) const
{
    const int index = m_ids.indexOf(id);
    if (index < 0)
        return QIcon();
    return m_buttons[index]->icon();
}

// --- фабрики кнопок ------------------------------------------------------

namespace RibbonUi {

QToolButton *bigButton(const QIcon &icon, const QString &text, const QString &tooltip)
{
    auto *button = new QToolButton;
    button->setIcon(icon);
    button->setText(text);
    button->setToolTip(tooltip.isEmpty() ? text : tooltip);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setIconSize(QSize(28, 28));
    button->setAutoRaise(true);
    button->setMinimumWidth(56);
    return button;
}

QToolButton *smallButton(const QIcon &icon, const QString &tooltip)
{
    auto *button = new QToolButton;
    button->setIcon(icon);
    button->setToolTip(tooltip);
    button->setIconSize(QSize(22, 22));
    button->setAutoRaise(true);
    button->setFixedSize(30, 30);
    return button;
}

QToolButton *iconButton(QAction *action, int buttonSize, int iconSize)
{
    auto *button = new QToolButton;
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(iconSize, iconSize));
    button->setAutoRaise(true);
    button->setFixedSize(buttonSize, buttonSize);
    return button;
}

QWidget *stackedMenuButton(QToolButton *main, QMenu *menu, const QIcon &chevron, int width)
{
    auto *host = new QWidget;
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(main, 0, Qt::AlignHCenter);

    if (menu) {
        auto *arrow = new QToolButton;
        // Собственную стрелку QToolButton прячем в таблице стилей:
        // указателем служит сам значок-шеврон.
        arrow->setObjectName(QStringLiteral("RibbonChevron"));
        arrow->setIcon(chevron);
        arrow->setIconSize(QSize(16, 16));
        arrow->setAutoRaise(true);
        arrow->setFixedSize(width, 18);
        arrow->setPopupMode(QToolButton::InstantPopup);
        arrow->setMenu(menu);
        layout->addWidget(arrow, 0, Qt::AlignHCenter);
    }

    return host;
}

QWidget *buttonGrid(const QVector<QToolButton *> &buttons, int rows)
{
    rows = qMax(1, rows);
    auto *host = new QWidget;
    auto *grid = new QGridLayout(host);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    // Заполняем построчно: порядок в списке совпадает с тем, что видит
    // пользователь слева направо, сверху вниз.
    const int columns = qMax(1, (buttons.size() + rows - 1) / rows);
    for (int i = 0; i < buttons.size(); ++i)
        grid->addWidget(buttons[i], i / columns, i % columns);

    return host;
}

} // namespace RibbonUi
