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

  bool IsValid() const { return valid_; };

  bool IsPathExists(absl::string_view& path) const;

  bool ChangeDirectory(absl::string_view path);

  const std::vector<DirectoryEntry>& CurrentDirectoryEntries() const {
    return current_dir_entries_;
  };

  std::string ReadFile(absl::string_view path);

  DirectoryEntry GetPathInfo(absl::string_view path);

 private:
  std::ifstream in_;
  bool valid_ = false;

  BiosParameterBlock bpb_;
  ExtendedBiosParameterBlock ebpb_;
  /* FileAllocationTable fat_; */
  FileSystemInformation fs_info_;
  std::vector<DirectoryEntry> root_dir_entries_;
  std::vector<DirectoryEntry> current_dir_entries_;
};

}  // namespace fat32
