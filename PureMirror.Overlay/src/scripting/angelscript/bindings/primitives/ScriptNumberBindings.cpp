#include "pch.h"

#include "ScriptNumberBindings.h"

#include "ScriptNumber.h"
#include "scripting/angelscript/ScriptBindingUtils.h"
#include "angelscript.h"

#include <format>

namespace PureMirror::Overlay
{
    namespace
    {
        template <typename T> ScriptNumber<T>* CreateNumber()
        {
            return new ScriptNumber<T>{};
        }

        template <typename T> ScriptNumber<T>* CreateNumber(const T value)
        {
            return new ScriptNumber<T>{value};
        }

        template <typename T>
        bool RegisterNumber(asIScriptEngine& engine,
                            const ScriptBindingUtils& require,
                            const std::string_view objectName,
                            const std::string_view primitiveName,
                            std::string& error)
        {
            using Number = ScriptNumber<T>;

            const auto operation = [&](const std::string_view name) { return std::format("{} {}", objectName, name); };
            const auto defaultFactory = std::format("{}@ f()", objectName);
            const auto valueFactory = std::format("{}@ f({} value)", objectName, primitiveName);
            const auto primitiveAssignment = std::format("{}& opAssign({} value)", objectName, primitiveName);
            const auto objectAssignment = std::format("{}& opAssign(const {}&in value)", objectName, objectName);
            const auto primitiveConversion = std::format("{} opImplConv() const", primitiveName);
            const auto valueGetter = std::format("{} get_Value() const property", primitiveName);
            const auto valueSetter = std::format("void set_Value({} value) property", primitiveName);

            return require(engine.RegisterObjectType(objectName.data(), 0, asOBJ_REF), operation("type"), error) &&
                   require(
                       engine.RegisterObjectBehaviour(
                           objectName.data(), asBEHAVE_ADDREF, "void f()", asMETHOD(Number, AddRef), asCALL_THISCALL),
                       operation("addref"),
                       error) &&
                   require(
                       engine.RegisterObjectBehaviour(
                           objectName.data(), asBEHAVE_RELEASE, "void f()", asMETHOD(Number, Release), asCALL_THISCALL),
                       operation("release"),
                       error) &&
                   require(engine.RegisterObjectBehaviour(objectName.data(),
                                                          asBEHAVE_FACTORY,
                                                          defaultFactory.c_str(),
                                                          asFUNCTIONPR(CreateNumber<T>, (), Number*),
                                                          asCALL_CDECL),
                           operation("default factory"),
                           error) &&
                   require(engine.RegisterObjectBehaviour(objectName.data(),
                                                          asBEHAVE_FACTORY,
                                                          valueFactory.c_str(),
                                                          asFUNCTIONPR(CreateNumber<T>, (T), Number*),
                                                          asCALL_CDECL),
                           operation("value factory"),
                           error) &&
                   require(engine.RegisterObjectMethod(objectName.data(),
                                                       primitiveAssignment.c_str(),
                                                       asMETHODPR(Number, Assign, (T), Number&),
                                                       asCALL_THISCALL),
                           operation("primitive assignment"),
                           error) &&
                   require(engine.RegisterObjectMethod(objectName.data(),
                                                       objectAssignment.c_str(),
                                                       asMETHODPR(Number, Assign, (const Number&), Number&),
                                                       asCALL_THISCALL),
                           operation("object assignment"),
                           error) &&
                   require(engine.RegisterObjectMethod(objectName.data(),
                                                       primitiveConversion.c_str(),
                                                       asMETHOD(Number, ToPrimitive),
                                                       asCALL_THISCALL),
                           operation("primitive conversion"),
                           error) &&
                   require(engine.RegisterObjectMethod(
                               objectName.data(), valueGetter.c_str(), asMETHOD(Number, GetValue), asCALL_THISCALL),
                           operation("Value getter"),
                           error) &&
                   require(engine.RegisterObjectMethod(
                               objectName.data(), valueSetter.c_str(), asMETHOD(Number, SetValue), asCALL_THISCALL),
                           operation("Value setter"),
                           error);
        }
    }  // namespace

    bool RegisterScriptNumberBindings(asIScriptEngine& engine, std::string& error)
    {
        error.clear();
        const ScriptBindingUtils require{"number"};
        const std::string previousNamespace = engine.GetDefaultNamespace();

        auto successful = require(engine.SetDefaultNamespace(""), "global namespace", error) &&
                          RegisterNumber<std::int32_t>(engine, require, "Int", "int", error) &&
                          RegisterNumber<std::uint32_t>(engine, require, "UInt", "uint", error) &&
                          RegisterNumber<std::int64_t>(engine, require, "Long", "int64", error) &&
                          RegisterNumber<std::uint64_t>(engine, require, "ULong", "uint64", error);

        const auto reset =
            require(engine.SetDefaultNamespace(previousNamespace.c_str()), "restore default namespace", error);
        successful = successful && reset;
        return successful;
    }
}  // namespace PureMirror::Overlay
