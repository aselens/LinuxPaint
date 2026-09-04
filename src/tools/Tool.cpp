#include "tools/Tool.h"
#include "Canvas.h"
#include "Document.h"

#include <QBrush>
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>

#include <utility>

StrokeStyle strokeStyleForBrush(BrushType brush)
{
    switch (brush) {
    case BrushType::Brush:            return StrokeStyle::Solid;
    case BrushType::Calligraphy1:     return StrokeStyle::Calligraphy1;
    case BrushType::Calligraphy2:     return StrokeStyle::Calligraphy2;
    case BrushType::Airbrush:         return StrokeStyle::Airbrush;
    case BrushType::OilBrush:         return StrokeStyle::Oil;
    case BrushType::Crayon:           return StrokeStyle::Crayon;
    case BrushType::Marker:           return StrokeStyle::Marker;
    case BrushType::NaturalPencil:    return StrokeStyle::NaturalPencil;
    case BrushType::WatercolourBrush: return StrokeStyle::Watercolour;
    }
    return StrokeStyle::Solid;
}

StrokeStyle strokeStyleForFill(FillMode fill)
{
    switch (fill) {
    case FillMode::None:          return StrokeStyle::Solid;
    case FillMode::Solid:         return StrokeStyle::Solid;
    case FillMode::Crayon:        return StrokeStyle::Crayon;
    case FillMode::Marker:        return StrokeStyle::Marker;
    case FillMode::Oil:           return StrokeStyle::Oil;
    case FillMode::NaturalPencil: return StrokeStyle::NaturalPencil;
    case FillMode::Watercolour:   return StrokeStyle::Watercolour;
    }
    return StrokeStyle::Solid;
}

