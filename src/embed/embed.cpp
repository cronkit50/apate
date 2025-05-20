#include "embed.hpp"

#include "log/log.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

static size_t CurlWriteToString(void* contents, size_t size, size_t nmemb, std::string* response) {
    const size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}


std::future<std::vector<float>> TransformSentence(const std::string_view message, const std::string_view server, int port){
    std::future<std::vector<float>> future = std::async(std::launch::async,

        [](const std::string &message,
            const std::string &server,
            const int port){

            CURL* curl = curl_easy_init();

            // make the embedding request
            nlohmann::json request;
            request["text"] = message;

            std::string requestStr = request.dump ();

            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");

            std::string embedResponse;
            std::string url = std::format("http://{}:{}/embed", server, port);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str ());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestStr.c_str ());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestStr.size ());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteToString);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &embedResponse);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

            auto HTTPCode = curl_easy_perform(curl);


            std::vector<float> result;

            if(HTTPCode != CURLE_OK){
                APATE_LOG_WARN("Failed to get embedding from server {}:{} - {}",
                               server,
                               port,
                               curl_easy_strerror(HTTPCode));
            }
            else{
                try{
                    nlohmann::json response = nlohmann::json::parse(embedResponse);

                    if(response.contains("embedding") && response["embedding"].is_array()){
                        for(const auto& item : response["embedding"]){
                            result.push_back(item.get<float>());
                        }
                    }
                    else{
                        APATE_LOG_WARN("Invalid embedding response - {}",
                                       embedResponse);
                    }
                }
                catch (...){
                    APATE_LOG_WARN("Failed to parse embedding response - {}",
                                   embedResponse);
                }
            }


            if (nullptr != headers){
                curl_slist_free_all (headers);
            }

            if (nullptr != curl){
                curl_easy_cleanup(curl);
            }

            return result;

            }, std::string (message), std::string (server), port);


    return future;
}

std::future<Embeddings> TransformSentences(const std::vector<std::string> &messages, const std::string_view server, int port){
    std::future<Embeddings> future = std::async(std::launch::async,

        [](const std::vector<std::string> &messages,
           const std::string &server,
           const int port){

            CURL* curl = curl_easy_init();

            // make the embedding request
            nlohmann::json request;
            for(const auto& message : messages){
                request["texts"].push_back(message);
            }

            std::string requestStr = request.dump ();

            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");

            std::string embedResponse;
            std::string url = std::format("http://{}:{}/embed", server, port);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str ());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestStr.c_str ());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestStr.size ());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteToString);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &embedResponse);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

            auto HTTPCode = curl_easy_perform(curl);

            Embeddings result;

            if(HTTPCode != CURLE_OK){
                APATE_LOG_WARN("Failed to get embedding from server {}:{} - {}",
                               server,
                               port,
                               curl_easy_strerror(HTTPCode));
            }
            else{
                try{
                    nlohmann::json response = nlohmann::json::parse(embedResponse);

                    if(response.contains("embedding") && response["embedding"].is_array()){
                        auto& embeddingsArray = response["embedding"];

                        for(const auto& embeddings : embeddingsArray){
                            result.push_back(embeddings.get<std::vector<float>>());
                        }
                    }
                    else{
                        APATE_LOG_WARN("Invalid embedding response - {}",
                                       embedResponse);
                    }
                }
                catch (...){
                    APATE_LOG_WARN("Failed to parse embedding response - {}",
                                   embedResponse);
                }
            }


            if (nullptr != headers){
                curl_slist_free_all (headers);
            }

            if (nullptr != curl){
                curl_easy_cleanup(curl);
            }

            return result;

            }, messages, std::string (server), port);


    return future;
}
