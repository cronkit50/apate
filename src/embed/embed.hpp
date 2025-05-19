#ifndef EMBED_HPP
#define EMBED_HPP

#include <future>
#include <vector>

std::future<std::vector<float>> TransformSentence(const std::string_view message,
                                                  const std::string_view server = "127.0.0.1",
                                                  int port = 5000);


#endif