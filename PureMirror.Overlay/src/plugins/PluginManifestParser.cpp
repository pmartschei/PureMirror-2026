#include "pch.h"

#include "PluginManifestParser.h"

#include "src/core/versions/SemanticVersion.h"
#include "src/core/versions/SemanticVersionRange.h"

#include <stdexcept>
#include <unordered_set>

namespace PureMirror::Overlay
{
    namespace
    {
        class JsonReader
        {
          public:
            explicit JsonReader(const std::string_view json) : m_Json(json) {}

            PluginManifest ReadManifest()
            {
                PluginManifest manifest;
                std::unordered_set<std::string> fields;
                Expect('{');
                SkipWhitespace();
                if (Consume('}'))
                    return manifest;

                while (true)
                {
                    const auto field = ReadString();
                    if (!fields.insert(field).second)
                        throw std::runtime_error("Duplicate field '" + field + "'.");
                    Expect(':');

                    if (field == "schemaVersion")
                        manifest.SchemaVersion = ReadUnsignedInteger();
                    else if (field == "id")
                        manifest.Id = ReadString();
                    else if (field == "name")
                        manifest.Name = ReadString();
                    else if (field == "version")
                        manifest.Version = ReadString();
                    else if (field == "apiVersion")
                        manifest.ApiVersion = ReadString();
                    else if (field == "entry")
                        manifest.Entry = ReadString();
                    else if (field == "exports")
                        manifest.Exports = ReadStringArray();
                    else if (field == "dependencies")
                        manifest.Dependencies = ReadDependencyObject();
                    else if (field == "optionalDependencies")
                        manifest.OptionalDependencies = ReadDependencyObject();
                    else if (field == "capabilities")
                        manifest.Capabilities = ReadStringArray();
                    else
                        SkipValue();

                    SkipWhitespace();
                    if (Consume('}'))
                        break;
                    Expect(',');
                    SkipWhitespace();
                    if (Peek() == '}')
                        throw std::runtime_error("Trailing commas are not valid JSON.");
                }

                SkipWhitespace();
                if (m_Position != m_Json.size())
                    throw std::runtime_error("Unexpected content after the root object.");
                return manifest;
            }

          private:
            char Peek() const
            {
                return m_Position < m_Json.size() ? m_Json[m_Position] : '\0';
            }

            void SkipWhitespace()
            {
                while (m_Position < m_Json.size() && (m_Json[m_Position] == ' ' || m_Json[m_Position] == '\t' ||
                                                      m_Json[m_Position] == '\r' || m_Json[m_Position] == '\n'))
                    ++m_Position;
            }

            bool Consume(const char expected)
            {
                SkipWhitespace();
                if (Peek() != expected)
                    return false;
                ++m_Position;
                return true;
            }

            void Expect(const char expected)
            {
                if (!Consume(expected))
                    throw std::runtime_error(std::string("Expected '") + expected + "'.");
            }

            std::string ReadString()
            {
                SkipWhitespace();
                if (Peek() != '"')
                    throw std::runtime_error("Expected a JSON string.");
                ++m_Position;

                std::string value;
                while (m_Position < m_Json.size())
                {
                    const auto character = m_Json[m_Position++];
                    if (character == '"')
                        return value;
                    if (static_cast<unsigned char>(character) < 0x20)
                        throw std::runtime_error("Unescaped control character in JSON string.");
                    if (character != '\\')
                    {
                        value.push_back(character);
                        continue;
                    }
                    if (m_Position >= m_Json.size())
                        break;

                    const auto escaped = m_Json[m_Position++];
                    switch (escaped)
                    {
                    case '"':
                    case '\\':
                    case '/':
                        value.push_back(escaped);
                        break;
                    case 'b':
                        value.push_back('\b');
                        break;
                    case 'f':
                        value.push_back('\f');
                        break;
                    case 'n':
                        value.push_back('\n');
                        break;
                    case 'r':
                        value.push_back('\r');
                        break;
                    case 't':
                        value.push_back('\t');
                        break;
                    default:
                        throw std::runtime_error("Unsupported escape sequence in JSON string.");
                    }
                }
                throw std::runtime_error("Unterminated JSON string.");
            }

