#include "vk_engine.h"

int main(int argc, char* argv[])
{
	VulkanEngine engine;

	engine.Init();
	engine.MainLoop();
	engine.Cleanup();

	return 0;
}