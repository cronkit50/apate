#include "openai.hpp"

openai::openai(void)
{
	CURL* curl = NULL;
	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
}
