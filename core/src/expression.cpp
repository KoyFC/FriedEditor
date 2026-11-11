#include "fried/editor/expression.h"

#include <charconv>
#include <cmath>

namespace fried::editor
{
    namespace
    {
        class Parser
        {
        public:
            explicit Parser(const std::string &text)
                : m_text(text)
                , m_position(0)
                , m_isValid(true)
            {
            }

            std::optional<double> parse()
            {
                double value = sum();
                skipSpaces();
                if (!m_isValid || m_position != m_text.size() || !std::isfinite(value))
                {
                    return std::nullopt;
                }
                return value;
            }

        private:
            void skipSpaces()
            {
                while (m_position < m_text.size() && (m_text[m_position] == ' ' || m_text[m_position] == '\t'))
                {
                    ++m_position;
                }
            }

            bool take(char symbol)
            {
                skipSpaces();
                if (m_position < m_text.size() && m_text[m_position] == symbol)
                {
                    ++m_position;
                    return true;
                }
                return false;
            }

            double sum()
            {
                double value = product();
                while (m_isValid)
                {
                    if (take('+'))
                    {
                        value += product();
                    }
                    else if (take('-'))
                    {
                        value -= product();
                    }
                    else
                    {
                        break;
                    }
                }
                return value;
            }

            double product()
            {
                double value = unary();
                while (m_isValid)
                {
                    if (take('*'))
                    {
                        value *= unary();
                    }
                    else if (take('/'))
                    {
                        double divisor = unary();
                        if (divisor == 0.0)
                        {
                            m_isValid = false;
                            return 0.0;
                        }
                        value /= divisor;
                    }
                    else
                    {
                        break;
                    }
                }
                return value;
            }

            double unary()
            {
                if (take('-'))
                {
                    return -unary();
                }
                if (take('+'))
                {
                    return unary();
                }
                return primary();
            }

            double primary()
            {
                if (take('('))
                {
                    double value = sum();
                    if (!take(')'))
                    {
                        m_isValid = false;
                    }
                    return value;
                }
                skipSpaces();
                const char *begin = m_text.data() + m_position;
                const char *end = m_text.data() + m_text.size();
                double value = 0.0;
                std::from_chars_result result = std::from_chars(begin, end, value, std::chars_format::general);
                if (result.ec != std::errc())
                {
                    m_isValid = false;
                    return 0.0;
                }
                m_position += (size_t)(result.ptr - begin);
                return value;
            }

            const std::string &m_text;
            size_t m_position;
            bool m_isValid;
        };
    }

    std::optional<double> evaluateExpression(const std::string &text)
    {
        return Parser(text).parse();
    }
}
