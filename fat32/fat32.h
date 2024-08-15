#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "types.h"

namespace fat32 {

// The FileSystem provides APIs to get info from FAT32 image file.
// References:
// 1. https://github.com/Vitaspiros/FATReader
// 2. https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
// 3. https://wiki.osdev.org/FAT#FAT_32
// 4. https://www.cs.uni.edu/~diesburg/courses/cop4610_fall10/
class FileSystem {
 public:
  FileSystem(const std::string& image_file);

  bool Refresh();

  bool IsValid() const { return valid_; };

  bool IsPathExists(absl::string_view& path) const;

  bool ChangeDirectory(absl::string_view path, bool parent = false);

  const std::vector<DirectoryEntry>& CurrentDirectoryEntries() const {
    return current_dir_entries_;
  };

  const DirectoryEntry* FindDirectoryEntry(absl::string_view path) const;

  bool ExportFile(absl::string_view path, const std::string& export_path);

  bool ReadFile(absl::string_view path, std::ostream& os);

  bool ReadFile(absl::string_view path, std::string* content);

  bool ReadFile(absl::string_view path, char* out);

  size_t ReadFile(const DirectoryEntry& entry, size_t offset, size_t size,
                  char* out);

  DirectoryEntry GetPathInfo(absl::string_view path);

 private:
  void Initialize(const std::string& image_file);

  bool ReadFile(const DirectoryEntry& entry, std::ostream& os);

 private:
  const std::string& image_file_;
  std::ifstream in_;
  bool valid_ = false;
  std::string current_path_;

  BiosParameterBlock bpb_;
  ExtendedBiosParameterBlock ebpb_;
  /* FileAllocationTable fat_; */
  FileSystemInformation fs_info_;
  std::vector<DirectoryEntry> root_dir_entries_;
  std::vector<DirectoryEntry> current_dir_entries_;
  };

}  // namespace fat32
