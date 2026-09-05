#include "Icons.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("linux-paint"));
    app.setOrganizationName(QStringLiteral("linux-paint"));
    // Версия приходит из CMake — см. PAINT_VERSION в CMakeLists.txt.
    app.setApplicationVersion(QStringLiteral(PAINT_VERSION));
    app.setDesktopFileName(QStringLiteral("linux-paint"));
    // Значок задаём на уровне приложения: часть оболочек берёт его отсюда,
    // а не из отдельного окна.
    app.setWindowIcon(Icons::application());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Растровый редактор в духе Microsoft Paint"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("файл"),
                                 QStringLiteral("Изображение, которое нужно открыть"));
    parser.process(app);

    MainWindow window;

    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty())
        window.openFile(arguments.first());

    window.show();
    return app.exec();
}
