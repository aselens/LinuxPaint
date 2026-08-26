#pragma once

#include <QObject>
#include <QImage>
#include <QSize>
#include <QRect>
#include <QColor>
#include <QString>
#include <QVector>

// Один слой изображения.
struct Layer {
    QImage image;
    QString name;
    bool visible = true;
};

// Растровый документ: стопка слоёв + история undo/redo + операции.
//
// Инструменты рисуют в активный слой — его и возвращает image(). Для показа
// и сохранения слои склеиваются в composite(). Такое разделение позволило
// добавить слои, не трогая ни один инструмент: они как писали в QImage,
// который вернул image(), так и пишут.
//
// История хранится снимками всей стопки. Это ровно то, что делает Paint:
// операции вроде поворота или обрезки меняют все слои сразу, поэтому
// попытка хранить дельты усложнила бы код без выигрыша.
class Document : public QObject
{
    Q_OBJECT

public:
    explicit Document(QObject *parent = nullptr);

    // --- содержимое -----------------------------------------------------
    // Активный слой. Неконстантная версия помечает склейку устаревшей:
    // вызывающий получает изменяемую ссылку и может писать в неё что угодно.
    QImage &image();
    const QImage &image() const;

    // Все видимые слои, склеенные в одно изображение.
    const QImage &composite() const;

    QSize size() const;
    int width() const { return size().width(); }
    int height() const { return size().height(); }

    void newImage(const QSize &size, const QColor &background = Qt::white);
    bool load(const QString &path);
    bool save(const QString &path, const QByteArray &format = QByteArray());

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path);
    QString displayName() const;

    bool isModified() const { return m_modified; }
    void setModified(bool modified);

    // --- слои -----------------------------------------------------------
    // Индекс 0 — самый нижний слой.
    int layerCount() const { return m_layers.size(); }
    int activeLayer() const { return m_active; }
    const Layer &layer(int index) const;

    void setActiveLayer(int index);
    void addLayer();                       // прозрачный, поверх активного
    void duplicateLayer(int index);
    void removeLayer(int index);
    void moveLayer(int index, int delta);  // delta = +1 вверх, -1 вниз
    void setLayerVisible(int index, bool visible);
    void mergeLayerDown(int index);        // слить с тем, что под ним

    // --- история --------------------------------------------------------
    // Инструмент вызывает beginEdit() перед первым изменением пикселей и
    // endEdit() когда штрих завершён.
    void beginEdit();
    void endEdit(const QRect &dirty = QRect());
    void abortEdit();          // откатить незавершённую правку (Esc)
    void touch(const QRect &dirty = QRect());   // сообщить о перерисовке

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void undo();
    void redo();
    void clearHistory();

    // --- операции над изображением --------------------------------------
    // Меняющие холст применяются ко всем слоям сразу, рисующие — к активному.
    void resizeCanvas(const QSize &size, const QColor &background = Qt::white);
    void resizeCanvas(const QSize &size, const QPoint &contentOffset,
                      const QColor &background);
    void scaleImage(const QSize &size, bool smooth = true);
    void crop(const QRect &rect);
    void rotate(int degrees);                      // 90 / 180 / 270
    void flip(Qt::Orientation orientation);
    void skew(double horizontalDeg, double verticalDeg);
    void invertColors(const QRect &area = QRect());
    void clearImage(const QColor &color);

    // Заливка по образцу (scanline flood fill). Возвращает залитую область.
    QRect floodFill(const QPoint &start, const QColor &fillColor, int tolerance = 0);

signals:
    void changed(const QRect &dirty);      // пустой прямоугольник == весь холст
    void sizeChanged(const QSize &size);
    void modifiedChanged(bool modified);
    void historyChanged();
    void filePathChanged(const QString &path);
    void layersChanged();                  // состав, порядок или видимость

private:
    // Снимок для истории: вся стопка плюс какой слой был активен.
    struct State {
        QVector<Layer> layers;
        int active = 0;
    };

    State currentState() const;
    void restoreState(const State &state);
    static bool sameState(const State &a, const State &b);

    void pushUndo();
    void invalidateComposite();
    QString nextLayerName() const;
    // Применить преобразование к каждому слою; возвращает новый размер.
    template <typename Fn>
    void transformLayers(Fn transform);

    static const int kMaxHistory = 32;

    QVector<Layer> m_layers;
    int m_active = 0;

    mutable QImage m_composite;
    mutable bool m_compositeDirty = true;

    State m_editSnapshot;
    bool m_editing = false;
    QVector<State> m_undo;
    QVector<State> m_redo;
    QString m_filePath;
    bool m_modified = false;
    int m_layerCounter = 0;
};
