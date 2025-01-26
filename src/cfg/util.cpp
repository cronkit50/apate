#include "util.hpp"


std::vector<std::string_view> Tokenize(const std::string_view view, const std::string_view delim)
{
    std::vector<std::string_view> split;
    if (delim.empty() || view.empty())
    {
        return split;
    }

    size_t lastPos = 0;
    size_t currPos = 0;
    while ((currPos = view.find(delim, lastPos)) != std::string::npos)
    {
        split.emplace_back(view.substr(lastPos,
                           currPos - lastPos));

        lastPos = currPos + delim.size();
    }

    // add trailing tokens
    if (lastPos < view.size())
    {
        split.emplace_back(view.substr(lastPos,
                           view.size() - lastPos));
    }

    return split;
}

std::string_view StripSpaces(const std::string_view view)
{
    if (view.empty())
    {
        return view;
    }
    size_t leadingSpaces  = view.find_first_not_of(' ');
    size_t trailingSpaces = view.find_last_not_of(' ');

    return view.substr(leadingSpaces, trailingSpaces - leadingSpaces + 1);
}
