#include "inspector_panel.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

using fried::editor::Editor;
using fried::editor::FieldView;
using Json = nlohmann::json;

namespace
{
    QString textOf(const Json &value)
    {
        return QString::fromStdString(value.is_string() ? value.get<std::string>() : std::string());
    }

    QColor colorOf(const Json &value)
    {
        if (!value.is_object())
        {
            return QColor(0, 0, 0);
        }
        return QColor(value.value("r", 0), value.value("g", 0), value.value("b", 0), value.value("a", 255));
    }

    // "game/sprite.png", or "engine/font.ttf@16" for a font.
    QString referenceText(const Json &value, bool isFont)
    {
        if (!value.is_object())
        {
            return QString();
        }
        QString text = textOf(value.value("root", Json())) + "/" + textOf(value.value("path", Json()));
        return isFont ? text + "@" + QString::number(value.value("size", 16)) : text;
    }

    std::optional<Json> referenceFrom(const QString &text, bool isFont)
    {
        if (text.trimmed().isEmpty())
        {
            return Json();
        }
        int slash = text.indexOf('/');
        if (slash <= 0)
        {
            return std::nullopt;
        }
        QString path = text.mid(slash + 1);
        Json reference = {{"root", text.left(slash).toStdString()}};
        if (isFont)
        {
            int at = path.lastIndexOf('@');
            bool isNumber = false;
            int size = at >= 0 ? path.mid(at + 1).toInt(&isNumber) : 16;
            if (at >= 0 && isNumber)
            {
                path = path.left(at);
            }
            reference["size"] = at >= 0 && isNumber ? size : 16;
        }
        reference["path"] = path.toStdString();
        return reference;
    }

    QString rectText(const Json &value)
    {
        if (!value.is_object())
        {
            return QString();
        }
        return QString("%1, %2, %3, %4").arg(value.value("x", 0)).arg(value.value("y", 0)).arg(value.value("width", 0)).arg(value.value("height", 0));
    }
}

InspectorPanel::InspectorPanel(Editor &editor, QWidget *parent)
    : QScrollArea(parent)
    , m_editor(editor)
{
    setWidgetResizable(true);
    setMinimumWidth(280);
}

void InspectorPanel::refresh()
{
    QString signature = QString::fromStdString(m_editor.selected().value_or(""));
    for (const auto &view : m_editor.inspector())
    {
        signature += "|" + QString::fromStdString(view.m_type);
    }
    if (signature != m_signature || !widget())
    {
        m_signature = signature;
        rebuild();
    }
    else
    {
        for (const auto &update : m_updaters)
        {
            update();
        }
    }
}

void InspectorPanel::rebuild()
{
    m_updaters.clear();
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);

    if (!m_editor.selected())
    {
        layout->addWidget(new QLabel(tr("Nothing selected")));
        layout->addStretch();
        setWidget(content);
        return;
    }

    auto *object = new QFormLayout;
    auto *name = new QLineEdit(content);
    name->setText(QString::fromStdString(m_editor.selectedObjectField("name").value_or(Json(*m_editor.selected())).get<std::string>()));
    connect(name, &QLineEdit::editingFinished, this, [this, name] { m_editor.renameSelected(name->text().toStdString()); });
    object->addRow(tr("Name"), name);
    m_updaters.push_back([this, name] {
        if (!name->hasFocus())
        {
            name->setText(QString::fromStdString(m_editor.selectedObjectField("name").value_or(Json(*m_editor.selected())).get<std::string>()));
        }
    });

    auto *priority = new QSpinBox(content);
    priority->setRange(-100000, 100000);
    auto priorityValue = [this] { return m_editor.selectedObjectField("priority").value_or(Json(0)).get<int>(); };
    priority->setValue(priorityValue());
    connect(priority, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) { m_editor.setObjectField("priority", Json(value)); });
    object->addRow(tr("Priority"), priority);
    m_updaters.push_back([priority, priorityValue] {
        if (!priority->hasFocus())
        {
            QSignalBlocker blocker(priority);
            priority->setValue(priorityValue());
        }
    });

    auto *active = new QCheckBox(content);
    auto activeValue = [this] { return m_editor.selectedObjectField("active").value_or(Json(true)).get<bool>(); };
    active->setChecked(activeValue());
    connect(active, &QCheckBox::toggled, this, [this](bool value) { m_editor.setObjectField("active", Json(value)); });
    object->addRow(tr("Active"), active);
    layout->addLayout(object);

    for (const auto &view : m_editor.inspector())
    {
        auto *box = new QGroupBox(QString::fromStdString(view.m_type), content);
        auto *form = new QFormLayout(box);
        for (const FieldView &field : view.m_fields)
        {
            form->addRow(QString::fromStdString(field.m_schema.m_name), fieldWidget(view.m_type, field));
        }
        if (view.m_isRemovable)
        {
            auto *remove = new QPushButton(tr("Remove component"), box);
            std::string type = view.m_type;
            connect(remove, &QPushButton::clicked, this, [this, type] { m_editor.removeComponent(type); });
            form->addRow(remove);
        }
        layout->addWidget(box);
    }

    auto *add = new QToolButton(content);
    add->setText(tr("Add component"));
    add->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(add);
    for (const std::string &type : m_editor.addableComponents())
    {
        menu->addAction(QString::fromStdString(type), [this, type] { m_editor.addComponent(type); });
    }
    add->setMenu(menu);
    add->setEnabled(!menu->isEmpty());
    layout->addWidget(add);
    layout->addStretch();
    setWidget(content);
}

