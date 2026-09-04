#pragma once

#include <QIcon>
#include <QMenu>
#include <QString>
#include <QVector>
#include <QWidget>

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QToolButton;
class QVBoxLayout;

// Группа ленты: ряд кнопок и подпись под ними («Буфер обмена», «Фигуры»…).
class RibbonGroup : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonGroup(const QString &title, QWidget *parent = nullptr);

    void addItem(QWidget *widget);
    void addSpacing(int px);
    QHBoxLayout *contentLayout() const { return m_content; }

    // Разделитель слева принадлежит группе: контекстные группы (например,
    // «Текст») скрываются целиком, не оставляя висящей чёрточки.
    void setLeadingSeparator(QWidget *separator) { m_separator = separator; }
    void setVisible(bool visible) override;

    // Свёрнутая лента прячет группы, не забывая, какие из них были
    // скрыты по своей причине (контекстная «Текст»).
    void setCollapsed(bool collapsed);

private:
    void applyVisibility();

    QHBoxLayout *m_content = nullptr;
    QLabel *m_title = nullptr;
    QWidget *m_separator = nullptr;
    bool m_wanted = true;
    bool m_collapsed = false;
};

// Сама лента — горизонтальная полоса групп, разделённых тонкими линиями.
class Ribbon : public QWidget
{
    Q_OBJECT

public:
    explicit Ribbon(QWidget *parent = nullptr);

    RibbonGroup *addGroup(const QString &title);
    void addSeparator();
    void addStretch();
    // Кнопка сворачивания в правом краю ленты.
    void addCornerWidget(QWidget *widget);

    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

private:
    QHBoxLayout *m_layout = nullptr;
    bool m_collapsed = false;
};

// Всплывающая галерея (кисти, фигуры, толщина линии) — сетка значков.
// Строка галереи: слева название, справа образец мазка во всю ширину —
// как в Paint, где кисть выбирают по виду штриха, а не по мелкому значку.
class GalleryRow : public QWidget
{
    Q_OBJECT

public:
    GalleryRow(int id, const QString &text, const QIcon &preview,
               QWidget *parent = nullptr);

    void setPreview(const QIcon &preview);
    void setChecked(bool on);

signals:
    void clicked(int id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int m_id = -1;
    QString m_text;
    QIcon m_preview;
    bool m_checked = false;
    bool m_hovered = false;
};

class GalleryPopup : public QMenu
{
    Q_OBJECT

public:
    explicit GalleryPopup(QWidget *parent = nullptr);

    // Размер образца. Тот, кто их рисует, должен знать его точно: иначе
    // картинку придётся масштабировать, а текстура кисти этого не терпит.
    static QSize previewSize();

    void addEntry(int id, const QIcon &preview, const QString &text);
    void setCurrentEntry(int id);
    int currentEntry() const { return m_current; }
    // Образцы рисуются цветом темы, поэтому при её смене их нужно
    // перерисовать так же, как значки действий.
    void updateEntryIcon(int id, const QIcon &preview);

signals:
    void entrySelected(int id);

private:
    QVBoxLayout *m_rows = nullptr;
    int m_current = -1;
    QVector<GalleryRow *> m_items;
    QVector<int> m_ids;
};

namespace RibbonUi {

// Крупная кнопка: значок сверху, подпись снизу — как в ленте Paint.
QToolButton *bigButton(const QIcon &icon, const QString &text, const QString &tooltip);
// Компактная кнопка только со значком.
QToolButton *smallButton(const QIcon &icon, const QString &tooltip);
// Сетка компактных кнопок в заданное число строк.
QWidget *buttonGrid(const QVector<QToolButton *> &buttons, int rows);

// Квадратная кнопка без подписи — основной элемент ленты Paint.
QToolButton *iconButton(QAction *action, int buttonSize, int iconSize);

// Кнопка со значком, под которой стоит «шеврон», раскрывающий меню.
// Возвращает контейнер; сама кнопка — в *mainOut, если он задан.
QWidget *stackedMenuButton(QToolButton *main, QMenu *menu, const QIcon &chevron,
                           int width);

} // namespace RibbonUi
