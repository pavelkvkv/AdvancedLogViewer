#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("AdvancedLogViewer2"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("ALV"));

    QWidget window;
    window.setWindowTitle(QStringLiteral("Advanced Log Viewer 2"));
    window.resize(800, 600);
    window.show();

    return app.exec();
}
