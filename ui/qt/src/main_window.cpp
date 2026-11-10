#include "main_window.h"

#include "fried/editor/version.h"

#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Fried Editor %1").arg(QString::fromStdString(fried::editor::version())));
    resize(1280, 720);
}
