#include "Dialogs.h"
#include "Icons.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>

namespace {
// Экранная плотность, от которой Paint пересчитывает сантиметры и дюймы.
const double kDpi = 96.0;
} // namespace

// --- ResizeSkewDialog ----------------------------------------------------

ResizeSkewDialog::ResizeSkewDialog(const QSize &current, QWidget *parent)
    : QDialog(parent)
    , m_original(current)
{
    setWindowTitle(tr("Изменение размеров и наклона"));
    setModal(true);

    auto *root = new QVBoxLayout(this);

    // --- изменение размеров ---
    auto *resizeBox = new QGroupBox(tr("Изменить размер"), this);
    auto *resizeLayout = new QVBoxLayout(resizeBox);

    auto *modeRow = new QHBoxLayout;
    m_percentButton = new QRadioButton(tr("проценты"), this);
    m_pixelButton = new QRadioButton(tr("пиксели"), this);
    m_percentButton->setChecked(true);
    modeRow->addWidget(m_percentButton);
    modeRow->addWidget(m_pixelButton);
    modeRow->addStretch(1);
    resizeLayout->addLayout(modeRow);

    auto *sizeForm = new QFormLayout;
    m_horizontal = new QSpinBox(this);
    m_vertical = new QSpinBox(this);
    m_horizontal->setRange(1, 10000);
    m_vertical->setRange(1, 10000);
    m_horizontal->setValue(100);
    m_vertical->setValue(100);
    sizeForm->addRow(tr("По горизонтали:"), m_horizontal);
    sizeForm->addRow(tr("По вертикали:"), m_vertical);
    resizeLayout->addLayout(sizeForm);

    m_keepAspect = new QCheckBox(tr("Сохранить пропорции"), this);
    m_keepAspect->setChecked(true);
    resizeLayout->addWidget(m_keepAspect);

    root->addWidget(resizeBox);

    // --- наклон ---
    auto *skewBox = new QGroupBox(tr("Наклон (градусы)"), this);
    auto *skewForm = new QFormLayout(skewBox);
    m_skewH = new QSpinBox(this);
    m_skewV = new QSpinBox(this);
    m_skewH->setRange(-89, 89);
    m_skewV->setRange(-89, 89);
    skewForm->addRow(tr("По горизонтали:"), m_skewH);
    skewForm->addRow(tr("По вертикали:"), m_skewV);
    root->addWidget(skewBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_percentButton, &QRadioButton::toggled, this, &ResizeSkewDialog::onPercentToggled);
    connect(m_horizontal, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ResizeSkewDialog::onHorizontalChanged);
    connect(m_vertical, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ResizeSkewDialog::onVerticalChanged);
}

void ResizeSkewDialog::onPercentToggled(bool percent)
{
    m_updating = true;
    if (percent) {
        m_horizontal->setRange(1, 1000);
        m_vertical->setRange(1, 1000);
        m_horizontal->setValue(100);
        m_vertical->setValue(100);
    } else {
        m_horizontal->setRange(1, 20000);
        m_vertical->setRange(1, 20000);
        m_horizontal->setValue(m_original.width());
        m_vertical->setValue(m_original.height());
    }
    m_updating = false;
}

void ResizeSkewDialog::onHorizontalChanged()
{
    if (m_updating || !m_keepAspect->isChecked())
        return;
    m_updating = true;
    if (resizeByPercent()) {
        m_vertical->setValue(m_horizontal->value());
    } else {
        const double ratio = double(m_original.height()) / qMax(1, m_original.width());
        m_vertical->setValue(qMax(1, qRound(m_horizontal->value() * ratio)));
    }
    m_updating = false;
}

void ResizeSkewDialog::onVerticalChanged()
{
    if (m_updating || !m_keepAspect->isChecked())
        return;
    m_updating = true;
    if (resizeByPercent()) {
        m_horizontal->setValue(m_vertical->value());
    } else {
        const double ratio = double(m_original.width()) / qMax(1, m_original.height());
        m_horizontal->setValue(qMax(1, qRound(m_vertical->value() * ratio)));
    }
    m_updating = false;
}

bool ResizeSkewDialog::resizeByPercent() const
{
    return m_percentButton->isChecked();
}

bool ResizeSkewDialog::keepAspectRatio() const
{
    return m_keepAspect->isChecked();
}

