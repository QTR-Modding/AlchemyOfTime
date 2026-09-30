#### WINDOWS ENVIRONMENT VARIABLES TO SET

1. **`COMMONLIB_SSE_FOLDER`**: The path to your clone of Commonlib.
2. **`VCPKG_ROOT`**: The path to your clone of [vcpkg](https://github.com/microsoft/vcpkg).
3. (optional) **`SKYRIM_FOLDER`**: path of your Skyrim Special Edition folder.
4. (optional) **`SKYRIM_MODS_FOLDER`**: path of the folder where your mods are.

#### THINGS TO EDIT

1. In LICENSE:
- **`YEAR`**
- **`YOURNAME`**
2. CMakeLists.txt
- **`AUTHORNAME`**
- **`MDDNAME`**
- (optional) Your plugin version. Default: `0.1.0.0`
3. vcpkg.json
- **`name`**: Your plugin's name.
- **`version-string`**: Your plugin version. Default: `0.1.0.0`

#### FEATURES
Automatically imports:
- [CLibUtil](https://github.com/powerof3/CLibUtil) by powerof3
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) by Thiago099

### Preset author guide

See the [AoT wiki](https://github.com/QTR-Modding/AlchemyOfTime/wiki) for configuration examples, including [location triggers](https://github.com/QTR-Modding/AlchemyOfTime/wiki#location-triggers), [perk conditions](https://github.com/QTR-Modding/AlchemyOfTime/wiki#perk-condition-triggers), [addon ordering](https://github.com/QTR-Modding/AlchemyOfTime/wiki#addon-order), and [shared YAML fields](https://github.com/QTR-Modding/AlchemyOfTime/wiki#shared-yaml-fields).

The wiki source is [docs/wiki/Home.md](docs/wiki/Home.md). Documentation changes are reviewed in pull requests and published to the same wiki page after merging to `main`.

### Shared YAML fields

See [Shared YAML fields in the guide](https://github.com/QTR-Modding/AlchemyOfTime/wiki#shared-yaml-fields) for anchors, aliases, and merge examples.

#### ADDON ORDER

See [Addon order in the guide](https://github.com/QTR-Modding/AlchemyOfTime/wiki#addon-order) for filename priority and repeated-trigger behavior.
