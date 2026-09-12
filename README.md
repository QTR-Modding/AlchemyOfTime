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

### Shared YAML fields

Use `<<` to inherit fields from an anchored mapping. Fields written on the entry override the template:

```yaml
fire: &fire
  FormEditorID: LIFAoTFires
  duration: 0.0611111
  sound: LIFAoTThawSound
  art_object: LIFAoTThawSteam

# Within a food rule:
transformers:
  - <<: *fire
    finalFormEditorID: FoodBeef
```

This works in default, custom, and addon presets. `<<: [*first, *second]` combines templates with the first taking precedence. Explicit nested maps replace inherited maps; they are not deeply merged. Templates stay within their YAML file.