QSize ResizeSkewDialog::resultSize() const
{
    if (resizeByPercent()) {
        return QSize(qMax(1, qRound(m_original.width() * m_horizontal->value() / 100.0)),
                     qMax(1, qRound(m_original.height() * m_vertical->value() / 100.0)));
    }
    return QSize(m_horizontal->value(), m_vertical->value());
}

double ResizeSkewDialog::skewHorizontal() const
{
    return m_skewH->value();
}

double ResizeSkewDialog::skewVertical() const
{
    return m_skewV->value();
}

// --- AttributesDialog ----------------------------------------------------

AttributesDialog::AttributesDialog(const QSize &current, const QString &fileInfo,
                                   QWidget *parent)
    : QDialog(parent)
    , m_original(current)
{
    setWindowTitle(tr("Свойства изображения"));
    setModal(true);

    auto *root = new QVBoxLayout(this);

    if (!fileInfo.isEmpty()) {
        auto *info = new QLabel(fileInfo, this);
        info->setTextFormat(Qt::PlainText);
        root->addWidget(info);
    }

    auto *form = new QFormLayout;
    m_width = new QDoubleSpinBox(this);
    m_height = new QDoubleSpinBox(this);
    m_width->setRange(1, 20000);
    m_height->setRange(1, 20000);
    m_width->setDecimals(0);
    m_height->setDecimals(0);
    m_width->setValue(current.width());
    m_height->setValue(current.height());

    m_units = new QComboBox(this);
    m_units->addItems({tr("Пиксели"), tr("Сантиметры"), tr("Дюймы")});

    form->addRow(tr("Ширина:"), m_width);
    form->addRow(tr("Высота:"), m_height);
    form->addRow(tr("Единицы:"), m_units);
    root->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_units, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AttributesDialog::onUnitChanged);
}

void AttributesDialog::onUnitChanged(int index)
{
    // Пересчитываем текущие значения в новые единицы, а не сбрасываем их.
    const double pixelsW = m_unitIndex == 0 ? m_width->value()
                          : m_unitIndex == 1 ? m_width->value() * kDpi / 2.54
                                             : m_width->value() * kDpi;
    const double pixelsH = m_unitIndex == 0 ? m_height->value()
                          : m_unitIndex == 1 ? m_height->value() * kDpi / 2.54
                                             : m_height->value() * kDpi;

    m_unitIndex = index;
    if (index == 0) {
        m_width->setDecimals(0);
        m_height->setDecimals(0);
        m_width->setValue(qRound(pixelsW));
        m_height->setValue(qRound(pixelsH));
    } else {
        m_width->setDecimals(2);
        m_height->setDecimals(2);
        const double factor = (index == 1) ? (2.54 / kDpi) : (1.0 / kDpi);
        m_width->setValue(pixelsW * factor);
        m_height->setValue(pixelsH * factor);
    }
}

QSize AttributesDialog::resultSize() const
{
    switch (m_unitIndex) {
    case 1:
        return QSize(qMax(1, qRound(m_width->value() * kDpi / 2.54)),
                     qMax(1, qRound(m_height->value() * kDpi / 2.54)));
    case 2:
        return QSize(qMax(1, qRound(m_width->value() * kDpi)),
                     qMax(1, qRound(m_height->value() * kDpi)));
    default:
        return QSize(qMax(1, int(m_width->value())), qMax(1, int(m_height->value())));
    }
}

// --- настройки -----------------------------------------------------------

