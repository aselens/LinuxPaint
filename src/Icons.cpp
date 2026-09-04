#include "Icons.h"
#include "FluentIcons.h"
#include "LogoData.h"
#include "tools/ShapeTool.h"

#include <QBuffer>
#include <QConicalGradient>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

namespace Icons {
namespace {

QColor g_foreground(60, 60, 60);

// Все пиктограммы рисуются в системе координат 64×64 и затем
// масштабируются под нужный размер. Стиль — контурный, одноцветный:
// так значки читаются одинаково в светлой и тёмной теме и совпадают
// по духу с оформлением современного Paint.
const int kBase = 64;
const double kStroke = 4.5;

QPixmap makePixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    return pm;
}

void prepare(QPainter &p, int size)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(double(size) / kBase, double(size) / kBase);
}

QPen line(double width = kStroke, Qt::PenStyle style = Qt::SolidLine)
{
    QPen pen(g_foreground);
    pen.setWidthF(width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setStyle(style);
    return pen;
}

void outline(QPainter &p, double width = kStroke)
{
    p.setPen(line(width));
    p.setBrush(Qt::NoBrush);
}

QPainterPath polyline(const QVector<QPointF> &points, bool closed = false)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;
    path.moveTo(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    if (closed)
        path.closeSubpath();
    return path;
}

// Наконечник стрелки в точке `to`, направленный от `from`.
void arrowHead(QPainter &p, const QPointF &from, const QPointF &to, double size)
{
    const double angle = qAtan2(to.y() - from.y(), to.x() - from.x());
    QPainterPath head;
    head.moveTo(to);
    head.lineTo(to.x() - size * qCos(angle - M_PI / 7.0),
                to.y() - size * qSin(angle - M_PI / 7.0));
    head.lineTo(to.x() - size * qCos(angle + M_PI / 7.0),
                to.y() - size * qSin(angle + M_PI / 7.0));
    head.closeSubpath();

    const QPen saved = p.pen();
    p.setPen(Qt::NoPen);
    p.setBrush(g_foreground);
    p.drawPath(head);
    p.setPen(saved);
    p.setBrush(Qt::NoBrush);
}

void drawMagnifier(QPainter &p, int sign)
{
    outline(p);
    p.drawEllipse(QPointF(28, 28), 17, 17);
    p.setPen(line(6));
    p.drawLine(40, 40, 55, 55);

    if (sign != 0) {
        p.setPen(line(4));
        p.drawLine(21, 28, 35, 28);
        if (sign > 0)
            p.drawLine(28, 21, 28, 35);
    }
}

// Цветные значки для инструментов, которых нет в цветном наборе Fluent.
// Там их всего несколько штук — карандаш, кисть и шестерёнка, — а в Paint
// в цвете нарисовано больше. Недостающие рисуем сами, из нескольких частей
// разного цвета, а не одной заливкой: именно это отличает настоящий цветной
// значок от подкрашенного контура.
//
// Возвращает пустой QIcon, если для инструмента своего рисунка нет.
QIcon colouredTool(ToolId id, int size)
{
    if (id != ToolId::Eraser && id != ToolId::Fill && id != ToolId::ColorPicker)
        return QIcon();

    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);
    p.setPen(Qt::NoPen);

    // Цвета сняты пипеткой прямо со скриншота Paint, а не подобраны на глаз.
    const QColor eraserBody(0xF6, 0xAE, 0xAC);
    const QColor eraserLight(0xF8, 0xC1, 0xC0);
    const QColor eraserBand(0xFD, 0xF7, 0xF6);
    const QColor paintBlue(0x51, 0xA0, 0xC5);
    const QColor paintShade(0x4A, 0x8D, 0xAD);
    const QColor paintDark(0x40, 0x71, 0x89);
    const QColor paintDrop(0xFF, 0x63, 0x27);
    const QColor pickerBlue(0x54, 0xAC, 0xD4);
    const QColor pickerBody(0xDD, 0xDE, 0xDE);

    switch (id) {
    case ToolId::Eraser: {
        // Розовая резинка под наклоном: светлая верхняя грань, белая манжета
        // с одного конца.
        p.save();
        p.translate(31, 34);
        p.rotate(-38);
        p.setBrush(eraserBody);
        p.drawRoundedRect(QRectF(-21, -13, 42, 26), 5, 5);
        p.setBrush(eraserLight);
        p.drawRoundedRect(QRectF(-21, -13, 42, 11), 5, 5);
        p.setBrush(eraserBand);
        p.drawRoundedRect(QRectF(-21, -13, 14, 26), 5, 5);
        p.restore();
        break;
    }

    case ToolId::Fill: {
        // Синее ведро, наклонённое влево, и оранжевая капля краски под ним.
        p.save();
        p.translate(28, 27);
        p.rotate(-35);

        QPainterPath bucket;
        bucket.moveTo(-15, -10);
        bucket.lineTo(15, -10);
        bucket.lineTo(10, 16);
        bucket.lineTo(-10, 16);
        bucket.closeSubpath();
        p.setBrush(paintBlue);
        p.drawPath(bucket);

        p.setBrush(paintShade);
        p.drawRect(QRectF(-15, -10, 30, 5));

        p.setPen(QPen(paintDark, 3));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(-15, -23, 30, 24), 0, 180 * 16);
        p.restore();

        p.setPen(Qt::NoPen);
        p.setBrush(paintDrop);
        QPainterPath drop;
        drop.moveTo(46, 34);
        drop.cubicTo(53, 44, 53, 52, 46, 54);
        drop.cubicTo(39, 52, 39, 44, 46, 34);
        drop.closeSubpath();
        p.drawPath(drop);
        break;
    }

    case ToolId::ColorPicker: {
        // Пипетка: синяя колба справа вверху, светлый корпус по диагонали
        // вниз-влево, кончик внизу.
        p.setBrush(pickerBody);
        p.save();
        p.translate(29, 37);
        p.rotate(45);
        p.drawRect(QRectF(-5, -15, 10, 30));
        p.restore();

        p.setBrush(pickerBlue);
        p.save();
        p.translate(44, 21);
        p.rotate(45);
        p.drawRoundedRect(QRectF(-9, -12, 18, 24), 7, 7);
        p.restore();

        p.setBrush(paintShade);
        QPainterPath tip;
        tip.moveTo(10, 55);
        tip.lineTo(15, 40);
        tip.lineTo(25, 50);
        tip.closeSubpath();
        p.drawPath(tip);
        break;
    }

    default:
        break;
    }

    p.end();
    return QIcon(pm);
}

