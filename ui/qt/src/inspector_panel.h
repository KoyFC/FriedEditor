#pragma once

#include "fried/editor/editor.h"

#include <QScrollArea>

#include <functional>
#include <vector>

// A form built from the player's schema: one widget per field kind, whatever the component.
class InspectorPanel : public QScrollArea
{
    Q_OBJECT

public:
    explicit InspectorPanel(fried::editor::Editor &editor, QWidget *parent = nullptr);

    void refresh();

private:
    QWidget *fieldWidget(const std::string &type, const fried::editor::FieldView &field);
    void rebuild();

    fried::editor::Editor &m_editor;
    QString m_signature;
    std::vector<std::function<void()>> m_updaters;
};
