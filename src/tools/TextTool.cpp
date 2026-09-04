#include "tools/TextTool.h"
#include "Canvas.h"
#include "Document.h"

#include <QFontMetrics>
#include <QPainter>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QtMath>

namespace {

// Насколько рамка отстоит от текста, в экранных пикселях. Этот зазор — то
// самое место, куда не дотягивается редактор, и потому единственное, где
// мышь достаётся холсту.
const int kFramePad = 6;

// Сторона маркера. Меньше зазора вдвое с запасом: иначе маркер наползёт
// на редактор и перестанет нажиматься.
const int kGripSize = 7;

const QColor kFrameColour(0x00, 0x78, 0xD4);

// Минимальная ширина рамки в пикселях изображения. Высоту считаем от шрифта:
// схлопнуть рамку тоньше строки бессмысленно.
const int kMinWidth = 24;

}   // namespace

TextTool::~TextTool()
{
    destroyEditor();
}

void TextTool::createEditor(const QRect &box)
{
    destroyEditor();

    m_box = box;
    m_editor = new QTextEdit(m_canvas);
    m_editor->setFrameStyle(QFrame::NoFrame);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setStyleSheet(QStringLiteral("QTextEdit { background: transparent; }"));
    m_editor->viewport()->setAutoFillBackground(false);
    m_editor->document()->setDocumentMargin(2);
    m_editor->setAcceptRichText(false);

    // Редактор новый — про него ничего не применено. Без сброса viewChanged()
    // решил бы, что шрифт и цвет уже на месте (они остались от прошлой
    // надписи), и вторая надпись подряд осталась бы вовсе без формата.
    m_appliedFont = QFont();
    m_appliedColor = QColor();
    m_appliedScale = 0.0;

    // Набранное перестало влезать — рамка едет вниз сама, как в Paint.
    // Редактор указан контекстом связи: она исчезнет вместе с ним.
    QObject::connect(m_editor->document(), &QTextDocument::contentsChanged,
                     m_editor, [this] { growToFit(); });

    viewChanged();
    m_editor->show();
    m_editor->setFocus(Qt::MouseFocusReason);
}

void TextTool::destroyEditor()
{
    if (!m_editor)
        return;
    m_editor->hide();
    m_editor->deleteLater();
    m_editor = nullptr;
    m_grip = Grip::None;
    m_hoverGrip = Grip::None;
}

void TextTool::syncEditorGeometry()
{
    if (!m_editor)
        return;
    m_editor->setGeometry(m_canvas->imageRectToWidget(m_box));
}

void TextTool::growToFit()
{
    if (!m_editor || m_grip != Grip::None)
        return;

    // Высота документа считается в экранных пикселях, а рамка живёт
    // в пикселях изображения — приводим к общему масштабу.
    const double zoom = m_canvas->zoom() > 0 ? m_canvas->zoom() : 1.0;
    const int needed = qCeil(m_editor->document()->size().height() / zoom) + 2;
    if (needed <= m_box.height())
        return;

    m_box.setHeight(needed);
    syncEditorGeometry();
    requestRepaint();
}

// --- рамка и маркеры -----------------------------------------------------

QRect TextTool::frameRect() const
{
    return m_canvas->imageRectToWidget(m_box)
        .adjusted(-kFramePad, -kFramePad, kFramePad, kFramePad);
}

QRect TextTool::gripRect(const QPoint &centre) const
{
    return QRect(centre.x() - kGripSize / 2, centre.y() - kGripSize / 2,
                 kGripSize, kGripSize);
}

