#pragma once

#include <optional>
#include <string>

namespace fried::editor
{
    // Evaluates a number typed in an inspector field: decimals, + - * /, parentheses and unary signs.
    // Returns nothing for malformed text, a division by zero or a result that is not finite.
    std::optional<double> evaluateExpression(const std::string &text);
}
