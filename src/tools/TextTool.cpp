#include "tools/TextTool.h"
#include "Canvas.h"
#include "Document.h"

#include <QFontMetrics>
#include <QPainter>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

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
}

void TextTool::syncEditorGeometry()
{
    if (!m_editor)
        return;
    m_editor->setGeometry(m_canvas->imageRectToWidget(m_box));
}

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

        const QTextCursor saved = m_editor->textCursor();
        m_editor->selectAll();
        m_editor->setFontFamily(settings().font.family());
        m_editor->setFontPointSize(scaledSize);
        m_editor->setFontWeight(settings().font.bold() ? QFont::Bold : QFont::Normal);
        m_editor->setFontItalic(settings().font.italic());
        m_editor->setFontUnderline(settings().font.underline());
        m_editor->setTextColor(settings().color1);
        m_editor->setTextCursor(saved);

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

    // Клик пришёл в холст, а не в редактор — значит, мимо рамки: фиксируем.
    if (m_editor)
        commit();

    m_origin = pos;
    m_dragging = true;
    m_active = true;
    m_box = QRect(pos.toPoint(), QSize(0, 0));
    requestRepaint();
}

void TextTool::move(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(mods)
    if (!m_dragging)
        return;
    m_box = QRectF(m_origin, pos).normalized().toAlignedRect();
    requestRepaint();
}

void TextTool::release(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods)
{
    Q_UNUSED(button) Q_UNUSED(mods)
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

    painter.save();
    if (m_editor && settings().textOpaque)
        painter.fillRect(m_box, settings().color2);

    QPen pen(QColor(0, 120, 215));
    pen.setCosmetic(true);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(m_box);
    painter.restore();
}

QCursor TextTool::cursor() const
{
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
