#ifndef APATE_HPP
#define APATE_HPP


#include <dpp/dpp.h>

class apate {
public:
	apate(const std::string& token);

private:
	dpp::cluster bot;
};

#endif