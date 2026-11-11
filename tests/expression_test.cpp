#include "doctest/doctest.h"

#include "fried/editor/expression.h"

using fried::editor::evaluateExpression;

TEST_CASE("expressions evaluate the four operations")
{
    CHECK(evaluateExpression("920*2").value() == doctest::Approx(1840.0));
    CHECK(evaluateExpression("10 - 4").value() == doctest::Approx(6.0));
    CHECK(evaluateExpression("7/2").value() == doctest::Approx(3.5));
    CHECK(evaluateExpression("1+2*3").value() == doctest::Approx(7.0));
    CHECK(evaluateExpression("(1+2)*3").value() == doctest::Approx(9.0));
    CHECK(evaluateExpression("-3*-2").value() == doctest::Approx(6.0));
    CHECK(evaluateExpression("1.5*4").value() == doctest::Approx(6.0));
    CHECK(evaluateExpression("42").value() == doctest::Approx(42.0));
}

TEST_CASE("expressions reject what is not a number")
{
    CHECK_FALSE(evaluateExpression("").has_value());
    CHECK_FALSE(evaluateExpression("abc").has_value());
    CHECK_FALSE(evaluateExpression("1/0").has_value());
    CHECK_FALSE(evaluateExpression("2*").has_value());
    CHECK_FALSE(evaluateExpression("(1+2").has_value());
    CHECK_FALSE(evaluateExpression("1 2").has_value());
    CHECK_FALSE(evaluateExpression("1e999").has_value());
}
