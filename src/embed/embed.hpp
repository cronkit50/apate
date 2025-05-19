#ifndef EMBED_HPP
#define EMBED_HPP

#include <future>
#include <vector>
typedef std::vector<std::vector<float>> Embeddings;

std::future<Embeddings> TransformSentences(const std::vector<std::string> &messages,
                                           const std::string_view server = "127.0.0.1",
                                           int port = 5000);

#endif