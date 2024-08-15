#include "fat32.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "spdlog/spdlog.h"

namespace fat32 {

namespace {

// End Of Cluster Chain value
constexpr uint32_t kEocc = 0x0FFFFFF8;
// Bad Cluster value
constexpr uint32_t kBadCluster = 0x0FFFFFF7;

uint32_t ComposeCluster(uint16_t clusterHigh, uint16_t clusterLow) {
  return (static_cast<uint32_t>(clusterHigh) << 16) |
         static_cast<uint32_t>(clusterLow);
}

void DebugPrintDirectoryEntryInfo(DirectoryEntry entry) {
  spdlog::debug("Filename: {}", entry.filename);
  if (!entry.longFilename.empty()) {
    spdlog::debug("Long filename: {}", entry.longFilename);
  }

  spdlog::debug("Type: {}", entry.IsDirectory() ? "Directory" : "File");
  spdlog::debug("Attributes: {}{}{}{}{}{}", entry.IsReadOnly() ? 'R' : '-',
                entry.IsHidden() ? 'H' : '-', entry.IsSystem() ? 'S' : '-',
                entry.IsVolumeIdEntry() ? 'V' : '-',
                entry.IsDirectory() ? 'D' : '-', entry.IsArchive() ? 'A' : '-');
  spdlog::debug("Creation datetime: {}", entry.CreationDatetime());
  spdlog::debug("Last modification datetime: {}",
                entry.LastModificationDatetime());
  spdlog::debug("Last accessed date: {}", entry.LastAccessedDate());

  spdlog::debug("First cluster: 0x{:X} ({:X}, {:X})",
                ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow),
                entry.firstClusterHigh, entry.firstClusterLow);
  spdlog::debug("Size (in bytes): {}", entry.size);
}

void DebugPrintTitle(const std::string &title) {
  int padding = (30 - title.size() - 2) / 2;
  std::string border(padding, '=');
  spdlog::debug("{} {} {}", border, title, border);
}

void DebugPrintBPBInfo(const BiosParameterBlock &bpb) {
  DebugPrintTitle("BPB");
  if (bpb.jmp[0] == 0xEB && bpb.jmp[2] == 0x90) {
    spdlog::debug("Jump instruction code: 0x{:X} 0x{:X} 0x{:X}", bpb.jmp[0],
                  bpb.jmp[1], bpb.jmp[2]);
  }

  spdlog::debug("OEM Identifier: {}", bpb.oem);
  spdlog::debug("Bytes per sector: {}", bpb.bytesPerSector);
  spdlog::debug("Sectors per cluster: {}", bpb.sectorsPerCluster);
  spdlog::debug("Reserved sectors: {}", bpb.reservedSectors);
  spdlog::debug("rootDirectoryEntries16: {}", bpb.rootDirectoryEntries16);
  spdlog::debug("Number of FATs: {}", bpb.countFats);
  spdlog::debug("Number of total sectors: {}", bpb.sectorsCount32);
  spdlog::debug("Media descriptor type: 0X{}", bpb.mediaDescriptorType);
  spdlog::debug("Number of sectors per track: {}", bpb.sectorsPerTrack);
  spdlog::debug("Number of heads on the disk: {}", bpb.headsCount);
  spdlog::debug("Number of hidden sectors: {}", bpb.hiddenSectors);
}

void DebugPrintEBPBInfo(const ExtendedBiosParameterBlock &ebpb) {
  DebugPrintTitle("EBPB");
  spdlog::debug("Sectors per FAT: {}", ebpb.sectorsPerFAT);
  spdlog::debug("Flags: {:X}", ebpb.flags);
  spdlog::debug("FAT version number: {:X}.{:X}",
                (ebpb.FATVersion & 0xff00) >> 8, ebpb.FATVersion & 0xff);
  spdlog::debug("Root directory cluster: {:X}", ebpb.rootDirCluster);
  spdlog::debug("FSInfo sector: {:X}", ebpb.FSInfoSector);
  spdlog::debug("Backup Boot Sector: {:X}", ebpb.backupBootSector);
  spdlog::debug("Drive type: {} (0x{:X})",
                (ebpb.driveNumber == 0
                     ? "Floppy"
                     : (ebpb.driveNumber == 0x80 ? "Hard Disk" : "Other")),
                ebpb.driveNumber);
  if (ebpb.signature == 0x28 || ebpb.signature == 0x29) {
    spdlog::debug("EBPB signature found: 0x{:X}", ebpb.signature);
  }
  spdlog::debug("Volume ID: {}", ebpb.volumeId);
  spdlog::debug("Volume Label: {}", ebpb.volumeLabel);
  spdlog::debug("System identifier: {}", ebpb.systemType);
}

void DebugPrintFSInfo(const FileSystemInformation &fsInfo) {
  DebugPrintTitle("FSInfo");
  constexpr uint32_t kLeadSignature = 0x41615252;
  constexpr uint32_t kStructSignature = 0x61417272;
  constexpr uint32_t kTrailSignature = 0xAA550000;
  spdlog::debug("Top signature {}", fsInfo.leadSignature == kLeadSignature
                                        ? "matches!"
                                        : "doesn't match!");
  spdlog::debug("Middle signature {}",
                fsInfo.structSignature == kStructSignature ? "matches!"
                                                           : "doesn't match!");
  spdlog::debug("Last known free cluster count: {}", fsInfo.freeClusters);
  spdlog::debug("Available clusters start: 0x{:X}",
                fsInfo.availableClusterStart);
  spdlog::debug("Bottom signature {}",
                (fsInfo.trailSignature == kTrailSignature ? "matches!"
                                                          : "doesn't match!"));
}

inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

template <class T>
void ReadSized(T *result, std::ifstream &in);

template <>
void ReadSized(uint8_t *result, std::ifstream &in) {
  in.read(reinterpret_cast<char *>(result), 1);
}

template <>
void ReadSized<>(uint16_t *result, std::ifstream &in) {
  uint16_t little_endian_data;
  in.read(reinterpret_cast<char *>(&little_endian_data), 2);
  *result = le16toh(little_endian_data);
}

template <>
void ReadSized(uint32_t *result, std::ifstream &in) {
  uint32_t little_endian_data;
  in.read(reinterpret_cast<char *>(&little_endian_data), 4);
  *result = le32toh(little_endian_data);
}

// Convert little-endian to host byte order.
template <class T>
void Read(T *result, std::ifstream &in) {
  ReadSized(result, in);
}

void ReadBPB(BiosParameterBlock *bpb, std::ifstream &in) {
  in.read((char *)bpb->jmp, 3);
  in.read((char *)bpb->oem, 8);
  bpb->oem[8] = '\0';
  Read(&bpb->bytesPerSector, in);
  Read(&bpb->sectorsPerCluster, in);
  Read(&bpb->reservedSectors, in);
  Read(&bpb->countFats, in);
  Read(&bpb->rootDirectoryEntries16, in);
  Read(&bpb->sectorsCount16, in);
  Read(&bpb->mediaDescriptorType, in);
  Read(&bpb->sectorsPerFAT16, in);
  Read(&bpb->sectorsPerTrack, in);
  Read(&bpb->headsCount, in);
  Read(&bpb->hiddenSectors, in);
  Read(&bpb->sectorsCount32, in);
}

uint32_t GetNextCluster(std::ifstream &in, const BiosParameterBlock &bpb,
                        uint32_t cluster) {
  const uint64_t firstFATSector = bpb.reservedSectors;
  // Each cluster address is 4 bytes in FAT32.
  const uint64_t offset = static_cast<uint64_t>(cluster) * 4;
  uint32_t result;

  in.seekg(firstFATSector * static_cast<uint64_t>(bpb.bytesPerSector) + offset);
  Read(&result, in);

  return result & 0x0FFFFFFF;  // only 28 bits are used
}

uint64_t GetClusterAddress(const BiosParameterBlock &bpb,
                           const ExtendedBiosParameterBlock &ebpb,
                           const uint32_t cluster) {
  const uint64_t firstDataSector =
      bpb.reservedSectors + (bpb.countFats * ebpb.sectorsPerFAT);
  return (static_cast<uint64_t>(cluster - 2) *
              static_cast<uint64_t>(bpb.sectorsPerCluster) +
          firstDataSector) *
         static_cast<uint64_t>(bpb.bytesPerSector);
}

// return size of read data.
uint32_t ReadFile(const BiosParameterBlock &bpb,
                  const ExtendedBiosParameterBlock &ebpb,
                  const DirectoryEntry &entry, std::ifstream &in,
                  const uint32_t offset, const uint32_t size,
                  std::ostream &out_stream) {
  if (offset > entry.size) {
    return 0;
  }

  DebugPrintDirectoryEntryInfo(entry);
  const uint32_t first_cluster =
      ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow);
  const uint32_t bytes_per_cluster = bpb.sectorsPerCluster * bpb.bytesPerSector;

