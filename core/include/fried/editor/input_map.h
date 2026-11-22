#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace fried::editor
{
    // The name of a key, a button or an axis in project.fried, and the number the engine gives it.
    struct InputName
    {
        std::string m_name;
        int m_code = 0;
    };

    // The names a source can be written with, which the engine reports with its schema so the editor never keeps a list of its own.
    struct InputNames
    {
        std::vector<InputName> m_keys;
        std::vector<InputName> m_mouseButtons;
        std::vector<InputName> m_gamepadButtons;
        std::vector<InputName> m_gamepadAxes;

        bool isEmpty() const { return m_keys.empty(); }
        static InputNames fromJson(const nlohmann::json &value);
    };

    // The input actions of a project are the `input` block of project.fried, edited as it is so what the editor does not know about stays. These are the rules the engine's InputMapParser enforces, so a block the editor accepts is a block the build accepts.

    // What is wrong with the block, one sentence for each problem that says where, and nothing when it is right.
    std::vector<std::string> checkInputBlock(const nlohmann::json &block, const InputNames &names);

    // The types an action has, and the kinds of binding each one takes.
    const std::vector<std::string> &inputActionTypes();
    std::vector<std::string> bindingKindsFor(const std::string &type);

    // A binding of a kind, with sources that are valid until the person picks others.
    nlohmann::json newBinding(const std::string &kind);

    // How many sources a binding of the kind has, and what each one is for ("Up", "Down", "Left", "Right" for a vector).
    int bindingPartCount(const std::string &kind);
    std::string bindingPartLabel(const std::string &kind, int part);

    // The kind of a binding object, empty when it is not one.
    std::string bindingKind(const nlohmann::json &binding);

    // One source of a binding, as the text the file has.
    std::string bindingPart(const nlohmann::json &binding, int part);
    void setBindingPart(nlohmann::json *binding, int part, const std::string &text);

    // A source as it is read on screen: "key:Space" is Key Space.
    std::string sourceLabel(const std::string &text);

    // A name that is not among the taken ones: "Action", then "Action2", "Action3".
    std::string uniqueInputName(const std::vector<std::string> &taken, const std::string &base);

    // The text of a source from a kind ("key", "mouse", "pad", "padAxis", "touch") and a name.
    std::string sourceText(const std::string &kind, const std::string &name);

    // The name of the key that has the number, which is how a key that was pressed becomes a source. Empty when the engine has no such key.
    std::string keyNameOf(const InputNames &names, int code);
}