TextTool::Grip TextTool::gripAt(const QPoint &widgetPos) const
{
    if (!m_editor)
        return Grip::None;

    const QRect frame = frameRect();
    const QPoint centre = frame.center();

    const struct { Grip grip; QPoint at; } grips[] = {
        { Grip::TopLeft,     frame.topLeft() },
        { Grip::Top,         QPoint(centre.x(), frame.top()) },
        { Grip::TopRight,    frame.topRight() },
        { Grip::Right,       QPoint(frame.right(), centre.y()) },
        { Grip::BottomRight, frame.bottomRight() },
        { Grip::Bottom,      QPoint(centre.x(), frame.bottom()) },
        { Grip::BottomLeft,  frame.bottomLeft() },
        { Grip::Left,        QPoint(frame.left(), centre.y()) },
    };

    for (const auto &g : grips) {
        if (gripRect(g.at).contains(widgetPos))
            return g.grip;
    }

    // Мимо маркеров, но в пределах рамки — значит, по её полю: тянем целиком.
    if (frame.contains(widgetPos))
        return Grip::Move;

    return Grip::None;
}

QRect TextTool::boxForGrip(Grip grip, const QPointF &pos) const
{
    // Тянуть рамку за пределы холста незачем — ограничиваем саму точку,
    // тогда и рамка никуда не денется.
    const QRect limits = doc()->image().rect();
    const QPoint p(qBound(limits.left(), qRound(pos.x()), limits.right()),
                   qBound(limits.top(), qRound(pos.y()), limits.bottom()));

    QRect box = m_gripBox;

    if (grip == Grip::Move) {
        box.translate(p - m_gripStart.toPoint());
        // Целиком уехать за край не даём: подпираем рамку границами холста.
        if (box.right() > limits.right())
            box.moveRight(limits.right());
        if (box.bottom() > limits.bottom())
            box.moveBottom(limits.bottom());
        if (box.left() < limits.left())
            box.moveLeft(limits.left());
        if (box.top() < limits.top())
            box.moveTop(limits.top());
        return box;
    }

    switch (grip) {
    case Grip::TopLeft:     box.setTopLeft(p);      break;
    case Grip::Top:         box.setTop(p.y());      break;
    case Grip::TopRight:    box.setTopRight(p);     break;
    case Grip::Right:       box.setRight(p.x());    break;
    case Grip::BottomRight: box.setBottomRight(p);  break;
    case Grip::Bottom:      box.setBottom(p.y());   break;
    case Grip::BottomLeft:  box.setBottomLeft(p);   break;
    case Grip::Left:        box.setLeft(p.x());     break;
    default:                                        break;
    }

    // Схлопывать рамку в точку нельзя: подпираем ту сторону, которую тянут,
    // а противоположную оставляем на месте.
    const QFontMetrics metrics(settings().font);
    const int minHeight = metrics.height() + 4;

    const bool movesLeft = grip == Grip::Left || grip == Grip::TopLeft
                           || grip == Grip::BottomLeft;
    const bool movesTop = grip == Grip::Top || grip == Grip::TopLeft
                          || grip == Grip::TopRight;

    if (box.width() < kMinWidth) {
        if (movesLeft)
            box.setLeft(box.right() - kMinWidth + 1);
        else
            box.setRight(box.left() + kMinWidth - 1);
    }
    if (box.height() < minHeight) {
        if (movesTop)
            box.setTop(box.bottom() - minHeight + 1);
        else
            box.setBottom(box.top() + minHeight - 1);
    }

    return box;
}

// --- вид -----------------------------------------------------------------

