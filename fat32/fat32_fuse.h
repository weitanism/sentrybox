#pragma once

#include "absl/strings/string_view.h"
#include "fat32.h"

namespace fat32 {

bool MountFat32(fat32::FileSystem& fat32_fs, absl::string_view mount_path);

}  // namespace fat32
