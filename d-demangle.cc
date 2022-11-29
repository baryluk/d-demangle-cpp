#include <stdexcept>
#include <string>

#if __cplusplus >= 201103L
#include <unordered_map>
#else
#include <cstdio>
#include <map>
#endif

// #define DEMANGLE_D_DEBUG

#if defined(DEMANGLE_D_DEBUG)
#undef DEMANGLE_D_DEBUG
#include <iostream>
namespace demangle_d {
struct DemangleScopeDebugger {
  static int indent_level;

  const std::string msg_;
  const size_t *offset_ptr_;
  const size_t start_offset_;
  const std::string s_;
  DemangleScopeDebugger(const std::string &msg, const std::string &s, const size_t *offset) : msg_(msg), offset_ptr_(offset), start_offset_(*offset), s_(s) {
    for (int i = 0; i < indent_level; i++) {
      std::cout << "  ";
    }
    std::cout << "> " << msg << "  - s[" << start_offset_ << "] = '" << s[start_offset_] << "'" << std::endl;
    indent_level++;
  }
  ~DemangleScopeDebugger() {
    indent_level--;
    for (int i = 0; i < indent_level; i++) {
      std::cout << "  ";
    }
    std::cout << "< " << msg_ << "  - s[" << start_offset_ << ".." << (*offset_ptr_) << "] = \"" << s_.substr(start_offset_, *offset_ptr_ - start_offset_) << "\"" << std::endl;
  }
};
int DemangleScopeDebugger::indent_level = 0;
}  // namespace demangle_d
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b
#define DEMANGLE_D_DEBUG(x) DemangleScopeDebugger CONCAT(demangle_debugger, __COUNTER__)(#x, s, offset)
#else
#define DEMANGLE_D_DEBUG(x) do {} while (0)
#endif  // DEMANGLE_D_DEBUG

#include "d-demangle.h"


// https://dlang.org/spec/abi.html#name_mangling
// Some stress tests:
// https://gist.github.com/baryluk/1e44595acad20039b4ac7fe0cde829ef

// Simple stuff
// _D7example__T6squareZQiFNaNbNiNfiZi

// Simple backrefs
// _D7example1X5cloneMxFCQuQoQfZQi

// internal symbols
// _D7example1X7__ClassZ.1884    (example.X.__Class.1884)

// _D3std9algorithm8mutation__T11moveEmplaceTASQBq8datetime8timezone13PosixTimeZone14TempTransitionZQCrFNaNbNiKQCoKQCsZv
// | c++filt  -s dlang
// std.algorithm.mutation.moveEmplace!(std.datetime.timezone.PosixTimeZone.TempTransition[]).moveEmplace(ref
// std.datetime.timezone.PosixTimeZone.TempTransition[], ref
// std.datetime.timezone.PosixTimeZone.TempTransition[])

// _D3std5range__T9GeneratorS_D3rcu8gen_listFZ9__lambda1FNfZiZQBs8popFrontMFNfZv
// std.range.Generator!(rcu.gen_list().__lambda1()).Generator.popFront()
// _D3std5range__T9GeneratorS |_D 3rcu 8gen_listFZ 9__lambda1FNfZi Z| QBs
// 8popFrontMFNfZv

