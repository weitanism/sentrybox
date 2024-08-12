#include "fat32.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace fat32 {

namespace {

// End Of Cluster Chain value
const int EOCC = 0x0FFFFFF8;
// Bad Cluster value
const int BAD_CLUSTER = 0x0FFFFFF7;

void PrintDate(const Date &date) {
  std::cout << (int)date.day << "/" << (int)date.month << "/"
            << (int)date.year + 1980;
}

void PrintDatetime(const Datetime &datetime) {
  std::cout << (int)datetime.day << "/" << (int)datetime.month << "/"
            << (int)datetime.year + 1980 << " " << std::fixed
            << std::setfill('0') << std::setw(2) << (int)datetime.hour << ":"
            << (int)datetime.minutes << ":" << (int)datetime.seconds;
}

int ComposeCluster(unsigned short clusterHigh, unsigned short clusterLow) {
  return (clusterHigh << 16) | clusterLow;
}

void PrintDirectoryEntryInfo(DirectoryEntry entry) {
  std::cout << "Filename: " << entry.filename << std::endl;
  if (!entry.longFilename.empty()) {
    std::cout << "Long filename: " << entry.longFilename << std::endl;
  }

  std::cout << "Type: " << (entry.IsDirectory() ? "Directory" : "File")
            << std::endl;
  std::cout << "Attributes: " << (entry.IsReadOnly() ? 'R' : '-')
            << (entry.IsHidden() ? 'H' : '-') << (entry.IsSystem() ? 'S' : '-')
            << (entry.IsVolumeIdEntry() ? 'V' : '-')
            << (entry.IsDirectory() ? 'D' : '-')
            << (entry.IsArchive() ? 'A' : '-') << std::endl;
  std::cout << "Creation datetime: ";
  PrintDatetime(entry.CreationDatetime());
  std::cout << std::endl << "Last modification datetime: ";
  PrintDatetime(entry.LastModificationDatetime());
  std::cout << std::endl << "Last accessed date: ";
  PrintDate(entry.LastAccessedDate());

  std::cout << std::endl
            << "First cluster: 0x" << std::hex << std::uppercase
            << ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow);
  std::cout << std::endl
            << "Size (in bytes): " << std::dec << std::nouppercase << entry.size
            << std::endl;
}

void PrintTitle(const std::string &title) {
  int padding = (60 - title.size() - 2) / 2;
  for (int i = 0; i < padding; i++) {
    std::cout << "=";
  }
  std::cout << " " << title << " ";
  for (int i = 0; i < padding; i++) {
    std::cout << "=";
  }
  std::cout << std::endl;
}

void PrintBPBInfo(const BiosParameterBlock &bpb) {
  PrintTitle("BPB");
  if (bpb.jmp[0] == 0xEB && bpb.jmp[2] == 0x90) {
    std::cout << "Jump instruction code found: " << std::hex << std::uppercase
              << (int)bpb.jmp[0] << ' ' << (int)bpb.jmp[1] << ' '
              << (int)bpb.jmp[2] << std::endl;
  }

  std::cout << std::dec << std::nouppercase;

  std::cout << "OEM Identifier: " << bpb.oem << std::endl;
  std::cout << "Bytes per sector: " << bpb.bytesPerSector << std::endl;
  std::cout << "Sectors per cluster: " << (int)bpb.sectorsPerCluster
            << std::endl;
  std::cout << "Reserved sectors: " << bpb.reservedSectors << std::endl;
  std::cout << "rootDirectoryEntries16: " << bpb.rootDirectoryEntries16
            << std::endl;
  std::cout << "Number of FATs: " << (int)bpb.countFats << std::endl;
  std::cout << "Number of total sectors: " << bpb.sectorsCount32 << std::endl;
  std::cout << "Media descriptor type: 0X" << std::hex << std::uppercase
            << (int)bpb.mediaDescriptorType << std::dec << std::nouppercase
            << std::endl;
  std::cout << "Number of sectors per track: " << bpb.sectorsPerTrack
            << std::endl;
  std::cout << "Number of heads on the disk: " << bpb.headsCount << std::endl;
  std::cout << "Number of hidden sectors: " << bpb.hiddenSectors << std::endl;
}

