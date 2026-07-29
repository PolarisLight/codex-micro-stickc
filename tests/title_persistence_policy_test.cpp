#include "TitlePersistencePolicy.h"

static_assert(!titleNeedsPersistence("same", "same"),
              "unchanged titles do not write NVS");
static_assert(titleNeedsPersistence("old", "new"),
              "changed titles are persisted");
static_assert(titleNeedsPersistence("", "task"),
              "new assignments are persisted");
static_assert(titleNeedsPersistence("task", ""),
              "removed assignments are persisted");

int main() { return 0; }
