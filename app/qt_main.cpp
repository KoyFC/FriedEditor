#include "main_window.h"

#include "fried/editor/editor.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>

namespace
{
    bool hasPlayer(const QString &engine)
    {
        return QFileInfo::exists(engine + "/editor/player/build.hxml");
    }

    // A project made by the Project Manager has the engine as a submodule, and the engine's own sandbox sits inside the engine.
    QString findEngine(const QString &project)
    {
        for (const QString &candidate : {project + "/engine", QDir(project).absoluteFilePath("..")})
        {
            QString path = QDir::cleanPath(candidate);
            if (hasPlayer(path))
            {
                return path;
            }
        }
        return QString();
    }
}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName("Fried Editor");

    QCommandLineParser parser;
    parser.addPositionalArgument("project", "The project folder, the one with project.fried.");
    QCommandLineOption engineOption("engine", "The Fried Engine checkout. Found next to the project by default.", "path");
    parser.addOption(engineOption);
    parser.addHelpOption();
    parser.process(application);

    if (parser.positionalArguments().isEmpty())
    {
        QMessageBox::information(nullptr, "Fried Editor", "Start it with the project's folder: fried_editor <project> [--engine <path>]");
        return 1;
    }

    QString project = QDir(parser.positionalArguments().first()).absolutePath();
    QString engine = parser.isSet(engineOption) ? QDir(parser.value(engineOption)).absolutePath() : findEngine(project);
    if (engine.isEmpty() || !hasPlayer(engine))
    {
        QMessageBox::critical(nullptr, "Fried Editor", "No Fried Engine with an editor player was found. Pass its folder with --engine.");
        return 1;
    }

    fried::editor::Editor editor;
    std::string error;
    if (!editor.open(project.toStdString(), engine.toStdString(), &error))
    {
        QMessageBox::critical(nullptr, "Fried Editor", QString::fromStdString(error));
        return 1;
    }

    MainWindow window(editor);
    window.show();
    return application.exec();
}
