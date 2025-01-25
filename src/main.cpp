// DPP extraneous warnings
#pragma warning( disable : 4251 )

#include <iostream>

#include <dpp/cluster.h>
int main(int argc, char* argv[]) {

    dpp::cluster bot ("future token");

    std::cout << "Hello World!";
}
