#pragma once

#include <string>

#include "spdlog/fmt/ostr.h"

namespace fat32 {

struct Time {
  uint8_t hour;
  uint8_t minutes;
  uint8_t seconds;
};

struct Date {
  uint8_t year;
  uint8_t month;
  uint8_t day;
};

struct Datetime : Date {
  uint8_t hour;
  uint8_t minutes;
  uint8_t seconds;

  time_t ToTimestamp() const {
    struct tm tm;
    tm.tm_year = static_cast<int>(year) + 1980 - 1900;
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minutes;
    tm.tm_sec = seconds;
    return timegm(&tm);
  }
};

namespace {

Date ConvertToDate(uint16_t date) {
  Date result;
  result.day = date & 0b11111;
  date >>= 5;
  result.month = date & 0b1111;
  date >>= 4;
  result.year = date & 0b1111111;

  return result;
}

Datetime ConvertToDatetime(uint16_t date, uint16_t time) {
  Datetime result;
  result.day = date & 0b11111;
  date >>= 5;
  result.month = date & 0b1111;
  date >>= 4;
  result.year = date & 0b1111111;

  result.hour = time & 0b1111;
  time >>= 5;
  result.minutes = time & 0b111111;
  time >>= 6;
  result.seconds = (time & 0b11111) * 2;

  return result;
}

}  // namespace

struct BiosParameterBlock {
  unsigned char jmp[3];
  char oem[9];
  uint16_t bytesPerSector;    // 512, 1024, 2048 or 4096.
  uint8_t sectorsPerCluster;  // 1, 2, 4, 8, 16, 32, 64, or 128
  uint16_t reservedSectors;   // not be 0 and can be any non-zero value.
  uint8_t countFats;
  uint16_t rootDirectoryEntries16;  // FAT12/16 only
  uint16_t sectorsCount16;          // FAT12/16 only
  uint8_t mediaDescriptorType;  // 0xF8 for “fixed” media, 0xF0 for removable.
  uint16_t sectorsPerFAT16;     // FAT12/16 only
  uint16_t sectorsPerTrack;
  uint16_t headsCount;
  uint32_t hiddenSectors;
  uint32_t sectorsCount32;
};

struct ExtendedBiosParameterBlock {
  uint32_t sectorsPerFAT;
  uint16_t flags;
  uint16_t FATVersion;
  uint32_t rootDirCluster;
  uint16_t FSInfoSector;
  uint16_t backupBootSector;
  uint8_t driveNumber;
  uint8_t signature;
  uint32_t volumeId;
  char volumeLabel[12];
  char systemType[9];
};

struct FileSystemInformation {
  uint32_t leadSignature;
  uint32_t structSignature;
  uint32_t freeClusters;
  uint32_t availableClusterStart;
  uint32_t trailSignature;
};

struct DirectoryEntry {
  // raw fields on disk
  char filename[12];
  std::string longFilename;
  uint8_t attributes;
  uint8_t creationTimeHS;
  uint16_t creationTime;
  uint16_t creationDate;
  uint16_t lastAccessedDate;
  uint16_t firstClusterHigh;
  uint16_t lastModificationTime;
  uint16_t lastModificationDate;
  uint16_t firstClusterLow;
  uint32_t size;  // size in bytes of file/directory described by this entry

  // processed fields
  std::string name;
  bool IsReadOnly() const { return (attributes & 0x01) != 0; }
  bool IsHidden() const { return (attributes & 0x02) != 0; }
  bool IsSystem() const { return (attributes & 0x04) != 0; }
  bool IsVolumeIdEntry() const { return (attributes & 0x08) != 0; }
  bool IsDirectory() const { return (attributes & 0x10) != 0; }
  bool IsArchive() const { return (attributes & 0x20) != 0; }

  Datetime CreationDatetime() const {
    return ConvertToDatetime(creationDate, creationTime);
  };
  Datetime LastModificationDatetime() const {
    return ConvertToDatetime(lastModificationDate, lastModificationTime);
  };
  Date LastAccessedDate() { return ConvertToDate(lastAccessedDate); };
};

struct LongFileNameDirectoryEntry {
  uint8_t order;
  char topName[10];
  uint8_t longEntryType;
  uint8_t checksum;
  char middleName[12];
  char bottomName[4];
};

}  // namespace fat32

template <>
struct fmt::formatter<fat32::Date> : fmt::formatter<std::string> {
  auto format(fat32::Date dt, format_context &ctx) const
      -> decltype(ctx.out()) {
    return format_to(ctx.out(), "{}/{}/{}", dt.year + 1980, dt.month, dt.day);
  }
};

template <>
struct fmt::formatter<fat32::Datetime> : fmt::formatter<std::string> {
  auto format(fat32::Datetime dt, format_context &ctx) const
      -> decltype(ctx.out()) {
    return format_to(ctx.out(), "{} {:02}:{:02}:{:02}",
                     static_cast<fat32::Date>(dt), dt.hour, dt.minutes,
                     dt.seconds);
  }
};
