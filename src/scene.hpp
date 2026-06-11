#pragma once

#include<vector>
#include "cgp/cgp.hpp"

#include "environment.hpp"

using cgp::mesh_drawable;



struct gui_parameters {
	bool display_frame = false;
	bool display_wireframe = false;
	float sea_level = -0.8f;
	int reflect = 0;
	int is_terrain = 0;
	
};



// The structure of the custom scene
struct scene_structure : cgp::scene_inputs_generic {
	
	// ****************************** //
	// Standard Functions
	// ****************************** //

	void initialize();  // Standard initialization to be called before the animation loop
	void display_frame();     // The frame display to be called within the animation loop
	void display_gui(); // The display of the GUI, also called within the animation loop

	// ****************************** //
	// Context
	// ****************************** //

	// Environment controller (background color, )
	environment_structure environment; 
	// Window where the scene is displayed
	window_structure window; 
	// Storage for inputs status (mouse, keyboard, window dimension)
	input_devices inputs; 
	// Standard GUI element storage
	gui_parameters gui; 

	// Display information at the start of the program
	void display_info();

	// ****************************** //
	// Camera controller
	// ****************************** //

	// Controller of the camera (extrinsic parameters: position/orientation) -- to be adapted to the desired model and behavior
	camera_controller_orbit_euler camera_control; 

	// The model of camera projection (intrinsic parameters)
	camera_projection_perspective camera_projection;


	//textures qui me seront utiles
	cgp::opengl_texture_image_structure texture_rock; // Identifiant pour la texture de la montagne
	cgp::opengl_texture_image_structure texture_grass; // second texture used for terrain blending
	cgp::opengl_texture_image_structure texture_sky; // texture ciel pour réflexion eau

	std::vector<cgp::vec3> tree_positions;

	// Per-tree scaling factor to vary tree sizes
	std::vector<float> tree_scales;

	// Grass positions and scales
	std::vector<cgp::vec3> grass_positions;
	std::vector<float> grass_scales;

	// Prototype drawable for a single grass blade
	cgp::mesh_drawable grass;


	
	
	// ****************************** //
	// Elements and shapes of the scene
	// ****************************** //

	mesh_drawable global_frame;          // The standard global frame

	timer_basic timer;

	mesh_drawable terrain;
	mesh_drawable water;
	// Stored mesh for water so we can animate its vertices for waves
	mesh water_mesh;
	float water_base_z = -0.8f;
	mesh_drawable tree;
	mesh_drawable cube1;
	mesh_drawable cube2;
	mesh_drawable sky;
	mesh_drawable ship;
	mesh_drawable sunken_ship;
	mesh_drawable treasure_chest;


	//Birds
	mesh_drawable bird_body;
	mesh_drawable bird_head;
	mesh_drawable bird_beak;
	mesh_drawable bird_left_wing;
	mesh_drawable bird_right_wing;
	mesh_drawable bird_tail;
	std::vector<cgp::vec3> tree2_positions;
	std::vector<float> tree2_scales;
	std::vector<mesh_drawable> tree2_parts;

	//Birds movement
	vec3 bird_position = { 0.0f, 0.0f, 8.0f };
	vec3 bird_direction = { 1.0f, 0.0f, 0.0f };

	struct BirdGroup {
		int count;
		float scale;
		cgp::vec3 color;
		float orbit_radius_x;
		float orbit_radius_y;
		float base_altitude;
		float angular_speed;
		float phase;

		// Nouveau : trajectoire forêt
		bool forest_bird = false;      // true = ce groupe fait le tour depuis la forêt
		cgp::vec3 nest_position;       // position du nid dans la forêt
		float tour_duration = 20.0f;   // durée d'un aller-retour complet en secondes
	};
	std::vector<BirdGroup> bird_groups;

	float bird_orbit_radius_x = 10.0f;
	float bird_orbit_radius_y = 7.0f; 
	float bird_base_altitude = 6.0f;
	float bird_angular_speed = 0.5f;

	float bird_wing_angle = 0.0f;
	float bird_wing_amplitude = 0.6f;
	float bird_wing_frequency = 3.0f;

	// Shadow mapping resources
	int shadow_resolution = 2048;
	cgp::opengl_fbo_structure shadow_fbo;
	cgp::mat4 light_view_projection; 
	cgp::vec3 light_direction = { -0.5f, -0.6f, -1.0f };
	float shadow_bias = 0.007f; // bias to reduce shadow acne
	float shadow_strength = 0.55f; // multiplicative factor when in shadow

	


	// ****************************** //
	// Callback functions
	// ****************************** //
	void mouse_move_event();
	void mouse_click_event();
	void keyboard_event();
	void idle_frame();

};





