#pragma once

namespace PureMirror::Overlay
{
    enum class PluginDependencyIssueType
    {
        DuplicatePlugin,
        MissingDependency,
        IncompatibleVersion,
        SelfDependency
    };
}  // namespace PureMirror::Overlay