void PrintEBPBInfo(const ExtendedBiosParameterBlock &ebpb) {
  PrintTitle("EBPB");
  std::cout << "Sectors per FAT: " << ebpb.sectorsPerFAT << std::endl;
  std::cout << std::hex << std::uppercase << "Flags: " << ebpb.flags
            << std::endl;
  std::cout << std::dec << std::nouppercase
            << "FAT version number: " << ((ebpb.FATVersion & 0xff00) >> 8)
            << '.' << (ebpb.FATVersion & 0xff) << std::endl;
  std::cout << "Root directory cluster: " << ebpb.rootDirCluster << std::endl;
  std::cout << "FSInfo sector: " << ebpb.FSInfoSector << std::endl;
  std::cout << "Backup Boot Sector: " << ebpb.backupBootSector << std::endl;
  std::cout << "Drive type: "
            << (ebpb.driveNumber == 0
                    ? "Floppy"
                    : (ebpb.driveNumber == 0x80 ? "Hard Disk" : "Other"))
            << std::hex << " (0x" << ebpb.driveNumber << ")" << std::endl;
  if (ebpb.signature == 0x28 || ebpb.signature == 0x29) {
    std::cout << "EBPB signature found: 0x" << ebpb.signature << std::endl;
  }
  std::cout << "Volume ID: " << ebpb.volumeId << std::endl;
  std::cout << "Volume Label: " << ebpb.volumeLabel << std::endl;
  if (std::string(ebpb.systemType) == "FAT32   ") {
    std::cout << "System identifier correct: " << ebpb.systemType << std::endl;
  }
}

