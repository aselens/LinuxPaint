#include "tools/MyPaintEngine.h"

#ifdef HAVE_LIBMYPAINT

#include <mypaint-brush.h>
#include <mypaint-fixed-tiled-surface.h>
#include <mypaint-tiled-surface.h>
#include <mypaint-config.h>

#include <QPainter>
#include <QtMath>

#include <cstdint>

namespace {

// Тайл поверхности: 64×64 пикселя, четыре канала по 16 бит, цвет домножен
// на альфу, полная непрозрачность — 1 << 15.
const int kTile = MYPAINT_TILE_SIZE;
const int kOne = 1 << 15;

// Настройки кистей Paint, выраженные в параметрах libmypaint.
//
// Кисти собираются из умолчаний движка, а не из готовых файлов .myb: так у
// программы не появляется ещё одной внешней зависимости, а сами значения
// видно прямо здесь и можно править под свой вкус.
struct BrushRecipe {
    double hardness;        // резкость края, 0..1
    double opaque;          // кроющая способность отпечатка
    double dabsPerRadius;   // сколько отпечатков на радиус пути
    double radiusScale;     // радиус относительно заданной толщины
    double radiusJitter;    // разброс радиуса
    double offsetJitter;    // разброс положения
    double ellipseRatio;    // вытянутость отпечатка (1 — круг)
    double ellipseAngle;    // её наклон, градусы
    double smudge;          // подхват цвета с холста
    double smudgeLength;    // насколько долго подхваченный цвет держится
    double slowTracking;    // сглаживание хода руки
    double speedOpacity;    // как скорость влияет на плотность
};

BrushRecipe recipeFor(StrokeStyle style)
{
    switch (style) {
    case StrokeStyle::Solid:
        return {0.90, 1.00, 3.0, 1.00, 0.00, 0.00, 1.00,   0.0, 0.00, 0.5, 2.0, 0.00};
    case StrokeStyle::Calligraphy1:
        return {0.95, 1.00, 4.0, 1.05, 0.00, 0.00, 4.00, -45.0, 0.00, 0.5, 1.5, 0.00};
    case StrokeStyle::Calligraphy2:
        return {0.95, 1.00, 4.0, 1.05, 0.00, 0.00, 4.00,  45.0, 0.00, 0.5, 1.5, 0.00};
    case StrokeStyle::Airbrush:
        // Мягкое облако, плотность которого набирается проходами.
        return {0.05, 0.06, 6.0, 1.80, 0.00, 0.10, 1.00,   0.0, 0.00, 0.5, 1.0, 0.00};
    case StrokeStyle::Oil:
        // Масло тянет за собой цвет с холста — за это отвечает smudge.
        return {0.75, 0.95, 5.0, 1.00, 0.06, 0.08, 1.20,   0.0, 0.55, 0.6, 2.5, 0.00};
    case StrokeStyle::Crayon:
        return {0.45, 0.30, 4.0, 1.00, 0.25, 0.55, 1.00,   0.0, 0.00, 0.5, 1.0, 0.35};
    case StrokeStyle::Marker:
        return {0.95, 0.40, 5.0, 1.00, 0.00, 0.00, 1.80, -30.0, 0.00, 0.5, 2.0, 0.00};
    case StrokeStyle::NaturalPencil:
        return {0.35, 0.22, 4.0, 0.75, 0.20, 0.45, 1.00,   0.0, 0.00, 0.5, 1.0, 0.45};
    case StrokeStyle::Watercolour:
        // Акварель размывает то, по чему прошла, и ложится тонким слоем.
        return {0.10, 0.10, 3.0, 1.40, 0.10, 0.15, 1.00,   0.0, 0.70, 0.9, 3.0, 0.20};
    }
    return {0.90, 1.00, 3.0, 1.00, 0.00, 0.00, 1.00, 0.0, 0.00, 0.5, 2.0, 0.00};
}

} // namespace

struct MyPaintEngine::Private {
    MyPaintBrush *brush = nullptr;
    MyPaintFixedTiledSurface *fixed = nullptr;
    MyPaintSurface *surface = nullptr;

    QImage base;            // холст, каким он был до начала мазка
    QSize size;
    bool firstPoint = true;

