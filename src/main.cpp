#include <iostream>
#include <cstdlib>
#include <stdexcept>

//#define GLM_FORCE_RADIANS
//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#define GLM_ENABLE_EXPERIMENTAL
//#include <glm/gtx/hash.hpp>
//
//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
//
//#define TINYOBJLOADER_IMPLEMENTATION
//#include <tiny_obj_loader.h>

#include "application.hpp"

const std::string MODEL_PATH = "../../../src/models/viking_room.obj";
const std::string TEXTURE_PATH = "../../../src/textures/viking_room.png";

const int MAX_FRAMES_IN_FLIGHT = 2;

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