  uint32_t current_cluster = first_cluster;
  uint32_t bytes_to_read = std::min(size, entry.size - offset);
  uint32_t size_read = 0;
  uint32_t pos = 0;

  while (true) {
    in.seekg(GetClusterAddress(bpb, ebpb, current_cluster));
    if (pos + bytes_per_cluster > offset) {
      uint32_t size_to_read;
      if (pos < offset) {
        // on first reading, the head address of the cluster may be
        // smaller than offset.
        in.ignore(offset - pos);
        size_to_read =
            std::min(bytes_to_read, bytes_per_cluster - (offset - pos));
      } else {
        size_to_read = std::min(bytes_to_read, bytes_per_cluster);
      }
      std::vector<char> buffer(size_to_read);
      in.read(buffer.data(), size_to_read);
      out_stream.write(buffer.data(), size_to_read);
      bytes_to_read -= size_to_read;
      size_read += size_to_read;
      // assert(in.is_open());
    }
    pos += bytes_per_cluster;

    if (bytes_to_read == 0) {
      spdlog::debug("[EOF] read all data");
      break;
    }

    uint32_t next_cluster = GetNextCluster(in, bpb, current_cluster);
    if (next_cluster >= kEocc) {
      spdlog::debug("[EOF] end of cluster");
      break;
    } else if (next_cluster == kBadCluster) {
      spdlog::warn("[EOF] bad cluster - stopping");
      break;
    }
    current_cluster = next_cluster;
  }
  return size_read;
}

