#include "TitleUiMode.h"

static_assert(!nextTitleUiActive(false, false), "boot stays status-only");
static_assert(nextTitleUiActive(false, true), "first packet enables titles");
static_assert(nextTitleUiActive(true, false), "title mode remains latched");
static_assert(nextTitleUiActive(true, true), "additional packets keep titles");
static_assert(taskStatusAssigned(false, false),
              "status-only mode never reports unassigned");
static_assert(!taskStatusAssigned(true, false),
              "title mode reports an empty title as unassigned");
static_assert(taskStatusAssigned(true, true),
              "title mode reports a populated title as assigned");

int main() { return 0; }