namespace paintutil {
namespace {

inline double randomDouble()
{
    return QRandomGenerator::global()->generateDouble();
}

// --- отпечаток кисти -----------------------------------------------------

// Зерно — то, чем след мелка отличается от следа маркера. Считается по
// координатам холста, а не отпечатка: зерно принадлежит бумаге и стоит на
// месте, поэтому второй проход ложится в те же ямки, что и первый. Случайные
// точки, которые сыпались раньше, при каждом движении выпадали заново — от
// этого след и выглядел рябью из пикселей, а не фактурой.
enum class Grain {
    None,
    Paper,      // крупный зуб бумаги: мелок садится на выступы
    Graphite,   // мелкое зерно карандаша
    Wash        // разводы акварели
};

struct DabSpec {
    double radiusScale;   // радиус относительно половины толщины
    double aspect;        // сжатие поперёк: 1 — круг, меньше — перо
    double angle;         // собственный наклон отпечатка, градусы
    double hardness;      // где начинает спадать край: 1 — резкий, 0 — размытый
    double flow;          // кроющая способность одного отпечатка
    double spacing;       // шаг между отпечатками в долях радиуса
    bool followsPath;     // разворачивается по направлению движения
    bool bristled;        // разбит на щетинки
    Grain grain;
    bool buildUp;         // темнеет ли при повторном проходе внутри мазка
};

DabSpec specFor(StrokeStyle style)
{
    switch (style) {
    case StrokeStyle::Solid:
        // Обычная круглая кисть: почти сплошной край, плотный шаг.
        return {1.00, 1.00,   0.0, 0.86, 1.00, 0.16, false, false, Grain::None,     false};
    case StrokeStyle::Calligraphy1:
        return {1.05, 0.30, -45.0, 0.90, 1.00, 0.10, false, false, Grain::None,     false};
    case StrokeStyle::Calligraphy2:
        return {1.05, 0.30,  45.0, 0.90, 1.00, 0.10, false, false, Grain::None,     false};
    case StrokeStyle::Airbrush:
        // Размытое облако с малой кроющей способностью. Кроющая способность
        // подобрана так, чтобы один проход давал примерно две трети плотности:
        // за проход по одному месту проходит около двадцати отпечатков, и
        // 1 − (1 − 0,06)^20 как раз даёт эти две трети. Дальше плотность
        // набирается вторым и третьим проходом — как у настоящего аэрографа.
        return {1.70, 1.00,   0.0, 0.40, 0.06, 0.10, false, false, Grain::None,     true};
    case StrokeStyle::Oil:
        // Щетина идёт вдоль движения и оставляет продольные борозды.
        return {1.00, 0.85,   0.0, 0.75, 0.95, 0.09, true,  true,  Grain::None,     false};
    case StrokeStyle::Crayon:
        // Воск ложится на выступы бумаги и не достаёт до впадин: белые
        // прожилки в следе — это зерно, а не пропущенные отпечатки.
        return {1.00, 1.00,   0.0, 0.55, 0.12, 0.13, false, false, Grain::Paper,    true};
    case StrokeStyle::Marker:
        // Скошенный наконечник. Внутри одного мазка не темнеет: маркер даёт
        // ровный полупрозрачный слой, сколько ни води по одному месту.
        return {1.00, 0.55, -30.0, 0.95, 0.38, 0.09, false, false, Grain::None,     false};
    case StrokeStyle::NaturalPencil:
        // Графит еле заметен с одного нажима и набирает силу штриховкой.
        return {0.75, 1.00,   0.0, 0.45, 0.10, 0.11, false, false, Grain::Graphite, true};
    case StrokeStyle::Watercolour:
        // Широкое размытое пятно: слои складываются, как настоящие заливки.
        return {1.30, 1.00,   0.0, 0.30, 0.09, 0.18, false, false, Grain::Wash,     true};
    }
    return {1.00, 1.00, 0.0, 0.86, 1.00, 0.16, false, false, Grain::None, false};
}

// Спад кроющей способности от центра отпечатка к краю. Два отрезка прямой,
// как в libmypaint: до rr = hardness кроет почти полностью, дальше плавно
// сходит на нет. rr — квадрат расстояния от центра в долях радиуса.
double dabOpacity(double rr, double hardness)
{
    if (rr >= 1.0)
        return 0.0;
    hardness = qBound(0.02, hardness, 0.98);
    if (rr <= hardness)
        return 1.0 - rr * (1.0 / hardness - 1.0);
    return hardness * (1.0 - rr) / (1.0 - hardness);
}

// Быстрый шум по целым координатам. Нужен ровно один: воспроизводимый,
// без состояния и без таблиц.
inline double hashNoise(int x, int y, quint32 salt)
{
    quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u + salt * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return double(h & 0xFFFFu) / 65535.0;
}

double grainAt(Grain grain, int x, int y)
{
    switch (grain) {
    case Grain::None:
        return 1.0;
    case Grain::Paper: {
        // Три частоты сразу: мелкая рябь, средние комки, крупные пятна.
        // Ниже порога краска не ложится вовсе — это впадины бумаги.
        const double n = 0.45 * hashNoise(x, y, 1)
                       + 0.35 * hashNoise(x / 2, y / 2, 2)
                       + 0.20 * hashNoise(x / 5, y / 5, 3);
        return n < 0.40 ? 0.0 : (n - 0.40) / 0.60;
    }
    case Grain::Graphite: {
        const double n = 0.6 * hashNoise(x, y, 4) + 0.4 * hashNoise(x / 2, y / 2, 5);
        return 0.30 + 0.70 * n;
    }
    case Grain::Wash: {
        const double n = 0.5 * hashNoise(x / 3, y / 3, 6) + 0.5 * hashNoise(x / 7, y / 7, 7);
        return 0.55 + 0.45 * n;
    }
    }
    return 1.0;
}

// Готовит отпечаток: карта кроющей способности с мягким краем.
QImage buildDab(const DabSpec &spec, int width, double angleDegrees, bool antialias)
{
    const double radius = qMax(0.5, width / 2.0) * spec.radiusScale;
    const double alongRadius = radius;
    const double acrossRadius = qMax(0.4, radius * spec.aspect);

    const int extent = int(qCeil(qMax(alongRadius, acrossRadius))) + 1;
    const int size = extent * 2 + 1;

    QImage dab(size, size, QImage::Format_Grayscale8);
    dab.fill(0);

    const double theta = qDegreesToRadians(angleDegrees + spec.angle);
    const double cs = qCos(theta);
    const double sn = qSin(theta);
    const int bristles = qMax(3, int(radius / 1.6));

    for (int y = 0; y < size; ++y) {
        uchar *line = dab.scanLine(y);
        for (int x = 0; x < size; ++x) {
            const double dx = x - extent;
            const double dy = y - extent;
            // Переходим в систему отпечатка: вдоль его оси и поперёк неё.
            const double along = (dx * cs + dy * sn) / alongRadius;
            const double across = (-dx * sn + dy * cs) / acrossRadius;

            double a = dabOpacity(along * along + across * across, spec.hardness);
            if (a <= 0.0)
                continue;

            if (spec.bristled) {
                // Волосок к волоску: плотнее по осям щетинок, реже между ними.
                a *= 0.40 + 0.60 * (0.5 + 0.5 * qCos(across * M_PI * bristles));
            }
            // Без сглаживания край обязан быть ступенчатым: это режим Paint,
            // где важны отдельные пиксели, а не мягкость.
            if (!antialias)
                a = (a >= 0.5) ? 1.0 : 0.0;

            line[x] = uchar(qBound(0.0, a, 1.0) * 255.0 + 0.5);
        }
    }
    return dab;
}

// Кладёт отпечаток в карту покрытия мазка. Правило прибавки задаёт стиль:
// «не копится» — берём большее из старого и нового, «копится» — складываем
// как две полупрозрачные плёнки, и второй проход выходит темнее первого.
void stampDab(QImage &mask, const QImage &dab, const QPointF &centre,
              const QRect &clip, const DabSpec &spec)
{
    const int half = dab.width() / 2;
    const int originX = qRound(centre.x()) - half;
    const int originY = qRound(centre.y()) - half;

    const int firstY = qMax(clip.top(), originY);
    const int lastY = qMin(clip.bottom(), originY + dab.height() - 1);
    const int firstX = qMax(clip.left(), originX);
    const int lastX = qMin(clip.right(), originX + dab.width() - 1);

    for (int y = firstY; y <= lastY; ++y) {
        const uchar *src = dab.constScanLine(y - originY);
        uchar *dst = mask.scanLine(y);
        for (int x = firstX; x <= lastX; ++x) {
            double a = src[x - originX] / 255.0;
            if (a <= 0.0)
                continue;

            a *= spec.flow;
            if (spec.grain != Grain::None)
                a *= grainAt(spec.grain, x, y);
            if (a <= 0.0)
                continue;

            const double had = dst[x] / 255.0;
            const double now = spec.buildUp ? had + a * (1.0 - had) : qMax(had, a);
            if (now > had)
                dst[x] = uchar(qBound(0.0, now, 1.0) * 255.0 + 0.5);
        }
    }
}

// Переносит на холст только прибавку покрытия.
//
// Незакрашенной остаётся доля (1 − было). Чтобы после наложения осталась
// доля (1 − стало), поверх нужно положить краску с непрозрачностью
// (стало − было) / (1 − было). Из этой формулы и берётся главное свойство
// нового движка: сколько бы отпечатков ни легло на одно место, цвет придёт
// ровно к тому, что записано в карте покрытия, и ни на шаг темнее.
void compositeDelta(QImage &target, const QImage &mask, const QImage &before,
                    const QRect &area, const QColor &colour)
{
    QImage patch(area.size(), QImage::Format_ARGB32_Premultiplied);
    if (patch.isNull())
        return;
    patch.fill(Qt::transparent);

    const double tint = colour.alphaF();
    const int red = colour.red();
    const int green = colour.green();
    const int blue = colour.blue();

    for (int y = 0; y < area.height(); ++y) {
        const uchar *was = before.constScanLine(y);
        const uchar *now = mask.constScanLine(area.top() + y) + area.left();
        QRgb *out = reinterpret_cast<QRgb *>(patch.scanLine(y));

        for (int x = 0; x < area.width(); ++x) {
            if (now[x] <= was[x])
                continue;

            const double had = was[x] / 255.0;
            if (had >= 1.0)
                continue;

            const double added = ((now[x] / 255.0) - had) / (1.0 - had) * tint;
            const int alpha = int(qBound(0.0, added, 1.0) * 255.0 + 0.5);
            if (alpha <= 0)
                continue;

            // Изображение с домноженной альфой: цвет уже приглушён ею.
            out[x] = qRgba(red * alpha / 255, green * alpha / 255,
                           blue * alpha / 255, alpha);
        }
    }

    QPainter p(&target);
    p.drawImage(area.topLeft(), patch);
}

} // namespace

// --- Stroke --------------------------------------------------------------

void Stroke::begin(const QSize &canvas, const QColor &colour, int width,
                   StrokeStyle style, bool antialias)
{
    end();
    if (canvas.isEmpty())
        return;

    // Карта покрытия — один байт на пиксель холста. Это дешевле копии
    // самого холста и позволяет считать прибавку точно.
    m_mask = QImage(canvas, QImage::Format_Grayscale8);
    if (m_mask.isNull())
        return;
    m_mask.fill(0);

    m_colour = colour;
    m_width = qMax(1, width);
    m_style = style;
    m_antialias = antialias;
    m_carry = 0.0;
}

void Stroke::end()
{
    m_mask = QImage();
    m_dabs.clear();
    m_carry = 0.0;
}

const QImage &Stroke::dabFor(double angleDegrees)
{
    const DabSpec spec = specFor(m_style);
    // Направление огрубляем до шестнадцати секторов: перестраивать отпечаток
    // на каждый шаг пути незачем, а на глаз разница неразличима.
    const int sector = spec.followsPath
                           ? ((int(qRound(angleDegrees / 22.5)) % 16) + 16) % 16
                           : 0;

    auto it = m_dabs.find(sector);
    if (it == m_dabs.end())
        it = m_dabs.insert(sector, buildDab(spec, m_width, sector * 22.5, m_antialias));
    return it.value();
}

QRect Stroke::addSegment(QImage &target, const QPointF &from, const QPointF &to)
{
    if (m_mask.isNull() || target.isNull())
        return QRect();

    // Тонкая сплошная линия — это карандаш: ему нужны чёткие пиксели, а не
    // отпечатки с мягким краем. Непрозрачный цвет поверх самого себя не
    // темнеет, так что накопитель здесь и не нужен.
    if (m_style == StrokeStyle::Solid && (!m_antialias || m_width <= 1)) {
        QPainter p(&target);
        QPen pen(m_colour);
        pen.setWidth(m_width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        if (from == to)
            p.drawPoint(from);
        else
            p.drawLine(from, to);
        return strokeBounds(from, to, m_width).intersected(target.rect());
    }

    const DabSpec spec = specFor(m_style);
    const double radius = qMax(0.5, m_width / 2.0) * spec.radiusScale;
    const double step = qMax(0.5, radius * spec.spacing);

    const double dx = to.x() - from.x();
    const double dy = to.y() - from.y();
    const double length = qSqrt(dx * dx + dy * dy);

    QVector<QPointF> centres;
    if (length < 1e-6) {
        // Одиночное касание. Следующий отпечаток должен отстоять на шаг,
        // иначе он ляжет ровно сюда же.
        centres.append(from);
        m_carry = step;
    } else {
        // Остаток шага переносится на следующий отрезок: без этого отпечатки
        // сбивались бы в кучу на каждом стыке, и швы было бы видно.
        double travelled = m_carry;
        while (travelled <= length) {
            const double t = travelled / length;
            centres.append(QPointF(from.x() + dx * t, from.y() + dy * t));
            travelled += step;
        }
        m_carry = travelled - length;
    }

    if (centres.isEmpty())
        return QRect();

    const double angle = (length < 1e-6) ? 0.0 : qRadiansToDegrees(qAtan2(dy, dx));
    const QImage &dab = dabFor(angle);
    const int half = dab.width() / 2;

    QRect area;
    for (const QPointF &centre : std::as_const(centres)) {
        area = area.united(QRect(qRound(centre.x()) - half, qRound(centre.y()) - half,
                                 dab.width(), dab.height()));
    }
    area = area.intersected(target.rect()).intersected(m_mask.rect());
    if (area.isEmpty())
        return QRect();

    // Снимок покрытия до отрезка: по нему и считается прибавка.
    const QImage before = m_mask.copy(area);
    if (before.isNull())
        return QRect();

    for (const QPointF &centre : std::as_const(centres))
        stampDab(m_mask, dab, centre, area, spec);

    compositeDelta(target, m_mask, before, area, m_colour);
    return area;
}

void drawPolyline(QImage &target, const QVector<QPointF> &points,
                  const QColor &colour, int width, StrokeStyle style,
                  bool antialias)
{
    if (target.isNull() || points.isEmpty())
        return;

    Stroke stroke;
    stroke.begin(target.size(), colour, width, style, antialias);
    if (!stroke.isActive())
        return;

    if (points.size() == 1) {
        stroke.addSegment(target, points.first(), points.first());
    } else {
        for (int i = 1; i < points.size(); ++i)
            stroke.addSegment(target, points.at(i - 1), points.at(i));
    }
    stroke.end();
}

void strokePath(QImage &target, const QPainterPath &path, const QColor &color,
                int width, StrokeStyle style, bool antialias)
{
    if (target.isNull() || path.isEmpty())
        return;

    if (style == StrokeStyle::Solid) {
        QPainter p(&target);
        p.setRenderHint(QPainter::Antialiasing, antialias);
        QPen pen(color);
        pen.setWidth(qMax(1, width));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        return;
    }

    // Текстурные стили ведём по контуру одним мазком: перекрытия внутри
    // обводки не темнеют, а на стыках дуг не остаётся комков.
    const double length = path.length();
    if (length <= 0.0)
        return;

    // Шаг разбивки берём мельче шага между отпечатками — тогда ломаная
    // повторяет кривую, и её изломы не видны.
    const int steps = qBound(2, int(length / 0.75), 4000);

    QVector<QPointF> points;
    points.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i)
        points.append(path.pointAtPercent(double(i) / steps));

    drawPolyline(target, points, color, width, style, antialias);
}

QBrush styledBrush(const QColor &color, FillMode fill)
{
    if (fill == FillMode::Solid || fill == FillMode::None)
        return QBrush(color);

    // Текстурная заливка — тайл 48×48, который Qt размножит по фигуре.
    const int tileSize = 48;
    QImage tile(tileSize, tileSize, QImage::Format_ARGB32);
    tile.fill(Qt::transparent);

    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    int passes = 0;
    int alphaLow = 0;
    int alphaHigh = 0;
    double radius = 1.0;

    switch (fill) {
    case FillMode::Crayon:        passes = 1400; alphaLow =  90; alphaHigh = 210; radius = 0.9; break;
    case FillMode::Marker:        passes =  900; alphaLow =  50; alphaHigh =  90; radius = 2.2; break;
    case FillMode::Oil:           passes = 1100; alphaLow = 170; alphaHigh = 250; radius = 1.8; break;
    case FillMode::NaturalPencil: passes =  900; alphaLow =  60; alphaHigh = 150; radius = 0.7; break;
    case FillMode::Watercolour:   passes =  600; alphaLow =  25; alphaHigh =  60; radius = 3.0; break;
    default:                      passes =  800; alphaLow = 200; alphaHigh = 255; radius = 1.0; break;
    }

    for (int i = 0; i < passes; ++i) {
        QColor c = color;
        c.setAlpha(alphaLow + int(randomDouble() * (alphaHigh - alphaLow)));
        p.setBrush(c);
        const double x = randomDouble() * tileSize;
        const double y = randomDouble() * tileSize;
        const double r = radius * (0.6 + randomDouble() * 0.8);
        p.drawEllipse(QPointF(x, y), r, r);
        // Дубли по краям, чтобы стык тайлов не бросался в глаза.
        if (x < r) p.drawEllipse(QPointF(x + tileSize, y), r, r);
        if (y < r) p.drawEllipse(QPointF(x, y + tileSize), r, r);
    }
    p.end();

    return QBrush(tile);
}

QRect strokeBounds(const QPointF &a, const QPointF &b, int width)
{
    QRectF r(a, b);
    r = r.normalized();
    const double pad = qMax(2, width * 2);
    return r.adjusted(-pad, -pad, pad, pad).toAlignedRect();
}

} // namespace paintutil