// Готовит значок из набора Fluent. Пустой QIcon означает, что значка с таким
// именем нет либо система не умеет рисовать SVG — тогда вызывающий рисует
// запасной вариант своими руками.
QIcon fluent(const char *name, int size)
{
    // Имени может не быть вовсе: так помечены значки, для которых мы
    // намеренно оставили свой рисунок. Проверка обязательна — дальше имя
    // уходит в strcmp, а тот пустого указателя не переживает.
    if (!name)
        return QIcon();

    const char *source = FluentIcons::svg(name);
    if (!source)
        return QIcon();

    QByteArray data(source);

    // Одноцветные значки перекрашиваем под тему — это подмена одного цвета.
    // Полноцветные (имена с _color) трогать нельзя: там цветов много, и
    // подменять в них нечего.
    data.replace(QByteArray(FluentIcons::kSourceColour), g_foreground.name().toUtf8());

    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer, QByteArrayLiteral("svg"));
    reader.setScaledSize(QSize(size, size));

    const QImage rendered = reader.read();
    if (rendered.isNull())
        return QIcon();

    return QIcon(QPixmap::fromImage(rendered));
}

void drawDashedBox(QPainter &p)
{
    QPen pen = line(3.5, Qt::DashLine);
    pen.setDashPattern({3.0, 2.0});
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(9, 11, 46, 42));
}

void drawRotationArc(QPainter &p, bool clockwise)
{
    p.save();
    if (!clockwise) {
        p.translate(kBase, 0);
        p.scale(-1, 1);
    }
    outline(p);
    p.drawArc(QRectF(13, 15, 38, 38), 200 * 16, -240 * 16);
    arrowHead(p, QPointF(40, 8), QPointF(50, 18), 12);
    p.restore();
}

void drawSheet(QPainter &p)
{
    // Лист с загнутым уголком.
    outline(p);
    p.drawPath(polyline({{16, 7}, {38, 7}, {49, 18}, {49, 57}, {16, 57}}, true));
    p.drawPath(polyline({{38, 7}, {38, 18}, {49, 18}}));
}

} // namespace

void setForeground(const QColor &colour)
{
    g_foreground = colour;
}

QColor foreground()
{
    return g_foreground;
}

// --- значки инструментов -------------------------------------------------

