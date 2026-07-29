#pragma once

constexpr bool titleNeedsPersistence(const char* current, const char* next) {
  return *current != *next
             ? true
             : (*current == '\0'
                    ? false
                    : titleNeedsPersistence(current + 1, next + 1));
}
