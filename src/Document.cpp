#include "Document.h"

#include <QPainter>
#include <QTransform>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QStack>
#include <QtMath>

namespace {

// Расстояние между цветами — наибольшее расхождение по каналам.
inline int colourDistance(QRgb a, QRgb b)
{
    return qMax(qMax(qAbs(qRed(a)   - qRed(b)),   qAbs(qGreen(a) - qGreen(b))),
                qMax(qAbs(qBlue(a)  - qBlue(b)),  qAbs(qAlpha(a) - qAlpha(b))));
}

inline bool colorsMatch(QRgb a, QRgb b, int tolerance)
{
    if (a == b)
        return true;
    if (tolerance <= 0)
        return false;
    return colourDistance(a, b) <= tolerance;
}

// Пиксель сглаженного края — это смесь цвета линии и цвета заливаемого
// фона в какой-то пропорции. Зная оба крайних цвета, долю фона можно
// вычислить и подменить в смеси именно её, не тронув долю линии.
//
// Просто подкрашивать такие пиксели поверх бесполезно: пиксель, наполовину
// состоящий из фона, и после подкраски остаётся светлее заливки — отсюда и
// брался тонкий ободок по краю фигуры.
inline QRgb replaceBackground(QRgb pixel, QRgb background, QRgb lineColour, QRgb fill)
{
    const int span = colourDistance(lineColour, background);
    if (span <= 0)
        return fill;

    // t — доля цвета линии в пикселе, share — доля фона.
    const double t = qBound(0.0, double(colourDistance(pixel, background)) / span, 1.0);
    const double share = 1.0 - t;

    auto shift = [share](int channel, int from, int to) {
        return int(qBound(0.0, channel + share * (to - from) + 0.5, 255.0));
    };

    return qRgba(shift(qRed(pixel),   qRed(background),   qRed(fill)),
                 shift(qGreen(pixel), qGreen(background), qGreen(fill)),
                 shift(qBlue(pixel),  qBlue(background),  qBlue(fill)),
                 shift(qAlpha(pixel), qAlpha(background), qAlpha(fill)));
}

// Форматы без альфа-канала: перед сохранением подкладываем белый фон,
// иначе прозрачные пиксели станут чёрными.
bool formatSupportsAlpha(const QByteArray &format)
{
    const QByteArray f = format.toLower();
    return f == "png" || f == "tif" || f == "tiff" || f == "webp" || f == "ico";
}

} // namespace

Document::Document(QObject *parent)
    : QObject(parent)
{
    newImage(QSize(1152, 648));
}

// --- слои и склейка ------------------------------------------------------

QImage &Document::image()
{
    // Ссылка изменяемая, значит писать в неё могут в любой момент —
    // считаем склейку устаревшей сразу, а не гадаем, изменилось ли что-то.
    m_compositeDirty = true;
    return m_layers[m_active].image;
}

const QImage &Document::image() const
{
    return m_layers[m_active].image;
}

const QImage &Document::composite() const
{
    // Один видимый слой смешивать не с чем — отдаём его напрямую.
    // Это обычный случай, и он не стоит ни одного копирования.
    if (m_layers.size() == 1 && m_layers.first().visible)
        return m_layers.first().image;

    if (m_compositeDirty) {
        m_composite = QImage(size(), QImage::Format_ARGB32);
        m_composite.fill(Qt::transparent);

        QPainter p(&m_composite);
        for (const Layer &layer : m_layers) {
            if (layer.visible)
                p.drawImage(0, 0, layer.image);
        }
        p.end();

        m_compositeDirty = false;
    }
    return m_composite;
}

QSize Document::size() const
{
    return m_layers.isEmpty() ? QSize() : m_layers.first().image.size();
}

const Layer &Document::layer(int index) const
{
    return m_layers[qBound(0, index, m_layers.size() - 1)];
}

void Document::invalidateComposite()
{
    m_compositeDirty = true;
}

QString Document::nextLayerName() const
{
    return tr("Слой %1").arg(m_layerCounter + 1);
}

void Document::setActiveLayer(int index)
{
    index = qBound(0, index, m_layers.size() - 1);
    if (m_active == index)
        return;
    m_active = index;
    emit layersChanged();
}