namespace demangle_d {

namespace {

// Checks if the s[offset...offset+x.size()] == x[0...x.size()].
// If yes, modify offset beyond match.
inline bool startswith(const std::string &s, const std::string &x,
                       size_t *offset) {
#if DEMANGLE_D_DEBUG
  std::cout << "Testing for " << x << " @ " << *offset << " :     "
            << s.substr(*offset, 60) << "..." << std::endl;
#endif
  // if (x == "Z") {
  //     abort();
  // }
  const size_t off = *offset;
  if (s.size() < off + x.size()) {
    return false;
  }
  if (s.rfind(x, off) == off) {
    *offset += x.size();
    return true;
  } else {
    return false;
  }
}

// This is similar to startswith with string, but with optimization for single
// character.
//
// Important difference: Assumes (without checking) that s[*offset] is legal to
// access. So, check that before using this function (using canread1).
inline bool startswith(const std::string &s, const char x, size_t *offset) {
#if DEMANGLE_D_DEBUG
  std::cout << "Testing for " << x << " @ " << *offset << " :     "
            << s.substr(*offset, 60) << "..." << std::endl;
#endif
  const size_t off = *offset;
  // if (s.size() < off + 1) {
  //   return false;
  // }
  if (s[off] == x) {
    *offset += 1;
    return true;
  } else {
    return false;
  }
}

// Helper to run before startswith.
inline bool canread1(const std::string &s, const size_t *offset) {
  return *offset < s.size();
}

// Number for LName
size_t number(const std::string &s, size_t *offset, size_t limit = 8) {
  DEMANGLE_D_DEBUG(number);
  const size_t off = *offset;
  size_t ret = 0;
  size_t i = 0;
  while (off + i < s.size() && '0' <= s[off + i] && s[off + i] <= '9' &&
         i < limit) {
    ret = 10 * ret + (s[off + i] - '0');
    i++;
  }
  if (i == 0) {
    throw std::runtime_error("Missing number");
  }
  if (ret == 0) {
    throw std::runtime_error("Malformed number or LName");
  }
  if (off + i == s.size() || i >= limit) {
    throw std::runtime_error("Too big number or too long LName");
    return -1;  // We do not modify offset, to indicate bad number.
  }
  *offset += i;
  return ret;
}

// Generic number for sample for string literals. Integer literals.
size_t number0(const std::string &s, size_t *offset, size_t limit = 10) {
  DEMANGLE_D_DEBUG(number0);
  const size_t off = *offset;
  size_t ret = 0;
  size_t i = 0;
  while (off + i < s.size() && '0' <= s[off + i] && s[off + i] <= '9' &&
         i < limit) {
    ret = 10 * ret + (s[off + i] - '0');
    i++;
  }
  if (i == 0) {
    throw std::runtime_error("Missing number");
  }
  if (i >= limit) {
    throw std::runtime_error("Too big number or too long LName");
    return -1;  // We do not modify offset, to indicate bad number.
  }
  *offset += i;
  return ret;
}

size_t base26(const std::string &s, size_t *offset) {
  DEMANGLE_D_DEBUG(base26);
  const size_t off = *offset;
  size_t o = 0;
  size_t i = 0;
  // Numbers in back references are encoded with base 26 by upper case letters
  // A - Z for higher digits but lower case letters a - z for the last digit.
  while (off + i < s.size() && 'A' <= s[off + i] && s[off + i] <= 'Z' &&
         i < 5) {
    o = 26 * o + (s[off + i] - 'A');
    i++;
  }
  if (off + i < s.size() && 'a' <= s[off + i] && s[off + i] <= 'z') {
    o = 26 * o + (s[off + i] - 'a');
    i++;
    *offset += i;
    return o;
  } else {
    throw std::runtime_error("Malformed back reference");
  }
  return -1;
}

#if __cplusplus >= 201103L
std::string int2string(size_t value) {
  return std::to_string(value);
}
#else
std::string int2string(size_t value) {
  char buffer[20]; // Max num of digits for 64 bit number
  ::sprintf(buffer, "%ld", value);
  return std::string(buffer);
}
#endif

#if __cplusplus >= 201103L
using Refs = typename std::unordered_map<size_t, std::string>;
#else
#define Refs std::map<size_t, std::string>
#endif

// "Q" must be already consumed.
std::string back_reference(const std::string &s, size_t *offset, Refs *refs) {
  DEMANGLE_D_DEBUG(back_reference);
  // IdentifierBackRef or TypeBackRef
  // base26 offset.
  const size_t current_offset = *offset;
  const size_t o = base26(s, offset);
  const size_t absolute_o = current_offset - o;
#if DEMANGLE_D_DEBUG
  std::cout << "Backreference (-" << o << "): " << absolute_o << std::endl;
  std::cout << "Current references:" << std::endl;
  for (const auto &ref : *refs) {
    std::cout << "   " << ref.first << " : " << ref.second << std::endl;
  }
#endif
  if (absolute_o >= s.size() || absolute_o < 2) {
    // Too far or it points to _D prefix.
    // There are more restrictions, but it should be okish.
    // We can keep track of valid targets in a table or something.
    throw std::runtime_error("Back reference out of range");
  }
  // To distinguish between the type of the back reference a look-up
  // of the back referenced character is necessary:
  // An identifier back reference always points to a digit 0 to 9,
  // while a type back reference always points to a letter.
  if ('0' <= s[absolute_o] && s[absolute_o] <= '9') {
    std::string ret = (*refs)[absolute_o - 1];
    (*refs)[current_offset - 1] = ret;  // -1 to go back to Q.
    // IdentifierBackRef
    return ret;
  } else if ('A' <= s[absolute_o] &&
             s[absolute_o] <= 'Z') {  //  || s[absolute_o] == '_') {
    std::string ret = (*refs)[absolute_o - 1];
    (*refs)[current_offset - 1] = ret;  // -1 to go back to Q.
    // TypeBackRef
    return ret;
  } else {
    std::string ret = (*refs)[absolute_o - 1];
    // This is not documented directly in spec.
    // Backrefs can be used by future backrefs too. Not just LNames.
    (*refs)[current_offset - 1] = ret;  // -1 to go back to Q.
    return ret;
    // throw std::runtime_error(std::string("Invalid back reference, got first
    // character ") + s[absolute_o]);
  }
}

std::string LName(const std::string &s, size_t *offset, Refs *refs) {
  DEMANGLE_D_DEBUG(Lname);
  const size_t start_offset = *offset;
  // TODO(baryluk): There should be no leading zeros. 0 identifies an anonymous
  // symbol.
  const size_t length = number(s, offset, 8);
  const size_t off = *offset;
  size_t offset2 = off;
  if (startswith(s, "__S", &offset2)) {
    // function-local parent symbol
    // We need to consume number.
    const size_t num2 = number(s, &offset2, 8);
    (void)num2;  // FIXME
    // TODO(baryluk): ...
    throw std::runtime_error("Not imeplemented");
    (*refs)[start_offset] = "function-local_parent_symbol";  // TODO(baryluk);
    return "function-local_parent_symbol";
  } else {
    const std::string ret = s.substr(off, length);
#if DEMANGLE_D_DEBUG
    std::cout << "LName " << ret << " of length " << length << std::endl;
#endif
    *offset += length;
    // ret should be a standard D identifier.
    // It should start with _ or Alpha. Then continue with _, Alpha or Digit.

    (*refs)[start_offset] = ret;
    // (*refs)[off] = ret;
    return ret;
  }
}

std::string LNameOrBackref(const std::string &s, size_t *offset, Refs *refs) {
  if (!canread1(s, offset)) {
    DEMANGLE_D_DEBUG(NOT_LnameOrBackref);
    throw std::runtime_error("Cannot read LName or backref, truncated string?");
  }
  DEMANGLE_D_DEBUG(LnameOrBackref);
  if (startswith(s, 'Q', offset)) {
    return back_reference(s, offset, refs);
  } else {
    return LName(s, offset, refs);
  }
}

unsigned char hexdigit(unsigned char c) {
  // DEMANGLE_D_DEBUG(hexdigit);
  if ('0' <= c && c <= '9') {
    return c - '0';
  }
  if ('A' <= c && c <= 'F') {
    return c - 'A' + 10;
  }
  // This is not documented. But CharWidth Number _ HexDigits is using lower
  // case digits. Instead of upper case as indicate in the grammar.
  if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  }
  throw std::runtime_error(std::string("Invalid hex digit ") +
                           static_cast<char>(c));
  return 0;
}

std::string hexfloat(const std::string &s, size_t *offset) {
  DEMANGLE_D_DEBUG(hexfloat);
  if (startswith(s, "NAN", offset)) {
    return "nan";
  }
  if (startswith(s, "INF", offset)) {
    return "inf";
  }
  if (startswith(s, "NINF", offset)) {
    return "-inf";
  }
  std::string ret;
  if (startswith(s, "N", offset)) {
    ret += '-';
  }
  ret += "0x1";
  std::string mantissa;
  while (canread1(s, offset) && s[*offset] != 'P') {
    const unsigned char h = s[*(offset++)];
    // Just run to verify it is hex.
    const unsigned char h_value = hexdigit(h);
    (void)h_value;
    mantissa += h;
  }
  if (mantissa.size() > 0) {
    ret += '.';
    ret += mantissa;
  }
  if (!(mantissa.size() == 8 || mantissa.size() == 16 ||
        mantissa.size() == 20)) {
    throw std::runtime_error("Unsupported floating point width");
  }
  if (!startswith(s, "P", offset)) {
    throw std::runtime_error("Malformed hex float exponent");
  }
  const bool exponent_negative = startswith(s, "N", offset);
  const size_t e = number0(s, offset, 5);
  if (e) {  // Skip if exponent 0.
    if (exponent_negative) {
      ret += '-';
    } else {
      // optional, but lets be explicit.
      ret += '+';
    }
    ret += int2string(e);
  }
  if (mantissa.size() == 8) {
    ret += 'f';
  } else if (mantissa.size() == 20) {
    ret += 'L';
  }
  return ret;
}

// Forward declaration.
std::string mangled_name(const std::string &s, size_t *offset,
                         bool return_types = true,
                         bool function_attributes = true);

std::string value(const std::string &s, size_t *offset, Refs *refs,
                  const std::string &type_hint);

std::string qualified_name(const std::string &s, size_t *offset, Refs *refs,
                           bool return_types = true,
                           bool function_attributes = true);

std::string value0(const std::string &s, size_t *offset, Refs *refs,
                   const std::string &type_hint = "") {
  if (!canread1(s, offset)) {
    DEMANGLE_D_DEBUG(NOT_value0);
    throw std::runtime_error("Error decoding value0 - to short string");
  }
  DEMANGLE_D_DEBUG(value0);
  // TODO(baryluk): Check if switch is faster.
  if (startswith(s, 'n', offset)) {
    if (type_hint.size()) {
      return "cast(" + type_hint + ")(null)";
    }
    return "null";
  }
  if (startswith(s, 'i', offset)) {
    // positive numeric literals (including character literals)
    std::string i = int2string(number0(s, offset, 11));
    if (type_hint.size()) {
      if (type_hint[0] == 'u') {
        i += 'u';
      }
      if (type_hint == "long" || type_hint == "ulong") {
        i += 'l';
      }
    }
    return i;
  }
  if (startswith(s, 'N', offset)) {
    // negative numeric literals
    const size_t n = number0(s, offset, 11);
    std::string i = int2string(n);
    if (type_hint.size()) {
      if (type_hint[0] == 'u') {
        i += 'u';
      }
      if (type_hint == "long" || type_hint == "ulong") {
        i += 'l';
      }
    }
    return "-" + i;
  }
  if (startswith(s, 'e', offset)) {
    // real or imaginary floating point literal
    // TODO(baryluk): What about purely imaginary number?
    // It looks like there is no way to distinguish this.
    // (it is based on context from outside).
    if (type_hint == "ifloat" || type_hint == "idouble" ||
        type_hint == "ireal") {
      return hexfloat(s, offset) + "i";
    } else {
      return hexfloat(s, offset);
    }
  }
  if (startswith(s, 'c', offset)) {
    // assert(type_hint[0] == 'c');
    const std::string real = hexfloat(s, offset);
    if (startswith(s, "c", offset)) {
      const std::string imag = hexfloat(s, offset);
      if (imag[0] == '-') {
        return real + imag + "i";
      } else {
        return real + "+" + imag + "i";
      }
    } else {
      throw std::runtime_error("Invalid complex literal");
    }
  }
  if (startswith(s, 'A', offset)) {
    bool assoc = true;
    // HACK. Detects "[]" to detect dynamic array.
    if (type_hint[type_hint.size() - 2] == '[') {
      // TODO(baryluk): Handle static arrays, and multilevel arrays.
      assoc = false;
    }
    // An array or asssociative array literal. Number is the length of the
    // array. Value is repeated Number times for a normal array, and
    // 2 * Number times for an associative array.
    const size_t n = number0(s, offset, 11);
    std::string ret = "[";
    for (size_t i = 0; i < n; i++) {
      if (ret.length() >= 2) {
        ret += ", ";
      }
      if (!assoc) {
        const std::string v = value0(s, offset, refs);
        ret += v;
      } else {
        const std::string k = value0(s, offset, refs);
        const std::string v = value0(s, offset, refs);
        ret += k;
        ret += ": ";
        ret += v;
      }
    }
    ret += ")";
    return ret;
  }
  if (startswith(s, 'S', offset)) {
    if (startswith(s, "_D", offset)) {
      // Does it uses own refs?
      std::string rr = qualified_name(s, offset, refs);
      return rr;
    }
    // Struct literal
    const size_t n = number0(s, offset, 11);
    std::string ret = "(";
    for (size_t i = 0; i < n; i++) {
      if (ret.length() >= 2) {
        ret += ", ";
      }
      const std::string v = value0(s, offset, refs);
      ret += v;
    }
    ret += ")";
    return ret;
  }
  if (startswith(s, 'f', offset)) {
    std::string m = mangled_name(s, offset);
    return m;
  }
  if (startswith(s, 'a', offset)) {
    // byte string
    const size_t n = number0(s, offset, 11);
#if DEMANGLE_D_DEBUG
    std::cout << "String literal of length " << n << std::endl;
#endif
    if (!startswith(s, "_", offset)) {
      throw std::runtime_error("Invalid string literal");
    }
    std::string ret = "\"";
    size_t j = n;
    while (j > 0) {
      j--;
      const unsigned char uHex = s[*offset];
      const unsigned char lHex = s[(*offset) + 1];
      // Note: Spec says it is HexDigit which 0-9, A-F.
      // But a-f also is produced. So we accept it.
      const unsigned char u = hexdigit(uHex);
      const unsigned char l = hexdigit(lHex);
      const unsigned char c = (u << 4) + l;
#if DEMANGLE_D_DEBUG
      std::cout << "String character " << c << std::endl;
#endif
      if (c == '\n') {
        ret += "\\n";
      } else if (c == '\t') {
        ret += "\\t";
      } else if (c == '\r') {
        ret += "\\r";
      } else if (c == '\b') {
        ret += "\\b";
      } else if (c == '\\') {
        ret += "\\\\";
      } else if (c == '"') {
        ret += "\\\"";
      } else if (c == '\0') {
        ret += "\\0";
      } else if (c == '\a') {
        ret += "\\a";
      } else if (c == '\f') {
        ret += "\\f";
      } else if (c == '\v') {
        ret += "\\v";
      } else if (c < 32) {
        char buf[5];
        buf[0] = '\\';
        buf[1] = 'x';
        buf[2] = uHex;
        buf[3] = lHex;
        buf[4] = '\0';
        ret += buf;
      } else {
        // Note ', does not need to be escape to "\\'" in double quoted strings.
        // (but it is allowed).
        ret += c;
      }
      *offset += 2;
    }
    ret += '"';
    // postfix (optional for 1-byte strings)
    // ret += "c";
    return ret;
  }
  if (startswith(s, 'w', offset)) {
    // 2-byte string
    const size_t n = number(s, offset);
    (void)n;  // FIXME
    if (!startswith(s, "_", offset)) {
      throw std::runtime_error("Invalid string literal");
    }
    // Similar to 1 byte strings, but with unicode, and \u for escape of 2-byte
    // values. ret += "w"; // postfix
    throw std::runtime_error("2-byte strings not implemented");
  }
  if (startswith(s, 'd', offset)) {
    // 4-byte string
    const size_t n = number(s, offset);
    (void)n;  // FIXME
    if (!startswith(s, "_", offset)) {
      throw std::runtime_error("Invalid string literal");
    }
    // Similar to 1 byte strings, but with unicode, and \u for escape of 2-byte
    // values, and \U for escape of 4 byte values.
    //
    // ret += "d"; // postfix
    throw std::runtime_error("4-byte strings not implemented");
  }
  throw std::runtime_error("Unknown value");
}

std::string value(const std::string &s, size_t *offset, Refs *refs,
                  const std::string &type_hint) {
  DEMANGLE_D_DEBUG(value);
  const size_t start_offset = *offset;
  const std::string ret = value0(s, offset, refs, type_hint);
  (*refs)[start_offset] = ret;
  return ret;
}

std::string funcattrs(const std::string &s, size_t *offset) {
  DEMANGLE_D_DEBUG(funcattrs);
  std::string ret;
  while (true) {
    // TODO(baryluk): What about sequence of NaNg for example.
    // Na is a func attr, but Ng is a first parameter attribute.
    if (startswith(s, "N", offset)) {
      if (!canread1(s, offset)) {
        DEMANGLE_D_DEBUG(NOT_funcattrs);
        throw std::runtime_error("Invalid truncated N sequence for funcattrs");
      }
      // TODO(baryluk): Check if switch is faster.
      if (startswith(s, 'a', offset)) {  // Na
        ret += " pure";
      } else if (startswith(s, 'i', offset)) {  // Ni
        ret += " @nogc";
      } else if (startswith(s, 'b', offset)) {  // Nb
        ret += " nothrow";
      } else if (startswith(s, 'd', offset)) {  // Nd
        ret += " @property";
      } else if (startswith(s, 'c', offset)) {  // Nc
        ret += " ref";
      } else if (startswith(s, 'j', offset)) {  // Nj
        ret += " return";
      } else if (startswith(s, 'l', offset)) {  // Nl
        ret += " scope";
      } else if (startswith(s, 'e', offset)) {  // Ne
        ret += " @trusted";
      } else if (startswith(s, 'f', offset)) {  // Nf
        ret += " @safe";
      } else if (startswith(s, 'm', offset)) {  // Nm
        ret += " @live";
      } else {
        DEMANGLE_D_DEBUG(UNKNOWN_funcattrs);
        throw std::runtime_error("Unknown function attribute");
      }
    } else {
      break;
    }
  }
  return ret;  // can be empty.
}

std::string type_modifiers(const std::string &s, size_t *offset) {
  DEMANGLE_D_DEBUG(type_modifiers);
  std::string ret;
  // TODO(baryluk): Should be const(T), ...
  if (startswith(s, "O", offset)) {
    ret += "shared ";
  } else if (startswith(s, "y", offset)) {
    return "immutable ";
  }
  if (startswith(s, "Ng", offset)) {
    ret += "inout ";  // ? "wild"
  }
  if (startswith(s, "x", offset)) {
    ret += "const ";
  }
  return ret;
}

std::string call_convention(const std::string &s, size_t *offset) {
  if (!canread1(s, offset)) {
    DEMANGLE_D_DEBUG(NOT_call_convention);
    throw std::runtime_error("Missing call convention");
  }
  DEMANGLE_D_DEBUG(call_convention);
  // TODO(baryluk): Check if switch is faster.
  if (startswith(s, 'F', offset)) {
    return "";  // D
  }
  if (startswith(s, 'U', offset)) {
    return "extern(C)";
  }
  if (startswith(s, 'W', offset)) {
    return "extern(Windows)";
  }
  if (startswith(s, 'R', offset)) {
    return "extern(C++)";
  }
  if (startswith(s, 'Y', offset)) {
    return "extern(ObjectiveC)";
  }
  // No longer in use or specified in ABI.
  if (startswith(s, 'V', offset)) {
    return "extern(Pascal)";
  }
  DEMANGLE_D_DEBUG(UNKNOWN_call_convention);
  throw std::runtime_error(std::string("Unknown call convention ") +
                           s[*offset]);
  return "";
}

std::string param_close(const std::string &s, size_t *offset) {
  if (!canread1(s, offset)) {
    DEMANGLE_D_DEBUG(NOT_param_close);
    throw std::runtime_error("Missing function param close");
  }
  DEMANGLE_D_DEBUG(param_close);
  if (startswith(s, 'Z', offset)) {
    return ")";
  }
  if (startswith(s, 'X', offset)) {
    return "...)";
  }
  if (startswith(s, 'Y', offset)) {
    return ", ...)";
  }
  DEMANGLE_D_DEBUG(UNKNOWN_param_close);
  throw std::runtime_error("Unknown function param close style");
  return "";
}

// Forward declarations.
std::string mangled_name0(const std::string &s, size_t *offset, Refs *refs,
                          bool return_types = true,
                          bool function_attributes = true);
std::string parameters(const std::string &s, size_t *offset, Refs *refs,
                       bool return_types = true,
                       bool function_attributes = true);
std::string parameter2(const std::string &s, size_t *offset, Refs *refs,
                       bool return_types = true,
                       bool function_attributes = true);
std::string type_function_no_return(const std::string &s, size_t *offset,
                                    Refs *refs, const std::string &name = "",
                                    bool return_types = true,
                                    bool function_attributes = true);

// We provide name, in case of parsing functions.
// This is because functions are of the form:  func_attrs return_type name
// params. But in mangled stream it is name fun_attrs params return_type
std::string type(const std::string &s, size_t *offset, Refs *refs,
                 const std::string &name = "", bool return_types = true,
                 bool function_attributes = true) {
  if (!canread1(s, offset)) {
    DEMANGLE_D_DEBUG(NOT_type);
    throw std::runtime_error("Cannot demangle type - missing TypeX");
  }

  DEMANGLE_D_DEBUG(type);
  const size_t start_offset = *offset;
  if (startswith(s, 'Q', offset)) {
    // TODO: Verify that it is a type back reference.
    return back_reference(s, offset, refs);
  }
  std::string ret = type_modifiers(s, offset);
  bool basic = false;
  // TypeX
  if (startswith(s, 'A', offset)) {
    // TypeArray
    const std::string element_type =
        type(s, offset, refs, "", return_types, function_attributes);
    ret += element_type;
    ret += "[]";
  } else if (startswith(s, 'G', offset)) {
    // TypeStaticArray
    const size_t n = number(s, offset);
    const std::string element_type =
        type(s, offset, refs, "", return_types, function_attributes);
    ret += element_type;
    ret += '[';
    ret += int2string(n);
    ret += ']';
  } else if (startswith(s, 'H', offset)) {
    // TypeAssocArray
    // Spec does not specify which is key, which is value.
    const std::string key_type =
        type(s, offset, refs, "", return_types, function_attributes);
    const std::string value_type =
        type(s, offset, refs, "", return_types, function_attributes);
    ret += value_type;
    ret += '[';
    ret += key_type;
    ret += ']';
  } else if (startswith(s, 'P', offset)) {
    // TypePointer
    ret += type(s, offset, refs);
    ret += '*';
  } else if (startswith(s, "Nh", offset)) {
    // TypeVector
    ret += "__vector(";
    ret += type(s, offset, refs);
    ret += ')';
  } else if (startswith(s, 'I', offset) || startswith(s, 'C', offset) ||
             startswith(s, 'S', offset) || startswith(s, "E", offset) ||
             startswith(s, 'T', offset)) {
    // TypeIdent, TypeClass, TypeStruct, TypeEnum, TypeTypedef
    ret += qualified_name(s, offset, refs, return_types, function_attributes);
  } else if (startswith(s, 'D', offset)) {
    // TypeDelegate
    std::string o = type_modifiers(s, offset);  // optional
    ret += qualified_name(s, offset, refs, return_types, function_attributes);
    ret += " delegate";
    ret += o;
  } else if (startswith(s, 'v', offset)) {
    ret += "void";
    basic = true;
  } else if (startswith(s, 'g', offset)) {
    ret += "byte";
    basic = true;
  } else if (startswith(s, 'h', offset)) {
    ret += "ubyte";
    basic = true;
  } else if (startswith(s, 's', offset)) {
    ret += "short";
    basic = true;
  } else if (startswith(s, 't', offset)) {
    ret += "ushort";
    basic = true;
  } else if (startswith(s, 'i', offset)) {
    ret += "int";
    basic = true;
  } else if (startswith(s, 'k', offset)) {
    ret += "uint";
    basic = true;
  } else if (startswith(s, 'l', offset)) {
    ret += "long";
    basic = true;
  } else if (startswith(s, 'm', offset)) {
    ret += "ulong";
    basic = true;
  } else if (startswith(s, "zi", offset)) {
    ret += "cent";
    basic = true;
  } else if (startswith(s, "zk", offset)) {
    ret += "ucent";
    basic = true;
  } else if (startswith(s, 'f', offset)) {
    ret += "float";
    basic = true;
  } else if (startswith(s, 'd', offset)) {
    ret += "double";
    basic = true;
  } else if (startswith(s, 'e', offset)) {
    ret += "real";
    basic = true;
  } else if (startswith(s, 'o', offset)) {
    ret += "ifloat";
    basic = true;
  } else if (startswith(s, 'p', offset)) {
    ret += "idouble";
    basic = true;
  } else if (startswith(s, 'j', offset)) {
    ret += "ireal";
    basic = true;
  } else if (startswith(s, 'q', offset)) {
    ret += "cfloat";
    basic = true;
  } else if (startswith(s, 'r', offset)) {
    ret += "cdouble";  // complex
    basic = true;
  } else if (startswith(s, 'c', offset)) {
    ret += "creal";
    basic = true;
  } else if (startswith(s, 'b', offset)) {
    ret += "bool";
    basic = true;
  } else if (startswith(s, 'a', offset)) {
    ret += "char";
    basic = true;
  } else if (startswith(s, 'u', offset)) {
    ret += "wchar";
    basic = true;
  } else if (startswith(s, 'w', offset)) {
    ret += "dchar";
    basic = true;
  } else if (startswith(s, "Nn", offset)) {
    // TypeNoreturn
    // ret += "noreturn";  // void, bottom type?
    ret += "typeof(*null)";
    basic = true;
  } else if (startswith(s, 'n', offset)) {
    // TypeNull
    ret += "typeof(null)";  // void, bottom type?
    basic = true;
  } else if (startswith(s, 'B', offset)) {
    // TypeTuple
    ret += "tuple!";
    // TODO: Technically only closing with Z is allowed. (Not X, Y).
    ret += parameters(s, offset, refs);
  } else {
    DEMANGLE_D_DEBUG(type_fallbackt_to_type_function);
    std::string function_signature;
    // TypeFunction
    try {
      // TODO(baryluk): If calling convention is non-D, this is not correct. int
      // extern(C)(int, int);
      size_t offset2 = *offset;
      function_signature = type_function_no_return(
          s, &offset2, refs, "", return_types, function_attributes);
      *offset = offset2;
    } catch (const std::runtime_error &e) {
      // not a function
      // Assume it is qualified_name.
      // ret += LName(s, offset, refs);
      ret += qualified_name(s, offset, refs, return_types, function_attributes);

      // Store type reference for back reference lookups.
      (*refs)[start_offset] = ret;
      return ret;
      // throw;
    }
    const std::string return_type =
        type(s, offset, refs, "", return_types, function_attributes);
    if (name.size()) {
      if (return_types && return_type.size()) {
        ret += return_type;
        ret += ' ';
      }
      ret += name;
      ret += ' ';
      ret += function_signature;
    } else {
      if (return_types && return_type.size()) {
        ret += return_type;
        ret += ' ';
      }
      ret += function_signature;
    }
  }

  if (!basic) {
    // Store type reference for back reference lookups.
    (*refs)[start_offset] = ret;
  }
  return ret;
}

std::string type_function_no_return(const std::string &s, size_t *offset,
                                    Refs *refs, const std::string &name,
                                    bool return_types,
                                    bool function_attributes) {
  DEMANGLE_D_DEBUG(type_function_no_return);
  const std::string cc = call_convention(s, offset);
  std::string ret = cc;
  // function
  const std::string fa = funcattrs(s, offset);
  if (name.size()) {
    // if (fa.size()) {
    //   ret += "";
    // }
    ret += name;
  }
  ret += parameters(s, offset, refs, return_types, function_attributes);
  if (function_attributes) {
    if (fa.size()) {
      // ret += " ";  // fa is already prefixed with space
      ret += fa;
    }
  }
  return ret;
}

std::string parameter2(const std::string &s, size_t *offset, Refs *refs,
                       bool return_types, bool function_attributes) {
  DEMANGLE_D_DEBUG(parameter2);
  std::string prefix;
  if (canread1(s, offset)) {
    if (startswith(s, 'I', offset)) {
      prefix = "in ";
    } else if (startswith(s, 'J', offset)) {
      prefix = "out ";
    } else if (startswith(s, 'K', offset)) {
      prefix = "ref ";
    } else if (startswith(s, 'L', offset)) {
      prefix = "lazy ";
    }
  }
  // TODO(baryluk): What about inout? Is it deprecated or synonymous with ref?
  const std::string t =
      type(s, offset, refs, "", return_types, function_attributes);
  prefix += t;
  return prefix;
}

std::string parameters(const std::string &s, size_t *offset, Refs *refs,
                       bool return_types, bool function_attributes) {
  DEMANGLE_D_DEBUG(parameters);
  std::string ret = "(";
  while (true) {
    try {
      const std::string c = param_close(s, offset);
      ret += c;
      break;
    } catch (const std::runtime_error &ex) {
      // No X, Y, Z. Assume more params.
      // (For tuple only Z is allowed).
    }
    bool scope = false;
    bool _return = false;
    // Specification says only M or Nk (or none) can happen:
    //
    // Parameters:
    //     Parameter
    //     Parameter Parameters
    //
    // Parameter:
    //     Parameter2
    //     M Parameter2     // scope
    //     Nk Parameter2    // return
    //
    // Parameter2:
    //     Type
    //     I Type     // in
    //     J Type     // out
    //     K Type     // ref
    //     L Type     // lazy
    //
    // (And there is no Type with prefix M - that would be ambigous anyway).
    //
    // But I did see this:
    // _D4core8internal5array8capacity__T22_d_arraysetlengthTImplHTAPmTQdZ18_d_arraysetlengthTFNaNbNeMNkKQBmmZm
    //                                                                                               ^^^
    if (startswith(s, "M", offset)) {
      scope = true;
    }
    // Note: This should be } else if { according to spec. But spec is wrong.
    if (startswith(s, "Nk", offset)) {
      _return = true;
    }
    const std::string p =
        parameter2(s, offset, refs, return_types, function_attributes);
    if (p.size() == 0) {
      break;
    }
    // TODO(baryluk): What about "scope return" ? This looks to be not
    // supported. (It is undocumented by spec, but actually supported
    // and required to demangle real world symbols)
    // I think it is, via type_function_no_return() - it is a function
    // modifier, not parameter attribute.
    if (ret.size() >= 2) {
      ret += ", ";
    }
    if (scope) {
      ret += "scope ";
    }
    if (_return) {
      ret += "return ";
    }
    ret += p;
  }
  // ret += ")";
  return ret;  // can be empty
}

std::string symbol_name(const std::string &s, size_t *offset, Refs *refs,
                        bool return_types, bool function_attributes) {
  // Parse SymbolName
  if (!canread1(s, offset)) {
    DEMANGLE_D_DEBUG(NOT_symbol_name);
    throw std::runtime_error("Cannot parse symbol_name, truncated string");
  }
  DEMANGLE_D_DEBUG(symbol_name);
  if (startswith(s, "__T", offset) || startswith(s, "__U", offset)) {
    DEMANGLE_D_DEBUG(symbol_name_template);
#if DEMANGLE_D_DEBUG
    std::cout << "Parsing template" << std::endl;
#endif
    // TmplateInstanceName
    // (__U is for symbols declared inside template constraint)
    std::string l = LNameOrBackref(s, offset, refs);
    // TODO(baryluk): For simple types and values, and single arg, omit "(" and
    // ")". As a heruistic we can just check for count and presence of spaces
    // and some other chars.
    std::string template_args = "!(";
    // It looks like spec does not allow empty TemplateArgs.
    // This is a bug in mangling spec. Empty template arguments are allowed,
    // and do mangle to one with Z immediately following template name.
    while (canread1(s, offset) && !startswith(s, 'Z', offset)) {
      DEMANGLE_D_DEBUG(symbol_name_template_arg);
#if DEMANGLE_D_DEBUG
      std::cout << "Parsing template arg" << std::endl;
#endif
      // TemplateArg
      // If a template argument matches a specialized template parameter,
      // the argument is mangled with prefix H.
      bool specialized = false;
      if (startswith(s, 'H', offset)) {
        specialized = true;
      }
      (void)specialized;  // FIXME
      if (template_args.size() >= 3) {
        template_args += ", ";
      }
      // TemplateArgX
      if (startswith(s, 'T', offset)) {
        // Type
        const std::string t =
            type(s, offset, refs, "", return_types, function_attributes);
        template_args += t;
      } else if (startswith(s, 'V', offset)) {
        const std::string t =
            type(s, offset, refs, "", return_types, function_attributes);
        const std::string v = value(s, offset, refs, t);
        // Type Value
        // template_args += t;
        // template_args += " ";
        template_args += v;
      } else if (startswith(s, 'S', offset)) {
        if (startswith(s, "_D", offset)) {
          // Not documented.
          // Does it uses own refs?
          const std::string rr = qualified_name(s, offset, refs, return_types,
                                                function_attributes);
          template_args += rr;
        } else {
          // QualifiedName
          const std::string q = qualified_name(s, offset, refs, return_types,
                                               function_attributes);
          template_args += q;
        }
      } else if (startswith(s, 'X', offset)) {
        // Number ExternallyMangledName
        // ExternallyMangledName can be any series of characters allowed on the
        // current platform, e.g. generated by functions with C++ linkage or
        // annotated with pragma(mangle,...).
        const size_t n = number(s, offset);
        // Very similar to Lname, just without __S support.
        template_args += s.substr(*offset, n);
        // TODO(baryluk): Should it also be stored in refs, For handling with Q?
        *offset += n;
        // } else if ('0' <= s[*offset] && s[*offset] <= '9') {
        //   // alias? This is not documented.
        //   template_args += LName(s, offset, refs);
      } else {
        DEMANGLE_D_DEBUG(symbol_name_template_arg_unknown);
        throw std::runtime_error("Unsupported template argument kind");
      }
    }
#if DEMANGLE_D_DEBUG
    std::cout << "Parsing template done" << std::endl;
#endif
    l += template_args;
    l += ')';
    return l;
  } else if (startswith(s, '0', offset)) {
    // anonymous symbols
    return "(anonymous)";
  } else if (startswith(s, 'Q', offset)) {
    const std::string b = back_reference(s, offset, refs);
    return b;
  } else {
    const std::string l = LName(s, offset, refs);
    return l;
  }
}

std::string qualified_name(const std::string &s, size_t *offset, Refs *refs,
                           bool return_types, bool function_attributes) {
  DEMANGLE_D_DEBUG(qualified_name);
  std::string ret;
  while (true) {
    DEMANGLE_D_DEBUG(qualified_name_symbol_try);
    std::string sym;
    try {
      size_t offset2 = *offset;
      sym = symbol_name(s, &offset2, refs, return_types, function_attributes);
      *offset = offset2;  // success
    } catch (const std::runtime_error &ex) {
      DEMANGLE_D_DEBUG(qualified_name_symbol_no);
#if DEMANGLE_D_DEBUG
      std::cout << "  " << ex.what() << std::endl;
#endif
      // throw;
      return ret;
    }
    DEMANGLE_D_DEBUG(qualified_name_symbol_yes);

    bool needs_this = false;
    std::string tms;
    if (startswith(s, "M", offset)) {
      needs_this = true;
      // The M means that the symbol is a function that requires a this pointer.
      // Class or struct fields are mangled without M. To disambiguate M from
      // being a Parameter with modifier scope, the following type needs to be
      // checked for being a TypeFunction.
      tms = type_modifiers(s, offset);
    }
    (void)needs_this;  // FIXME
    try {
      size_t offset2 = *offset;
#if DEMANGLE_D_DEBUG
      std::cout << " Trying type_function_no_return with sym " << sym
                << std::endl;
#endif
      const std::string tfnr = type_function_no_return(
          s, &offset2, refs, sym, return_types, function_attributes);
#if DEMANGLE_D_DEBUG
      std::cout << " Finished type_function_no_return with sym " << sym << "  "
                << tfnr << std::endl;
#endif
      *offset = offset2;
      if (ret.size()) {
        ret += ".";
      }
      ret += tfnr;
      try {
        size_t offset3 = *offset;
        const std::string return_type =
            type(s, &offset3, refs, "", return_types, function_attributes);
        *offset = offset3;
        if (return_types) {
          ret = return_type + " " + ret;
        }
#if DEMANGLE_D_DEBUG
        std::cout << " Finished qualified_name " << ret << std::endl;
#endif
      } catch (...) {
      }
      if (tms.size()) {
        ret += ' ';
        // Remove last character (space). (pop_back is C++11).
        tms.resize(tms.size() - 1); 
        ret += tms;
      }
      return ret;
    } catch (const std::runtime_error &ex) {
#if DEMANGLE_D_DEBUG
      std::cout << "  " << ex.what() << std::endl;
#endif
      // Assume not a function
      if (tms.size()) {
        // type modifiers present, but not a function!
        throw;
      }
      if (ret.size()) {
        ret += ".";
      }
      ret += sym;
    }
  }
  return ret;
}

std::string mangled_name0(const std::string &s, size_t *offset, Refs *refs,
                          bool return_types, bool function_attributes) {
  DEMANGLE_D_DEBUG(mangled_name0);
  const std::string q =
      qualified_name(s, offset, refs, return_types, function_attributes);
  if (startswith(s, "Z", offset)) {
#if DEMANGLE_D_DEBUG
    std::cout << "Internal demangling finished" << std::endl;
#endif
    return q;  // Internal
  }
  DEMANGLE_D_DEBUG(mangled_name0_qualified_name_done);
  if (canread1(s, offset)) {
    std::string t = type(s, offset, refs, q, return_types, function_attributes);
    DEMANGLE_D_DEBUG(mangled_name0_qualified_type_done);
    if (t.size()) {
      // For functions we need to do a bit of rearanging of return type.
      t += ' ';
      t += q;
      return t;
    } else {
      return q;
    }
  } else {
    return q;
  }
}

std::string mangled_name(const std::string &s, size_t *offset,
                         bool return_types, bool function_attributes) {
  DEMANGLE_D_DEBUG(mangled_name);
  if (!startswith(s, "_D", offset)) {
    return "";  // or s?
  }
  // assert(offset == 2);

  Refs valid_back_refs;
  return mangled_name0(s, offset, &valid_back_refs, return_types,
                       function_attributes);
}

}  // namespace

std::string demangle(const std::string &s, bool return_types,
                     bool function_attributes) {
  size_t offset = 0;
  std::string ret = mangled_name(s, &offset, return_types, function_attributes);
  if (offset != s.size()) {
    // To handle some weird cases like: example.X.__Class.1884 from
    // _D7example1X7__ClassZ.1884
    ret += s.substr(offset, s.size() - offset);
    return ret;
    // return s;  // Could not demangle.
  }
  return ret;
}

}  // namespace demangle_d
