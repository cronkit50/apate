#include "util.hpp"

std::vector<std::string> SplitString(const std::string_view& view, const std::string_view& delim)
{
    std::vector<std::string> split;
    if (delim.empty() || view.empty())
    {
        return split;
    }

    size_t lastPos = 0;
    size_t currPos = 0;
    while ((currPos = view.find(delim, lastPos)) != std::string::npos)
    {
        split.push_back(std::string(view.substr(lastPos,
                                    currPos - lastPos)));

        lastPos = currPos + delim.size();
    }

    // add trailing tokens
    if (lastPos < view.size())
    {
        split.push_back(std::string(view.substr(lastPos, view.size() - lastPos)));
    }

    return split;
}