QIcon tool(ToolId id, int size)
{
    // Порядок предпочтений: полноцветный значок Microsoft, затем наш
    // собственный цветной, затем одноцветный из того же набора, и лишь
    // в самом конце — рисованный запасной.
    const QIcon own = colouredTool(id, size);
    if (!own.isNull())
        return own;

    const char *fluentName = nullptr;
    switch (id) {
    // У карандаша берём полноцветную версию Microsoft: в Paint он жёлтый,
    // и это их же рисунок — значит, цвета совпадут точно. Кисть в Paint
    // одноцветная, хотя цветная версия у них тоже есть.
    case ToolId::Pencil:      fluentName = "tool_pencil_color"; break;
    case ToolId::Brush:       fluentName = "tool_brush";        break;
    case ToolId::Eraser:      fluentName = "tool_eraser";    break;
    case ToolId::Fill:        fluentName = "tool_fill";      break;
    case ToolId::Text:        fluentName = "tool_text";      break;
    case ToolId::ColorPicker: fluentName = "tool_picker";    break;
    case ToolId::Magnifier:   fluentName = "tool_magnifier"; break;
    case ToolId::Select:      fluentName = "tool_select";    break;
    case ToolId::FreeSelect:  fluentName = "tool_lasso";     break;
    case ToolId::Shape:       fluentName = "tool_shapes";    break;
    }

    const QIcon ready = fluent(fluentName, size);
    if (!ready.isNull())
        return ready;

    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);
    outline(p);

    switch (id) {
    case ToolId::Pencil:
        // Корпус карандаша по диагонали плюс отчёркнутый кончик.
        p.drawPath(polyline({{12, 52}, {16, 38}, {42, 12}, {52, 22}, {26, 48}}, true));
        p.drawLine(16, 38, 26, 48);
        p.drawLine(45, 15, 49, 19);
        break;

    case ToolId::Brush:
        p.drawPath(polyline({{24, 40}, {40, 24}}));           // черенок
        p.setPen(line(kStroke));
        p.drawPath(polyline({{38, 18}, {52, 12}, {46, 26}}, true));
        p.drawPath(polyline({{14, 50}, {20, 34}, {30, 44}, {14, 50}}, true));
        p.drawLine(24, 40, 38, 22);
        break;

    case ToolId::Eraser:
        p.save();
        p.translate(32, 34);
        p.rotate(-35);
        p.drawRoundedRect(QRectF(-21, -13, 42, 26), 4, 4);
        p.drawLine(-5, -13, -5, 13);
        p.restore();
        break;

    case ToolId::Fill:
        p.save();
        p.translate(29, 31);
        p.rotate(-30);
        p.drawPath(polyline({{-16, -12}, {16, -12}, {11, 17}, {-11, 17}}, true));
        p.drawArc(QRectF(-16, -24, 32, 24), 0, 180 * 16);
        p.restore();
        p.drawPath(polyline({{51, 33}, {57, 45}, {45, 45}}, true));
        break;

    case ToolId::Text: {
        QFont f;
        f.setPixelSize(48);
        f.setBold(false);
        p.setFont(f);
        p.setPen(g_foreground);
        p.drawText(QRectF(0, 0, kBase, kBase), Qt::AlignCenter, QStringLiteral("A"));
        break;
    }

    case ToolId::ColorPicker:
        p.drawPath(polyline({{12, 52}, {14, 42}, {40, 16}, {48, 24}, {22, 50}}, true));
        p.setPen(line(6));
        p.drawLine(42, 12, 52, 22);
        break;

    case ToolId::Magnifier:
        drawMagnifier(p, 0);
        break;

    case ToolId::Select:
        drawDashedBox(p);
        break;

    case ToolId::FreeSelect: {
        QPen pen = line(3.5, Qt::DashLine);
        pen.setDashPattern({3.0, 2.0});
        p.setPen(pen);
        QPainterPath lasso;
        lasso.moveTo(14, 40);
        lasso.cubicTo(8, 16, 32, 6, 44, 16);
        lasso.cubicTo(58, 28, 48, 52, 30, 52);
        lasso.cubicTo(20, 52, 15, 47, 14, 40);
        p.drawPath(lasso);
        break;
    }

    case ToolId::Shape:
        p.drawRect(QRectF(8, 22, 28, 28));
        p.drawEllipse(QPointF(42, 26), 15, 15);
        break;
    }

    p.end();
    return QIcon(pm);
}

