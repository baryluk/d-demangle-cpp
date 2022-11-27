#include <string>

namespace demangle_d {

std::string demangle(const std::string &s, bool return_types = true,
                     bool function_attributes = true);

}  // namespace demangle_d