void ReadLongFilename(std::ifstream &in, std::string &buffer, int length) {
  int i = 0;
  for (; i < length / 2; i++) {
    unsigned char c;
    Read(&c, in);
    if (c == 0x00 || c == 0xFF) {
      break;
    };
    buffer.push_back(c);
    in.ignore();
  }

  // On name terminated before reading all the bytes, ignore the rest.
  in.ignore(length - i * 2 - 1);
}

bool ReadDirectoryEntry(std::ifstream &in,
                        std::vector<DirectoryEntry> &result) {
  constexpr uint8_t ATTR_READ_ONLY = 0x01;
  constexpr uint8_t ATTR_HIDDEN = 0x02;
  constexpr uint8_t ATTR_SYSTEM = 0x04;
  constexpr uint8_t ATTR_VOLUME_ID = 0x08;
  constexpr uint8_t ATTR_DIRECTORY = 0x10;
  constexpr uint8_t ATTR_ARCHIVE = 0x20;
  constexpr uint8_t ATTR_LONG_NAME =
      ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID;
  constexpr uint8_t ATTR_LONG_NAME_MASK = ATTR_READ_ONLY | ATTR_HIDDEN |
                                          ATTR_SYSTEM | ATTR_VOLUME_ID |
                                          ATTR_DIRECTORY | ATTR_ARCHIVE;
  constexpr uint8_t kFreeEntryIndicator = 0xE5;
  constexpr uint8_t kEndOfEntriesIndicator = 0x00;

  std::vector<std::string> longNameEntries;

  // First, assume starting with long filename directory entry.
  while (true) {
    uint64_t origin = in.tellg();

    unsigned char first_byte;
    Read(&first_byte, in);
    if (first_byte == kEndOfEntriesIndicator) {
      return false;
    };
    if (first_byte == kFreeEntryIndicator) {
      in.seekg(origin + 32L);
      continue;
    }

    std::string long_filename;
    // LDIR_Name1
    ReadLongFilename(in, long_filename, 10);

    unsigned char attr;
    Read(&attr, in);

    if ((attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) {
      LongFileNameDirectoryEntry entry;
      entry.order = first_byte;
      in.ignore();  // LDIR_Type must be zero
      Read(&entry.checksum, in);
      // LDIR_Name2
      ReadLongFilename(in, long_filename, 12);
      in.ignore(2);  // LDIR_FstClusLO must be zero
      // LDIR_Name2
      ReadLongFilename(in, long_filename, 4);

      const bool is_last_long_entry = (entry.order & 0x40) == 0x40;
      if (is_last_long_entry) {
        // There may be multiple "last" long entries, which should be
        // dropped except the last one.
        longNameEntries.clear();
      }

      longNameEntries.push_back(long_filename);
    } else {
      // End of long filename directory entry.
      in.seekg(origin);  // go back to read the data again as directory entry.
      break;
    }
  }

  // Second, read the actual directory entry.
  DirectoryEntry entry;
  in.read(entry.filename, 11);

  entry.filename[11] = '\0';
  Read(&entry.attributes, in);
  in.ignore();  // Reserved DIR_NTRes, must be 0.
  Read(&entry.creationTimeHS, in);
  Read(&entry.creationTime, in);
  Read(&entry.creationDate, in);
  Read(&entry.lastAccessedDate, in);
  Read(&entry.firstClusterHigh, in);
  Read(&entry.lastModificationTime, in);
  Read(&entry.lastModificationDate, in);
  Read(&entry.firstClusterLow, in);
  Read(&entry.size, in);

  if (!longNameEntries.empty()) {
    // iterate through the entries backwards and add them to string
    std::string filename;

    for (auto it = longNameEntries.crbegin(); it != longNameEntries.crend();
         ++it) {
      filename += *it;
    }

    entry.longFilename = filename;
  }

  entry.name = !entry.longFilename.empty() ? entry.longFilename
                                           : std::string(entry.filename);
  rtrim(entry.name);

  result.push_back(entry);

  return true;
}

void ReadDirectory(std::ifstream &in, const BiosParameterBlock &bpb,
                   const ExtendedBiosParameterBlock &ebpb, uint32_t cluster,
                   std::vector<DirectoryEntry> &entries) {
  uint64_t cluster_addr = GetClusterAddress(bpb, ebpb, cluster);
  in.seekg(cluster_addr);

  while (true) {
    bool has_next_entry = ReadDirectoryEntry(in, entries);
    // TODO: Delete this debug guard.
    if (entries.size() > 50) {
      spdlog::error("error while reading directory");
      break;
    }
    if (!has_next_entry) {
      break;
    };

    // If we read the whole cluster, go on to the next.
    if (static_cast<uint64_t>(in.tellg()) - cluster_addr >=
        bpb.sectorsPerCluster * bpb.bytesPerSector) {
      cluster = GetNextCluster(in, bpb, cluster);
      cluster_addr = GetClusterAddress(bpb, ebpb, cluster);
      in.seekg(cluster_addr);
      spdlog::debug("go on next cluster: {:X}, addr: {:X}", cluster,
                    cluster_addr);
    }
  }
}

void ReadEBPB(ExtendedBiosParameterBlock *ebpb, std::ifstream &in) {
  Read(&ebpb->sectorsPerFAT, in);
  Read(&ebpb->flags, in);
  Read(&ebpb->FATVersion, in);
  Read(&ebpb->rootDirCluster, in);
  Read(&ebpb->FSInfoSector, in);
  Read(&ebpb->backupBootSector, in);
  in.ignore(12);  // Reserved
  Read(&ebpb->driveNumber, in);
  in.ignore(1);  // Reserved
  Read(&ebpb->signature, in);
  Read(&ebpb->volumeId, in);
  in.read((char *)&ebpb->volumeLabel, 11);
  ebpb->volumeLabel[11] = '\0';
  in.read((char *)&ebpb->systemType, 8);
  ebpb->systemType[8] = '\0';
  in.ignore(420);  // Boot code
  in.ignore(2);    // Bootable partition signature (0xAA55)
}

void ReadFSInfo(const BiosParameterBlock &bpb,
                const ExtendedBiosParameterBlock &ebpb,
                FileSystemInformation *fsInfo, std::ifstream &in) {
  in.seekg(ebpb.FSInfoSector *
           bpb.bytesPerSector);  // seek to FSInfo start location
  Read(&fsInfo->leadSignature, in);
  in.ignore(480);  // Reserved
  Read(&fsInfo->structSignature, in);
  Read(&fsInfo->freeClusters, in);
  Read(&fsInfo->availableClusterStart, in);
  in.ignore(12);  // Reserved
  Read(&fsInfo->trailSignature, in);
}

bool IsBpbValid(const BiosParameterBlock &bpb) {
  return bpb.rootDirectoryEntries16 == 0 && bpb.sectorsCount16 == 0 &&
         bpb.sectorsPerFAT16 == 0 && bpb.sectorsCount32 != 0;
}

bool IsEbpbValid(const BiosParameterBlock &bpb,
                 const ExtendedBiosParameterBlock &ebpb) {
  uint32_t dataSectors =
      bpb.sectorsCount32 -
      (bpb.reservedSectors + (bpb.countFats * ebpb.sectorsPerFAT));
  uint32_t totalClusters = dataSectors / bpb.sectorsPerCluster;
  return totalClusters >= 65525;
}

bool GetSubDirectories(std::vector<DirectoryEntry> &current_dir_entries,
                       const absl::string_view &sub_dir_name, std::ifstream &in,
                       const BiosParameterBlock &bpb,
                       const ExtendedBiosParameterBlock &ebpb) {
  for (const DirectoryEntry &entry : current_dir_entries) {
    if (entry.name != sub_dir_name) {
      continue;
    }

    if (!entry.IsDirectory()) {
      return false;
    }

    DebugPrintDirectoryEntryInfo(entry);
    const uint32_t first_cluster =
        ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow);
    current_dir_entries.clear();
    ReadDirectory(in, bpb, ebpb, first_cluster, current_dir_entries);
    return true;
  }

  return false;
}

