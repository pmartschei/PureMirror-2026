#include "pch.h"

#include "SemanticVersionRange.h"

namespace PureMirror::Overlay
{
    namespace
    {
        enum class Comparison
        {
            Equal,
            Greater,
            GreaterOrEqual,
            Less,
            LessOrEqual
        };

        bool Evaluate(const SemanticVersion& actual, const Comparison comparison, const SemanticVersion& expected)
        {
            switch (comparison)
            {
            case Comparison::Equal:
                return actual == expected;
            case Comparison::Greater:
                return actual > expected;
            case Comparison::GreaterOrEqual:
                return actual >= expected;
            case Comparison::Less:
                return actual < expected;
            case Comparison::LessOrEqual:
                return actual <= expected;
            }
            return false;
        }

        bool VisitComparisons(const std::string_view expression,
                              const std::function<bool(Comparison, const SemanticVersion&)>& visitor)
        {
            std::size_t position{};
            bool foundComparison = false;
            while (position < expression.size())
            {
                while (position < expression.size() && std::isspace(static_cast<unsigned char>(expression[position])))
                    ++position;
                if (position == expression.size())
                    break;

                Comparison comparison{Comparison::Equal};
                if (expression.substr(position, 2) == ">=")
                {
                    comparison = Comparison::GreaterOrEqual;
                    position += 2;
                }
                else if (expression.substr(position, 2) == "<=")
                {
                    comparison = Comparison::LessOrEqual;
                    position += 2;
                }
                else if (expression[position] == '>')
                {
                    comparison = Comparison::Greater;
                    ++position;
                }
                else if (expression[position] == '<')
                {
                    comparison = Comparison::Less;
                    ++position;
                }
                else if (expression[position] == '=')
                {
                    ++position;
                }

                const auto start = position;
                while (position < expression.size() && !std::isspace(static_cast<unsigned char>(expression[position])))
                    ++position;
                const auto expected = SemanticVersion::Parse(expression.substr(start, position - start));
                if (!expected || !visitor(comparison, *expected))
                    return false;
                foundComparison = true;
            }
            return foundComparison;
        }
    }  // namespace

    bool SemanticVersionRange::IsValid(const std::string_view expression)
    {
        return VisitComparisons(expression, [](Comparison, const SemanticVersion&) { return true; });
    }

    bool SemanticVersionRange::Contains(const std::string_view expression, const SemanticVersion& version)
    {
        return VisitComparisons(expression,
                                [&](const Comparison comparison, const SemanticVersion& expected)
                                { return Evaluate(version, comparison, expected); });
    }
}  // namespace PureMirror::Overlay
