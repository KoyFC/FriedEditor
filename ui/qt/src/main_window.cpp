#include "main_window.h"

#include "asset_panel.h"
#include "hierarchy_panel.h"
#include "inspector_panel.h"
#include "viewport_widget.h"

#include "fried/editor/version.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QLabel>
#include <QDockWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

using fried::editor::Editor;
using fried::editor::Platform;

MainWindow::MainWindow(Editor &editor, QWidget *parent)
    : QMainWindow(parent)
    , m_editor(editor)
    , m_viewport(new ViewportWidget(editor, this))
    , m_hierarchy(new HierarchyPanel(editor, this))
    , m_assets(new AssetPanel(editor, this))
    , m_inspector(new InspectorPanel(editor, this))
{
    resize(1400, 800);
    setCentralWidget(m_viewport);

    auto *hierarchyDock = new QDockWidget(tr("Hierarchy"), this);
    hierarchyDock->setWidget(m_hierarchy);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
    auto *inspectorDock = new QDockWidget(tr("Inspector"), this);
    inspectorDock->setWidget(m_inspector);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    auto *assetDock = new QDockWidget(tr("Assets"), this);
    assetDock->setWidget(m_assets);
    addDockWidget(Qt::BottomDockWidgetArea, assetDock);

    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *save = file->addAction(tr("&Save"), QKeySequence::Save, [this] { this->save(); });
    file->addAction(tr("&Rebuild player"), [this] { m_editor.rebuildPlayer(); });
    file->addSeparator();
    file->addAction(tr("&Quit"), QKeySequence::Quit, [this] { close(); });
    Q_UNUSED(save);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    m_undo = edit->addAction(tr("&Undo"), QKeySequence::Undo, [this] { m_editor.commands().undo(); });
    m_redo = edit->addAction(tr("&Redo"), QKeySequence::Redo, [this] { m_editor.commands().redo(); });

    QToolBar *toolbar = addToolBar(tr("Platform"));
    toolbar->addWidget(new QLabel(tr(" Platform: "), this));
    auto *platforms = new QComboBox(this);
    for (Platform platform : fried::editor::s_platforms)
    {
        platforms->addItem(QString::fromStdString(fried::editor::platformLabel(platform)), (int)platform);
    }
    connect(platforms, qOverload<int>(&QComboBox::activated), this, [this, platforms](int index) {
        m_editor.setPlatform((Platform)platforms->itemData(index).toInt());
    });
    toolbar->addWidget(platforms);

    QToolBar *tools = addToolBar(tr("Tools"));
    auto *modes = new QActionGroup(this);
    auto addMode = [&](const QString &label, const QKeySequence &key, fried::editor::GizmoMode mode) {
        QAction *action = tools->addAction(label);
        action->setCheckable(true);
        action->setShortcut(key);
        action->setChecked(m_editor.gizmoMode() == mode);
        modes->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] { m_editor.setGizmoMode(mode); });
    };
    addMode(tr("Move (W)"), Qt::Key_W, fried::editor::GizmoMode::Move);
    addMode(tr("Rotate (E)"), Qt::Key_E, fried::editor::GizmoMode::Rotate);
    addMode(tr("Scale (R)"), Qt::Key_R, fried::editor::GizmoMode::Scale);

    m_editor.m_onChanged = [this] { m_refresh.start(); };
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(0);
    connect(&m_refresh, &QTimer::timeout, this, [this] { refresh(); });
    connect(&m_pump, &QTimer::timeout, this, [this] { m_editor.pump(); });
    m_pump.start(16);
    refresh();
}

void MainWindow::refresh()
{
    m_viewport->refresh();
    m_hierarchy->refresh();
    m_inspector->refresh();
    m_assets->refresh();

    auto &commands = m_editor.commands();
    m_undo->setEnabled(commands.canUndo());
    m_undo->setText(commands.canUndo() ? tr("&Undo %1").arg(QString::fromStdString(commands.undoLabel())) : tr("&Undo"));
    m_redo->setEnabled(commands.canRedo());
    m_redo->setText(commands.canRedo() ? tr("&Redo %1").arg(QString::fromStdString(commands.redoLabel())) : tr("&Redo"));

    setWindowTitle(QStringLiteral("%1%2 - %3 - Fried Editor %4")
                       .arg(QString::fromStdString(m_editor.document().name()), commands.isClean() ? "" : "*",
                            QString::fromStdString(m_editor.project().name()), QString::fromStdString(fried::editor::version())));
    statusBar()->showMessage(QString::fromStdString(m_editor.message()));
}

void MainWindow::save()
{
    std::string error;
    if (!m_editor.save(&error))
    {
        QMessageBox::warning(this, tr("Save"), QString::fromStdString(error));
    }
}

bool MainWindow::confirmDiscard()
{
    if (m_editor.commands().isClean())
    {
        return true;
    }
    auto answer = QMessageBox::question(this, tr("Unsaved changes"), tr("Save the scene before closing?"),
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save)
    {
        save();
        return m_editor.commands().isClean();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmDiscard())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}
