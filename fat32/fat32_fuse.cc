#include "fat32_fuse.h"

#include <string>

#include "spdlog/spdlog.h"

#define FUSE_USE_VERSION 31
#include <fuse.h>

namespace fat32 {

namespace fuse {

static FileSystem *fs = nullptr;

static int getattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info * /*fi*/) {
  spdlog::debug("getattr: {}", path);

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else {
    std::string filename(path + 1);  // +1 to skip the leading '/'.

    if (!fs->Refresh()) {
      return -EAGAIN;
    }
    fs->ChangeDirectory(filename, true);

    const char kPathDelimeter = '/';
    const auto &pos = filename.find_last_of(kPathDelimeter);
    if (pos != std::string::npos) {
      filename = filename.substr(pos + 1);
    }

    const auto &entries = fs->CurrentDirectoryEntries();
    const auto &it = std::find_if(
        entries.cbegin(), entries.cend(),
        [&filename](const auto &entry) { return entry.name == filename; });
    if (it == entries.cend()) {
      return -ENOENT;
    }
    if (it->IsDirectory()) {
      stbuf->st_mode = S_IFDIR | 0555;
    } else {
      stbuf->st_mode = S_IFREG | 0444;
    }
    stbuf->st_nlink = 1;
    stbuf->st_size = it->size;
    stbuf->st_mtim.tv_sec = it->LastModificationDatetime().ToTimestamp();
    stbuf->st_ctim.tv_sec = it->CreationDatetime().ToTimestamp();
  }
  return 0;
}

static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t /*offset*/, struct fuse_file_info * /*fi*/,
                   enum fuse_readdir_flags /*flags*/) {
  spdlog::debug("readdir: {}", path);

  if (strcmp(path, "/") == 0) {
    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);
  }

  std::string path_str(path + 1);  // +1 to skip the leading '/'.
  // TODO: Limit refresh rate.
  if (!fs->Refresh()) {
    return -EAGAIN;
  }
  fs->ChangeDirectory(path_str);
  for (const auto &entry : fs->CurrentDirectoryEntries()) {
    filler(buf, entry.name.data(), nullptr, 0, FUSE_FILL_DIR_PLUS);
  }
  return 0;
}

// int open(const char *path, struct fuse_file_info *fi) {
//   // if (files.find(path) == files.end()) return -ENOENT;
//   if ((fi->flags & 3) != O_RDONLY) {
//     return -EACCES;
//   };
//   return 0;
// }

int read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info * /*fi*/) {
  spdlog::debug("read: {}", path);

  std::string path_str(path + 1);
  if (!fs->Refresh()) {
    return -EAGAIN;
  }
  fs->ChangeDirectory(path_str, true);
  const DirectoryEntry *entry = fs->FindDirectoryEntry(path_str);
  if (entry == nullptr) {
    return -ENOENT;
  }
  if (entry->IsDirectory()) {
    return -EISDIR;
  }

  if (offset >= entry->size) {
    return 0;
  }

  if (offset + size > entry->size) {
    size = entry->size - offset;
  }

  return fs->ReadFile(*entry, offset, size, buf);
}

static struct fuse_operations operations {
  .getattr = getattr, .read = read, .readdir = readdir,
  // .open = this->open,
};

}  // namespace fuse

bool MountFat32(fat32::FileSystem &fat32_fs, absl::string_view mount_path) {
  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

  fuse_opt_add_arg(&args, "fat32fuse");
  // fuse_opt_add_arg(&args, "-h");
  fuse_opt_add_arg(&args, mount_path.data());
  fuse_opt_add_arg(&args, "-f");  // run in foreground
  fuse_opt_add_arg(&args, "-s");  // single thread
  // fuse_opt_add_arg(&args, "-oentry_timeout=0");
  // fuse_opt_add_arg(&args, "-oattr_timeout=0");

  fuse::fs = &fat32_fs;

  int ret = fuse_main(args.argc, args.argv, &fuse::operations, nullptr);
  fuse_opt_free_args(&args);
  return ret == 0;
}

}  // namespace fat32
