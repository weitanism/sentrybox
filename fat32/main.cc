#include <iostream>

#include "argparse/argparse.hpp"
#include "fat32.h"

int main(int argc, char** argv) {
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

  program.add_argument("action")
      .help("supported actions: ls, cat")
      .default_value(std::string{"ls"})
      .choices("ls", "cat", "export");

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
  std::cout << "file: " << file << std::endl;
  std::cout << "action: " << action << std::endl;
  std::cout << "path: " << path << std::endl;
  std::cout << "export path: " << export_path << std::endl;

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
    fs.ReadFile(path, "");
  } else if (action == "export") {
    fs.ReadFile(path, export_path);
  } else {
    std::cerr << "action '" << action << "' not implemented yet" << std::endl;
  }
  return 0;
}