            std::uint32_t ReadUnsignedInteger()
            {
                SkipWhitespace();
                const auto start = m_Position;
                while (m_Position < m_Json.size() && m_Json[m_Position] >= '0' && m_Json[m_Position] <= '9')
                    ++m_Position;
                if (start == m_Position)
                    throw std::runtime_error("Expected an unsigned integer.");

                std::uint64_t value{};
                for (auto index = start; index < m_Position; ++index)
                {
                    value = value * 10 + static_cast<std::uint64_t>(m_Json[index] - '0');
                    if (value > UINT32_MAX)
                        throw std::runtime_error("Integer exceeds the supported range.");
                }
                return static_cast<std::uint32_t>(value);
            }

            std::vector<std::string> ReadStringArray()
            {
                std::vector<std::string> values;
                Expect('[');
                SkipWhitespace();
                if (Consume(']'))
                    return values;
                while (true)
                {
                    values.push_back(ReadString());
                    SkipWhitespace();
                    if (Consume(']'))
                        return values;
                    Expect(',');
                    SkipWhitespace();
                    if (Peek() == ']')
                        throw std::runtime_error("Trailing commas are not valid JSON.");
                }
            }

            std::vector<PluginDependency> ReadDependencyObject()
            {
                std::vector<PluginDependency> dependencies;
                std::unordered_set<std::string> ids;
                Expect('{');
                SkipWhitespace();
                if (Consume('}'))
                    return dependencies;
                while (true)
                {
                    auto id = ReadString();
                    if (!ids.insert(id).second)
                        throw std::runtime_error("Duplicate dependency '" + id + "'.");
                    Expect(':');
                    dependencies.push_back({std::move(id), ReadString()});
                    SkipWhitespace();
                    if (Consume('}'))
                        return dependencies;
                    Expect(',');
                    SkipWhitespace();
                    if (Peek() == '}')
                        throw std::runtime_error("Trailing commas are not valid JSON.");
                }
            }

            void SkipValue()
            {
                SkipWhitespace();
                if (Peek() == '"')
                {
                    static_cast<void>(ReadString());
                    return;
                }
                if (Consume('{'))
                {
                    SkipWhitespace();
                    if (Consume('}'))
                        return;
                    while (true)
                    {
                        static_cast<void>(ReadString());
                        Expect(':');
                        SkipValue();
                        if (Consume('}'))
                            return;
                        Expect(',');
                    }
                }
                if (Consume('['))
                {
                    SkipWhitespace();
                    if (Consume(']'))
                        return;
                    while (true)
                    {
                        SkipValue();
                        if (Consume(']'))
                            return;
                        Expect(',');
                    }
                }

                const auto start = m_Position;
                while (m_Position < m_Json.size() &&
                       std::string_view{",]} \t\r\n"}.find(m_Json[m_Position]) == std::string_view::npos)
                    ++m_Position;
                if (start == m_Position)
                    throw std::runtime_error("Expected a JSON value.");
            }

            std::string_view m_Json;
            std::size_t m_Position{};
        };

