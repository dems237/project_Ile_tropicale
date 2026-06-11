
// CGP library
#include "cgp/cgp.hpp" 

// Running application
#include "application.hpp"

// Custom scene of this code
#include "scene.hpp"

#include <iostream>
#include <filesystem>



int main(int, char* argv[])
{
	std::cout << "project::path = " << project::path << std::endl;
	std::cout << "rock.jpeg exists? = " << std::filesystem::exists(project::path + "assets/rock.jpeg") << std::endl;
	std::cout << "rock.jpg exists?  = " << std::filesystem::exists(project::path + "assets/rock.jpg") << std::endl;
	scene_structure scene;
	application_structure app;
	app.initialize(argv[0], &scene);
	
	app.start_loop();

	return 0;
}

