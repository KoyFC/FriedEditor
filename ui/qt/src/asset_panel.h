#pragma once

#include "fried/editor/editor.h"

#include <QComboBox>
#include <QListWidget>
#include <QWidget>

// The mime type an asset travels as when it is dragged out of the browser: "game/sprite.png".
inline const char *assetMimeType()
{
    return "application/x-fried-asset";
}

class AssetPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AssetPanel(fried::editor::Editor &editor, QWidget *parent = nullptr);

    void refresh();

private:
    fried::editor::Editor &m_editor;
    QListWidget *m_list;
    QComboBox *m_filter;
    QString m_signature;
};
