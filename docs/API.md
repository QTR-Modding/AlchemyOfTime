# Read-only API

Call from the game thread after SKSE's `kDataLoaded`. Include `include/API.h` in your plugin:

```cpp
const auto api = AoTAPI::RequestAPI();
if (!api) return;

api->VisitInstances(ownerRefID, itemFormID,
    [](const AoTAPI::InstanceInfo* item, void*) {
        // Read item->count, item->timing and item->remainingHours here.
        return true;
    }, nullptr);
```

`ownerRefID` is the inventory owner's reference ID, or the item's reference ID for a world item. `itemFormID` is the current stage item's FormID. A callback receives each tracked stack separately, including stacks of the same item with different timers. Set either filter to zero to include all matching locations or items.

1. `VisitInstances(location, item, visitor, context)` reads tracked quantities, stages, active trigger, speed and timing. `source` identifies the original item whose rules apply. `startedAt` is the stage's starting game hour; `elapsed` and `duration` describe its active transformation, or its stage when not transforming. `sampledAt` and `nextUpdate` are absolute game hours; `nextUpdate` is AoT's scheduling boundary, while `timing` indicates whether an item transition can occur. `remainingHours` is clamped to zero for an overdue transition and is infinity when paused or without a scheduled transition; check `timing` to distinguish those cases. `progressPercentage` is 0..100, or -1 when unavailable. Stage progress decreases during reverse progression.
2. `VisitSources(source, visitor, context)` reads effective rules, including merged add-ons: stages, final item, transformations, time modifiers and their filters. Pass an original source FormID, or zero for every known source. Sources are created as AoT encounters items; this does not evaluate presets for untracked forms. A dynamic stage's `item` is zero until AoT creates it. Transformations and modifiers retain their configured priority order. Rule-level `containers` filters the rule; trigger-level `filters.containers` filters that trigger in inventories. Empty container filters are unrestricted. `filters.stages` lists allowed stage numbers.
3. `VisitLocations(visitor, context)` lists distinct inventory owners and world references with tracked stacks.

Calls are synchronous. Return `false` from a visitor to stop enumeration. The query itself returns `false` when AoT is not initialized, has been uninstalled, or the visitor is null; a successful query with no matches returns `true`. The context pointer is passed unchanged to your visitor.

Snapshot strings and arrays remain valid only during that callback. Copy any data you keep. Callbacks run after AoT releases its source lock, and may make another read-only query. Snapshots use plain structures and borrowed arrays; clients do not free their memory.

Queries read AoT's last recorded state and calculate timing at `sampledAt`. They do not register items, update inventories, create stage forms or change simulation. An overdue result can remain visible until AoT next updates that location.