    void release()
    {
        if (brush) {
            mypaint_brush_unref(brush);
            brush = nullptr;
        }
        if (surface) {
            mypaint_surface_unref(surface);
            surface = nullptr;
        }
        fixed = nullptr;
        base = QImage();
        size = QSize();
        firstPoint = true;
    }
};

MyPaintEngine::MyPaintEngine()
    : d(new Private)
{
}

MyPaintEngine::~MyPaintEngine()
{
    if (d) {
        d->release();
        delete d;
    }
}

bool MyPaintEngine::isAvailable()
{
    return true;
}

bool MyPaintEngine::isActive() const
{
    return d && d->surface != nullptr;
}

bool MyPaintEngine::begin(const QImage &target, const QColor &colour, int width,
                          StrokeStyle style, bool antialias)
{
    if (!d)
        return false;

    d->release();

    if (target.isNull())
        return false;

    d->fixed = mypaint_fixed_tiled_surface_new(target.width(), target.height());
    if (!d->fixed)
        return false;
    d->surface = mypaint_fixed_tiled_surface_interface(d->fixed);
    if (!d->surface) {
        d->fixed = nullptr;
        return false;
    }

    d->brush = mypaint_brush_new();
    if (!d->brush) {
        d->release();
        return false;
    }
    mypaint_brush_from_defaults(d->brush);

    const BrushRecipe recipe = recipeFor(style);
    const double radius = qMax(0.6, width / 2.0) * recipe.radiusScale;

    auto set = [this](MyPaintBrushSetting setting, double value) {
        mypaint_brush_set_base_value(d->brush, setting, float(value));
    };

    // Радиус в libmypaint задаётся логарифмом: так одинаковый сдвиг ползунка
    // одинаково заметен и на тонкой кисти, и на толстой.
    set(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, qLn(radius));
    set(MYPAINT_BRUSH_SETTING_HARDNESS, recipe.hardness);
    set(MYPAINT_BRUSH_SETTING_OPAQUE, recipe.opaque);
    set(MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS, recipe.dabsPerRadius);
    set(MYPAINT_BRUSH_SETTING_RADIUS_BY_RANDOM, recipe.radiusJitter);
    set(MYPAINT_BRUSH_SETTING_OFFSET_BY_RANDOM, recipe.offsetJitter);
    set(MYPAINT_BRUSH_SETTING_ELLIPTICAL_DAB_RATIO, recipe.ellipseRatio);
    set(MYPAINT_BRUSH_SETTING_ELLIPTICAL_DAB_ANGLE, recipe.ellipseAngle);
    set(MYPAINT_BRUSH_SETTING_SMUDGE, recipe.smudge);
    set(MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH, recipe.smudgeLength);
    set(MYPAINT_BRUSH_SETTING_SLOW_TRACKING, recipe.slowTracking);
    set(MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY, recipe.speedOpacity);

    // Сглаживание края — то самое, из-за чего мазок перестаёт распадаться
    // на пиксели. При выключенном сглаживании в настройках его отключаем:
    // это режим Paint, где важны отдельные пиксели.
    set(MYPAINT_BRUSH_SETTING_ANTI_ALIASING, antialias ? 1.0 : 0.0);

    // Цвет движок держит в HSV.
    float h = 0.0f;
    float s = 0.0f;
    float v = 0.0f;
    colour.toHsv().getHsvF(&h, &s, &v);
    if (h < 0.0f)   // у серых оттенков тон не определён
        h = 0.0f;
    set(MYPAINT_BRUSH_SETTING_COLOR_H, h);
    set(MYPAINT_BRUSH_SETTING_COLOR_S, s);
    set(MYPAINT_BRUSH_SETTING_COLOR_V, v);

    mypaint_brush_reset(d->brush);
    mypaint_brush_new_stroke(d->brush);

    d->base = target.copy();
    d->size = target.size();
    d->firstPoint = true;
    return !d->base.isNull();
}

void MyPaintEngine::end()
{
    if (d)
        d->release();
}

