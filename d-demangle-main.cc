#include <iostream>
#include <string>

#include "d-demangle.h"

int main(int argc, char **argv) {
  bool return_types = true;
  bool function_attributes = true;

  if (argc <= 1 || std::string(argv[1]) == "--help") {
    std::cerr << "Usage: " << argv[0]
              << " [--help] [--no_return_type] [--no_function_attributes] "
                 "_Dsymbol _Dsymbol ..."
              << std::endl;
    return 1;
  }

  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--no_return_types") {
      return_types = false;
      continue;
    }
    if (std::string(argv[i]) == "--no_function_attributes") {
      function_attributes = false;
      continue;
    }
    std::cout << demangle_d::demangle(argv[i], return_types,
                                      function_attributes)
              << std::endl;
  }
  return 0;
}
