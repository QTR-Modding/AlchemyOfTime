# Alchemy of Time - Easy Guide

## What is Alchemy of Time?

**Alchemy of Time** is a tool that lets items in your game change over time or interact with other items nearby. For example, food can spoil, items can turn into something else, and time can speed up or slow down for specific items. You can change how this works using simple files.

To set it up, you'll use **INI**, **YAML**, and **TXT** files, which are easy to edit.

Supported item types are FOOD, INGR, MEDC, POSN, ARMO, WEAP, SCRL, BOOK, SLGM, MISC

---

## Getting Started

This guide covers released AoT 1.3.5. A **trigger** is a form whose presence, location, or conditions activate a time modulator or transformer.

### Smallest addon example

With AoT's supplied default files installed and FOOD enabled, save this as `SKSE/Plugins/AlchemyOfTime/FOOD/addon/PreserveBeef.yml`:

```yaml
formsLists:
  - forms: FoodBeef
    timeModulators:
      - FormEditorID: IceWraithTeeth
        magnitude: 0
```

This pauses raw beef's stage progression while Ice Wraith Teeth are nearby in the world or in the same inventory. It is useful with a spoilage preset; the supplied default itself has no timed evolution. A matching transformer takes priority over this modulator. Restart the game after editing presets.

For cold areas or weather-based conditions, see [Location triggers](#location-triggers) and [Perk condition triggers](#perk-condition-triggers).

### Turning It On

First, open the **INI file** and turn on the item type you want. Add this:

```ini
[MODULES]
<ITEMTYPE>=true
```

Replace `item_type` with the type of items you want to use (like FOOD). To see the full list of item types, check the `moduleskeyvals` section in the [Settings.h file](https://github.com/QTR-Modding/AlchemyOfTime/blob/main/include/Settings.h).

---

## Stages and Default Settings

### What are Stages?

Items go through different **stages** as time passes. For example, food might go from Fresh to Stale to Spoiled. \
Each stage changes what the item does or looks like and can have specific properties defined in YAML files under the `stages` section. Here's what you can set for each stage:

- **no**: *(Required)* A unique number identifying the stage. It must start at `0` for the initial stage and increment by `1` for each subsequent stage.
- **FormEditorID**: Stage `0` uses the original item and does not need this field. For later stages, only FOOD and MISC can omit it to create a dynamic derivative of the original item. INGR, MEDC, POSN, ARMO, WEAP, SCRL, BOOK, and SLGM require an explicit replacement form for every later stage.
- **name**: The name of the stage (e.g., Fresh, Stale). This name will appear next to the original item name in parentheses if the stage is a dynamic form.
- **duration**: *(Required)* The time in in-game hours the item remains in this stage; it must be positive. `.inf` means an infinite duration.
- **value**: Overrides the value of a dynamically generated stage form (FOOD or MISC). Ignored for stage `0` and stages with an explicit `FormEditorID`.
- **weight**: Overrides the weight of a dynamically generated stage form (FOOD or MISC). Ignored for stage `0` and stages with an explicit `FormEditorID`.
- **crafting\_allowed**: *(Boolean)* Whether the item can be used in crafting at this stage.
- **mgeffect**: Overrides the effects of a dynamically generated FOOD stage only. Ignored for stage `0`, stages with an explicit `FormEditorID`, and other modules. Each effect includes:
  - **FormEditorID**: The form editor ID of the magic effect. Leaving this empty will result in an empty magic effect, useful if you want to remove the original magic effect on the item.
  - **magnitude**: The strength of the effect.
  - **duration**: Duration of the effect.
- **color**: A hexadecimal value (RGBA) defining the item's color at this stage.
- **sound**: The Sound Descriptor form to be attached to the item at this stage.
- **art\_object**: The Art Object form to be attached to the item at this stage.
- **effect\_shader**: The Effect Shader form to be attached to the item at this stage.

These properties allow complete customization of each stage's behavior, appearance, and functionality. If any required field is missing, the stage will be ignored or result in an error.

### Final Form *(Required)*

- **finalFormEditorID**: *(Required)* Specifies the **terminal item** that the item transforms into after completing all stages. This field is required for default and custom stage rules. An addon needs a destination only inside each transformer.

### Default Settings

- **Default settings** affect all items of a specific type.
- These are stored in files named `AoT_default<ITEMTYPE>.yml`. For example, food uses `AoT_defaultFOOD.yml`.
- Default settings apply to every item of that type unless you make a **custom setting** for specific items.
AoT supplies a default file for each supported item type with an infinite stage and `finalFormEditorID: 0xF` (Gold). Gold is a required placeholder; the infinite stage never reaches it. These defaults let addons work without a separate custom rule and without changing unrelated items over time. Keep the supplied defaults installed, or use defaults supplied by another preset.

For an item without a custom rule, AoT starts tracking it through the default when stage `0` has a duration below `9999` hours or the item has an addon. Custom rules still require their own `stages` and `finalFormEditorID`.

Stage and transformer durations are **in-game hours**, not seconds. At timescale 20, `1` hour is 180 real seconds, `0.05` is 9 seconds, and `15` is 45 minutes. Magic-effect durations under `mgeffect` are seconds.

#### Example Default File

Here's an example for food stages:

```yaml
stages:
  - name: Fresh
    FormEditorID:
    duration: 1   # Stays fresh for 1 in-game hour
    no: 0

  - name: Stale
    FormEditorID:
    duration: 1
    no: 1
    crafting_allowed: true # will behave as Stage 0 item in crafting menus
    value: 1
    mgeffect:
      - FormEditorID: 0003EB42
        magnitude: 100
        duration: 10

  - name: Spoiled
    FormEditorID:
    duration: 2
    no: 2
    value: 0
    weight: 0
    mgeffect:
      - FormEditorID: 0003EB42
        magnitude: 1
        duration: 5

  - name: Rotten
    FormEditorID:
    duration: 2
    no: 3
    value: 0

finalFormEditorID: 0x34CDF # Salt Pile; choose your intended final item

# Time Modulators
# Items near the food will affect how quickly stages progress.
timeModulators:
  - FormEditorID: IceWraithTeeth
    magnitude: 0.5  # Time progresses 50% slower
    color: 61b6f3a3
```

---

## Additional Features

### Containers

1. **containers** at the rule level restricts evolution by the inventory owner's **base form**, not its placed reference ID. An omitted or empty list is unrestricted. For world items this rule-level filter compares the item's own base form.
2. **containers** inside a transformer or time modulator restricts that trigger in inventories; it does not restrict the trigger for world items. For example, `containers: 0x7` matches the player's base form.

To make a location/perk trigger apply only to world items, put its own location/perk ID in its `containers` field. That form cannot be an inventory owner's base form, so the inventory check never matches. See the freezing example below.

### Time Modulators

Time modulators change how quickly items progress through their stages and can even reverse time progression. Their trigger can be a nearby object, an item in the same inventory, a location, or a perk condition list. They include the following properties:

- **FormEditorID**: *(Required)* The trigger form: an object base form, location, or perk. This field also accepts a list or form group.
- **magnitude**: *(Required)* A multiplier for stage progression: `1` is normal, `0.5` is half speed, `0` pauses, and negative values reverse progression. This does not set a transformer's duration.
- **color**: RGBA color in hexadecimal format.
- **sound**: The Sound Descriptor form to be attached to the stage item.
- **art_object**: The Art Object form to be attached to the stage item.
- **effect_shader**: The Effect Shader form to be attached to the stage item.
- **containers**: Specifies the containers (form IDs, editor IDs, or local IDs) where the time modulator is allowed to take effect. Can be a scalar or array type.
- **allowed_stages**: Stage numbers this modulator can affect. Omitted or empty means all stages; `[0]` means only the initial stage.

### Transformers

Transformers allow items to **bypass the stage cycle** and transform into another form after a specified duration. They use the same object, location, and perk triggers as time modulators. Transformers include:

- **FormEditorID**: *(Required)* The **trigger** ID, not the item being transformed. The affected item is selected by the rule's `owners` or `forms`. This field also accepts a list or form group.
- **finalFormEditorID**: *(Required)* The ID of the new item after transformation.
- **duration**: *(Required)* Positive duration in **in-game hours** for the transformation to occur.
- **allowed_stages**: Specifies the stages where the transformation can happen. If omitted or empty, all stages are allowed; `[0]` selects only the initial stage.
- **color**: RGBA color in hexadecimal format.
- **sound**: The Sound Descriptor form to be attached during transformation.
- **art_object**: The Art Object form attached to the transforming item.
- **effect_shader**: The Effect Shader form attached to the transforming item.
- **containers**: Specifies the containers (form IDs, editor IDs, or local IDs) where the transformer is allowed to take effect. Can be a scalar or array type.

### Location triggers

A location is a Skyrim **LCTN record**, not a cell, region, weather record, or a place name typed as text. Use its EditorID or FormID in the trigger's `FormEditorID` field.

For example, this addon halves raw beef's stage progression in Whiterun and its child locations:

```yaml
formsLists:
  - forms: FoodBeef
    timeModulators:
      - FormEditorID: WhiterunLocation
        magnitude: 0.5
```

AoT compares the location against the world item's current location, or the inventory owner's current location. A parent location also matches its descendants. This means a location trigger can affect food carried by the player, NPCs, or containers there. It is not an automatic temperature or snow check.

For a mod that defines a location `ColdLIF` and a frozen item `FoodVenisonF`, a world-only freezing addon is:

```yaml
formsLists:
  - forms: FoodVenison
    transformers:
      - FormEditorID: ColdLIF
        containers: ColdLIF
        finalFormEditorID: FoodVenisonF
        duration: 0.05
```

Both example mod-specific forms must exist in an enabled plugin. Remove `containers: ColdLIF` if venison should also freeze in inventories in that location. `duration: 0.05` means nine real seconds at timescale 20, while the game clock is advancing.

### Perk condition triggers

A **PERK record** can hold a list of Skyrim conditions. AoT evaluates that record's top-level conditions; it does not require the player to have acquired the perk and does not execute its perk entries.

Create a perk in your plugin, for example `MyColdConditions`, and set its conditions to the checks you need. Then reference it like this:

```yaml
formsLists:
  - forms: FoodBeef
    timeModulators:
      - FormEditorID: MyColdConditions
        magnitude: 0
```

For a world item, AoT passes the item's reference as both the condition subject and target. For inventory items, it passes the inventory owner's reference as both. Choose condition functions and their Run On settings accordingly: an actor-only condition is not suitable for a piece of food lying on the ground. Weather or other environmental behavior depends on the conditions you author; a perk's name alone does nothing.

Use `containers: MyColdConditions` inside the trigger if those conditions should only affect world items. Inventory conditions are reevaluated during updates; do not expect a paused menu to advance a timed transformation.

### Trigger priority

AoT uses the first qualifying transformer; if none qualifies, it uses the first qualifying time modulator. It does not multiply all matching modulators together. Order triggers of the same kind from highest to lowest priority. For example, put a nearby-fire transformer before a location-based freezing transformer so the fire wins when both match.

---

## Custom Settings

### What are Custom Settings?

Custom settings let you change the rules for **specific items**. You can:

- Use **Form IDs** or **Editor IDs** for items.
- Use simple names (like "Book") to target items by words they contain.

Custom settings go into files in this folder:

```
SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\custom\
```

You can add as many files as you want.

Each `ownerLists` entry is a complete stage rule and needs `stages` and `finalFormEditorID`. Use an addon instead if you only want to add triggers.

### Example Custom File

```yaml
ownerLists:
  - owners: [Soup, Stew]  # Items with these names
    stages:
      - name: Fresh
        FormEditorID:
        duration: 2
        no: 0
        crafting_allowed: true

      - name: Stale
        FormEditorID:
        duration: 2
        no: 1
        crafting_allowed: false
        mgeffect:
          - FormEditorID: 0003EB42
            magnitude: 100
            duration: 10

    finalFormEditorID: 0x34CDF # Salt Pile; choose your intended final item
    timeModulators:
      - FormEditorID: IceWraithTeeth
        magnitude: 0.7

```

---

## Addon

### What is an Addon?

Addons let you add **extra behaviors** to items, like slowing time or turning items into something new. They work by targeting specific items using the **forms** field, which lists the form IDs, Editor IDs, or local IDs of items the addon behaviors will modify. This field can be a scalar or an array type in the YAML configuration. They are made up of two key parts:

1. **Time Modulators**
2. **Transformers**

Addons do not define stages. They extend the custom or default rule used by each item. AoT's supplied infinite defaults are enough when you want only triggered transformations.

### Addon order

Addon `.yml` files are combined in alphabetical filename order, ignoring letter case. Names differing only in case use their exact spelling as a tie-breaker. For example, `10_Cooking.yml` supplies triggers before `20_Freezing.yml` for the same food and trigger kind. This is AoT's filename order, not ESP load order.

Repeated entries for the same food combine in entry order. Among addons, redefining the same trigger of the same kind replaces its complete settings without moving its priority. Omitted optional fields are cleared; empty allowed-stage and container lists mean unrestricted. Unrelated triggers remain, and rule-level container lists combine. Existing triggers in the underlying custom/default rule keep their positions; filename ordering describes the addons added to that rule.

### Example Addon File

Here is a transformer addon. Its fire, sound, and art forms require the plugin that defines them:

```yaml
formsLists:
  - forms: 03003540  # Target form (item): form ID, Editor ID, or local ID; can be scalar or array type
    transformers:
      - FormEditorID: 000d61b6
        finalFormEditorID: 0300353F  # New item (cooked version)
        duration: 0.05
        color: f30a86a3
        sound: 0x1~quantAoTBBQ.esp
        art_object: _quantSmokeExhaleFX
```

---

## Exclude Lists

### What are Exclude Lists?

Exclude lists let you choose items that should **not** change at all. You can exclude items using:

- Names (like "Wine").
- Full EditorIDs.

Exclude lists go into `TXT` files in this folder:

```
SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\exclude\
```

### Example Exclude File

Here's an example of an exclude list:

```
Book
Scroll
Wine
```


This will stop all items with "Book," "Scroll," or "Wine" in their name from being changed.

Matching is case-insensitive and uses whole words or phrases. For an EditorID, write the full EditorID.

The released exclude reader matches text against names and EditorIDs; it does not resolve numeric FormIDs or expand form groups in these TXT files.

---

## Tips

### 1. Addon Features in Presets

All addon features, like time modulators and transformers, can also be included in **default** and **custom** settings. Put them beside `stages`, not inside an individual stage. Use `allowed_stages` to restrict a trigger to particular stage numbers.

#### Example:

```yaml
ownerLists:
  - owners: FoodBeef
    stages:
      - no: 0
        name: Fresh
        duration: 24
    finalFormEditorID: 0x34CDF # Salt Pile
    timeModulators:
      - FormEditorID: IceWraithTeeth
        magnitude: 0.5
```

### 2. Modular Design

Custom, addon, and exclude settings are **modular**. You can keep things tidy by creating multiple files for different ideas or item types.

#### Example:

- `magic_items.yml`: Custom rules for magic items.
- `food_spoil.yml`: Rules for food spoilage.

### 3. Multiple Owners

Use multiple `ownerLists` entries with `owners` for custom rules, or multiple `formsLists` entries with `forms` for addons. Every custom entry must be complete:

#### Example:

```yaml
ownerLists:
  - owners: FoodBeef
    stages:
      - {no: 0, name: Fresh, duration: 24}
    finalFormEditorID: 0x34CDF
  - owners: FoodVenison
    stages:
      - {no: 0, name: Fresh, duration: 12}
    finalFormEditorID: 0x34CDF
```

### 4. Indentation Matters

YAML files rely on correct **indentation** to work properly. Always match the spacing in your examples. For instance:

#### Good Example:

```yaml
ownerLists:
  - owners: [Soup, Stew]
    stages:
      - name: Fresh
        FormEditorID:
        duration: 2
        no: 0
        crafting_allowed: true
    finalFormEditorID: 0x34CDF
```

#### Bad Example:

```yaml
ownerLists:
- owners: [Soup, Stew]
stages:
  - name: Fresh
    FormEditorID:
    duration: 2
    no: 0
    crafting_allowed: true
```

### 5. Use Local IDs if EditorIDs or FormIDs do not work: https://thiago099.github.io/spid-helper/ 
#### Example: `0x1~quantAoTBBQ.esp`

### 6. Use Form Groups
Use a [form group](https://github.com/QTR-Modding/CLibUtilsQTR/wiki/Form-Groups) to maintain a reusable list of forms for list-valued selectors, such as addon `forms`, trigger `FormEditorID`, and `containers`. A destination such as `finalFormEditorID` needs one actual form, not a group of possible destinations. See also [exclude lists](#exclude-lists) and [multiple owners](#3-multiple-owners).
Put your form group `txt` files in `SKSE/Plugins/AlchemyOfTime/formGroups`.

### 7. Unique Overarching Node Names

Custom settings and addon settings have their unique overarching node names. These nodes act as identifiers for their respective settings and must be used correctly to avoid conflicts.

#### Node Names:

- **ownerLists**: Used for custom settings, which define specific item behaviors or overrides.
- **formsLists**: Used for addon settings, which define additional behaviors like transformers and time modulators.

#### Example:

For custom settings:
```yaml
ownerLists:
  - owners: [Soup, Stew]
    stages:
      - no: 0
        name: Fresh
        duration: 1
    finalFormEditorID: 0x34CDF
```

For addon settings:
```yaml
formsLists:
  - forms: 03003540  # Target form (item)
    transformers:
      - FormEditorID: 000d61b6
        finalFormEditorID: 0300353F
        duration: 0.05
```



---

## Form identifiers

Despite the field name `FormEditorID`, AoT accepts these identifiers:

1. An EditorID, such as `FoodBeef`.
2. A full hexadecimal FormID, such as `00065C99` (legacy unprefixed IDs have seven or eight hex digits).
3. A prefixed hexadecimal FormID, such as `0x65C99` or `0xF`. `0X` also works. Short unprefixed strings are not shorthand numeric IDs.
4. A plugin-local ID followed by `~PluginName.esp`, such as `0x65C99~Skyrim.esm`. This avoids hard-coding the plugin's load-order index. Use the local record ID, not a full runtime ID with its load-order prefix.

Quotes are optional for these YAML values; `0xF` and `"0xF"` both work. Use identifiers for the required record type: a transformer destination is an item, while its trigger can be an object, location, or perk. See [Form Groups](#6-use-form-groups) for fields that accept reusable lists.

## Shared YAML fields

An **anchor** (`&name`) gives a YAML value a reusable name. An **alias** (`*name`) refers to that value. The merge key `<<` copies fields from a mapping into another mapping.

For example, this complete addon shares one preservation definition between two foods:

```yaml
preservation: &preservation
  FormEditorID: IceWraithTeeth
  magnitude: 0

formsLists:
  - forms: FoodBeef
    timeModulators:
      - <<: *preservation
  - forms: FoodVenison
    timeModulators:
      - <<: *preservation
        magnitude: 0.5
```

Beef's stage progression pauses, while venison progresses at half speed. Explicit fields override inherited fields, including `0` and `null`. `<<: [*first, *second]` accepts several mappings, with the first taking precedence over later mappings. An explicit nested mapping replaces the inherited nested mapping; this is not a deep merge. A YAML sequence can be reused with an alias but cannot be merged with `<<` as though it were a mapping.

Anchors stay within their YAML document. This works in default, custom, and addon presets. It does not provide functions or parameter substitution.

## Where to Put Files

- **Default Settings**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\AoT_default<ITEMTYPE>.yml`
- **Custom Settings**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\custom\`
- **Addon**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\addon\`
- **Exclude Lists**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\exclude\`
- **Form Groups**: `SKSE/Plugins/AlchemyOfTime/formGroups`

You can replace `<ITEMTYPE>` with the type of items you want to change (like FOOD, BOOK, etc.). Check `moduleskeyvals` in the [Settings.h file](https://github.com/QTR-Modding/AlchemyOfTime/blob/main/include/Settings.h) for the full list.

---

## Final Thoughts

The **Alchemy of Time** mod gives you full control over how items behave in your game. You can:
- Set up global rules using **default settings**.
- Customize specific items with **custom settings**.
- Add extra effects using **addons** with **time modulators** and **transformers**.
- Exclude items you don't want to change.
- Use form groups to keep things tidy.

Make sure to double-check your **indentations** so your files work correctly!

