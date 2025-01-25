#ifndef UTIL_HPP
#define UTIL_HPP

#include <vector>
#include <string>

// splits a string by some arbitrary delimiter
std::vector<std::string> SplitString(const std::string_view& view, const std::string_view& input);

#endif