#pragma once

constexpr bool nextTitleUiActive(bool active, bool validLabelsPacket) {
  return active || validLabelsPacket;
}

constexpr bool taskStatusAssigned(bool titleUiActive, bool labelAssigned) {
  return !titleUiActive || labelAssigned;
}