QIcon brush(BrushType id, int size)
{
    // Значок кисти — реальный мазок этой же кистью.
    QImage img(kBase, kBase, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    paintutil::drawPolyline(img, {QPointF(12, 50), QPointF(30, 26), QPointF(52, 16)},
                            g_foreground, 9, strokeStyleForBrush(id), true);

    QPixmap pm = QPixmap::fromImage(img);
    if (size != kBase)
        pm = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QIcon(pm);
}

QIcon brushSample(BrushType id, const QSize &size)
{
    QImage img(size, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    // Волна, как в Paint: подъём, спад и снова подъём. По одной дуге видно
    // и толщину, и текстуру, и то, как кисть ведёт себя на повороте.
    const double left = 8.0;
    const double right = size.width() - 8.0;
    const double span = right - left;
    const double middle = size.height() / 2.0;
    const double swing = size.height() * 0.30;

    QPainterPath path;
    path.moveTo(left, middle + swing);
    path.cubicTo(left + span * 0.28, middle - swing * 2.4,
                 left + span * 0.72, middle + swing * 2.4,
                 right, middle - swing);

    // Дугу разбиваем на короткие отрезки: кисть должна пройти по ней так же,
    // как прошла бы по холсту, — с тем же ходом и той же фактурой.
    const int steps = 64;
    QVector<QPointF> points;
    points.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i)
        points.append(path.pointAtPercent(double(i) / steps));

    paintutil::drawPolyline(img, points, g_foreground, 7,
                            strokeStyleForBrush(id), true);

    return QIcon(QPixmap::fromImage(img));
}

QIcon shape(ShapeType id, int size)
{
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);
    // Толщину считаем от итогового размера значка. Рисуем мы в поле 64
    // пикселя, а показываем клеткой около 24 — контур в 3,4 сжимается до
    // 1,3 пикселя и размывается сглаживанием в серое. Отсюда и берётся
    // ощущение, что фигуры блёклые. Ставим столько, чтобы после сжатия
    // линия была почти в два пикселя и оставалась белой.
    outline(p, 4.6);

    // Фигура занимает почти всё поле значка: в плашке ленты клетки мелкие,
    // и запас по краям только съедал бы читаемость.
    const QRectF box(7, 8, 50, 48);

    if (id == ShapeType::Line) {
        p.drawLine(QPointF(8, 56), QPointF(56, 8));
    } else if (id == ShapeType::Curve) {
        QPainterPath path;
        path.moveTo(6, 50);
        path.cubicTo(20, 4, 44, 60, 58, 16);
        p.drawPath(path);
    } else if (id == ShapeType::Polygon) {
        p.drawPath(polyline({{8, 50}, {22, 10}, {46, 22}, {56, 54}}, true));
    } else {
        p.drawPath(ShapeTool::pathForShape(id, box));
    }

    p.end();
    return QIcon(pm);
}

QIcon colourSwatch(const QColor &colour, int size)
{
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);
    p.setPen(QPen(QColor(140, 140, 140), 3));
    p.setBrush(colour);
    p.drawEllipse(QPointF(32, 32), 25, 25);
    p.end();
    return QIcon(pm);
}

QIcon strokeStyle(StrokeStyle style, int size)
{
    QImage img(kBase, kBase, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    paintutil::drawPolyline(img, {QPointF(10, 40), QPointF(54, 24)},
                            g_foreground, 8, style, true);
    QPixmap pm = QPixmap::fromImage(img);
    if (size != kBase)
        pm = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QIcon(pm);
}

QIcon fillStyle(FillMode fill, int size)
{
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);

    outline(p, 4.0);
    if (fill == FillMode::None) {
        p.drawRect(QRectF(11, 15, 42, 34));
        p.setPen(line(4.5));
        p.drawLine(15, 45, 49, 19);
    } else {
        p.setBrush(paintutil::styledBrush(g_foreground, fill));
        p.drawRect(QRectF(11, 15, 42, 34));
    }

    p.end();
    return QIcon(pm);
}

QIcon lineWidth(int width, int size)
{
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);
    p.setPen(Qt::NoPen);
    p.setBrush(g_foreground);
    p.drawRoundedRect(QRectF(8, 32 - width / 2.0, 48, width), width / 2.0, width / 2.0);
    p.end();
    return QIcon(pm);
}

// --- значки команд -------------------------------------------------------

