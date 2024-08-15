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
constexpr int kEocc = 0x0FFFFFF8;
// Bad Cluster value
constexpr int kBadCluster = 0x0FFFFFF7;

int ComposeCluster(unsigned short clusterHigh, unsigned short clusterLow) {
  return (clusterHigh << 16) | clusterLow;
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

  spdlog::debug("First cluster: 0x{:X}",
                ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow));
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
  constexpr u_int32_t kLeadSignature = 0x41615252;
  constexpr u_int32_t kStructSignature = 0x61417272;
  constexpr u_int32_t kTrailSignature = 0xAA550000;
  spdlog::debug("Top signature {}", fsInfo.leadSignature == kLeadSignature
                                        ? "matches!"
                                        : "doesn't match!");
  spdlog::debug("Middle signature {}",
                fsInfo.structSignature == kStructSignature ? "matches!"
                                                           : "doesn't match!");
  spdlog::debug("Last known free cluster count: 0x{:X}", fsInfo.freeClusters);
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
void ReadSized(u_int8_t *result, std::ifstream &in) {
  in.read(reinterpret_cast<char *>(result), 1);
}

template <>
void ReadSized<>(u_int16_t *result, std::ifstream &in) {
  u_int16_t little_endian_data;
  in.read(reinterpret_cast<char *>(&little_endian_data), 2);
  *result = le16toh(little_endian_data);
}

template <>
void ReadSized(u_int32_t *result, std::ifstream &in) {
  u_int32_t little_endian_data;
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

int GetNextCluster(std::ifstream &in, const BiosParameterBlock &bpb,
                   int cluster) {
  const int firstFATSector = bpb.reservedSectors;
  const int offset = cluster * 4;  // Each cluster address is 4 bytes in FAT32
  u_int32_t result;

  in.seekg(firstFATSector * bpb.bytesPerSector + offset);
  Read(&result, in);

  return result & 0x0FFFFFFF;  // only 28 bits are used
}

int GetClusterAddress(const BiosParameterBlock &bpb,
                      const ExtendedBiosParameterBlock &ebpb, int cluster) {
  const int firstDataSector =
      bpb.reservedSectors + (bpb.countFats * ebpb.sectorsPerFAT);
  return ((cluster - 2) * bpb.sectorsPerCluster + firstDataSector) *
         bpb.bytesPerSector;
}

// return size of read data.
size_t ReadFile(const BiosParameterBlock &bpb,
                const ExtendedBiosParameterBlock &ebpb,
                const DirectoryEntry &entry, std::ifstream &in,
                const size_t offset, const size_t size,
                std::ostream &out_stream) {
  if (offset > entry.size) {
    return 0;
  }

  DebugPrintDirectoryEntryInfo(entry);
  const int first_cluster =
      ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow);
  const size_t bytes_per_cluster = bpb.sectorsPerCluster * bpb.bytesPerSector;

  int current_cluster = first_cluster;
  size_t bytes_to_read = std::min(size, entry.size - offset);
  size_t size_read = 0;
  size_t pos = 0;

  while (true) {
    in.seekg(GetClusterAddress(bpb, ebpb, current_cluster));
    if (pos + bytes_per_cluster > offset) {
      if (pos < offset) {
        // on first reading, the head address of the cluster may be
        // smaller than offset.
        in.ignore(offset - pos);
      }
      const size_t size_to_read = std::min(bytes_to_read, bytes_per_cluster);
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

    int next_cluster = GetNextCluster(in, bpb, current_cluster);
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

void ReadLFNPart(std::ifstream &in, std::string &buffer, int length) {
  int i = 0;
  for (i = 0; i < length / 2; i++) {
    unsigned char c;
    Read(&c, in);
    if (c == 0x00 || c == 0xFF) break;
    buffer.push_back(c);
    in.ignore();
  }

  // If we finished before we read all the bytes, we should read them now
  in.ignore(length - i * 2 - 1);
}

bool ReadDirectoryEntry(std::ifstream &in,
                        std::vector<DirectoryEntry> &result) {
  std::vector<std::string> longNameEntries;
  bool run = false;

  do {
    unsigned char firstByte, eleventhByte;
    int origin = in.tellg();
    Read(&firstByte, in);
    in.seekg((int)in.tellg() + 10);
    Read(&eleventhByte, in);
    in.seekg(origin);  // go back, so we can read the data again

    // long filename entry
    if (eleventhByte == 0x0F) {
      std::string longNameBuffer;
      LongFileNameDirectoryEntry entry;
      Read(&entry.order, in);
      // We read the top name
      ReadLFNPart(in, longNameBuffer, 10);
      in.ignore();  // the eleventh byte: attributes
      Read(&entry.longEntryType, in);
      Read(&entry.checksum, in);
      // Middle name
      ReadLFNPart(in, longNameBuffer, 12);
      in.ignore(2);  // always zero
      // Bottom name
      ReadLFNPart(in, longNameBuffer, 4);

      longNameEntries.push_back(longNameBuffer);

      run = true;
      continue;
    }
    // test the first byte
    // end of directory
    if (firstByte == 0x00) return false;
    // unused entry
    if (firstByte == 0xE5) {
      // go to the next entry
      in.seekg((int)in.tellg() + 32);
      continue;
    }

    DirectoryEntry entry;
    in.read((char *)&entry.filename, 11);
    entry.filename[11] = '\0';
    Read(&entry.attributes, in);
    in.ignore();  // Reserved
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

      for (int i = longNameEntries.size() - 1; i >= 0; i--) {
        filename += longNameEntries.at(i);
      }

      entry.longFilename = filename;
    }
    entry.name =
        !entry.longFilename.empty() ? entry.longFilename : entry.filename;
    rtrim(entry.name);

    result.push_back(entry);
    run = false;

  } while (run);
  return true;
}

void ReadDirectory(std::ifstream &in, const BiosParameterBlock &bpb,
                   const ExtendedBiosParameterBlock &ebpb, int cluster,
                   std::vector<DirectoryEntry> &entries) {
  // const int firstFATSector = bpb.reservedSectors;
  // const int firstDataSector =
  //     firstFATSector + (bpb.countFats * ebpb.sectorsPerFAT);

  in.seekg(GetClusterAddress(bpb, ebpb, cluster));

  int origin = in.tellg();
  int current_cluster = cluster;

  while (true) {
    bool hasMore = ReadDirectoryEntry(in, entries);
    if (!hasMore) {
      break;
    };

    // If we Read the whole cluster, go on to the next
    if ((int)in.tellg() - origin >=
        bpb.sectorsPerCluster * bpb.bytesPerSector) {
      int next_cluster = GetNextCluster(in, bpb, current_cluster);
      int clusterAddress = GetClusterAddress(bpb, ebpb, next_cluster);
      cluster = next_cluster;
      in.seekg(clusterAddress);
      origin = clusterAddress;
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
  int dataSectors = bpb.sectorsCount32 - (bpb.reservedSectors +
                                          (bpb.countFats * ebpb.sectorsPerFAT));
  int totalClusters = dataSectors / bpb.sectorsPerCluster;
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

    const int first_cluster =
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
  CharArrayBuffer(CharT *base, size_t size) { this->setp(base, base + size); }
};

}  // namespace

FileSystem::FileSystem(const std::string &image_file)
    : image_file_(image_file) {
  Initialize(image_file);
}

bool FileSystem::Refresh() {
  if (in_) {
    in_.close();
  }

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

size_t FileSystem::ReadFile(const DirectoryEntry &entry, size_t offset,
                            size_t size, char *out) {
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
  std::size_t pos = path.find_last_of(kPathDelimeter);
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
