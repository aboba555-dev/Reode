# Third-party components

Reode's own code is under the MIT License (see `LICENSE`). The components
below are downloaded at build time and are not part of this repository.
Release builds include their license texts in `licenses/`.

| Component | License | Where it ends up |
|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | `Reode.dll` |
| [miniz](https://github.com/richgel999/miniz) | MIT | `Reode.dll` |
| [GoogleTest](https://github.com/google/googletest) | BSD-3-Clause | tests only, not shipped |

### Optional: Geode compatibility layer

Built only with `-DREODE_GEODE_COMPAT=ON` and never part of a release.

| Component | License | Where it ends up |
|---|---|---|
| [Geode SDK](https://github.com/geode-sdk/geode) headers | BSL-1.0 | `Geode.dll` |
| [matjson](https://github.com/geode-sdk/json) | BSL-1.0 | `Geode.dll` |
| [result](https://github.com/geode-sdk/result) | BSL-1.0 | `Geode.dll` |
| [TulipHook](https://github.com/geode-sdk/TulipHook) headers | BSL-1.0 | `Geode.dll` |
| [fmt](https://github.com/fmtlib/fmt) | MIT | `Geode.dll` |
| [asp2](https://github.com/dankmeme01/asp2) | MIT | `Geode.dll` |
| [arc](https://github.com/dankmeme01/arc) | MIT | `Geode.dll` |
| [nontype_functional](https://github.com/geode-sdk/nontype_functional) | BSD-2-Clause | `Geode.dll` |

## Geode bindings: no license stated

[geode-sdk/bindings](https://github.com/geode-sdk/bindings) describes the
game's classes and function addresses. It states no license. Nothing from it
is stored in this repository, but the build downloads it and runs Geode's
code generator on it to produce the C++ headers that `Geode.dll` (the Geode
compatibility layer) is compiled against. Whether `Geode.dll` can be
distributed under Reode's MIT License is therefore unclear, and releases do
not include it. Do not distribute builds made with `REODE_GEODE_COMPAT=ON`
unless this is settled with the Geode team.

Geometry Dash is a trademark of RobTop Games. Reode is not affiliated with
RobTop Games or the Geode team.