template <typename CharT, typename TraitsT = std::char_traits<CharT> >
class CharArrayBuffer : public std::basic_streambuf<CharT, TraitsT> {
 public:
  CharArrayBuffer(CharT *base, uint32_t size) { this->setp(base, base + size); }
};

}  // namespace

FileSystem::FileSystem(const std::string &image_file)
    : image_file_(image_file) {
  Initialize(image_file);
}

bool FileSystem::Refresh() {
  in_.close();

  spdlog::debug("refreshing");

  valid_ = false;
  current_path_.clear();
  bpb_ = BiosParameterBlock();
  ebpb_ = ExtendedBiosParameterBlock();
  fs_info_ = FileSystemInformation();
  root_dir_entries_.clear();
  current_dir_entries_.clear();

  Initialize(image_file_);

  if (!valid_) {
    spdlog::debug("not valid after refreshing!");
  }

  return valid_;
}

void FileSystem::Initialize(const std::string &image_file) {
  in_.open(image_file, std::ios::binary);
  if (!in_) {
    spdlog::error("failed to read fat32 image file {}", image_file);
    valid_ = false;
    return;
  }

  ReadBPB(&bpb_, in_);
  DebugPrintBPBInfo(bpb_);
  if (bpb_.jmp[0] == 0xEB && bpb_.jmp[2] == 0x90) {
    spdlog::debug("FAT image detected (by JMP signature)");
  } else {
    spdlog::error("image does not have the correct JMP signature.");
    spdlog::error("probably not a valid FAT image.");
    valid_ = false;
    return;
  }

  if (!IsBpbValid(bpb_)) {
    spdlog::error("invalid BPB");
    valid_ = false;
    return;
  }

  ReadEBPB(&ebpb_, in_);
  DebugPrintEBPBInfo(ebpb_);
  if (!IsEbpbValid(bpb_, ebpb_)) {
    spdlog::error("invalid EBPB");
    valid_ = false;
    return;
  }

  ReadFSInfo(bpb_, ebpb_, &fs_info_, in_);
  DebugPrintFSInfo(fs_info_);

  ReadDirectory(in_, bpb_, ebpb_, ebpb_.rootDirCluster, root_dir_entries_);
  current_dir_entries_ = root_dir_entries_;
  current_path_ = "";

  valid_ = true;
}