// --- Tool ----------------------------------------------------------------

Tool::Tool(Canvas *canvas)
    : m_canvas(canvas)
{
}

Tool::~Tool() = default;

Document *Tool::doc() const
{
    return m_canvas->document();
}

ToolSettings &Tool::settings() const
{
    return m_canvas->settings();
}

QImage &Tool::image() const
{
    // Полупрозрачный штрих сначала копится в отдельном слое и лишь потом
    // ложится на холст целиком. Иначе перекрывающиеся отпечатки кисти
    // складывали бы прозрачность сами с собой, и штрих выходил бы
    // плотным в местах наложения вместо ровной полупрозрачной линии.
    if (m_canvas->hasStrokeLayer())
        return m_canvas->strokeLayer();
    return m_canvas->document()->image();
}

QColor Tool::colorForButton(Qt::MouseButton button) const
{
    return (button == Qt::RightButton) ? settings().color2 : settings().color1;
}

QColor Tool::alternateColor() const
{
    return (m_button == Qt::RightButton) ? settings().color1 : settings().color2;
}

void Tool::requestRepaint(const QRect &imageRect)
{
    m_canvas->updateImageRect(imageRect);
}

void Tool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos) Q_UNUSED(mods)
    m_button = button;
    m_active = true;
}

void Tool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos) Q_UNUSED(mods)
}

void Tool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(pos) Q_UNUSED(button) Q_UNUSED(mods)
    m_active = false;
    m_button = Qt::NoButton;
}

void Tool::doubleClick(const QPointF &pos, Qt::MouseButton button)
{
    Q_UNUSED(pos) Q_UNUSED(button)
}

void Tool::paintOverlay(QPainter &painter)
{
    Q_UNUSED(painter)
}

void Tool::tick()
{
}

void Tool::viewChanged()
{
}

QCursor Tool::cursor() const
{
    return QCursor(Qt::CrossCursor);
}

void Tool::commit()
{
}

void Tool::cancel()
{
    m_active = false;
    m_button = Qt::NoButton;
}
