#pragma once

#include "fried/editor/editor.h"

#include <QListWidget>
#include <QWidget>

class HierarchyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit HierarchyPanel(fried::editor::Editor &editor, QWidget *parent = nullptr);

    void refresh();

private:
    fried::editor::Editor &m_editor;
    QListWidget *m_list;
    bool m_isRefreshing = false;
};
