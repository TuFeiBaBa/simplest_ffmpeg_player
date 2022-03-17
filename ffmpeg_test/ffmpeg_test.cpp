#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
}

int main()
{
	std::cout << "Hello World!\n";

	std::cout << avcodec_configuration() << std::endl;
}