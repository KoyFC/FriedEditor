#pragma once

#include "fried/editor/editor.h"

#include <QComboBox>
#include <QMainWindow>
#include <QTimer>

class AssetPanel;
class HierarchyPanel;
class InspectorPanel;
class ViewportWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(fried::editor::Editor &editor, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void refresh();
    void save();
    bool confirmDiscard();

    fried::editor::Editor &m_editor;
    ViewportWidget *m_viewport;
    HierarchyPanel *m_hierarchy;
    AssetPanel *m_assets;
    InspectorPanel *m_inspector;
    QAction *m_undo;
    QAction *m_redo;
    QTimer m_pump;
    QTimer m_refresh;
};