QIcon action(Action id, int size)
{
    const char *fluentName = nullptr;
    switch (id) {
    case Action::New:             fluentName = "act_new";          break;
    case Action::Open:            fluentName = "act_open";         break;
    case Action::Save:            fluentName = "act_save";         break;
    case Action::SaveAs:          fluentName = "act_saveas";       break;
    case Action::Print:           fluentName = "act_print";        break;
    case Action::Properties:      fluentName = "act_properties";   break;
    case Action::Exit:            fluentName = "act_exit";         break;
    case Action::Undo:            fluentName = "act_undo";         break;
    case Action::Redo:            fluentName = "act_redo";         break;
    case Action::Cut:             fluentName = "act_cut";          break;
    case Action::Copy:            fluentName = "act_copy";         break;
    case Action::Paste:           fluentName = "act_paste";        break;
    case Action::Crop:            fluentName = "act_crop";         break;
    case Action::ResizeImage:     fluentName = "act_resize";       break;
    case Action::RotateRight:     fluentName = "act_rotate_cw";    break;
    case Action::RotateLeft:      fluentName = "act_rotate_ccw";   break;
    case Action::FlipHorizontal:  fluentName = "act_flip_h";       break;
    case Action::FlipVertical:    fluentName = "act_flip_v";       break;
    case Action::SelectAll:       fluentName = "act_select_all";   break;
    case Action::InvertSelection: fluentName = "act_select_off";   break;
    case Action::DeleteSelection: fluentName = "act_delete";       break;
    case Action::InvertColours:   fluentName = "act_invert";       break;
    case Action::ClearImage:      fluentName = "act_new";          break;
    case Action::ZoomIn:          fluentName = "act_zoom_in";      break;
    case Action::ZoomOut:         fluentName = "act_zoom_out";     break;
    case Action::ZoomReset:       fluentName = "act_zoom_fit";     break;
    case Action::Grid:            fluentName = "act_grid";         break;
    case Action::Rulers:          fluentName = "act_ruler";        break;
    case Action::Fullscreen:      fluentName = "act_fullscreen";   break;
    case Action::SwapColours:     fluentName = "act_swap";         break;
    case Action::Size:            fluentName = "act_size";         break;
    case Action::About:           fluentName = "act_about";        break;
    case Action::Share:           fluentName = "act_share";        break;
    case Action::Settings:        fluentName = "act_settings";     break;
    case Action::Chevron:         fluentName = "act_chevron_down"; break;
    case Action::ChevronUp:       fluentName = "act_chevron_up";   break;
    case Action::CursorPosition:  fluentName = "act_cursor";       break;
    case Action::CanvasSize:      fluentName = "act_image";        break;
    case Action::FitToWindow:     fluentName = "act_zoom_fit";     break;
    case Action::Opacity:         fluentName = "act_opacity";      break;
    case Action::Layers:          fluentName = "act_layers";       break;
    case Action::AddLayer:        fluentName = "act_layer_add";    break;
    case Action::LayerVisible:    fluentName = "act_eye";          break;
    case Action::LayerHidden:     fluentName = "act_eye_off";      break;
    // Палитру оставляем своей: у Fluent на её месте одноцветный значок,
    // а нам нужен именно радужный круг, как в Paint.
    case Action::EditColours:     fluentName = nullptr;            break;
    }

    const QIcon ready = fluent(fluentName, size);
    if (!ready.isNull())
        return ready;

    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    prepare(p, size);
    outline(p);

    switch (id) {
    case Action::New:
    case Action::ClearImage:
        drawSheet(p);
        break;

    case Action::Open:
        p.drawPath(polyline({{7, 50}, {7, 16}, {25, 16}, {30, 23}, {49, 23}, {49, 50}}, true));
        p.drawLine(7, 30, 49, 30);
        break;

    case Action::Save:
    case Action::SaveAs:
        // Дискета: корпус со срезанным углом, шторка и этикетка.
        p.drawPath(polyline({{11, 11}, {45, 11}, {53, 19}, {53, 53}, {11, 53}}, true));
        p.drawPath(polyline({{20, 11}, {20, 26}, {44, 26}, {44, 11}}));
        p.drawRect(QRectF(19, 36, 26, 17));
        if (id == Action::SaveAs) {
            p.setPen(line(5));
            p.drawLine(46, 47, 58, 47);
            p.drawLine(52, 41, 52, 53);
        }
        break;

    case Action::Print:
        p.drawRoundedRect(QRectF(9, 23, 46, 22), 4, 4);
        p.drawPath(polyline({{19, 23}, {19, 9}, {45, 9}, {45, 23}}));
        p.drawRect(QRectF(19, 39, 26, 16));
        break;

    case Action::Properties:
        p.drawRect(QRectF(10, 13, 44, 38));
        p.setPen(line(3.5));
        p.drawLine(18, 25, 46, 25);
        p.drawLine(18, 33, 38, 33);
        p.drawLine(18, 41, 42, 41);
        break;

    case Action::Exit:
        p.drawPath(polyline({{34, 12}, {12, 12}, {12, 52}, {34, 52}}));
        p.drawLine(26, 32, 54, 32);
        arrowHead(p, QPointF(40, 32), QPointF(56, 32), 11);
        break;

    case Action::Undo:
    case Action::Redo: {
        p.save();
        if (id == Action::Redo) {
            p.translate(kBase, 0);
            p.scale(-1, 1);
        }
        QPainterPath arc;
        arc.moveTo(15, 46);
        arc.cubicTo(15, 20, 51, 20, 51, 44);
        p.drawPath(arc);
        arrowHead(p, QPointF(26, 34), QPointF(15, 46), 12);
        p.restore();
        break;
    }

    case Action::Cut:
        p.setPen(line(4));
        p.drawLine(21, 9, 41, 39);
        p.drawLine(43, 9, 23, 39);
        p.drawEllipse(QPointF(19, 48), 8, 8);
        p.drawEllipse(QPointF(45, 48), 8, 8);
        break;

    case Action::Copy:
        p.drawRoundedRect(QRectF(9, 9, 30, 36), 3, 3);
        p.drawRoundedRect(QRectF(25, 21, 30, 36), 3, 3);
        break;

    case Action::Paste:
        p.drawRoundedRect(QRectF(11, 13, 42, 44), 4, 4);
        p.drawRoundedRect(QRectF(23, 7, 18, 12), 2, 2);
        p.setPen(line(3.5));
        p.drawLine(21, 32, 43, 32);
        p.drawLine(21, 42, 37, 42);
        break;

    case Action::Crop:
        p.drawPath(polyline({{18, 5}, {18, 46}, {59, 46}}));
        p.drawPath(polyline({{5, 18}, {46, 18}, {46, 59}}));
        break;

    case Action::ResizeImage:
        p.drawRect(QRectF(8, 8, 26, 26));
        p.drawRect(QRectF(30, 30, 26, 26));
        p.setPen(line(3.5));
        p.drawLine(38, 26, 52, 12);
        arrowHead(p, QPointF(44, 20), QPointF(54, 10), 9);
        break;

    case Action::RotateRight:
        drawRotationArc(p, true);
        break;

    case Action::RotateLeft:
        drawRotationArc(p, false);
        break;

    case Action::FlipHorizontal:
        p.drawPath(polyline({{26, 12}, {8, 32}, {26, 52}}, true));
        p.drawPath(polyline({{38, 12}, {56, 32}, {38, 52}}, true));
        p.setPen(line(3, Qt::DashLine));
        p.drawLine(32, 6, 32, 58);
        break;

    case Action::FlipVertical:
        p.drawPath(polyline({{12, 26}, {32, 8}, {52, 26}}, true));
        p.drawPath(polyline({{12, 38}, {32, 56}, {52, 38}}, true));
        p.setPen(line(3, Qt::DashLine));
        p.drawLine(6, 32, 58, 32);
        break;

    case Action::SelectAll:
        drawDashedBox(p);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(g_foreground.red(), g_foreground.green(), g_foreground.blue(), 60));
        p.drawRect(QRectF(12, 14, 40, 36));
        break;

    case Action::InvertSelection:
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(g_foreground.red(), g_foreground.green(), g_foreground.blue(), 60));
        p.drawRect(QRectF(11, 13, 42, 38));
        p.setBrush(Qt::NoBrush);
        drawDashedBox(p);
        outline(p, 3.5);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(24, 24, 20, 18));
        break;

    case Action::DeleteSelection:
        drawDashedBox(p);
        p.setPen(line(5));
        p.drawLine(21, 23, 43, 43);
        p.drawLine(43, 23, 21, 43);
        break;

    case Action::InvertColours: {
        p.drawEllipse(QPointF(32, 32), 22, 22);
        QPainterPath half;
        half.moveTo(32, 10);
        half.arcTo(QRectF(10, 10, 44, 44), 90, -180);
        half.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(g_foreground);
        p.drawPath(half);
        break;
    }

    case Action::ZoomIn:
        drawMagnifier(p, 1);
        break;

    case Action::ZoomOut:
        drawMagnifier(p, -1);
        break;

    case Action::ZoomReset:
        drawMagnifier(p, 0);
        break;

    case Action::Grid:
        p.setPen(line(3));
        for (int i = 0; i <= 3; ++i) {
            const double c = 12 + i * 13.0;
            p.drawLine(QPointF(12, c), QPointF(51, c));
            p.drawLine(QPointF(c, 12), QPointF(c, 51));
        }
        break;

    case Action::Rulers:
        p.drawRect(QRectF(7, 22, 50, 20));
        p.setPen(line(3));
        for (int i = 1; i < 6; ++i)
            p.drawLine(QPointF(7 + i * 8.3, 22), QPointF(7 + i * 8.3, i % 2 ? 30 : 34));
        break;

    case Action::Fullscreen:
        p.drawPath(polyline({{8, 24}, {8, 8}, {24, 8}}));
        p.drawPath(polyline({{40, 8}, {56, 8}, {56, 24}}));
        p.drawPath(polyline({{56, 40}, {56, 56}, {40, 56}}));
        p.drawPath(polyline({{24, 56}, {8, 56}, {8, 40}}));
        break;

    case Action::EditColours: {
        // Цветовой круг — единственный намеренно цветной значок,
        // как и в оригинале: он обозначает выбор произвольного цвета.
        QConicalGradient wheel(QPointF(32, 32), 90);
        for (int i = 0; i <= 12; ++i)
            wheel.setColorAt(i / 12.0, QColor::fromHsv((i * 30) % 360, 235, 245));
        p.setPen(Qt::NoPen);
        p.setBrush(wheel);
        p.drawEllipse(QPointF(32, 32), 25, 25);
        p.setBrush(QColor(255, 255, 255));
        p.drawEllipse(QPointF(32, 32), 9, 9);
        break;
    }

    case Action::SwapColours:
        p.drawEllipse(QPointF(21, 21), 13, 13);
        p.drawEllipse(QPointF(43, 43), 13, 13);
        p.setPen(line(3.5));
        p.drawLine(40, 16, 54, 16);
        arrowHead(p, QPointF(46, 16), QPointF(56, 16), 8);
        p.drawLine(24, 48, 10, 48);
        arrowHead(p, QPointF(18, 48), QPointF(8, 48), 8);
        break;

    case Action::Size:
        p.setPen(Qt::NoPen);
        p.setBrush(g_foreground);
        p.drawRoundedRect(QRectF(10, 16, 44, 3), 1.5, 1.5);
        p.drawRoundedRect(QRectF(10, 27, 44, 6), 3, 3);
        p.drawRoundedRect(QRectF(10, 40, 44, 10), 5, 5);
        break;

    case Action::About: {
        p.drawEllipse(QPointF(32, 32), 23, 23);
        p.setPen(line(5));
        p.drawLine(32, 28, 32, 45);
        p.drawPoint(QPointF(32, 20));
        break;
    }

    case Action::Share:
        p.drawEllipse(QPointF(46, 15), 8, 8);
        p.drawEllipse(QPointF(16, 32), 8, 8);
        p.drawEllipse(QPointF(46, 49), 8, 8);
        p.setPen(line(3.5));
        p.drawLine(23, 28, 39, 19);
        p.drawLine(23, 36, 39, 45);
        break;

    case Action::Settings: {
        // Шестерёнка: кольцо и восемь зубцов по окружности.
        p.drawEllipse(QPointF(32, 32), 11, 11);
        p.setPen(line(5));
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4.0;
            p.drawLine(QPointF(32 + qCos(a) * 17, 32 + qSin(a) * 17),
                       QPointF(32 + qCos(a) * 24, 32 + qSin(a) * 24));
        }
        break;
    }

    // Галочку рисуем во всю ширину поля и потолще: при сжатии до 12–16
    // пикселей мелкий знак вырождался в еле заметную чёрточку.
    case Action::Chevron:
        p.setPen(line(8));
        p.drawPath(polyline({{10, 22}, {32, 44}, {54, 22}}));
        break;

    case Action::ChevronUp:
        p.setPen(line(8));
        p.drawPath(polyline({{10, 44}, {32, 22}, {54, 44}}));
        break;

    case Action::CursorPosition:
        // Стрелка курсора — метка координат в строке состояния.
        p.setPen(Qt::NoPen);
        p.setBrush(g_foreground);
        p.drawPath(polyline({{16, 8}, {16, 50}, {27, 39}, {34, 55},
                             {42, 51}, {35, 36}, {50, 34}}, true));
        break;

    case Action::CanvasSize:
        p.drawRect(QRectF(10, 14, 44, 36));
        p.setPen(line(3.5));
        p.drawLine(10, 24, 54, 24);
        break;

    case Action::FitToWindow:
        p.drawRect(QRectF(8, 14, 48, 36));
        p.setPen(line(3.5, Qt::DashLine));
        p.drawRect(QRectF(20, 24, 24, 16));
        break;

    case Action::Layers:
        // Стопка листов, смещённых по диагонали.
        p.drawPath(polyline({{32, 8}, {56, 22}, {32, 36}, {8, 22}}, true));
        p.setPen(line(3.5));
        p.drawPath(polyline({{10, 32}, {32, 45}, {54, 32}}));
        p.drawPath(polyline({{10, 42}, {32, 55}, {54, 42}}));
        break;

    case Action::AddLayer:
        p.drawEllipse(QPointF(32, 32), 22, 22);
        p.setPen(line(5));
        p.drawLine(21, 32, 43, 32);
        p.drawLine(32, 21, 32, 43);
        break;

    case Action::LayerVisible: {
        // Глаз: два дуговых века и зрачок.
        QPainterPath eye;
        eye.moveTo(6, 32);
        eye.cubicTo(18, 14, 46, 14, 58, 32);
        eye.cubicTo(46, 50, 18, 50, 6, 32);
        p.drawPath(eye);
        p.drawEllipse(QPointF(32, 32), 8, 8);
        break;
    }

    case Action::LayerHidden: {
        QPainterPath eye;
        eye.moveTo(6, 32);
        eye.cubicTo(18, 14, 46, 14, 58, 32);
        eye.cubicTo(46, 50, 18, 50, 6, 32);
        p.drawPath(eye);
        p.drawEllipse(QPointF(32, 32), 8, 8);
        p.setPen(line(5));
        p.drawLine(12, 52, 52, 12);
        break;
    }

    case Action::Opacity: {
        // Капля, наполовину залитая, — привычный знак прозрачности.
        QPainterPath drop;
        drop.moveTo(32, 8);
        drop.cubicTo(50, 30, 54, 38, 54, 42);
        drop.arcTo(QRectF(10, 20, 44, 44), 0, -180);
        drop.cubicTo(10, 38, 14, 30, 32, 8);
        drop.closeSubpath();

        QPainterPath lower;
        lower.addRect(QRectF(0, 42, kBase, kBase - 42));

        p.setPen(Qt::NoPen);
        p.setBrush(g_foreground);
        p.drawPath(drop.intersected(lower));

        outline(p, 4.0);
        p.drawPath(drop);
        break;
    }
    }

    p.end();
    return QIcon(pm);
}