SettingsDialog::SettingsDialog(const AppSettings &current, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Параметры"));
    setModal(true);

    auto *root = new QVBoxLayout(this);

    // --- оформление ---
    auto *appearance = new QGroupBox(tr("Оформление"), this);
    auto *appearanceForm = new QFormLayout(appearance);
    m_theme = new QComboBox(this);
    const Theme::Mode modes[] = {Theme::Mode::System, Theme::Mode::Light, Theme::Mode::Dark};
    for (Theme::Mode mode : modes)
        m_theme->addItem(Theme::modeName(mode), int(mode));
    m_theme->setCurrentIndex(m_theme->findData(int(current.theme)));
    appearanceForm->addRow(tr("Тема:"), m_theme);
    root->addWidget(appearance);

    // --- новый рисунок ---
    auto *canvas = new QGroupBox(tr("Новый рисунок"), this);
    auto *canvasForm = new QFormLayout(canvas);
    m_width = new QSpinBox(this);
    m_height = new QSpinBox(this);
    m_width->setRange(1, 20000);
    m_height->setRange(1, 20000);
    m_width->setSuffix(tr(" пикс."));
    m_height->setSuffix(tr(" пикс."));
    m_width->setValue(current.defaultCanvas.width());
    m_height->setValue(current.defaultCanvas.height());
    canvasForm->addRow(tr("Ширина:"), m_width);
    canvasForm->addRow(tr("Высота:"), m_height);
    root->addWidget(canvas);

    // --- рисование и вид ---
    auto *view = new QGroupBox(tr("Рисование и вид"), this);
    auto *viewLayout = new QVBoxLayout(view);
    m_antialias = new QCheckBox(tr("Сглаживание линий"), this);
    m_antialias->setChecked(current.antialias);
    m_antialias->setToolTip(tr("Смягчает края линий и фигур.\n"
                               "Карандаш рисует резко в любом случае."));
    m_rulers = new QCheckBox(tr("Показывать линейки"), this);
    m_rulers->setChecked(current.rulers);
    m_grid = new QCheckBox(tr("Показывать линии сетки"), this);
    m_grid->setChecked(current.grid);
    m_statusBar = new QCheckBox(tr("Показывать строку состояния"), this);
    m_statusBar->setChecked(current.statusBar);
    viewLayout->addWidget(m_antialias);
    viewLayout->addWidget(m_rulers);
    viewLayout->addWidget(m_grid);
    viewLayout->addWidget(m_statusBar);
    root->addWidget(view);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AppSettings SettingsDialog::result() const
{
    AppSettings out;
    out.theme = Theme::Mode(m_theme->currentData().toInt());
    out.defaultCanvas = QSize(m_width->value(), m_height->value());
    out.antialias = m_antialias->isChecked();
    out.rulers = m_rulers->isChecked();
    out.grid = m_grid->isChecked();
    out.statusBar = m_statusBar->isChecked();
    return out;
}

// --- о программе ---------------------------------------------------------

namespace {

// Адрес репозитория. Подставить, когда проект будет опубликован, —
// больше эту ссылку нигде менять не нужно.
const char *kGithubUrl = "https://github.com/";
const char *kSiteUrl = "https://inomotion.pages.dev";

} // namespace

void showAboutDialog(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("About LinuxPaint"));
    dialog.setModal(true);

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(20, 18, 20, 14);
    root->setSpacing(16);

    auto *body = new QHBoxLayout;
    body->setSpacing(24);

    // Слева — текст, справа — логотип.
    auto *text = new QLabel(&dialog);
    text->setTextFormat(Qt::RichText);
    text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    text->setOpenExternalLinks(true);
    text->setWordWrap(true);
    text->setMinimumWidth(360);
    text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    text->setText(QStringLiteral(
                      "<h2 style='margin:0 0 12px 0;'>About LinuxPaint</h2>"
                      "<p style='margin:0 0 12px 0;'>"
                      "LinuxPaint; v.1.0 for Linux<br>"
                      "Officially InoMotion software"
                      "</p>"
                      "<p style='margin:0 0 12px 0;'>"
                      "Developer: Konstantin &quot;0corteZz&quot; Gorbunov<br>"
                      "Inomotion site: <a href=\"%1\">inomotion.pages.dev</a>"
                      "</p>"
                      "<p style='margin:0 0 12px 0;'>"
                      "&copy; 2026 InoMotion. All rights reserved."
                      "</p>"
                      "<p style='margin:0;'>"
                      "To learn how you can contribute to InoMotion LinuxPaint, "
                      "check out the project on <a href=\"%2\">GitHub</a>."
                      "</p>")
                      .arg(QString::fromLatin1(kSiteUrl),
                           QString::fromLatin1(kGithubUrl)));

    auto *logo = new QLabel(&dialog);
    logo->setPixmap(Icons::application().pixmap(128, 128));
    logo->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    logo->setFixedWidth(136);

    body->addWidget(text, 1);
    body->addWidget(logo, 0, Qt::AlignTop);
    root->addLayout(body);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    dialog.exec();
}