QWidget *InspectorPanel::fieldWidget(const std::string &type, const FieldView &field)
{
    const std::string name = field.m_schema.m_name;
    const std::string kind = field.m_schema.m_kind;
    auto current = [this, type, name]() -> Json {
        for (const auto &view : m_editor.inspector())
        {
            if (view.m_type == type)
            {
                for (const FieldView &candidate : view.m_fields)
                {
                    if (candidate.m_schema.m_name == name)
                    {
                        return candidate.m_value;
                    }
                }
            }
        }
        return Json();
    };
    auto set = [this, type, name](const Json &value) { m_editor.setField(type, name, value); };

    if (kind == "int")
    {
        auto *spin = new QSpinBox;
        spin->setRange(-1000000000, 1000000000);
        spin->setValue(field.m_value.is_number() ? field.m_value.get<int>() : 0);
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [set](int value) { set(Json(value)); });
        m_updaters.push_back([spin, current] {
            if (!spin->hasFocus())
            {
                QSignalBlocker blocker(spin);
                spin->setValue(current().is_number() ? current().get<int>() : 0);
            }
        });
        return spin;
    }
    if (kind == "float")
    {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(-1e9, 1e9);
        spin->setDecimals(3);
        spin->setValue(field.m_value.is_number() ? field.m_value.get<double>() : 0.0);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [set](double value) { set(Json(value)); });
        m_updaters.push_back([spin, current] {
            if (!spin->hasFocus())
            {
                QSignalBlocker blocker(spin);
                spin->setValue(current().is_number() ? current().get<double>() : 0.0);
            }
        });
        return spin;
    }
    if (kind == "bool")
    {
        auto *check = new QCheckBox;
        check->setChecked(field.m_value.is_boolean() && field.m_value.get<bool>());
        connect(check, &QCheckBox::toggled, this, [set](bool value) { set(Json(value)); });
        m_updaters.push_back([check, current] {
            QSignalBlocker blocker(check);
            check->setChecked(current().is_boolean() && current().get<bool>());
        });
        return check;
    }
    if (kind == "color")
    {
        auto *button = new QPushButton;
        auto paint = [button](const Json &value) {
            QColor color = colorOf(value);
            button->setText(color.name(QColor::HexArgb));
            button->setStyleSheet(QString("background-color: %1; color: %2;").arg(color.name(), color.lightness() > 128 ? "black" : "white"));
        };
        paint(field.m_value);
        connect(button, &QPushButton::clicked, this, [this, button, current, set, paint] {
            QColor chosen = QColorDialog::getColor(colorOf(current()), button, QString(), QColorDialog::ShowAlphaChannel);
            if (chosen.isValid())
            {
                Json value = {{"r", chosen.red()}, {"g", chosen.green()}, {"b", chosen.blue()}, {"a", chosen.alpha()}};
                set(value);
                paint(value);
            }
        });
        m_updaters.push_back([current, paint] { paint(current()); });
        return button;
    }
    if (kind == "flip")
    {
        auto *combo = new QComboBox;
        for (const std::string &value : field.m_schema.m_values)
        {
            combo->addItem(QString::fromStdString(value));
        }
        combo->setCurrentText(textOf(field.m_value));
        connect(combo, &QComboBox::activated, this, [combo, set] { set(Json(combo->currentText().toStdString())); });
        m_updaters.push_back([combo, current] {
            QSignalBlocker blocker(combo);
            combo->setCurrentText(textOf(current()));
        });
        return combo;
    }

    // Text, rectangles and asset references are typed, and applied when the line is finished.
    auto *edit = new QLineEdit;
    bool isFont = kind == "font";
    auto show = [kind, isFont](const Json &value) {
        if (kind == "string")
        {
            return textOf(value);
        }
        if (kind == "rect")
        {
            return rectText(value);
        }
        return referenceText(value, isFont);
    };
    edit->setText(show(field.m_value));
    if (kind == "rect")
    {
        edit->setPlaceholderText(tr("x, y, width, height (empty for the whole image)"));
    }
    else if (kind != "string")
    {
        edit->setPlaceholderText(isFont ? tr("engine/NunitoSans.ttf@16") : tr("game/sprite.png"));
    }
    connect(edit, &QLineEdit::editingFinished, this, [this, edit, kind, isFont, set, current, show] {
        if (kind == "string")
        {
            set(Json(edit->text().toStdString()));
        }
        else if (kind == "rect")
        {
            QStringList parts = edit->text().split(',', Qt::SkipEmptyParts);
            if (parts.isEmpty())
            {
                set(Json());
            }
            else if (parts.size() == 4)
            {
                set(Json{{"x", parts[0].trimmed().toInt()}, {"y", parts[1].trimmed().toInt()}, {"width", parts[2].trimmed().toInt()}, {"height", parts[3].trimmed().toInt()}});
            }
            else
            {
                edit->setText(show(current()));
            }
        }
        else if (auto reference = referenceFrom(edit->text(), isFont))
        {
            set(*reference);
        }
        else
        {
            edit->setText(show(current()));
        }
    });
    m_updaters.push_back([edit, current, show] {
        if (!edit->hasFocus())
        {
            edit->setText(show(current()));
        }
    });
    return edit;
}