QIcon withChevron(const QIcon &base, int size)
{
    QPixmap pm = makePixmap(size);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Основной значок ужимаем и прижимаем к левому верхнему углу,
    // освобождая правый нижний под шеврон.
    const int inner = qMax(8, int(size * 0.78));
    base.paint(&p, QRect(0, 0, inner, inner));

    QPen pen(g_foreground);
    pen.setWidthF(qMax(1.0, size / 13.0));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);

    const double w = size * 0.30;
    const double x = size - w - size * 0.04;
    const double y = size - w * 0.5 - size * 0.16;
    p.drawLine(QPointF(x, y), QPointF(x + w / 2.0, y + w / 2.0));
    p.drawLine(QPointF(x + w / 2.0, y + w / 2.0), QPointF(x + w, y));

    p.end();
    return QIcon(pm);
}

QIcon application()
{
    // Логотип берём из текста, вшитого в сам исполняемый файл (LogoData.h).
    // Никаких путей и ресурсов: где бы программа ни оказалась — в пакете,
    // в AppImage или запущенная прямо из каталога сборки — данные едут с ней.
    QByteArray data = QByteArray::fromRawData(kLogoSvg, int(sizeof(kLogoSvg) - 1));
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer, QByteArrayLiteral("svg"));
    // Размер задаём явно: в самом файле заявлено 4096×4096, и без этого
    // на значок ушло бы 64 МБ памяти.
    reader.setScaledSize(QSize(256, 256));

    const QImage rendered = reader.read();
    if (!rendered.isNull())
        return QIcon(QPixmap::fromImage(rendered));

    // Отрисовка SVG выполняется подключаемым модулем Qt. Если его в системе
    // нет, рисуем запасной значок, чтобы окно не осталось совсем без него.

    QPixmap pm = makePixmap(128);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255));
    p.drawRoundedRect(QRectF(4, 4, 56, 56), 8, 8);
    p.setPen(QPen(QColor(120, 120, 130), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(4, 4, 56, 56), 8, 8);

    const QColor swatches[] = {QColor(220, 60, 60), QColor(245, 170, 50),
                               QColor(250, 220, 70), QColor(90, 180, 90),
                               QColor(70, 140, 220), QColor(140, 90, 190)};
    for (int i = 0; i < 6; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(swatches[i]);
        p.drawEllipse(QPointF(16 + (i % 3) * 15, 39 + (i / 3) * 13), 5.5, 5.5);
    }

    p.save();
    p.translate(40, 20);
    p.rotate(35);
    p.setBrush(QColor(150, 105, 70));
    p.drawRect(QRectF(-3, -16, 6, 20));
    p.setBrush(QColor(60, 130, 200));
    QPainterPath hair;
    hair.moveTo(-3, 4);
    hair.lineTo(3, 4);
    hair.lineTo(1, 14);
    hair.lineTo(-1, 14);
    hair.closeSubpath();
    p.drawPath(hair);
    p.restore();

    p.end();
    return QIcon(pm);
}

} // namespace Icons
