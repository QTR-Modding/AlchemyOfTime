# Alchemy of Time - Easy Guide

## What is Alchemy of Time?

**Alchemy of Time** is a tool that lets items in your game change over time or interact with other items nearby. For example, food can spoil, items can turn into something else, and time can speed up or slow down for specific items. You can change how this works using simple files.

To set it up, you'll use **INI**, **YAML**, and **TXT** files, which are easy to edit.

Supported item types are FOOD, INGR, MEDC, POSN, ARMO, WEAP, SCRL, BOOK, SLGM, MISC

---

## Getting Started

### Turning It On

First, open the **INI file** and turn on the item type you want. Add this:

```ini
[MODULES]
<ITEMTYPE>=true
```

Replace `item_type` with the type of items you want to use (like FOOD). To see the full list of item types, check the `moduleskeyvals` section in the [Settings.h file](https://github.com/QY-MODS/AlchemyOfTime/blob/main/include/Settings.h).

---

## Stages and Default Settings

### What are Stages?

Items go through different **stages** as time passes. For example, food might go from Fresh to Stale to Spoiled. \
Each stage changes what the item does or looks like and can have specific properties defined in YAML files under the `stages` section. Here’s what you can set for each stage:

- **no**: *(Required)* A unique number identifying the stage. It must start at `0` for the initial stage and increment by `1` for each subsequent stage.
- **FormEditorID**: *(Required)* The form ID, editor ID, or local ID for the item at this stage. Stage `0` is reserved for the original/initial item, hence **you do not need to specify the **FormEditorID** for stage `0`**. If not specified for later stages, a dynamic form based on the original item will be used.
- **name**: The name of the stage (e.g., Fresh, Stale). This name will appear next to the original item name in parentheses if the stage is a dynamic form.
- **duration**: *(Required)* The time (in-game hours) the item remains in this stage.
- **value**: Overrides the item’s value during this stage.
- **weight**: Overrides the item’s weight during this stage.
- **crafting\_allowed**: *(Boolean)* Whether the item can be used in crafting at this stage.
- **mgeffect**: A list of magic effects that override the item's default effects during this stage. Each effect includes:
  - **FormEditorID**: The form editor ID of the magic effect. Leaving this empty will result in an empty magic effect, useful if you want to remove the original magic effect on the item.
  - **magnitude**: The strength of the effect.
  - **duration**: Duration of the effect.
- **color**: A hexadecimal value (RGBA) defining the item’s color at this stage.
- **sound**: The Sound Descriptor form to be attached to the item at this stage.
- **art\_object**: The Art Object form to be attached to the item at this stage.
- **effect\_shader**: The Effect Shader form to be attached to the item at this stage.

These properties allow complete customization of each stage's behavior, appearance, and functionality. If any required field is missing, the stage will be ignored or result in an error.

### Final Form *(Required)*

- **finalFormEditorID**: *(Required)* Specifies the **terminal item** that the item transforms into after completing all stages. This field is **mandatory** for any preset to function.

### Default Settings

- **Default settings** affect all items of a specific type.
- These are stored in files named `AoT_default<ITEMTYPE>.yml`. For example, food uses `AoT_defaultFOOD.yml`.
- Default settings apply to every item of that type unless you make a **custom setting** for specific items.
> ⚠️ Default settings must have a `duration` less than 9999 **or** the Form that the default setting would apply must have an [Addon](https://github.com/QTR-Modding/AlchemyOfTime/wiki#addon).

#### Example Default File

Here’s an example for food stages:

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

finalFormEditorID: ccBGS_RootRotScaleIngredient

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

- **containers**: A list of containers (form IDs, editor IDs, or local IDs) that allow evolution **only** when the item is in those specified containers or inventories.

### Time Modulators

Time modulators change how quickly items progress through their stages and can even reverse time progression. They affect nearby items or items in the same inventory and include the following properties:

- **FormEditorID**: *(Required)* The form ID, editor ID, or local ID of the modulating item.
- **magnitude**: *(Required)* A multiplier that changes the speed and direction of time progression (e.g., 0.5 means 50% slower, negative values reverse the direction).
- **color**: RGBA color in hexadecimal format.
- **sound**: The Sound Descriptor form to be attached to the stage item.
- **art_object**: The Art Object form to be attached to the stage item.
- **effect_shader**: The Effect Shader form to be attached to the stage item.
- **containers**: Specifies the containers (form IDs, editor IDs, or local IDs) where the time modulator is allowed to take effect. Can be a scalar or array type.

### Transformers

Transformers allow items to **bypass the stage cycle** and transform into another form after a specified duration. They affect nearby items or items in the same inventory. Transformers include:

- **FormEditorID**: *(Required)* The ID of the item to transform.
- **finalFormEditorID**: *(Required)* The ID of the new item after transformation.
- **duration**: *(Required)* Time (in-game) for the transformation to occur.
- **allowed_stages**: Specifies the stages where the transformation can happen. If omitted, all stages are allowed.
- **color**: RGBA color in hexadecimal format.
- **sound**: The Sound Descriptor form to be attached during transformation.
- **art_object**: The Art Object form attached to the transforming item.
- **effect_shader**: The Effect Shader form attached to the transforming item.
- **containers**: Specifies the containers (form IDs, editor IDs, or local IDs) where the transformer is allowed to take effect. Can be a scalar or array type.

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

    finalFormEditorID: ccBGS_RootRotScaleIngredient
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

### Example Addon File

Here’s an example addon file combining both time modulators and transformers:

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
- Form IDs or Editor IDs.

Exclude lists go into `TXT` files in this folder:

```
SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\exclude\
```

### Example Exclude File

Here’s an example of an exclude list:

```
Book
Scroll
Wine
```


This will stop all items with "Book," "Scroll," or "Wine" in their name from being changed.

If you want to exclude using EditorIDs you need to put the full EditorID of each item.

For excluding with FormIDs, use 8 digits that include the index, e.g. E8000807.

---

## Tips

### 1. Addon Features in Presets

All addon features, like time modulators and transformers, can also be included in **default** and **custom** settings.

#### Example:

```yaml
stages:
  - name: Fresh
    duration: 1
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

In **custom** and **addon** settings, you can add multiple `owners` nodes to affect many items within the same file.

#### Example:

```yaml
ownerLists:
  - owners: [Book, MagicScroll]
    (populate)
  - owners: [Potion, Scroll]
    (populate)        
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
**Anywhere** where you write the ID of a form, you can put the name of a [form group](https://github.com/QTR-Modding/CLibUtilsQTR/wiki/Form-Groups) instead! This includes [exclude lists](https://github.com/QTR-Modding/AlchemyOfTime/wiki#3-multiple-owners) and [multiple owners](https://github.com/QTR-Modding/AlchemyOfTime/wiki#3-multiple-owners). <br>
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
      - name: Fresh
        duration: 1
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

## Where to Put Files

- **Default Settings**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\AoT_default<ITEMTYPE>.yml`
- **Custom Settings**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\custom\`
- **Addon**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\addon\`
- **Exclude Lists**: `SKSE\Plugins\AlchemyOfTime\<ITEMTYPE>\exclude\`
- **Form Groups**: `SKSE/Plugins/AlchemyOfTime/formGroups`

You can replace `<ITEMTYPE>` with the type of items you want to change (like FOOD, BOOK, etc.). Check `moduleskeyvals` in the [Settings.h file](https://github.com/QY-MODS/AlchemyOfTime/blob/main/include/Settings.h) for the full list.

---

## Final Thoughts

The **Alchemy of Time** mod gives you full control over how items behave in your game. You can:
- Set up global rules using **default settings**.
- Customize specific items with **custom settings**.
- Add extra effects using **addons** with **time modulators** and **transformers**.
- Exclude items you don’t want to change.
- Use form groups to keep things tidy.

Make sure to double-check your **indentations** so your files work correctly!

