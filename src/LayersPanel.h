#pragma once

#include <QImage>
#include <QWidget>

class Document;

class QTimer;
class QToolButton;
class QVBoxLayout;

// Один элемент списка слоёв: миниатюра поверх шахматки, рамка выбора
// и кнопка-глаз для показа/скрытия.
class LayerItem : public QWidget
{
    Q_OBJECT

public:
    LayerItem(int index, const QImage &content, bool visible, bool selected,
              QWidget *parent = nullptr);

    int index() const { return m_index; }

signals:
    void picked(int index);
    void visibilityToggled(int index);
    void menuRequested(int index, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    int m_index;
    QImage m_thumbnail;
    bool m_selected = false;
};

// Панель слоёв у правого края рабочей области: кнопка добавления сверху,
// под ней столбец миниатюр — верхний слой первым, как в Paint.
class LayersPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayersPanel(Document *document, QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    void showItemMenu(int index, const QPoint &globalPos);
    void updateHeight();          // высота панели — по числу слоёв

    Document *m_document = nullptr;
    QToolButton *m_addButton = nullptr;
    QVBoxLayout *m_itemsLayout = nullptr;
    // Миниатюры обновляются не на каждый мазок, а спустя паузу после него.
    QTimer *m_thumbnailTimer = nullptr;
};
