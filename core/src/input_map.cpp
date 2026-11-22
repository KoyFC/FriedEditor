#include "fried/editor/input_map.h"

#include <algorithm>

using Json = nlohmann::json;

namespace fried::editor
{
    namespace
    {
        std::vector<InputName> listOf(const Json &value, const char *key)
        {
            std::vector<InputName> names;
            if (value.is_object() && value.contains(key) && value[key].is_array())
            {
                for (const Json &entry : value[key])
                {
                    if (entry.is_object() && entry.contains("name") && entry["name"].is_string())
                    {
                        names.push_back({entry["name"].get<std::string>(), entry.value("code", 0)});
                    }
                }
            }
            return names;
        }

        bool has(const std::vector<InputName> &names, const std::string &name)
        {
            return std::any_of(names.begin(), names.end(), [&](const InputName &entry) { return entry.m_name == name; });
        }

        // Says what is wrong with a source, or nothing.
        std::string sourceProblem(const Json &value, const InputNames &names)
        {
            if (!value.is_string())
            {
                return "a source is not text";
            }
            std::string text = value.get<std::string>();
            std::size_t colon = text.find(':');
            std::string kind = colon == std::string::npos ? std::string() : text.substr(0, colon);
            std::string name = colon == std::string::npos ? std::string() : text.substr(colon + 1);
            if (kind == "key")
            {
                return has(names.m_keys, name) ? std::string() : "\"" + name + "\" is not a key";
            }
            if (kind == "mouse")
            {
                return name == "ScrollX" || name == "ScrollY" || has(names.m_mouseButtons, name) ? std::string() : "\"" + name + "\" is not a mouse button";
            }
            if (kind == "pad")
            {
                return has(names.m_gamepadButtons, name) ? std::string() : "\"" + name + "\" is not a gamepad button";
            }
            if (kind == "padAxis")
            {
                return has(names.m_gamepadAxes, name) ? std::string() : "\"" + name + "\" is not a gamepad axis";
            }
            if (kind == "touch")
            {
                bool isNumber = !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c); });
                return isNumber ? std::string() : "a touch source is touch: and the number of the device";
            }
            return "\"" + text + "\" is written key:W, mouse:Left, pad:South, padAxis:LeftX or touch:0";
        }
    }

    InputNames InputNames::fromJson(const Json &value)
    {
        InputNames names;
        names.m_keys = listOf(value, "keys");
        names.m_mouseButtons = listOf(value, "mouseButtons");
        names.m_gamepadButtons = listOf(value, "gamepadButtons");
        names.m_gamepadAxes = listOf(value, "gamepadAxes");
        return names;
    }

    const std::vector<std::string> &inputActionTypes()
    {
        static const std::vector<std::string> types = {"button", "value", "vector2"};
        return types;
    }

    std::vector<std::string> bindingKindsFor(const std::string &type)
    {
        if (type == "value")
        {
            return {"single", "axis"};
        }
        if (type == "vector2")
        {
            return {"vector", "stick"};
        }
        return {"single"};
    }

    Json newBinding(const std::string &kind)
    {
        if (kind == "axis")
        {
            return {{"axis", Json::array({"key:Left", "key:Right"})}};
        }
        if (kind == "vector")
        {
            return {{"vector", Json::array({"key:Up", "key:Down", "key:Left", "key:Right"})}};
        }
        if (kind == "stick")
        {
            return {{"stick", Json::array({"LeftX", "LeftY"})}};
        }
        return {{"single", "key:Space"}};
    }

    int bindingPartCount(const std::string &kind)
    {
        return kind == "vector" ? 4 : kind == "axis" || kind == "stick" ? 2 : 1;
    }

    std::string bindingPartLabel(const std::string &kind, int part)
    {
        static const char *const vector[] = {"Up", "Down", "Left", "Right"};
        static const char *const axis[] = {"Negative", "Positive"};
        static const char *const stick[] = {"X axis", "Y axis"};
        if (kind == "vector" && part >= 0 && part < 4)
        {
            return vector[part];
        }
        if (kind == "axis" && part >= 0 && part < 2)
        {
            return axis[part];
        }
        if (kind == "stick" && part >= 0 && part < 2)
        {
            return stick[part];
        }
        return std::string();
    }

    std::string bindingKind(const Json &binding)
    {
        if (!binding.is_object())
        {
            return std::string();
        }
        for (const char *kind : {"single", "axis", "vector", "stick"})
        {
            if (binding.contains(kind))
            {
                return kind;
            }
        }
        return std::string();
    }

    std::string bindingPart(const Json &binding, int part)
    {
        std::string kind = bindingKind(binding);
        if (kind == "single")
        {
            return binding["single"].is_string() ? binding["single"].get<std::string>() : std::string();
        }
        if (!kind.empty() && binding[kind].is_array() && part >= 0 && part < (int)binding[kind].size() && binding[kind][part].is_string())
        {
            return binding[kind][part].get<std::string>();
        }
        return std::string();
    }

    void setBindingPart(Json *binding, int part, const std::string &text)
    {
        std::string kind = bindingKind(*binding);
        if (kind == "single")
        {
            (*binding)["single"] = text;
        }
        else if (!kind.empty() && (*binding)[kind].is_array() && part >= 0 && part < (int)(*binding)[kind].size())
        {
            (*binding)[kind][part] = text;
        }
    }

    std::string sourceLabel(const std::string &text)
    {
        std::size_t colon = text.find(':');
        if (colon == std::string::npos)
        {
            // A stick axis has no kind in front of it.
            return text;
        }
        std::string kind = text.substr(0, colon);
        std::string name = text.substr(colon + 1);
        if (kind == "key")
        {
            return "Key " + name;
        }
        if (kind == "mouse")
        {
            return name.compare(0, 6, "Scroll") == 0 ? "Mouse wheel " + name.substr(6) : "Mouse " + name;
        }
        if (kind == "pad")
        {
            return "Pad " + name;
        }
        if (kind == "padAxis")
        {
            return "Pad axis " + name;
        }
        if (kind == "touch")
        {
            return "Touch " + name;
        }
        return text;
    }

    std::string sourceText(const std::string &kind, const std::string &name)
    {
        return kind.empty() ? name : kind + ":" + name;
    }

    std::string uniqueInputName(const std::vector<std::string> &taken, const std::string &base)
    {
        auto isTaken = [&](const std::string &name) { return std::find(taken.begin(), taken.end(), name) != taken.end(); };
        if (!isTaken(base))
        {
            return base;
        }
        for (int number = 2;; ++number)
        {
            std::string name = base + std::to_string(number);
            if (!isTaken(name))
            {
                return name;
            }
        }
    }

    std::string keyNameOf(const InputNames &names, int code)
    {
        for (const InputName &entry : names.m_keys)
        {
            if (entry.m_code == code)
            {
                return entry.m_name;
            }
        }
        return std::string();
    }

    std::vector<std::string> checkInputBlock(const Json &block, const InputNames &names)
    {
        std::vector<std::string> problems;
        if (!block.is_object() || !block.contains("maps"))
        {
            return problems;
        }
        if (!block["maps"].is_array())
        {
            problems.push_back("The maps of the input are not a list.");
            return problems;
        }
        std::vector<std::string> mapNames;
        for (const Json &map : block["maps"])
        {
            std::string mapName = map.is_object() && map.contains("name") && map["name"].is_string() ? map["name"].get<std::string>() : std::string();
            if (mapName.empty())
            {
                problems.push_back("An input map has no name.");
                continue;
            }
            if (std::find(mapNames.begin(), mapNames.end(), mapName) != mapNames.end())
            {
                problems.push_back("There are two input maps named " + mapName + ".");
            }
            mapNames.push_back(mapName);
            std::vector<std::string> actionNames;
            for (const Json &action : map.value("actions", Json::array()))
            {
                std::string name = action.is_object() && action.contains("name") && action["name"].is_string() ? action["name"].get<std::string>() : std::string();
                if (name.empty())
                {
                    problems.push_back("An action of " + mapName + " has no name.");
                    continue;
                }
                std::string where = mapName + "/" + name;
                if (std::find(actionNames.begin(), actionNames.end(), name) != actionNames.end())
                {
                    problems.push_back("The map " + mapName + " has two actions named " + name + ".");
                }
                actionNames.push_back(name);
                std::string type = action.value("type", "button");
                std::vector<std::string> kinds = bindingKindsFor(type);
                if (std::find(inputActionTypes().begin(), inputActionTypes().end(), type) == inputActionTypes().end())
                {
                    problems.push_back("The action " + where + " has the type " + type + ", which is not button, value or vector2.");
                    continue;
                }
                for (const Json &binding : action.value("bindings", Json::array()))
                {
                    std::string kind = bindingKind(binding);
                    if (kind.empty() || binding.size() != 1)
                    {
                        problems.push_back("The action " + where + " has a binding that is not exactly one of single, axis, vector or stick.");
                        continue;
                    }
                    if (std::find(kinds.begin(), kinds.end(), kind) == kinds.end())
                    {
                        problems.push_back("The action " + where + " is a " + type + ", which does not take a " + kind + " binding.");
                        continue;
                    }
                    for (int part = 0; part < bindingPartCount(kind); ++part)
                    {
                        std::string text = bindingPart(binding, part);
                        std::string problem = text.empty() ? "a source is missing" : kind == "stick" ? (has(names.m_gamepadAxes, text) ? std::string() : "\"" + text + "\" is not a gamepad axis") : sourceProblem(text, names);
                        if (!problem.empty())
                        {
                            problems.push_back("The action " + where + ": " + problem + ".");
                        }
                    }
                }
            }
        }
        return problems;
    }
}
