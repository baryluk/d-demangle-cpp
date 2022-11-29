#ifndef DEMANGLE_D_H
#define DEMANGLE_D_H

#include <string>

namespace demangle_d {

std::string demangle(const std::string &s, bool return_types = true,
                     bool function_attributes = true);

}  // namespace demangle_d

#endif  // DEMANGLE_D_H