void PrintFSInfo(const FileSystemInformation &fsInfo) {
  PrintTitle("FSInfo");
  constexpr u_int32_t kLeadSignature = 0x41615252;
  constexpr u_int32_t kStructSignature = 0x61417272;
  constexpr u_int32_t kTrailSignature = 0xAA550000;
  std::cout << std::dec << "Top signature "
            << (fsInfo.leadSignature == kLeadSignature ? "matches!"
                                                       : "doesn't match!")
            << std::endl;
  std::cout << "Middle signature "
            << (fsInfo.structSignature == kStructSignature ? "matches!"
                                                           : "doesn't match!")
            << std::endl;
  std::cout << "Last known free cluster count: ";
  if (fsInfo.freeClusters == 0xFFFFFFFF) {
    std::cout << "N/A" << std::endl;
  } else {
    std::cout << fsInfo.freeClusters << std::endl;
  }
  std::cout << "Start looking for available clusters at cluster number: ";
  if (fsInfo.availableClusterStart == 0xFFFFFFFF) {
    std::cout << "N/A" << std::endl;
  } else {
    std::cout << fsInfo.availableClusterStart << std::endl;
  }
  std::cout << "Bottom signature "
            << (fsInfo.trailSignature == kTrailSignature ? "matches!"
                                                         : "doesn't match!")
            << std::endl;
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

void ReadFile(const BiosParameterBlock &bpb,
              const ExtendedBiosParameterBlock &ebpb,
              const DirectoryEntry &entry, std::ifstream &in) {
  PrintTitle("File Info");
  PrintDirectoryEntryInfo(entry);
  const int first_cluster =
      ComposeCluster(entry.firstClusterHigh, entry.firstClusterLow);
  const int bytes_per_cluster = bpb.sectorsPerCluster * bpb.bytesPerSector;

  int current_cluster = first_cluster;

  PrintTitle("File Content");
  while (true) {
    in.seekg(GetClusterAddress(bpb, ebpb, current_cluster));
    std::string data(bytes_per_cluster + 1, '\0');
    in.read(data.data(), bytes_per_cluster);
    std::cout << data;
    // assert(in.is_open());

    int next_cluster = GetNextCluster(in, bpb, current_cluster);
    if (next_cluster >= EOCC) {
      std::cout << std::endl << "end of file" << std::endl;
      break;
    } else if (next_cluster == BAD_CLUSTER) {
      std::cerr << "bad cluster - stopping" << std::endl;
      break;
    }
    current_cluster = next_cluster;
  }
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

// std::string ComputeSizeString(int size) {
//   int power =
//       std::floor(std::log10(static_cast<double>(size)) / std::log10(1024));
//   float scaled_size = size / std::pow(1024, power);
//   std::ostringstream stream;
//   stream.precision(2);
//   stream << std::fixed << scaled_size;
//   std::string result = std::move(stream).str();

//   switch (power) {
//     case 1:
//       result.push_back('K');
//       break;
//     case 2:
//       result.push_back('M');
//       break;
//     case 3:
//       result.push_back('G');
//       break;
//     case 4:
//       result.push_back('T');
//       break;
//   }
//   // If the power is 0 (the size is less than a kibibyte), we print the size
//   in
//   // bytes ("B")
//   if (power > 0) {
//     result.push_back('i');
//   }
//   result.push_back('B');
//   return result;
// }

// void PrintDirectoryEntry(DirectoryEntry entry) {
//   bool isDirectory = (entry.attributes & 0x10) != 0;
//   std::cout << (isDirectory ? 'D' : 'F') << '\t';
//   if (!entry.longFilename.empty()) {
//     std::cout << entry.longFilename << '\t';
//   } else {
//     std::cout << entry.filename << '\t';
//   }
//   Time creationTime = ConvertToTime(entry.creationTime);
//   Date creationDate = ConvertToDate(entry.creationDate);

//   std::cout << std::fixed << std::setfill('0') << std::setw(2)
//             << (int)creationTime.hour << ":" << std::setw(2)
//             << (int)creationTime.minutes << ":" << std::setw(2)
//             << (int)creationTime.seconds << '\t';
//   std::cout << std::setw(2) << (int)creationDate.day << "/" << std::setw(2)
//             << (int)creationDate.month << "/" << std::setw(2)
//             << (int)creationDate.year + 1980;
//   if (!isDirectory) {
//     std::cout << '\t' << ComputeSizeString(entry.size);
//   }
//   std::cout << std::endl;
// }

bool GetSubDirectories(std::vector<DirectoryEntry> &current_dir_entries,
                       const absl::string_view &sub_dir_name, std::ifstream &in,
                       const BiosParameterBlock &bpb,
                       const ExtendedBiosParameterBlock &ebpb) {
  for (const DirectoryEntry &entry : current_dir_entries) {
    if (entry.name != sub_dir_name) {
      continue;
    }

    const bool isDirectory = (entry.attributes & 0x10) != 0;
    if (!isDirectory) {
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

}  // namespace

FileSystem::FileSystem(const std::string &image_file) {
  in_ = std::ifstream(image_file, std::ios::binary);
  if (!in_.is_open()) {
    std::cerr << "failed to Read file!" << std::endl;
    valid_ = false;
    return;
  }

  ReadBPB(&bpb_, in_);
  PrintBPBInfo(bpb_);
  if (bpb_.jmp[0] == 0xEB && bpb_.jmp[2] == 0x90) {
    std::cout << "FAT image detected (by JMP signature)" << std::endl;
  } else {
    std::cerr << "image does not have the correct JMP signature." << std::endl;
    std::cerr << "probably not a valid FAT image." << std::endl;
    valid_ = false;
    return;
  }

  if (!IsBpbValid(bpb_)) {
    std::cerr << "invalid BPB" << std::endl;
    valid_ = false;
    return;
  }

  ReadEBPB(&ebpb_, in_);
  PrintEBPBInfo(ebpb_);
  if (!IsEbpbValid(bpb_, ebpb_)) {
    std::cerr << "invalid EBPB" << std::endl;
    valid_ = false;
    return;
  }

  ReadFSInfo(bpb_, ebpb_, &fs_info_, in_);
  PrintFSInfo(fs_info_);

  ReadDirectory(in_, bpb_, ebpb_, ebpb_.rootDirCluster, root_dir_entries_);

  // PrintTitle("Root Directory");
  // for (const DirectoryEntry &entry : root_dir_entries_) {
  //   // bool hasLongFilename = !entry.longFilename.empty();
  //   // std::string entry_filename;
  //   // if (hasLongFilename) {
  //   //   entry_filename = entry.longFilename;
  //   // } else {
  //   //   entry_filename = std::string((char *)(entry.filename));
  //   // }
  //   // rtrim(entry_filename);
  //   // PrintDirectoryEntryInfo(entry);
  //   std::cout << entry.name << std::endl;
  // }

  valid_ = true;
  PrintTitle("FileSystem Initialized");
}

std::string FileSystem::ReadFile(absl::string_view path) {
  const char kPathDelimeter = '/';
  std::size_t pos = path.find_last_of(kPathDelimeter);
  absl::string_view filename;
  if (pos != absl::string_view::npos) {
    absl::string_view parent_dir = path.substr(0, pos);
    filename = path.substr(pos + 1);
    if (!ChangeDirectory(parent_dir)) {
      std::cerr << "failed to change to parent dir" << std::endl;
      return "";
    }
  } else {
    current_dir_entries_ = root_dir_entries_;
    filename = path;
  }
  auto dir_entry =
      std::find_if(current_dir_entries_.cbegin(), current_dir_entries_.cend(),
                   [&filename](auto entry) {return entry.name == filename; });
  if (dir_entry == current_dir_entries_.cend() || dir_entry->IsDirectory()) {
    std::cerr << "file not exists or is a directory" << std::endl;
    return "";
  }
  fat32::ReadFile(bpb_, ebpb_, *dir_entry, in_);
  return "";
};

bool FileSystem::ChangeDirectory(absl::string_view path) {
  if (path == "") {
    // root directory
    current_dir_entries_ = root_dir_entries_;
    return true;
  }

  std::cout << "change diretory to " << path << std::endl;
  const char kPathDelimeter = '/';
  std::vector<absl::string_view> path_segments =
      absl::StrSplit(path, kPathDelimeter);
  current_dir_entries_ = root_dir_entries_;
  for (const std::string_view &dir_name : path_segments) {
    if (!GetSubDirectories(current_dir_entries_, dir_name, in_, bpb_, ebpb_)) {
      return false;
    }
  }
  return true;
}

}  // namespace fat32