void Document::addLayer()
{
    pushUndo();

    Layer layer;
    layer.image = QImage(size(), QImage::Format_ARGB32);
    layer.image.fill(Qt::transparent);
    layer.name = nextLayerName();
    ++m_layerCounter;

    m_layers.insert(m_active + 1, layer);
    m_active = m_active + 1;

    invalidateComposite();
    setModified(true);
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

void Document::duplicateLayer(int index)
{
    if (index < 0 || index >= m_layers.size())
        return;

    pushUndo();

    Layer copy = m_layers[index];
    copy.name = nextLayerName();
    ++m_layerCounter;

    m_layers.insert(index + 1, copy);
    m_active = index + 1;

    invalidateComposite();
    setModified(true);
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

void Document::removeLayer(int index)
{
    // Последний слой удалить нельзя: документу нужен хотя бы один.
    if (m_layers.size() <= 1 || index < 0 || index >= m_layers.size())
        return;

    pushUndo();
    m_layers.remove(index);
    m_active = qBound(0, m_active >= index ? m_active - 1 : m_active,
                      m_layers.size() - 1);

    invalidateComposite();
    setModified(true);
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

void Document::moveLayer(int index, int delta)
{
    const int target = index + delta;
    if (index < 0 || index >= m_layers.size()
        || target < 0 || target >= m_layers.size())
        return;

    pushUndo();
    m_layers.swapItemsAt(index, target);
    if (m_active == index)
        m_active = target;
    else if (m_active == target)
        m_active = index;

    invalidateComposite();
    setModified(true);
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

void Document::setLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= m_layers.size())
        return;
    if (m_layers[index].visible == visible)
        return;

    m_layers[index].visible = visible;
    invalidateComposite();
    setModified(true);
    emit layersChanged();
    emit changed(QRect());
}

void Document::mergeLayerDown(int index)
{
    if (index <= 0 || index >= m_layers.size())
        return;

    pushUndo();

    {
        QPainter p(&m_layers[index - 1].image);
        p.drawImage(0, 0, m_layers[index].image);
    }
    m_layers.remove(index);
    m_active = index - 1;

    invalidateComposite();
    setModified(true);
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

// --- файлы ---------------------------------------------------------------

void Document::newImage(const QSize &size, const QColor &background)
{
    Layer base;
    base.image = QImage(size, QImage::Format_ARGB32);
    base.image.fill(background);
    base.name = tr("Фон");

    m_layers.clear();
    m_layers.append(base);
    m_active = 0;
    m_layerCounter = 0;

    m_filePath.clear();
    m_editing = false;
    m_editSnapshot = State();
    invalidateComposite();
    clearHistory();
    setModified(false);

    emit layersChanged();
    emit sizeChanged(size);
    emit filePathChanged(m_filePath);
    emit changed(QRect());
}

bool Document::load(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull())
        return false;

    if (img.format() != QImage::Format_ARGB32)
        img = img.convertToFormat(QImage::Format_ARGB32);

    Layer base;
    base.image = img;
    base.name = tr("Фон");

    m_layers.clear();
    m_layers.append(base);
    m_active = 0;
    m_layerCounter = 0;

    m_editing = false;
    m_editSnapshot = State();
    invalidateComposite();
    clearHistory();
    setFilePath(path);
    setModified(false);

    emit layersChanged();
    emit sizeChanged(img.size());
    emit changed(QRect());
    return true;
}

bool Document::save(const QString &path, const QByteArray &format)
{
    QByteArray fmt = format;
    if (fmt.isEmpty())
        fmt = QFileInfo(path).suffix().toLower().toUtf8();
    if (fmt.isEmpty())
        fmt = "png";

    // В файл идёт склейка: обычные растровые форматы слоёв не хранят.
    QImage out = composite();
    if (!formatSupportsAlpha(fmt)) {
        QImage flat(out.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, out);
        p.end();
        out = flat;
    }

    QImageWriter writer(path, fmt);
    if (fmt == "jpg" || fmt == "jpeg")
        writer.setQuality(92);
    if (!writer.write(out))
        return false;

    setFilePath(path);
    setModified(false);
    return true;
}

void Document::setFilePath(const QString &path)
{
    if (m_filePath == path)
        return;
    m_filePath = path;
    emit filePathChanged(m_filePath);
}

QString Document::displayName() const
{
    if (m_filePath.isEmpty())
        return tr("Безымянный");
    return QFileInfo(m_filePath).fileName();
}

void Document::setModified(bool modified)
{
    if (m_modified == modified)
        return;
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

// --- история -------------------------------------------------------------

Document::State Document::currentState() const
{
    State state;
    state.layers = m_layers;
    state.active = m_active;
    return state;
}

void Document::restoreState(const State &state)
{
    m_layers = state.layers;
    m_active = qBound(0, state.active, m_layers.size() - 1);
    invalidateComposite();
}

bool Document::sameState(const State &a, const State &b)
{
    if (a.active != b.active || a.layers.size() != b.layers.size())
        return false;
    for (int i = 0; i < a.layers.size(); ++i) {
        if (a.layers[i].visible != b.layers[i].visible)
            return false;
        if (a.layers[i].image != b.layers[i].image)
            return false;
    }
    return true;
}

void Document::pushUndo()
{
    m_undo.append(currentState());
    while (m_undo.size() > kMaxHistory)
        m_undo.removeFirst();
    m_redo.clear();
}

void Document::beginEdit()
{
    if (m_editing)
        return;
    m_editing = true;
    m_editSnapshot = currentState();
}

void Document::endEdit(const QRect &dirty)
{
    if (m_editing) {
        m_editing = false;
        // Снимок кладём в историю только если что-то реально изменилось.
        if (!sameState(m_editSnapshot, currentState())) {
            m_undo.append(m_editSnapshot);
            while (m_undo.size() > kMaxHistory)
                m_undo.removeFirst();
            m_redo.clear();
            setModified(true);
            emit historyChanged();
        }
        m_editSnapshot = State();
    }
    invalidateComposite();
    emit changed(dirty);
}

void Document::abortEdit()
{
    if (!m_editing)
        return;
    m_editing = false;
    restoreState(m_editSnapshot);
    m_editSnapshot = State();
    emit changed(QRect());
}

void Document::touch(const QRect &dirty)
{
    invalidateComposite();
    emit changed(dirty);
}

void Document::undo()
{
    if (m_undo.isEmpty())
        return;

    const QSize before = size();

    m_redo.append(currentState());
    restoreState(m_undo.takeLast());
    setModified(true);

    if (size() != before)
        emit sizeChanged(size());
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

void Document::redo()
{
    if (m_redo.isEmpty())
        return;

    const QSize before = size();

    m_undo.append(currentState());
    restoreState(m_redo.takeLast());
    setModified(true);

    if (size() != before)
        emit sizeChanged(size());
    emit layersChanged();
    emit historyChanged();
    emit changed(QRect());
}

void Document::clearHistory()
{
    m_undo.clear();
    m_redo.clear();
    emit historyChanged();
}

// --- операции ------------------------------------------------------------

// Прогоняет каждый слой через переданное преобразование. Все операции
// уровня холста обязаны менять слои согласованно, иначе стопка развалится
// на изображения разного размера.
template <typename Fn>
void Document::transformLayers(Fn transform)
{
    for (Layer &layer : m_layers) {
        layer.image = transform(layer.image);
        if (layer.image.format() != QImage::Format_ARGB32)
            layer.image = layer.image.convertToFormat(QImage::Format_ARGB32);
    }
    invalidateComposite();
}

void Document::resizeCanvas(const QSize &size, const QColor &background)
{
    resizeCanvas(size, QPoint(0, 0), background);
}

void Document::resizeCanvas(const QSize &newSize, const QPoint &contentOffset,
                            const QColor &background)
{
    if (newSize.isEmpty())
        return;
    if (newSize == size() && contentOffset.isNull())
        return;

    pushUndo();

    // Фон подкладываем только под нижний слой: верхние обязаны остаться
    // прозрачными, иначе они закрасят всё, что под ними.
    bool bottom = true;
    for (Layer &layer : m_layers) {
        QImage img(newSize, QImage::Format_ARGB32);
        img.fill(bottom ? background : QColor(Qt::transparent));
        QPainter p(&img);
        p.drawImage(contentOffset, layer.image);
        p.end();
        layer.image = img;
        bottom = false;
    }

    invalidateComposite();
    setModified(true);
    emit sizeChanged(newSize);
    emit historyChanged();
    emit changed(QRect());
}

void Document::scaleImage(const QSize &newSize, bool smooth)
{
    if (newSize == size() || newSize.isEmpty())
        return;

    pushUndo();
    transformLayers([&](const QImage &img) {
        return img.scaled(newSize, Qt::IgnoreAspectRatio,
                          smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
    });

    setModified(true);
    emit sizeChanged(newSize);
    emit historyChanged();
    emit changed(QRect());
}

void Document::crop(const QRect &rect)
{
    const QRect r = rect.intersected(QRect(QPoint(0, 0), size()));
    if (r.isEmpty() || r == QRect(QPoint(0, 0), size()))
        return;

    pushUndo();
    transformLayers([&](const QImage &img) { return img.copy(r); });

    setModified(true);
    emit sizeChanged(r.size());
    emit historyChanged();
    emit changed(QRect());
}

void Document::rotate(int degrees)
{
    degrees = ((degrees % 360) + 360) % 360;
    if (degrees == 0)
        return;

    pushUndo();
    QTransform t;
    t.rotate(degrees);
    transformLayers([&](const QImage &img) {
        return img.transformed(t, Qt::FastTransformation);
    });

    setModified(true);
    emit sizeChanged(size());
    emit historyChanged();
    emit changed(QRect());
}

void Document::flip(Qt::Orientation orientation)
{
    pushUndo();
    const bool horizontal = (orientation == Qt::Horizontal);
    transformLayers([&](const QImage &img) {
        return img.mirrored(horizontal, !horizontal);
    });

    setModified(true);
    emit historyChanged();
    emit changed(QRect());
}

void Document::skew(double horizontalDeg, double verticalDeg)
{
    if (qFuzzyIsNull(horizontalDeg) && qFuzzyIsNull(verticalDeg))
        return;

    pushUndo();

    const double hs = qTan(qDegreesToRadians(qBound(-89.0, horizontalDeg, 89.0)));
    const double vs = qTan(qDegreesToRadians(qBound(-89.0, verticalDeg, 89.0)));

    QTransform t;
    t.shear(hs, vs);

    const QRect src(QPoint(0, 0), size());
    const QRect mapped = t.mapRect(src);

    bool bottom = true;
    for (Layer &layer : m_layers) {
        QImage out(mapped.size(), QImage::Format_ARGB32);
        out.fill(bottom ? QColor(Qt::white) : QColor(Qt::transparent));

        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.translate(-mapped.topLeft());
        p.setTransform(t, true);
        p.drawImage(0, 0, layer.image);
        p.end();

        layer.image = out;
        bottom = false;
    }

    invalidateComposite();
    setModified(true);
    emit sizeChanged(mapped.size());
    emit historyChanged();
    emit changed(QRect());
}

void Document::invertColors(const QRect &area)
{
    // Обращение цветов — операция над содержимым, а не над холстом,
    // поэтому применяется к активному слою.
    QImage &img = m_layers[m_active].image;
    QRect r = area.isNull() ? img.rect() : area.intersected(img.rect());
    if (r.isEmpty())
        return;

    pushUndo();

    for (int y = r.top(); y <= r.bottom(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = r.left(); x <= r.right(); ++x) {
            const QRgb c = line[x];
            line[x] = qRgba(255 - qRed(c), 255 - qGreen(c), 255 - qBlue(c), qAlpha(c));
        }
    }

    invalidateComposite();
    setModified(true);
    emit historyChanged();
    emit changed(r);
}

void Document::clearImage(const QColor &color)
{
    pushUndo();
    // Нижний слой заливаем цветом, верхние очищаем в прозрачность —
    // иначе «очистить» превратило бы прозрачный слой в непрозрачный.
    m_layers[m_active].image.fill(m_active == 0 ? color : QColor(Qt::transparent));

    invalidateComposite();
    setModified(true);
    emit historyChanged();
    emit changed(QRect());
}

// Заливка в два этапа.
//
// Первый — классический обход «по строкам»: вместо рекурсии по пикселям
// закрашиваем целые горизонтальные отрезки и кладём в стек только
// точки-затравки для соседних строк.
//
// Второй — смягчение каймы. Края фигур нарисованы со сглаживанием, то есть
// состоят из пикселей, промежуточных между цветом области и цветом линии.
// В допуск они не попадают, и после одного лишь первого этапа вокруг
// заливки остаётся заметный ободок исходного цвета. Поэтому пиксели,
// граничащие с залитыми, докрашиваются частично — тем слабее, чем дальше
// их цвет от исходного. Через такие пиксели заливка дальше не идёт, так
// что за пределы фигуры она не убегает.
QRect Document::floodFill(const QPoint &start, const QColor &fillColor, int tolerance)
{
    QImage &img = m_layers[m_active].image;
    if (!img.rect().contains(start))
        return QRect();

    const QRgb target = img.pixel(start);
    const QRgb replacement = fillColor.rgba();
    if (colorsMatch(target, replacement, 0))
        return QRect();

    const int w = img.width();
    const int h = img.height();

    QVector<quint8> filled(w * h, 0);

    QStack<QPoint> stack;
    stack.push(start);

    int minX = w, minY = h, maxX = -1, maxY = -1;

    while (!stack.isEmpty()) {
        const QPoint seed = stack.pop();
        const int y = seed.y();
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));

        if (filled[y * w + seed.x()] || !colorsMatch(line[seed.x()], target, tolerance))
            continue;

        int left = seed.x();
        while (left > 0 && !filled[y * w + left - 1]
               && colorsMatch(line[left - 1], target, tolerance))
            --left;

        int right = seed.x();
        while (right < w - 1 && !filled[y * w + right + 1]
               && colorsMatch(line[right + 1], target, tolerance))
            ++right;

        for (int x = left; x <= right; ++x) {
            line[x] = replacement;
            filled[y * w + x] = 1;
        }

        minX = qMin(minX, left);
        maxX = qMax(maxX, right);
        minY = qMin(minY, y);
        maxY = qMax(maxY, y);

        // Ищем затравки в строках сверху и снизу: по одной на каждый
        // непрерывный отрезок подходящего цвета.
        for (int dy = -1; dy <= 1; dy += 2) {
            const int ny = y + dy;
            if (ny < 0 || ny >= h)
                continue;
            const QRgb *neighbour = reinterpret_cast<const QRgb *>(img.constScanLine(ny));
            bool inRun = false;
            for (int x = left; x <= right; ++x) {
                const bool match = !filled[ny * w + x]
                                   && colorsMatch(neighbour[x], target, tolerance);
                if (match && !inRun) {
                    stack.push(QPoint(x, ny));
                    inRun = true;
                } else if (!match) {
                    inRun = false;
                }
            }
        }
    }

    if (maxX < 0)
        return QRect();

    // --- кайма сглаживания ---
    // Три прохода покрывают переход толщиной до трёх пикселей; за один
    // проход кайма расширяется ровно на пиксель.
    for (int pass = 0; pass < 3; ++pass) {
        QVector<QPoint> feathered;

        const int y0 = qMax(0, minY - 1);
        const int y1 = qMin(h - 1, maxY + 1);
        const int x0 = qMax(0, minX - 1);
        const int x1 = qMin(w - 1, maxX + 1);

        for (int y = y0; y <= y1; ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = x0; x <= x1; ++x) {
                if (filled[y * w + x])
                    continue;

                const bool touching =
                    (x > 0        && filled[y * w + x - 1])
                    || (x < w - 1 && filled[y * w + x + 1])
                    || (y > 0     && filled[(y - 1) * w + x])
                    || (y < h - 1 && filled[(y + 1) * w + x]);
                if (!touching)
                    continue;

                // Цвет линии оцениваем по самому непохожему на фон соседу.
                // Залитые соседи исключены: они уже носят цвет заливки и
                // были бы приняты за линию.
                QRgb lineColour = line[x];
                int span = colourDistance(line[x], target);
                for (int dy = -1; dy <= 1; ++dy) {
                    const int ny = y + dy;
                    if (ny < 0 || ny >= h)
                        continue;
                    const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(ny));
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = x + dx;
                        if (nx < 0 || nx >= w || filled[ny * w + nx])
                            continue;
                        const int d = colourDistance(row[nx], target);
                        if (d > span) {
                            span = d;
                            lineColour = row[nx];
                        }
                    }
                }

                if (span <= tolerance)
                    continue;

                const QRgb updated = replaceBackground(line[x], target, lineColour,
                                                       replacement);
                if (updated == line[x])
                    continue;

                line[x] = updated;
                feathered.append(QPoint(x, y));
            }
        }

        if (feathered.isEmpty())
            break;

        // Помечаем кайму залитой только после прохода: иначе она стала бы
        // опорой сама для себя и заливка расползлась бы вширь.
        for (const QPoint &p : feathered) {
            filled[p.y() * w + p.x()] = 1;
            minX = qMin(minX, p.x());
            maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
    }

    invalidateComposite();
    setModified(true);
    return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}
