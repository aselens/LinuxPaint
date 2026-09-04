#include "tools/Tool.h"
#include "Canvas.h"
#include "Document.h"

#include <QBrush>
#include <QPainter>
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

inline double smoothstep(double from, double to, double value)
{
    const double t = qBound(0.0, (value - from) / (to - from), 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Значение шума в узле решётки. Воспроизводимое, без состояния и таблиц.
inline double latticeNoise(int x, int y, quint32 salt)
{
    quint32 h = quint32(x) * 374761393u + quint32(y) * 668265263u + salt * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return double(h & 0xFFFFu) / 65535.0;
}

// Гладкий шум: значения заданы в узлах решётки с шагом cell, между узлами —
// плавный переход. Ровно этого не хватало прежней фактуре: она бралась от
// целочисленного деления координат, то есть соседние пиксели получали одно
// и то же значение блоками, и «зерно» разваливалось на квадратики.
//
// Решётка замкнута по кругу с периодом tile, поэтому готовую фактуру можно
// разложить плиткой без швов.
double smoothNoise(int x, int y, int cell, int tile, quint32 salt)
{
    const int period = qMax(1, tile / cell);

    const double fx = double(x) / cell;
    const double fy = double(y) / cell;
    const int x0 = int(qFloor(fx));
    const int y0 = int(qFloor(fy));

    const double tx = smoothstep(0.0, 1.0, fx - x0);
    const double ty = smoothstep(0.0, 1.0, fy - y0);

    const int xa = ((x0 % period) + period) % period;
    const int ya = ((y0 % period) + period) % period;
    const int xb = (xa + 1) % period;
    const int yb = (ya + 1) % period;

    const double top = latticeNoise(xa, ya, salt)
                     + (latticeNoise(xb, ya, salt) - latticeNoise(xa, ya, salt)) * tx;
    const double bottom = latticeNoise(xa, yb, salt)
                        + (latticeNoise(xb, yb, salt) - latticeNoise(xa, yb, salt)) * tx;
    return top + (bottom - top) * ty;
}

// Фактура считается один раз на плитку и дальше только читается: три октавы
// гладкого шума на каждый пиксель мазка съели бы всё время отрисовки.
const int kGrainTile = 512;

QImage buildGrainTile(Grain grain)
{
    QImage tile(kGrainTile, kGrainTile, QImage::Format_Grayscale8);
    tile.fill(0);

    for (int y = 0; y < kGrainTile; ++y) {
        uchar *line = tile.scanLine(y);
        for (int x = 0; x < kGrainTile; ++x) {
            double value = 1.0;
            switch (grain) {
            case Grain::None:
                break;
            case Grain::Paper: {
                // Зуб бумаги: мелкая рябь, средние комки, крупные пятна.
                // Порог мягкий — у зерна не должно быть рубленого края.
                const double n = 0.46 * smoothNoise(x, y, 2, kGrainTile, 11)
                               + 0.34 * smoothNoise(x, y, 4, kGrainTile, 12)
                               + 0.20 * smoothNoise(x, y, 16, kGrainTile, 13);
                value = smoothstep(0.34, 0.66, n);
                break;
            }
            case Grain::Graphite: {
                const double n = 0.62 * smoothNoise(x, y, 2, kGrainTile, 21)
                               + 0.38 * smoothNoise(x, y, 8, kGrainTile, 22);
                value = 0.30 + 0.70 * smoothstep(0.15, 0.85, n);
                break;
            }
            case Grain::Wash: {
                // Разводы крупные и мягкие: это не зерно бумаги, а неровность
                // самой заливки.
                const double n = 0.55 * smoothNoise(x, y, 8, kGrainTile, 31)
                               + 0.45 * smoothNoise(x, y, 32, kGrainTile, 32);
                value = 0.55 + 0.45 * n;
                break;
            }
            }
            line[x] = uchar(qBound(0.0, value, 1.0) * 255.0 + 0.5);
        }
    }
    return tile;
}

// Плитка нужного зерна. Строится при первом обращении и живёт до конца
// работы; берётся один раз на отпечаток, а не на каждый его пиксель.
const QImage *grainTile(Grain grain)
{
    if (grain == Grain::None)
        return nullptr;

    static const QImage paper = buildGrainTile(Grain::Paper);
    static const QImage graphite = buildGrainTile(Grain::Graphite);
    static const QImage wash = buildGrainTile(Grain::Wash);

    switch (grain) {
    case Grain::Paper:    return &paper;
    case Grain::Graphite: return &graphite;
    case Grain::Wash:     return &wash;
    case Grain::None:     break;
    }
    return nullptr;
}

// Кладёт отпечаток в карту покрытия мазка.
//
// Отпечаток считается прямо здесь, а не берётся готовой картинкой: только
// так центр может стоять между пикселями. Раньше он округлялся до целого,
// и край наклонного мазка получал ступеньки в один пиксель — именно они
// и выглядели грубой «пиксельной» кромкой.
//
// Правило прибавки задаёт стиль: «не копится» — берём большее из старого
// и нового, «копится» — складываем как две полупрозрачные плёнки, и второй
// проход выходит темнее первого.
void stampDab(QImage &mask, const QPointF &centre, double angleDegrees,
              double radius, const DabSpec &spec, const QRect &clip, bool antialias)
{
    const double alongRadius = radius;
    const double acrossRadius = qMax(0.4, radius * spec.aspect);
    const double reach = qMax(alongRadius, acrossRadius) + 1.0;

    const int firstX = qMax(clip.left(), int(qFloor(centre.x() - reach)));
    const int lastX = qMin(clip.right(), int(qCeil(centre.x() + reach)));
    const int firstY = qMax(clip.top(), int(qFloor(centre.y() - reach)));
    const int lastY = qMin(clip.bottom(), int(qCeil(centre.y() + reach)));
    if (firstX > lastX || firstY > lastY)
        return;

    const double theta = qDegreesToRadians(angleDegrees + spec.angle);
    const double cs = qCos(theta);
    const double sn = qSin(theta);
    const int bristles = qMax(3, int(radius / 1.6));
    const QImage *grain = grainTile(spec.grain);

    for (int y = firstY; y <= lastY; ++y) {
        uchar *dst = mask.scanLine(y);
        const uchar *grainLine = grain ? grain->constScanLine(y & (kGrainTile - 1))
                                       : nullptr;
        // Считаем от середины пикселя: его цвет отвечает за то, что в центре.
        const double dy = y + 0.5 - centre.y();

        for (int x = firstX; x <= lastX; ++x) {
            const double dx = x + 0.5 - centre.x();
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

            a *= spec.flow;
            if (grainLine)
                a *= grainLine[x & (kGrainTile - 1)] / 255.0;
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
    m_carry = 0.0;
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

    // Направление нужно только тем кистям, что разворачиваются по пути.
    const double angle = (spec.followsPath && length >= 1e-6)
                             ? qRadiansToDegrees(qAtan2(dy, dx))
                             : 0.0;

    // Захват отпечатка: наибольший из двух радиусов плюс пиксель на мягкий край.
    const double reach = qMax(radius, qMax(0.4, radius * spec.aspect)) + 1.0;
    const int pad = int(qCeil(reach)) + 1;

    QRect area;
    for (const QPointF &centre : std::as_const(centres)) {
        area = area.united(QRect(int(qFloor(centre.x())) - pad,
                                 int(qFloor(centre.y())) - pad,
                                 pad * 2 + 2, pad * 2 + 2));
    }
    area = area.intersected(target.rect()).intersected(m_mask.rect());
    if (area.isEmpty())
        return QRect();

    // Снимок покрытия до отрезка: по нему и считается прибавка.
    const QImage before = m_mask.copy(area);
    if (before.isNull())
        return QRect();

    for (const QPointF &centre : std::as_const(centres))
        stampDab(m_mask, centre, angle, radius, spec, area, m_antialias);

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

    // Заливка берёт ту же фактуру, что и мазок этим же материалом: иначе
    // контур и заливка одной фигуры выглядели бы сделанными разным инструментом.
    // Прежде здесь сыпались сотни случайных пятнышек — они и давали ту самую
    // рябь из пикселей, потому что каждое пятнышко было размером в пиксель.
    Grain grain = Grain::None;
    double density = 1.0;

    switch (fill) {
    case FillMode::Crayon:        grain = Grain::Paper;    density = 0.85; break;
    case FillMode::NaturalPencil: grain = Grain::Graphite; density = 0.50; break;
    case FillMode::Watercolour:   grain = Grain::Wash;     density = 0.35; break;
    case FillMode::Marker:        grain = Grain::None;     density = 0.38; break;
    case FillMode::Oil:           grain = Grain::Wash;     density = 0.92; break;
    default:                      grain = Grain::None;     density = 1.00; break;
    }

    const QImage *texture = grainTile(grain);
    const int tileSize = texture ? kGrainTile : 8;

    QImage tile(tileSize, tileSize, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);

    const double tint = color.alphaF() * density;
    const int red = color.red();
    const int green = color.green();
    const int blue = color.blue();

    for (int y = 0; y < tileSize; ++y) {
        const uchar *grainLine = texture ? texture->constScanLine(y) : nullptr;
        QRgb *out = reinterpret_cast<QRgb *>(tile.scanLine(y));
        for (int x = 0; x < tileSize; ++x) {
            double a = tint;
            if (grainLine)
                a *= grainLine[x] / 255.0;
            const int alpha = int(qBound(0.0, a, 1.0) * 255.0 + 0.5);
            out[x] = qRgba(red * alpha / 255, green * alpha / 255,
                           blue * alpha / 255, alpha);
        }
    }

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