void TextTool::viewChanged()
{
    if (!m_editor)
        return;

    // Шрифт масштабируем вместе с холстом, иначе на увеличении текст
    // в редакторе не совпадёт с тем, что попадёт в изображение.
    const double pointSize = settings().font.pointSizeF() > 0
                                 ? settings().font.pointSizeF()
                                 : 12.0;
    const double scaledSize = qMax(1.0, pointSize * m_canvas->zoom());

    // Переприменять формат ко всему тексту нужно только когда реально
    // сменились шрифт, цвет или масштаб: иначе на каждой прокрутке
    // курсор набора прыгал бы в конец.
    const bool formatDirty = m_appliedFont != settings().font
                             || m_appliedColor != settings().color1
                             || !qFuzzyCompare(m_appliedScale + 1.0, scaledSize + 1.0);

    if (formatDirty) {
        QFont scaled = settings().font;
        scaled.setPointSizeF(scaledSize);
        m_editor->document()->setDefaultFont(scaled);

        // Цвет задаём и через таблицу стилей: пока документ пуст, формата
        // символов ещё нет, и текст брался бы из палитры темы — в тёмной
        // теме это светло-серый, на белом холсте почти невидимый.
        m_editor->setStyleSheet(
            QStringLiteral("QTextEdit { background: transparent; color: %1; }")
                .arg(settings().color1.name(QColor::HexRgb)));

        QTextCharFormat format;
        format.setFont(scaled);
        format.setForeground(settings().color1);

        // Запоминаем положение курсора числами, а не самим курсором: у него
        // с собой формат символов, и восстановление курсора возвращало бы
        // старый формат — цвет, только что назначенный для набора, пропадал.
        QTextCursor caret = m_editor->textCursor();
        const int anchor = caret.anchor();
        const int position = caret.position();

        QTextCursor whole(m_editor->document());
        whole.select(QTextCursor::Document);
        whole.mergeCharFormat(format);

        caret.setPosition(anchor);
        caret.setPosition(position, QTextCursor::KeepAnchor);
        m_editor->setTextCursor(caret);

        // А это — формат для того, что наберут дальше.
        m_editor->setCurrentCharFormat(format);

        m_appliedFont = settings().font;
        m_appliedColor = settings().color1;
        m_appliedScale = scaledSize;
    }

    syncEditorGeometry();
}

void TextTool::press(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (button != Qt::LeftButton)
        return;

    if (m_editor) {
        // Клик по рамке — берёмся за неё, а не начинаем новую надпись.
        const Grip grip = gripAt(m_canvas->imageToWidget(pos));
        if (grip != Grip::None) {
            m_grip = grip;
            m_gripBox = m_box;
            m_gripStart = pos;
            m_active = true;
            return;
        }

        // Мимо рамки — значит, набор закончен.
        commit();
    }

    m_origin = pos;
    m_dragging = true;
    m_active = true;
    m_box = QRect(pos.toPoint(), QSize(0, 0));
    requestRepaint();
}

void TextTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)

    if (m_grip != Grip::None) {
        m_box = boxForGrip(m_grip, pos);
        syncEditorGeometry();
        requestRepaint();
        return;
    }

    if (!m_dragging) {
        // Подсказываем видом курсора, что рамку можно тянуть. Холст ставит
        // курсор инструмента перед этим вызовом, так что последнее слово наше.
        if (m_editor) {
            const Grip hover = gripAt(m_canvas->imageToWidget(pos));
            if (hover != m_hoverGrip) {
                m_hoverGrip = hover;
                m_canvas->setCursor(cursor());
            }
        }
        return;
    }

    m_box = QRectF(m_origin, pos).normalized().toAlignedRect();
    requestRepaint();
}

void TextTool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(button) Q_UNUSED(mods)

    if (m_grip != Grip::None) {
        m_grip = Grip::None;
        m_active = false;
        // Пока рамку тянули, дорастить её под текст было нельзя — теперь можно.
        growToFit();
        requestRepaint();
        return;
    }

    if (!m_dragging)
        return;

    m_dragging = false;
    m_active = false;

    QRect box = QRectF(m_origin, pos).normalized().toAlignedRect();

    // Простой клик без протяжки — рамка по умолчанию, как в Paint.
    const QFontMetrics metrics(settings().font);
    const int minHeight = metrics.height() + 8;
    if (box.width() < 20)
        box.setWidth(240);
    if (box.height() < minHeight)
        box.setHeight(minHeight);

    box = box.intersected(doc()->image().rect().adjusted(0, 0, 1, 1));
    if (box.width() < 8 || box.height() < 8)
        return;

    createEditor(box);
    requestRepaint();
}