QRect MyPaintEngine::motion(QImage &target, const QPointF &pos, double seconds)
{
    if (!isActive() || target.isNull() || target.size() != d->size)
        return QRect();

    // Первая точка подаётся с нулевой длительностью: движок ставит перо
    // на место, не проводя линию из прошлого мазка.
    const double dtime = d->firstPoint ? 0.0 : qMax(0.0005, seconds);

    mypaint_surface_begin_atomic(d->surface);
    mypaint_brush_stroke_to(d->brush, d->surface,
                            float(pos.x()), float(pos.y()),
                            1.0f,          // давление: мышь всегда полное
                            0.0f, 0.0f,    // наклон пера
                            dtime);
    MyPaintRectangle roi;
    roi.x = 0;
    roi.y = 0;
    roi.width = 0;
    roi.height = 0;
    mypaint_surface_end_atomic(d->surface, &roi);

    d->firstPoint = false;

    QRect area(roi.x, roi.y, roi.width, roi.height);
    area = area.intersected(target.rect());
    if (area.isEmpty())
        return QRect();

    // Пересобираем задетый кусок: копия холста плюс слой мазка поверх.
    // Класть слой на уже готовый результат нельзя — он бы темнел с каждым
    // движением мыши.
    QImage layer(area.size(), QImage::Format_ARGB32);
    layer.fill(Qt::transparent);

    MyPaintTiledSurface *tiled = reinterpret_cast<MyPaintTiledSurface *>(d->fixed);

    const int firstTileX = area.left() / kTile;
    const int lastTileX = area.right() / kTile;
    const int firstTileY = area.top() / kTile;
    const int lastTileY = area.bottom() / kTile;

    for (int ty = firstTileY; ty <= lastTileY; ++ty) {
        for (int tx = firstTileX; tx <= lastTileX; ++tx) {
            MyPaintTileRequest request;
            // Последний параметр — «только чтение»: тайл мы не меняем.
            mypaint_tile_request_init(&request, 0, tx, ty, 1);
            mypaint_tiled_surface_tile_request_start(tiled, &request);
            const uint16_t *buffer = request.buffer;
            if (!buffer) {
                mypaint_tiled_surface_tile_request_end(tiled, &request);
                continue;
            }

            const int baseX = tx * kTile;
            const int baseY = ty * kTile;

            for (int y = 0; y < kTile; ++y) {
                const int canvasY = baseY + y;
                if (canvasY < area.top() || canvasY > area.bottom())
                    continue;
                QRgb *out = reinterpret_cast<QRgb *>(
                    layer.scanLine(canvasY - area.top()));

                for (int x = 0; x < kTile; ++x) {
                    const int canvasX = baseX + x;
                    if (canvasX < area.left() || canvasX > area.right())
                        continue;

                    const uint16_t *px = buffer + (y * kTile + x) * 4;
                    const int alpha = px[3];
                    if (alpha <= 0)
                        continue;

                    // Цвет в тайле домножен на альфу — возвращаем его обратно.
                    const int red = qBound(0, px[0] * 255 / alpha, 255);
                    const int green = qBound(0, px[1] * 255 / alpha, 255);
                    const int blue = qBound(0, px[2] * 255 / alpha, 255);
                    const int a8 = qBound(0, alpha * 255 / kOne, 255);

                    out[canvasX - area.left()] = qRgba(red, green, blue, a8);
                }
            }

            mypaint_tiled_surface_tile_request_end(tiled, &request);
        }
    }

    QPainter p(&target);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(area.topLeft(), d->base, area);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.drawImage(area.topLeft(), layer);
    return area;
}

#else   // libmypaint не найдена — остаётся заглушка

struct MyPaintEngine::Private {};

MyPaintEngine::MyPaintEngine() = default;

MyPaintEngine::~MyPaintEngine()
{
    delete d;
}

bool MyPaintEngine::isAvailable()
{
    return false;
}

bool MyPaintEngine::isActive() const
{
    return false;
}

bool MyPaintEngine::begin(const QImage &target, const QColor &colour, int width,
                          StrokeStyle style, bool antialias)
{
    Q_UNUSED(target) Q_UNUSED(colour) Q_UNUSED(width)
    Q_UNUSED(style) Q_UNUSED(antialias)
    return false;
}

QRect MyPaintEngine::motion(QImage &target, const QPointF &pos, double seconds)
{
    Q_UNUSED(target) Q_UNUSED(pos) Q_UNUSED(seconds)
    return QRect();
}

void MyPaintEngine::end()
{
}

#endif
