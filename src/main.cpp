#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include "application.hpp"

int main()
{
	VE::Application app;

	try
	{
		app.Run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}