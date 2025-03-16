#ifndef UTIL_HPP
#define UTIL_HPP

#include <vector>
#include <string>
#include <cstddef>

// splits a string by some arbitrary delimiter
std::vector<std::string_view> Tokenize(const std::string_view view, const std::string_view input);

std::string_view StripSpaces(const std::string_view view);


constexpr std::size_t operator "" _zu(unsigned long long const n){
    return static_cast<std::size_t>(n);
}

#endif