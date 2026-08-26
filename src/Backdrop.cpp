#include "Backdrop.h"

#include <QPainter>
#include <QRadialGradient>

Backdrop::Backdrop(QWidget *parent)
    : QWidget(parent)
    , m_base(0x1B, 0x1D, 0x23)
    , m_glow(0x46, 0x6E, 0xBE, 62)
{
}

void Backdrop::setColours(const QColor &base, const QColor &glow)
{
    if (m_base == base && m_glow == glow)
        return;
    m_base = base;
    m_glow = glow;
    update();
}

void Backdrop::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), m_base);

    if (m_glow.alpha() == 0)
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Одно пятно свечения. Центры вынесены за край окна: внутрь попадает
    // только край пятна, поэтому оно читается как отсвет, а не как круг.
    auto paintGlow = [&](const QPointF &centre, double radius, int alpha) {
        if (alpha <= 0 || radius <= 0.0)
            return;

        QColor core = m_glow;
        core.setAlpha(alpha);
        // Промежуточная точка гасит свечение вчетверо уже на середине
        // радиуса. Без неё слабый ореол расползается до самых краёв, и
        // вместо отсвета снова получается градиент во весь фон.
        QColor middle = m_glow;
        middle.setAlpha(alpha / 4);
        QColor edge = m_glow;
        edge.setAlpha(0);

        QRadialGradient gradient(centre, radius);
        gradient.setColorAt(0.0, core);
        gradient.setColorAt(0.45, middle);
        gradient.setColorAt(1.0, edge);

        painter.fillRect(rect(), gradient);
    };

    const double longSide = qMax(width(), height());

    // Основное — из левого нижнего угла.
    paintGlow(QPointF(width() * 0.02, height() * 1.06), longSide * 0.82,
              m_glow.alpha());

    // Второе, послабее — от правого края на середине высоты.
    paintGlow(QPointF(width() * 1.06, height() * 0.5), longSide * 0.55,
              m_glow.alpha() * 3 / 5);
}
