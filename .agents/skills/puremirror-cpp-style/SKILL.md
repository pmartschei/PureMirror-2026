---
name: puremirror-cpp-style
description: Apply the repository-specific PureMirror C++ structure, formatting, header dependency, precompiled-header, and test-placement conventions. Use whenever creating, editing, reviewing, refactoring, or testing C++ headers, source files, classes, structs, PCH usage, or includes anywhere in the PureMirror repository.
---

# PureMirror C++ Style

Apply these rules to every touched C++ file. Preserve the surrounding architecture unless the task explicitly asks for a broader refactor.

## Member variables

- Name every non-static member variable with the `m_` prefix followed by PascalCase.
- Apply the rule regardless of visibility.

```cpp
class Example
{
  private:
    std::string m_Name;
    bool m_IsEnabled{};
};
```

Do not introduce suffix-style members such as `name_`, `logger_`, or `isEnabled_`.

## Precompiled header

- Apply the PCH rules only when the owning project already contains a `pch.h` or `framework.h`.
- Never create a `pch.h`, `pch.cpp`, or `framework.h` solely to satisfy this skill.
- When `pch.h` exists, never include it from a `.h` file and include it normally as the first include in every `.cpp` file.
- When only `framework.h` exists, follow the project's existing include pattern and do not invent a `pch.h`.
- Never surround a `pch.h` include with `// clang-format off` and `// clang-format on`.

```cpp
#include "pch.h"

#include "Example.h"
```

## Classes and structs

- Put each class or struct in its own appropriately named header instead of defining multiple data types inside another class's header.
- Model standalone data structures as their own class or struct rather than embedding their full definition in an unrelated service, UI, or registry header.
- Add a source file when the type has non-trivial implementation; keep only genuinely trivial operations inline.
- Do not create a separate file for a tiny implementation-only helper that is local to one `.cpp` file.

## Includes

- Keep every `.h` file self-contained: include its direct dependencies such as `<string>`, `<vector>`, or another project header in that `.h` file.
- Never rely on `pch.h` or an earlier transitive include to make a header compile.
- When the project has a `framework.h`, decide for each dependency added to a `.h` file whether it is broadly used. If it is, also add it to `framework.h` for precompiled-header coverage.
- When the project has no `framework.h`, keep the necessary includes in the files that use them and do not create `framework.h` solely for centralization.
- Do not remove a required direct include from a `.h` file merely because the same dependency is present in `framework.h`.
- In projects with `pch.h` or `framework.h`, keep only file-specific dependencies local in `.cpp` files and put broadly used dependencies in the existing `framework.h`.
- Before changing includes, inspect an existing `framework.h` and avoid duplicate entries there.
- Keep project headers explicit in the file that directly uses their declarations.

## Test organization

- Put tests in a sibling project whose name is the implementation project's exact name plus the `.Tests` suffix.
- Mirror the implementation file's relative folder structure inside the test project.
- Name the test file after the tested implementation type with the `Tests` suffix.

For example:

```text
PureMirror.Overlay/src/core/Logger.cpp
PureMirror.Overlay.Tests/src/core/LoggerTests.cpp
```

If the implementation file is at the project root, place its test at the test project root as well.

## Editing workflow

1. Identify the owning PureMirror project and check whether `framework.h` or `pch.h` exists.
2. Make every touched header self-contained with direct includes and no `pch.h`.
3. If `framework.h` exists, evaluate each header dependency for additional inclusion there.
4. If `pch.h` exists, include it first in every touched `.cpp` file without clang-format guards; otherwise preserve the PCH-free project setup.
5. Place every new class or struct in its own file where required.
6. Rename touched member variables to the `m_PascalCase` form and update all references.
7. Add or update tests in the matching `.Tests` project and mirrored relative folder.
8. Run clang-format, build the affected project and its test project, and run relevant tests.

