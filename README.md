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

#### ADDON ORDER

Addon `.yml` files are combined in alphabetical filename order (case-sensitive).
For example, put cooking rules in `10_Cooking.yml` and freezing rules in
`20_Freezing.yml`. For a food named in both files, the cooking triggers are
checked before the appended freezing triggers of the same kind.

Repeated entries for the same food also combine in their order within a file.
If a later entry redefines the same trigger of the same kind, its complete
settings replace the earlier definition without moving its priority. Omitted
optional fields are cleared; an empty allowed-stage or container list remains
unrestricted. Unrelated triggers are retained. Top-level container lists are
combined. Transformers still take priority over time modulators.
