#pragma once

#include <string>

namespace fat32 {

struct Time {
  u_int8_t hour;
  u_int8_t minutes;
  u_int8_t seconds;
};

struct Date {
  u_int8_t year;
  u_int8_t month;
  u_int8_t day;
};

struct Datetime {
  u_int8_t year;
  u_int8_t month;
  u_int8_t day;
  u_int8_t hour;
  u_int8_t minutes;
  u_int8_t seconds;
};

namespace {

Date ConvertToDate(u_int16_t date) {
  Date result;
  result.day = date & 0b11111;
  date >>= 5;
  result.month = date & 0b1111;
  date >>= 4;
  result.year = date & 0b1111111;

  return result;
}

Datetime ConvertToDatetime(u_int16_t date, u_int16_t time) {
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
  u_int16_t bytesPerSector;    // 512, 1024, 2048 or 4096.
  u_int8_t sectorsPerCluster;  // 1, 2, 4, 8, 16, 32, 64, or 128
  u_int16_t reservedSectors;   // not be 0 and can be any non-zero value.
  u_int8_t countFats;
  u_int16_t rootDirectoryEntries16;  // FAT12/16 only
  u_int16_t sectorsCount16;          // FAT12/16 only
  u_int8_t mediaDescriptorType;  // 0xF8 for “fixed” media, 0xF0 for removable.
  u_int16_t sectorsPerFAT16;     // FAT12/16 only
  u_int16_t sectorsPerTrack;
  u_int16_t headsCount;
  u_int32_t hiddenSectors;
  u_int32_t sectorsCount32;
};

struct ExtendedBiosParameterBlock {
  u_int32_t sectorsPerFAT;
  u_int16_t flags;
  u_int16_t FATVersion;
  u_int32_t rootDirCluster;
  u_int16_t FSInfoSector;
  u_int16_t backupBootSector;
  u_int8_t driveNumber;
  u_int8_t signature;
  u_int32_t volumeId;
  char volumeLabel[12];
  char systemType[9];
};

struct FileSystemInformation {
  u_int32_t leadSignature;
  u_int32_t structSignature;
  u_int32_t freeClusters;
  u_int32_t availableClusterStart;
  u_int32_t trailSignature;
};

struct DirectoryEntry {
  // raw fields on disk
  char filename[12];
  std::string longFilename;
  u_int8_t attributes;
  u_int8_t creationTimeHS;
  u_int16_t creationTime;
  u_int16_t creationDate;
  u_int16_t lastAccessedDate;
  u_int16_t firstClusterHigh;
  u_int16_t lastModificationTime;
  u_int16_t lastModificationDate;
  u_int16_t firstClusterLow;
  u_int32_t size;

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
  u_int8_t order;
  char topName[10];
  u_int8_t longEntryType;
  u_int8_t checksum;
  char middleName[12];
  char bottomName[4];
};

}  // namespace fat32
