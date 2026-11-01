# Native API

Include the canonical `include/API.h`. Call this interface on the game thread.

## Numeric keywords

Use `GetValueKeyword` during `kDataLoaded` or later to create or reuse the keyword
for a finite number. This works before AoT finishes initialization, so another
plugin can distribute keywords in its own data-loaded handler:

```cpp
#include "API.h"

RE::FormID GetCookingKeyword(float hours) {
    const auto api = AoTAPI::RequestAPI();
    return api ? api->GetValueKeyword(hours) : 0;
}
```

The returned FormID identifies a keyword. Assign it through your normal keyword
distribution code. The function accepts zero and negative values as well as
positive values; non-finite values return zero. AoT owns keyword creation, reuse
and save/load remapping. Do not delete the keyword.

## Reading progress

After AoT initializes, this example reads each tracked group of an item in one
inventory:

```cpp
#include "API.h"

void ReadProgress(RE::FormID owner, RE::FormID item) {
    const auto api = AoTAPI::RequestAPI();
    if (!api) return;
    api->VisitInstances(owner, item, nullptr,
        [](const AoTAPI::InstanceInfo* instance, void*) {
            // Read count, remainingHours and progressPercent here.
        });
}
```

`owner` is the inventory owner's reference FormID; `item` is the current item
base FormID. The visitor runs once per matching tracked group, so items with
different progress remain separate. It runs synchronously after AoT releases
its data lock. Copy any information you need; do not retain the passed pointer.

`source` identifies the original item whose rule is running; `current` identifies
the current stage item, and `stage` is its stage number. `count` is the quantity
sharing this progress.

`remainingHours` describes the next transition in game hours, with zero meaning
due and `-1` meaning no scheduled transition. `progressPercent` is between zero
and 100, or `-1` when unavailable. `next` is the next result's FormID, or zero
when there is no next result or its form has not been created. `state` contains the Transforming, Paused and
Reversing flags; `multiplier` is the current progression multiplier.

A successful read returns true even if nothing matched. False means invalid
arguments or AoT is unavailable, loading or uninstalled. The query does not
register items, refresh triggers or advance the simulation.