void TextTool::paintOverlay(QPainter &painter)
{
    if (!m_dragging && !m_editor)
        return;

    // Подложка под текстом ложится ровно по рамке, поэтому рисуется
    // в координатах изображения — вместе с холстом.
    if (m_editor && settings().textOpaque) {
        painter.save();
        painter.fillRect(m_box, settings().color2);
        painter.restore();
    }

    if (!m_editor) {
        // Рамку ещё только тянут: она совпадает с будущим полем текста.
        painter.save();
        QPen pen(kFrameColour);
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_box);
        painter.restore();
        return;
    }

    // Рамка и маркеры набранного текста — в экранных пикселях: их размер
    // не должен зависеть от масштаба холста.
    painter.save();
    painter.resetTransform();
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect frame = frameRect();

    QPen pen(kFrameColour);
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frame);

    const QPoint centre = frame.center();
    const QPoint points[] = {
        frame.topLeft(),
        QPoint(centre.x(), frame.top()),
        frame.topRight(),
        QPoint(frame.right(), centre.y()),
        frame.bottomRight(),
        QPoint(centre.x(), frame.bottom()),
        frame.bottomLeft(),
        QPoint(frame.left(), centre.y()),
    };

    painter.setPen(QPen(kFrameColour, 1));
    painter.setBrush(Qt::white);
    for (const QPoint &p : points)
        painter.drawRect(gripRect(p));

    painter.restore();
}

QCursor TextTool::cursor() const
{
    switch (m_hoverGrip) {
    case Grip::Move:
        return QCursor(Qt::SizeAllCursor);
    case Grip::Top:
    case Grip::Bottom:
        return QCursor(Qt::SizeVerCursor);
    case Grip::Left:
    case Grip::Right:
        return QCursor(Qt::SizeHorCursor);
    case Grip::TopLeft:
    case Grip::BottomRight:
        return QCursor(Qt::SizeFDiagCursor);
    case Grip::TopRight:
    case Grip::BottomLeft:
        return QCursor(Qt::SizeBDiagCursor);
    case Grip::None:
        break;
    }
    return QCursor(Qt::IBeamCursor);
}

void TextTool::commit()
{
    if (!m_editor) {
        m_dragging = false;
        return;
    }

    const QString plain = m_editor->toPlainText();
    if (plain.trimmed().isEmpty()) {
        cancel();
        return;
    }

    // Готовим копию документа в «единичном» масштабе: на экране шрифт
    // увеличен вместе с холстом, а в изображение он должен лечь как есть.
    // Размер задан в самих символьных форматах, поэтому мало сменить
    // шрифт по умолчанию — нужно пройтись по всему тексту.
    QTextDocument *rendered = m_editor->document()->clone();
    rendered->setDefaultFont(settings().font);
    rendered->setDocumentMargin(2);

    QTextCursor cursor(rendered);
    cursor.select(QTextCursor::Document);
    QTextCharFormat format;
    format.setFont(settings().font);
    format.setForeground(settings().color1);
    cursor.mergeCharFormat(format);

    rendered->setTextWidth(m_box.width());

    doc()->beginEdit();
    {
        QPainter p(&image());
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        if (settings().textOpaque)
            p.fillRect(m_box, settings().color2);
        p.setClipRect(m_box);
        p.translate(m_box.topLeft());
        p.setPen(settings().color1);
        rendered->drawContents(&p, QRectF(0, 0, m_box.width(), m_box.height()));
    }
    delete rendered;

    const QRect dirty = m_box.adjusted(-2, -2, 2, 2);
    destroyEditor();
    m_box = QRect();
    m_dragging = false;
    m_active = false;

    doc()->endEdit(dirty);
    requestRepaint();
}

void TextTool::cancel()
{
    destroyEditor();
    m_box = QRect();
    m_dragging = false;
    m_active = false;
    requestRepaint();
}