bool FileSystem::ExportFile(absl::string_view path,
                            const std::string &export_path) {
  std::ofstream ofs(export_path, std::ios::binary);
  return ReadFile(path, ofs);
};

bool FileSystem::ReadFile(absl::string_view path, std::string *content) {
  const DirectoryEntry *dir_entry = FindDirectoryEntry(path);
  if (dir_entry == nullptr || dir_entry->IsDirectory()) {
    spdlog::debug("file not exists or is a directory: {}", path);
    return false;
  }

  content->resize(dir_entry->size);
  CharArrayBuffer buf(content->data(), dir_entry->size);
  std::ostream os(&buf);
  return ReadFile(*dir_entry, os);
}

bool FileSystem::ReadFile(absl::string_view path, std::ostream &os) {
  const DirectoryEntry *dir_entry = FindDirectoryEntry(path);
  if (dir_entry == nullptr || dir_entry->IsDirectory()) {
    spdlog::debug("file not exists or is a directory: {}", path);
    return false;
  }

  return ReadFile(*dir_entry, os);
}

uint32_t FileSystem::ReadFile(const DirectoryEntry &entry, uint32_t offset,
                              uint32_t size, char *out) {
  CharArrayBuffer buf(out, size);
  std::ostream os(&buf);
  return fat32::ReadFile(bpb_, ebpb_, entry, in_, offset, size, os);
}

