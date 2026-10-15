#pragma once
namespace kindle {
// Run only in a freshly forked, single-threaded CLI child, before exec.
// Stock helpers must not inherit any launcher lock, including shell duplicates.
bool closeInheritedDescriptors(int keep = -1);
}  // namespace kindle
