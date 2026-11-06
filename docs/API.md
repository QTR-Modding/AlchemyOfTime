# Read-only API

Call from the game thread after SKSE's `kDataLoaded`:

```cpp
#include "API.h"

const auto api = AoTAPI::RequestAPI();
float hours;
if (api && api->GetRemainingTime(ownerRefID, itemFormID, hours)) {
    // hours is the time until the item's next AoT update.
}
```

`ownerRefID` is the inventory owner's reference ID, or the item's reference ID for a world item. `itemFormID` is the current stage item's FormID. Both must be nonzero.

The result is in game hours: zero means overdue, infinity means no active countdown. If multiple tracked stacks match, the soonest update is returned. A paused stack does not hide another stack's active countdown. The call returns `false` when no matching item is tracked or AoT is unavailable, leaving `hours` unchanged.

This reads AoT's recorded state. It does not register items, update inventories or advance simulation. An overdue item remains overdue until AoT processes its location.