bool FileSystem::ReadFile(const DirectoryEntry &entry, std::ostream &os) {
  return fat32::ReadFile(bpb_, ebpb_, entry, in_, 0, entry.size, os) ==
         entry.size;
}

bool FileSystem::ChangeDirectory(absl::string_view path, bool parent) {
  const char kPathDelimeter = '/';
  if (parent) {
    const auto &pos = path.find_last_of(kPathDelimeter);
    if (pos != std::string::npos) {
      path = path.substr(0, pos);
    } else {
      path = "";
    }
  }

  if (current_path_ == path) {
    return true;
  }

  spdlog::debug("change diretory to {}{}", path, parent ? "/.." : "");

  if (path == "") {
    // root directory
    current_path_ = "";
    current_dir_entries_ = root_dir_entries_;
    return true;
  }

  std::vector<absl::string_view> path_segments =
      absl::StrSplit(path, kPathDelimeter);
  current_dir_entries_ = root_dir_entries_;
  for (const std::string_view &dir_name : path_segments) {
    if (!GetSubDirectories(current_dir_entries_, dir_name, in_, bpb_, ebpb_)) {
      spdlog::debug("not dir {} under {}", dir_name, current_path_);
      current_path_ = "";
      current_dir_entries_ = root_dir_entries_;
      return false;
    }
  }
  current_path_ = path;
  return true;
}

const DirectoryEntry *FileSystem::FindDirectoryEntry(
    absl::string_view path) const {
  const char kPathDelimeter = '/';
  const auto pos = path.find_last_of(kPathDelimeter);
  absl::string_view filename =
      pos != absl::string_view::npos ? path.substr(pos + 1) : path;
  const auto &entries = CurrentDirectoryEntries();
  auto it =
      std::find_if(entries.cbegin(), entries.cend(),
                   [&filename](auto entry) { return entry.name == filename; });
  if (it == entries.end()) {
    return nullptr;
  }
  return &(*it);
}

}  // namespace fat32
