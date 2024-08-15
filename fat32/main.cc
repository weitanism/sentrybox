#include <iostream>

#include "argparse/argparse.hpp"
#include "fat32.h"
#include "fat32_fuse.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"

int main(int argc, char** argv) {
  spdlog::cfg::load_env_levels();

  argparse::ArgumentParser program("fat32");
  program.add_argument("-v", "--verbose").flag();
  program.add_argument("-f", "--file")
      .required()
      .help("path to fat32 image file");
  program.add_argument("-p", "--path")
      .help("path to perform action on")
      .default_value(std::string{""});
  program.add_argument("-e", "--export-path")
      .help("path to save exported file")
      .default_value(std::string{""});
  program.add_argument("-m", "--mount-path")
      .help("path to mount fuse filesystem")
      .default_value(std::string{""});

  program.add_argument("action")
      .help("supported actions: ls, cat")
      .default_value(std::string{"ls"})
      .choices("ls", "cat", "export", "mount");

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }

  std::string file = program.get("file");
  std::string action = program.get("action");
  std::string path = program.get("path");
  std::string export_path = program.get("export-path");
  std::string mount_path = program.get("mount-path");
  spdlog::debug("file: {}", file);
  spdlog::debug("action: {}", action);
  spdlog::debug("path: {}", path);
  spdlog::debug("export path: {}", export_path);
  spdlog::debug("mount path: {}", mount_path);

  auto fs = fat32::FileSystem(file);
  if (!fs.IsValid()) {
    std::cerr << "invalid fat32 image file" << std::endl;
    return 1;
  }

  if (action == "ls") {
    if (!fs.ChangeDirectory(path)) {
      std::cerr << "failed to cd " << path << std::endl;
      return 1;
    }
    for (const auto& f : fs.CurrentDirectoryEntries()) {
      std::cout << f.name << (f.IsDirectory() ? "/" : "") << std::endl;
    }
  } else if (action == "cat") {
    fs.ChangeDirectory(path, true);
    std::ostream os(std::cout.rdbuf());
    fs.ReadFile(path, os);
  } else if (action == "export") {
    fs.ChangeDirectory(path, true);
    fs.ExportFile(path, export_path);
  } else if (action == "mount") {
    if (mount_path.empty()) {
      std::cerr << "--mount-path required" << std::endl;
      return 1;
    }
    bool succeed = fat32::MountFat32(fs, mount_path);
    if (!succeed) {
      std::cerr << "fuse exited abnormally!" << std::endl;
    }
    { std::cerr << "fuse fs unmounted" << std::endl; }
  } else {
    std::cerr << "action '" << action << "' not implemented yet" << std::endl;
  }
  return 0;
}