        bool IsValidPluginId(const std::string_view id)
        {
            if (id.empty() || id.front() == '.' || id.back() == '.')
                return false;
            bool previousWasDot = false;
            for (const auto character : id)
            {
                const auto valid = (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
                                   character == '-' || character == '.';
                if (!valid || (character == '.' && previousWasDot))
                    return false;
                previousWasDot = character == '.';
            }
            return true;
        }

        bool IsSafeScriptPath(const std::string_view path)
        {
            if (path.empty() || path.front() == '/' || path.front() == '\\' ||
                path.find('\\') != std::string_view::npos || path.find(':') != std::string_view::npos ||
                !path.ends_with(".as"))
                return false;

            std::size_t start{};
            while (start <= path.size())
            {
                const auto end = path.find('/', start);
                const auto component =
                    path.substr(start, end == std::string_view::npos ? path.size() - start : end - start);
                if (component.empty() || component == "." || component == "..")
                    return false;
                if (end == std::string_view::npos)
                    break;
                start = end + 1;
            }
            return true;
        }

        void RequireString(const std::string_view field,
                           const std::string& value,
                           std::vector<PluginManifestError>& errors)
        {
            if (value.empty())
                errors.push_back({std::string(field), "Field is required and must not be empty."});
        }

        void ValidateUniqueValues(const std::string_view field,
                                  const std::vector<std::string>& values,
                                  std::vector<PluginManifestError>& errors)
        {
            std::unordered_set<std::string> uniqueValues;
            for (const auto& value : values)
                if (!uniqueValues.insert(value).second)
                    errors.push_back({std::string(field), "Duplicate value '" + value + "'."});
        }
    }  // namespace

    PluginManifestParseResult PluginManifestParser::Parse(const std::string_view json) const
    {
        PluginManifestParseResult result;
        try
        {
            result.Manifest = JsonReader(json).ReadManifest();
        }
        catch (const std::exception& exception)
        {
            result.Errors.push_back({"$", exception.what()});
            return result;
        }

        auto& manifest = result.Manifest;
        if (manifest.SchemaVersion != 1)
            result.Errors.push_back({"schemaVersion", "Only schema version 1 is supported."});
        RequireString("id", manifest.Id, result.Errors);
        RequireString("name", manifest.Name, result.Errors);
        RequireString("version", manifest.Version, result.Errors);
        RequireString("apiVersion", manifest.ApiVersion, result.Errors);
        RequireString("entry", manifest.Entry, result.Errors);

        if (!manifest.Id.empty() && !IsValidPluginId(manifest.Id))
            result.Errors.push_back({"id", "Use lowercase letters, digits, dots, and hyphens."});
        if (!manifest.Version.empty() && !SemanticVersion::Parse(manifest.Version))
            result.Errors.push_back({"version", "Version must use major.minor.patch format."});
        if (!manifest.Entry.empty() && !IsSafeScriptPath(manifest.Entry))
            result.Errors.push_back({"entry", "Entry must be a package-relative .as path using forward slashes."});

        ValidateUniqueValues("exports", manifest.Exports, result.Errors);
        ValidateUniqueValues("capabilities", manifest.Capabilities, result.Errors);

        for (const auto& exportPath : manifest.Exports)
            if (!IsSafeScriptPath(exportPath))
                result.Errors.push_back({"exports", "Invalid export path '" + exportPath + "'."});

        std::unordered_set<std::string> dependencies;
        for (const auto& dependency : manifest.Dependencies)
        {
            if (!IsValidPluginId(dependency.Id))
                result.Errors.push_back({"dependencies", "Invalid plugin id '" + dependency.Id + "'."});
            if (dependency.Id == manifest.Id)
                result.Errors.push_back({"dependencies", "A plugin cannot depend on itself."});
            if (!SemanticVersionRange::IsValid(dependency.VersionRange))
                result.Errors.push_back({"dependencies", "Invalid version range '" + dependency.VersionRange + "'."});
            dependencies.insert(dependency.Id);
        }
        for (const auto& dependency : manifest.OptionalDependencies)
        {
            if (!IsValidPluginId(dependency.Id))
                result.Errors.push_back({"optionalDependencies", "Invalid plugin id '" + dependency.Id + "'."});
            if (dependency.Id == manifest.Id)
                result.Errors.push_back({"optionalDependencies", "A plugin cannot depend on itself."});
            if (!SemanticVersionRange::IsValid(dependency.VersionRange))
                result.Errors.push_back(
                    {"optionalDependencies", "Invalid version range '" + dependency.VersionRange + "'."});
            if (dependencies.contains(dependency.Id))
                result.Errors.push_back(
                    {"optionalDependencies", "Dependency '" + dependency.Id + "' is already required."});
        }
        return result;
    }
}  // namespace PureMirror::Overlay
