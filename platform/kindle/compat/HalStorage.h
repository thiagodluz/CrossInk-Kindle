#pragma once
#include "Arduino.h"
// Keep the reader-facing contract, implementing it with anchored POSIX fds.
#include "../../../lib/hal/HalStorage.h"
namespace kindle {
bool setStorageRoot(const char* root);
}
