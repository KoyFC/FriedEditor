#include "asset_panel.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QMimeData>
#include <QPixmap>
#include <QVBoxLayout>

using fried::editor::AssetKind;
using fried::editor::Editor;

namespace
{
    // Hands the reference of what is being dragged to whatever it is dropped on.
    class AssetList : public QListWidget
    {
    public:
        using QListWidget::QListWidget;

    protected:
        QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
        {
            auto *data = new QMimeData;
            if (!items.isEmpty())
            {
                data->setData(assetMimeType(), items.first()->data(Qt::UserRole).toString().toUtf8());
            }
            return data;
        }
    };
}

AssetPanel::AssetPanel(Editor &editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
    , m_filter(new QComboBox(this))
{
    auto *list = new AssetList(this);
    m_list = list;
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(48, 48));
    m_list->setGridSize(QSize(110, 84));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setWordWrap(true);
    m_list->setDragEnabled(true);
    m_list->setDefaultDropAction(Qt::CopyAction);

    m_filter->addItems({tr("All"), tr("Images"), tr("Fonts"), tr("Audio"), tr("Scenes")});
    connect(m_filter, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { refresh(); });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    auto *top = new QHBoxLayout;
    top->addWidget(m_filter);
    top->addStretch();
    layout->addLayout(top);
    layout->addWidget(m_list);

    // A double click does what a drop in the middle of the view does.
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        std::string root, path;
        if (fried::editor::parseAssetReference(item->data(Qt::UserRole).toString().toStdString(), &root, &path))
        {
            m_editor.dropAsset(root, path, m_editor.viewport().frameWidth() / 2.0, m_editor.viewport().frameHeight() / 2.0);
        }
    });
}

void AssetPanel::refresh()
{
    static const AssetKind kinds[] = {AssetKind::Other, AssetKind::Image, AssetKind::Font, AssetKind::Audio, AssetKind::Scene};
    int filter = m_filter->currentIndex();

    std::vector<fried::editor::AssetEntry> entries = m_editor.assets();
    QString signature = QString::number(filter);
    for (const auto &entry : entries)
    {
        signature += "|" + QString::fromStdString(entry.reference());
    }
    if (signature == m_signature)
    {
        return;
    }
    m_signature = signature;

    m_list->clear();
    for (const auto &entry : entries)
    {
        if (filter != 0 && entry.m_kind != kinds[filter])
        {
            continue;
        }
        auto *item = new QListWidgetItem(QFileInfo(QString::fromStdString(entry.m_path)).fileName(), m_list);
        item->setToolTip(QString::fromStdString(entry.reference()));
        item->setData(Qt::UserRole, QString::fromStdString(entry.reference()));
        if (entry.m_kind == AssetKind::Image)
        {
            item->setIcon(QIcon(QPixmap(QString::fromStdString(m_editor.assetFile(entry.m_root, entry.m_path))).scaled(48, 48, Qt::KeepAspectRatio)));
        }
    }
}
