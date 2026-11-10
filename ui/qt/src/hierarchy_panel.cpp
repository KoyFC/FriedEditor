#include "hierarchy_panel.h"

#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

using fried::editor::Editor;

HierarchyPanel::HierarchyPanel(Editor &editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_list(new QListWidget(this))
{
    auto *buttons = new QHBoxLayout;

    auto *add = new QToolButton(this);
    add->setText(tr("Add"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(add);
    connect(menu, &QMenu::aboutToShow, this, [this, menu] {
        menu->clear();
        menu->addAction(tr("Empty object"), [this] { m_editor.addObject(""); });
        for (const std::string &type : m_editor.componentTypes())
        {
            if (type != "Transform")
            {
                menu->addAction(QString::fromStdString(type), [this, type] { m_editor.addObject(type); });
            }
        }
    });
    add->setMenu(menu);
    buttons->addWidget(add);

    auto addButton = [&](const QString &text, std::function<void()> action) {
        auto *button = new QToolButton(this);
        button->setText(text);
        connect(button, &QToolButton::clicked, this, [action] { action(); });
        buttons->addWidget(button);
    };
    addButton(tr("Delete"), [this] { m_editor.removeSelected(); });
    addButton(tr("Up"), [this] { m_editor.moveSelected(-1); });
    addButton(tr("Down"), [this] { m_editor.moveSelected(1); });
    addButton(tr("Camera"), [this] { m_editor.setCameraToSelected(); });
    buttons->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(buttons);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (!m_isRefreshing)
        {
            m_editor.select(item ? std::optional<std::string>(item->data(Qt::UserRole).toString().toStdString()) : std::nullopt);
        }
    });
}

void HierarchyPanel::refresh()
{
    m_isRefreshing = true;
    m_list->clear();
    for (const auto &row : m_editor.hierarchy())
    {
        QString label = QString::fromStdString(row.m_name);
        if (row.m_isCamera)
        {
            label += tr("  (camera)");
        }
        auto *item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, QString::fromStdString(row.m_id));
        if (m_editor.selected() && *m_editor.selected() == row.m_id)
        {
            m_list->setCurrentItem(item);
        }
    }
    m_isRefreshing = false;
}
