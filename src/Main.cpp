
#include "BookSet.hpp"
#include "FixStream.hpp"

#include <fstream>
#include <iostream>

using namespace fix2book;

namespace {

bool ReadArgs(int argc,
              char *argv[],
              const char *&file,
              char &soh,
              size_t &numberOfLevels) {
  if (argc >= 2 && argv[1][0]) {
    file = &argv[1][0];
    soh = '^';
    numberOfLevels = 5;
    return true;
  }
  if (argc == 0) {
    std::cerr << "Wrong arguments." << std::endl;
  } else {
    std::cout << "Usage:" << std::endl
              << "\t" << argv[0] << R"( "fileName">" [ --debug ], where:)"
              << std::endl
              << std::endl
              << "\t\t <fileName>: path to input file, required;" << std::endl
              << std::endl;
  }
  return false;
}
}  // namespace

int main(int argc, char *argv[]) {
  try {
    const char *sourceFilePath;
    char soh = 0x01;
    auto numberOfLevels = std::numeric_limits<size_t>::max();
    if (!ReadArgs(argc, argv, sourceFilePath, soh, numberOfLevels)) {
      return 1;
    }

    std::fstream source(sourceFilePath);
    if (!source) {
      std::cerr << "Filed to open source file \"" << sourceFilePath << "\"."
                << std::endl;
      return 1;
    }
    FixStream fix(soh, source);

    BookSet books;
    while (fix) {
      const auto rev = books.GetRevision();
      fix >> books;
      if (rev >= books.GetRevision()) {
        continue;
      }
      books.Print(books.GetRevision(), numberOfLevels, std::cout);
    }

  } catch (const std::exception &ex) {
    std::cerr << "Fatal error: \"" << ex.what() << "\"." << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Fatal unknown error." << std::endl;
    return 1;
  }
}