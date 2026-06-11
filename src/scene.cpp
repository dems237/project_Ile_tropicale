#include "scene.hpp"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace cgp;

/*void deform_terrain(mesh& m)
{
	// Set the terrain to have a gaussian shape
	for (int k = 0; k < m.position.size(); ++k)
	{
		vec3& p = m.position[k];
		float d2 = p.x*p.x + p.y * p.y;
		float z = exp(-d2 / 4)-1;

		z = z + 0.05f*noise_perlin({ p.x,p.y });

		p = { p.x, p.y, z };
	}

	m.normal_update();
}*/

// Implémentation C++ de la fonction GLSL smoothstep

float smoothstep(float edge0, float edge1, float x) {
	// 1. On normalise et on contraint (clamp) la valeur entre 0.0 et 1.0
	float t = std::max(0.0f, std::min(1.0f, (x - edge0) / (edge1 - edge0)));

	// 2. On applique la courbe de lissage cubique
	return t * t * (3.0f - 2.0f * t);
}

float compute_island_mask(float x, float y) {
	float warp_x = x + 1.5f * noise_perlin({ x * 0.1f, y * 0.1f });
	float warp_y = y + 1.5f * noise_perlin({ x * 0.1f + 10.0f, y * 0.1f + 10.0f });
	float d2 = warp_x * warp_x + warp_y * warp_y;
	float continent_mask = std::exp(-d2 / 40.0f);

	float fault_distortion = 3.0f * noise_perlin({ y * 0.2f, x * 0.1f });
	float dist_to_fault = std::abs(x + fault_distortion);
	float rift_profile = smoothstep(01.2f, 3.5f, dist_to_fault);

	return continent_mask * rift_profile;
}


void deform_terrain(mesh& m)
{
	for (int k = 0; k < m.position.size(); ++k)
	{
		vec3& p = m.position[k];

		// 1. Le Continent Massif (Base unique)
		float warp_x = p.x + 1.5f * noise_perlin({ p.x * 0.1f, p.y * 0.1f });
		float warp_y = p.y + 1.5f * noise_perlin({ p.x * 0.1f + 10.0f, p.y * 0.1f + 10.0f });

		float d2 = warp_x * warp_x + warp_y * warp_y;
		// Une base très large (divisée par 15.0f) pour faire une seule grosse île
		float continent_mask = std::exp(-d2 / 40.0f);

		// 2. La Ligne de Fracture (La faille qui coupe l'île)
		// La faille suit globalement l'axe Y, mais on la tord avec du Perlin noise
		float fault_distortion = 3.0f * noise_perlin({ p.y * 0.2f, p.x * 0.1f });
		float dist_to_fault = std::abs(p.x + fault_distortion);

		// 3. Creuser le Canyon
		// smoothstep(largeur_du_canal, largeur_du_haut_de_falaise, distance)
		// En dessous de 0.8f : c'est sous l'eau. 
		// À partir de 2.5f : c'est le sommet de la montagne.
		float rift_profile = smoothstep(0.8f, 2.5f, dist_to_fault);

		// On coupe le continent avec le rift
		float island_mask = continent_mask * rift_profile;

		// 4. Relief Rocailleux (fBm)
		float mountain_noise = 0.0f;
		float amp = 1.2f;
		float freq = 0.5f;
		for (int i = 0; i < 4; ++i) {
			// Utilisation de std::abs pour avoir des arêtes rocheuses
			mountain_noise += amp * std::abs(noise_perlin({ p.x * freq, p.y * freq }));
			amp *= 0.5f;
			freq *= 2.0f;
		}

		// 5. Calcul des altitudes
		float water_level = -0.8f;
		float seabed_depth = -5.0f;
		float max_height = 8.0f; // Île très haute pour marquer le vertige de la faille

		float z;
		if (island_mask > 0.01f) {
			// Terrain émergé et falaises du canyon
			// Le bruit est fortement appliqué sur les hauteurs, atténué près de l'eau
			float height = water_level + (island_mask * max_height) - (mountain_noise * island_mask);

			// On s'assure de connecter la base sous-marine
			z = std::max(height, seabed_depth + 0.8f * noise_perlin({ p.x * 0.5f, p.y * 0.5f }));
		}
		else {
			// Le canal central et l'océan autour
			z = seabed_depth + 0.5f * noise_perlin({ p.x * 0.5f, p.y * 0.5f });
		}

		p = { p.x, p.y, z };
	}

	m.normal_update();
}






// Charge uniquement les faces d'un objet nommé dans un .obj
mesh load_obj_object(const std::string& filepath, const std::string& object_name)
{
	std::ifstream file(filepath);
	mesh m;

	std::vector<cgp::vec3> all_positions;
	std::vector<cgp::vec2> all_uvs;
	std::vector<cgp::vec3> all_normals;

	bool in_target = false;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream ss(line);
		std::string token;
		ss >> token;

		if (token == "o") {
			std::string name; ss >> name;
			in_target = (name == object_name);
		}
		else if (token == "v") {
			float x, y, z; ss >> x >> y >> z;
			all_positions.push_back({ x, y, z });
		}
		else if (token == "vt") {
			float u, v; ss >> u >> v;
			all_uvs.push_back({ u, v });
		}
		else if (token == "vn") {
			float x, y, z; ss >> x >> y >> z;
			all_normals.push_back({ x, y, z });
		}
		else if (token == "f" && in_target) {
			// Format: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
			std::vector<int> vi, ti, ni;
			std::string vert;
			while (ss >> vert) {
				std::replace(vert.begin(), vert.end(), '/', ' ');
				std::istringstream vs(vert);
				int v = 0, t = 0, n = 0;
				vs >> v >> t >> n;
				vi.push_back(v - 1);
				ti.push_back(t - 1);
				ni.push_back(n - 1);
			}
			// Triangulate (fan)
			for (int k = 1; k + 1 < vi.size(); ++k) {
				int idx = m.position.size();
				m.position.push_back(all_positions[vi[0]]);
				m.position.push_back(all_positions[vi[k]]);
				m.position.push_back(all_positions[vi[k + 1]]);
				if (!all_uvs.empty()) {
					m.uv.push_back({ all_uvs[ti[0]].x,   1.0f - all_uvs[ti[0]].y });
					m.uv.push_back({ all_uvs[ti[k]].x,   1.0f - all_uvs[ti[k]].y });
					m.uv.push_back({ all_uvs[ti[k + 1]].x, 1.0f - all_uvs[ti[k + 1]].y });
				}
				if (!all_normals.empty()) {
					m.normal.push_back(all_normals[ni[0]]);
					m.normal.push_back(all_normals[ni[k]]);
					m.normal.push_back(all_normals[ni[k + 1]]);
				}
				m.connectivity.push_back({ idx, idx + 1, idx + 2 });
			}
		}
	}
	m.fill_empty_field();
	return m;
}









// Main initialization function called once at program startup
// Sets up the camera, 3D scene elements, and the image animation system
void scene_structure::initialize()
{
	
	std::cout << "Start function scene_structure::initialize()" << std::endl;

	// Set the behavior of the camera and its initial position
	// ********************************************** //
	camera_control.initialize(inputs, window); 
	camera_control.set_rotation_axis_z(); // camera rotates around z-axis
	//   look_at(camera_position, targeted_point, up_direction)
	camera_control.look_at(
		{ 5.0f, -4.0f, 3.5f } /* position of the camera in the 3D scene */,
		{0,0,0} /* targeted point in 3D scene */,
		{0,0,1} /* direction of the "up" vector */);

	camera_projection = camera_projection_perspective{
		80.0f * Pi/180, // Field of view
		1.0f,           // Aspect ratio
		0.01f,          // Depth min
		1000            // Depth max
	};


	// General information
	display_info();

	// Create 3D coordinate frame (x, y, z axes) for visual reference
	global_frame.initialize_data_on_gpu(mesh_primitive_frame());

	// Initialize the shapes of the scene
	// ***************************************** //

	gui.display_frame = true;


	/* * * * * * *
		terrain
	* * * * * * */

	float L = 25.0f;
	mesh terrain_mesh = mesh_primitive_grid({ -L,-L,0 }, { L,-L,0 }, { L,L,0 }, { -L,L,0 }, 1000, 1000);
	// ...
	deform_terrain(terrain_mesh);
	
	
	terrain.initialize_data_on_gpu(terrain_mesh);
	terrain.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/sand.jpg", GL_REPEAT, GL_REPEAT);
	texture_rock.load_and_initialize_texture_2d_on_gpu(project::path + "assets/rock.jpg", GL_REPEAT, GL_REPEAT);
	// load a dedicated grass texture and bind it to unit 1
	texture_grass.load_and_initialize_texture_2d_on_gpu(project::path + "assets/grass.png", GL_REPEAT, GL_REPEAT);



	/* --- GENERATION D'HERBE ---
	// Create a simple grass blade mesh (a small triangle) and initialize the prototype drawable
	mesh grass_mesh;
	grass_mesh.position = { {0.0f, 0.0f, 0.0f}, {0.13f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.15f} };
	grass_mesh.connectivity = { {0,1,2} };
	grass_mesh.fill_empty_field();
	grass.initialize_data_on_gpu(grass_mesh);
	grass.material.color = { 0.28f, 0.65f, 0.18f };

	// Sample positions on the terrain for grass blades (prefer higher altitudes and gentle slopes)
	grass_positions.clear();
	grass_scales.clear();

	int grass_desired = 2000; // number of blades
	int grass_attempts = 0;
	int grass_attempts_max = 50000;
	while (grass_positions.size() < grass_desired && grass_attempts < grass_attempts_max) {

		++grass_attempts;
		float r = std::rand() / (float)RAND_MAX;
		int idx = (int)(r * (terrain_mesh.position.size() - 1));
		vec3 p = terrain_mesh.position[idx];
		vec3 n = terrain_mesh.normal[idx];

		// Place grass on relatively high areas (mountain) and not on steep slopes
		if (p.z > 0.50f && n.z > 0.75f) {
			grass_positions.push_back(p);
			// random scale between 0.6 and 1.4
			float s = 0.6f + 0.8f * (std::rand() / (float)RAND_MAX);
			grass_scales.push_back(s);
		}
	}

	std::cout << "Nombre d'herbes plantes : " << grass_positions.size() << std::endl;
	*/

	/* * * * * * *
	    Water
	* * * * * * */
	
	float sea_w = 100.0;
	float sea_z = -0.8f;
	// Create water mesh and store a copy to animate
	water_mesh = mesh_primitive_grid({ -sea_w,-sea_w,sea_z }, { sea_w,-sea_w,sea_z }, { sea_w,sea_w,sea_z }, { -sea_w,sea_w,sea_z }, 200, 200);
	water.initialize_data_on_gpu(water_mesh);
	//water.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/sea.png");
	water.material.color = {
	0.25f,
	0.52f,
	0.75f
	};

	water.material.phong.ambient = 0.35f;
	water.material.phong.diffuse = 0.45f;
	water.material.phong.specular = 8.0f;
	water.material.phong.specular_exponent = 128.0f;
	


	/* * * * * * *
		trees
	* * * * * * */
	tree.initialize_data_on_gpu(mesh_load_file_obj(project::path + "assets/palm_tree/palm_tree.obj"));
	tree.model.rotation = rotation_transform::from_axis_angle({ 1,0,0 }, Pi / 2.0f);
	tree.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/palm_tree/palm_tree.jpg", GL_REPEAT, GL_REPEAT);

	// --- NOUVEAU : PLANTATION DE LA FORÊT (Échantillonnage Aléatoire) ---
	tree_positions.clear();
	tree_scales.clear();

	// Seed RNG for variability
	std::srand((unsigned)std::time(nullptr));

	int arbres_souhaites = 200; // Le nombre total d'arbres sur l'île
	int tentatives_max = 20000; // Sécurité pour éviter une boucle infinie
	int tentatives = 0;

	// On boucle jusqu'à avoir planté nos arbres (ou atteint la limite de tentatives)
	while (tree_positions.size() < arbres_souhaites && tentatives < tentatives_max) {
		tentatives++;

		// 1. On pioche un sommet totalement au hasard sur le terrain
		float pourcentage = std::rand() / (float)RAND_MAX;

		// 2. On l'applique à la taille totale pour débloquer l'accès aux sommets
		int index_aleatoire = pourcentage * (terrain_mesh.position.size() - 1);

		vec3 p = terrain_mesh.position[index_aleatoire];
		vec3 n = terrain_mesh.normal[index_aleatoire];

		// 2. Règles de pousse (Biologie)
		// - p.z > -0.5f : L'arbre doit être hors de l'eau
		// - n.z > 0.75f : L'arbre peut pousser sur du plat et de légères pentes, mais pas sur les falaises (roche)
		if (p.z > -0.5f && n.z > 0.75f) {

			// Add the tree position
			tree_positions.push_back(p);

			// Assign a random scale for this tree between 0.6 and 1.4
			float scale = 0.6f + 0.8f * (std::rand() / (float)RAND_MAX);
			tree_scales.push_back(scale);
		}
	}


	std::cout << "Nombre d'arbres plantes : " << tree_positions.size() << std::endl;
	// ---------------------------------------------------------------------





	tree2_positions.clear();
	tree2_scales.clear();

	int arbres2_souhaites = 150;
	int tentatives2_max = 20000;
	int tentatives2 = 0;

	while (tree2_positions.size() < arbres2_souhaites && tentatives2 < tentatives2_max) {
		tentatives2++;

		float pourcentage = std::rand() / (float)RAND_MAX;
		int index_aleatoire = pourcentage * (terrain_mesh.position.size() - 1);

		vec3 p = terrain_mesh.position[index_aleatoire];
		vec3 n = terrain_mesh.normal[index_aleatoire];
		float island_mask = compute_island_mask(p.x, p.y);
		float theoretical_max = -0.6f + island_mask * 8.0f;
		bool on_continent = island_mask > 0.15f && p.z < theoretical_max * 0.85f;
		if (p.z > 1.0f && n.z > 0.65f && on_continent && p.z<7) {
			p.z -= 0.05;
			tree2_positions.push_back(p);
			float scale = 0.03f + 0.01f * (std::rand() / (float)RAND_MAX);
			tree2_scales.push_back(scale);
		}
	}
	// Charger chaque partie avec sa texture
	struct TreePart {
		std::string object_name;
		std::string texture_file;
	};

	std::vector<TreePart> parts = {
		{ "Walnut_L", "assets/tree/Texture/Walnut_L.jpg" },
		{ "Mossy_Tr", "assets/tree/Texture/Mossy_Tr.jpg" },
		{ "Bark___S", "assets/tree/Texture/Bark___S.jpg" },
		{ "Bark___0", "assets/tree/Texture/Bark___0.jpg" },
		{ "Bottom_T", "assets/tree/Texture/Bottom_T.jpg" },
		{ "Sonnerat", "assets/tree/Texture/Sonnerat.jpg" },
		{ "Bark___1", "assets/tree/Texture/Bark___1.jpg" },
		{ "Oak_Leav", "assets/tree/Texture/Oak_Leav.jpg" },
	};

	tree2_parts.clear();
	std::string obj_path = project::path + "assets/tree/trees9.obj";

	for (auto& part : parts) {
		mesh m = load_obj_object(obj_path, part.object_name);
		if (m.position.size() == 0) {
			std::cout << "Objet non trouve : " << part.object_name << std::endl;
			continue;
		}
		mesh_drawable d;
		d.initialize_data_on_gpu(m);
		d.texture.load_and_initialize_texture_2d_on_gpu(
			project::path + part.texture_file, GL_REPEAT, GL_REPEAT);
		d.model.rotation = rotation_transform::from_axis_angle({ 1,0,0 }, Pi / 2.0f);
		tree2_parts.push_back(d);
		std::cout << "Objet charge : " << part.object_name << " (" << m.position.size() << " vertices)" << std::endl;
	}



	/* * * * * * *
	* cubes 
	* * * * * * */
	cube1.initialize_data_on_gpu(mesh_primitive_cube({ 0,0,0 }, 0.5f));
	cube1.model.rotation = rotation_transform::from_axis_angle({ -1,1,0 }, Pi / 7.0f);
	cube1.model.translation = { 1.0f,1.0f,-0.1f };
	cube1.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/wood.jpg");
	cube2 = cube1;




	/* * * * * * * *
	      Sky+
	 * * * * * * * */

	sky.initialize_data_on_gpu(mesh_primitive_sphere(1.0f));
	sky.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/sky.jpg");
	// Charger la texture ciel dédiée à la réflexion de l'eau
	texture_sky.load_and_initialize_texture_2d_on_gpu(
		project::path + "assets/sky.jpg", GL_REPEAT, GL_REPEAT);
	sky.model.scaling = 100.f;
	sky.material.phong.ambient = 1.0f;
	sky.material.phong.diffuse = 0.0f;
	sky.material.phong.specular = 0.0f;



	/* * * * * * *
		ship
	* * * * * * */
	//Passons au au bateau de pirate
	mesh mesh_ship = mesh_load_file_obj(project::path + "assets/ship/ship.obj");
	ship.initialize_data_on_gpu(mesh_ship);
	ship.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/wood.jpg");
	ship.model.scaling = 0.05f;
	ship.model.rotation = cgp::rotation_transform::from_axis_angle(
		{ 1.0f, 0.0f, 0.0f },  // axe x
		cgp::Pi / 2.0f        // 90 degrés 
	);
	ship.model.translation = { 5.0f, 18.50f, 1.20f };


/* * * * * * * * * * * * *
	Sunken ship + coffre
 * * * * * * * * * * * * */

// Bateau coulé : même mesh que le ship de surface
	mesh mesh_sunken = mesh_load_file_obj(project::path + "assets/ship/ship.obj");
	sunken_ship.initialize_data_on_gpu(mesh_sunken);
	sunken_ship.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/wood.jpg");
	sunken_ship.model.scaling = 0.05f;
	// Couché sur le côté pour faire naufragé
	//sunken_ship.model.rotation = rotation_transform::from_axis_angle({ 0,1,0 }, Pi / 5.0f);
	sunken_ship.model.translation = { 18.0f, 9.0f, -4.8f };
	// Teinte bleutée pour l'effet sous-marin
	sunken_ship.material.color = { 0.55f, 0.65f, 0.75f };
	sunken_ship.material.phong.ambient = 0.5f;
	sunken_ship.material.phong.diffuse = 0.3f;
	sunken_ship.material.phong.specular = 0.1f;

	// Coffre : assemblage de primitives CGP
	// Corps = cube aplati
	mesh chest_body = mesh_primitive_cube({ 0,0,0 }, 1.0f);
	// Étirer : large, profond, peu haut
	for (vec3& p : chest_body.position) {
		p.x *= 1.4f;
		p.y *= 0.9f;
		p.z *= 0.6f;
	}
	chest_body.normal_update();
	treasure_chest.initialize_data_on_gpu(chest_body);
	treasure_chest.texture.load_and_initialize_texture_2d_on_gpu(project::path + "assets/wood.jpg");
	treasure_chest.model.translation = { 17.5f, 9.5f, -4.7f };
	treasure_chest.model.rotation = rotation_transform::from_axis_angle({ 0,0,1 }, 0.4f);
	treasure_chest.model.scaling = 0.4f;
	treasure_chest.material.color = { 0.55f, 0.40f, 0.20f };
	treasure_chest.material.phong.ambient = 0.4f;
	treasure_chest.material.phong.diffuse = 0.3f;
	treasure_chest.material.phong.specular = 0.05f;




	/* * * * * * * * * *
	       Birds
	 * * * * * * * * * */

	//Initializing the body of the bird
	
	mesh bird_body_mesh = mesh_primitive_sphere(1.0f);
	for (vec3& p : bird_body_mesh.position) {
		p.x *= 1.4f;
		p.y *= 0.45f;
		p.z *= 0.45f;
	}
	bird_body_mesh.normal_update();
	bird_body.initialize_data_on_gpu(
		bird_body_mesh
	);
	
	bird_body.material.color = { 0.65f, 0.65f, 0.68f };
	bird_body.model.scaling = 0.2f;

	//initializing the head now
	bird_head.initialize_data_on_gpu(
		mesh_primitive_sphere(0.45f)
	);
	
	bird_head.material.color = {
		0.8f,
		0.8f,
		0.82f
	};
	bird_head.model.scaling = 0.2f;


	//Initializing the beak
	bird_beak.initialize_data_on_gpu(
		mesh_primitive_cone(
			0.18f,
			0.6f,
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f }
		)
	);

	bird_beak.material.color = {
		0.95f,
		0.65f,
		0.1f
	};
	bird_beak.model.scaling = 0.2f;


	//Initializing the wings
	mesh left_wing_mesh;

	left_wing_mesh.position = {
		{ 0.4f,  0.0f, 0.0f },
		{-0.7f,  2.2f, 0.0f },
		{-1.0f,  0.2f, 0.0f }
	};

	left_wing_mesh.connectivity = {
		{0, 1, 2}
	};

	left_wing_mesh.fill_empty_field();

	bird_left_wing.initialize_data_on_gpu(left_wing_mesh);
	bird_left_wing.material.color = {
		0.55f,
		0.55f,
		0.58f
	};
	bird_left_wing.model.scaling = 0.2f;

	mesh right_wing_mesh;

	right_wing_mesh.position = {
		{ 0.4f,  0.0f, 0.0f },
		{-0.7f, -2.2f, 0.0f },
		{-1.0f, -0.2f, 0.0f }
	};

	right_wing_mesh.connectivity = {
		{0, 1, 2}
	};

	right_wing_mesh.fill_empty_field();

	bird_right_wing.initialize_data_on_gpu(right_wing_mesh);
	bird_right_wing.material.color = {
		0.55f,
		0.55f,
		0.58f
	};
	bird_right_wing.model.scaling = 0.2f;


	// Choisir une position de nid dans la forêt (un des arbres existants)
	vec3 nest_pos = { 0.0f, 0.0f, 0.0f };
	if (!tree2_positions.empty())
		nest_pos = tree2_positions[0] + vec3{ 0, 0, 0.5f };
	bird_groups = {
		// count, scale, color,                 rx,   ry,  alt,  speed, phase
		{ 4,  0.08f, {0.65f, 0.65f, 0.68f},  8.0f, 5.0f, 7.0f, 0.4f,  0.0f  },
		{ 7,  0.05f, {0.85f, 0.80f, 0.70f},  6.0f, 9.0f, 5.5f, 0.6f,  1.2f  },
		{ 3,  0.10f, {0.40f, 0.40f, 0.45f}, 11.0f, 7.0f, 9.0f, 0.3f,  2.5f  },
	};

	BirdGroup forest_group;
	forest_group.count = 3;
	forest_group.scale = 0.04f;
	forest_group.color = { 0.3f, 0.25f, 0.2f }; // brun foncé
	forest_group.forest_bird = true;
	forest_group.nest_position = nest_pos;
	forest_group.tour_duration = 25.0f;
	forest_group.angular_speed = 2.0f * Pi / forest_group.tour_duration;
	forest_group.orbit_radius_x = 9.0f;
	forest_group.orbit_radius_y = 7.0f;
	forest_group.base_altitude = 4.0f;
	forest_group.phase = 0.0f;
	bird_groups.push_back(forest_group);





	// Initialize shadow FBO for shadow mapping
	shadow_fbo.mode = cgp::opengl_fbo_mode::depth;
	shadow_fbo.width = shadow_resolution;
	shadow_fbo.height = shadow_resolution;
	shadow_fbo.initialize();

	// Compute light view-projection (directional light)
	vec3 ld = normalize(light_direction);
	// position the light far along the direction
	vec3 light_pos = -ld * 50.0f;
	vec3 target = { 0.0f, 0.0f, 0.0f };
	// Build lookAt manually using available functions: use build_translation and rotation
	mat4 view_light = mat4::build_translation(-light_pos);
	float ortho_size = 60.0f;
	// Build a simple orthographic projection matrix manually
	mat4 proj_light = mat4(
		1.0f / ortho_size, 0, 0, 0,
		0, 1.0f / ortho_size, 0, 0,
		0, 0, -2.0f / (200.0f - 1.0f), -(200.0f + 1.0f) / (200.0f - 1.0f),
		0, 0, 0, 1.0f);
	light_view_projection = proj_light * view_light;

	std::cout << "End function scene_structure::initialize()" << std::endl;
}

















vec3 bird_trajectory(float t, float radius_x, float radius_y, float altitude,
	float angular_speed) {
	float angle = angular_speed * t;

	float x = radius_x * std::cos(angle);
	float y = radius_y * std::sin(angle);

	float vertical_oscillation =
		1.2f * std::sin(0.7f * t);

	float z = altitude + vertical_oscillation;

	return { x, y, z };
}

vec3 bird_trajectory_derivative(
	float t,
	float radius_x,
	float radius_y,
	float angular_speed
)
{
	float angle = angular_speed * t;

	float dx =
		-radius_x
		* angular_speed
		* std::sin(angle);

	float dy =
		radius_y
		* angular_speed
		* std::cos(angle);

	float dz =
		1.2f
		* 0.7f
		* std::cos(0.7f * t);

	return { dx, dy, dz };
}

// This function is called permanently at every new frame
// Note that you should avoid having costly computation and large allocation defined there. This function is mostly used to call the draw() functions on pre-existing data.
void scene_structure::display_frame()
{
	// Set the light to the current position of the camera
    camera_projection.aspect_ratio = window.aspect_ratio();
	environment.camera_projection = camera_projection.matrix();
	environment.camera_view = camera_control.camera_model.matrix_view();
	environment.light = camera_control.camera_model.position();

	//Linkage avec la deuxieme texture pour la montagne
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texture_grass.id); // L'attribut qui contient le GLuint
	//opengl_uniform(terrain.shader, "image_texture_grass", 1);
	glActiveTexture(GL_TEXTURE0);
	
	environment.uniform_generic.uniform_int["is_terrain"] = gui.is_terrain;
	

	// Draw the 3D reference frame axes if enabled
	if (gui.display_frame)
		draw(global_frame, environment);

	// Update time
	timer.update();

	

	// Draw all the shapes

	// ---------- Shadow pass ----------
	GLint prev_viewport[4]; glGetIntegerv(GL_VIEWPORT, prev_viewport);
	glViewport(0, 0, shadow_fbo.width, shadow_fbo.height);
	shadow_fbo.bind();
	glClear(GL_DEPTH_BUFFER_BIT);

	mat4 prev_proj = environment.camera_projection;
	mat4 prev_view = environment.camera_view;
	environment.camera_projection = light_view_projection;
	environment.camera_view = mat4::build_identity();

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);

	// Terrain
	draw(terrain, environment);

	// Palmiers
	for (int i = 0; i < tree_positions.size(); ++i) {
		tree.model.translation = tree_positions[i];
		if (i < tree_scales.size()) tree.model.scaling = tree_scales[i];
		draw(tree, environment);
	}

	// Tree2
	for (int i = 0; i < tree2_positions.size(); ++i) {

		rotation_transform redressement = rotation_transform::from_axis_angle({ 1, 0, 0 }, 3.14159f / 2.0f);
		float angle_aleatoire = float(i) * 1.37f;
		rotation_transform pivot = rotation_transform::from_axis_angle({ 0, 0, 1 }, angle_aleatoire);
		rotation_transform rot = pivot * redressement;
		float scale = (i < tree2_scales.size()) ? tree2_scales[i] : 1.0f;

		// Dessiner toutes les parties au même endroit
		for (auto& part : tree2_parts) {
			part.model.translation = tree2_positions[i];
			part.model.rotation = rot;
			part.model.scaling = scale;
			draw(part, environment);
		}
	}

	// Cubes
	draw(cube1, environment);
	draw(cube2, environment);

	// Ship
	draw(ship, environment);

	// Oiseaux (tous les groupes)
	for (auto& group : bird_groups) {

		float cycle = std::fmod(timer.t, group.tour_duration);
		float u = cycle / group.tour_duration;
		vec3 group_center;
		vec3 group_dir;

		if (group.forest_bird) {
			if (u < 0.1f) {
				float blend = u / 0.1f;
				vec3 tour_start = { group.orbit_radius_x, 0.0f, group.base_altitude };
				group_center = group.nest_position + blend * (tour_start - group.nest_position);
				group_dir = normalize(tour_start - group.nest_position);
			}
			else if (u < 0.9f) {
				float a = 2.0f * Pi * ((u - 0.1f) / 0.8f);
				group_center = {
					group.orbit_radius_x * std::cos(a),
					group.orbit_radius_y * std::sin(a),
					group.base_altitude + 0.8f * std::sin(2.0f * a)
				};
				group_dir = normalize(vec3{
					-group.orbit_radius_x * std::sin(a),
					 group.orbit_radius_y * std::cos(a),
					 0.0f
					});
			}
			else {
				float blend = (u - 0.9f) / 0.1f;
				vec3 tour_end = { group.orbit_radius_x, 0.0f, group.base_altitude };
				group_center = tour_end + blend * (group.nest_position - tour_end);
				group_dir = normalize(group.nest_position - tour_end);
			}
		}
		else {
			float angle = group.angular_speed * timer.t + group.phase;
			group_center = {
				group.orbit_radius_x * std::cos(angle),
				group.orbit_radius_y * std::sin(angle),
				group.base_altitude + 1.2f * std::sin(0.7f * timer.t + group.phase)
			};
			group_dir = normalize(vec3{
				-group.orbit_radius_x * group.angular_speed * std::sin(angle),
				 group.orbit_radius_y * group.angular_speed * std::cos(angle),
				 0.7f * 0.7f * std::cos(0.7f * timer.t + group.phase)
				});
		}

		rotation_transform group_rotation =
			rotation_transform::from_vector_transform({ 1.0f, 0.0f, 0.0f }, group_dir);

		float fr_x = 0.5f + 0.1f * group.count;
		float fr_y = 0.3f + 0.05f * group.count;

		for (int bi = 0; bi < group.count; ++bi) {
			float theta = 2.0f * Pi * float(bi) / float(group.count);
			vec3 local_offset = {
				fr_x * std::cos(theta),
				fr_y * std::sin(theta),
				0.05f * std::sin(1.5f * theta)
			};
			vec3 pos = group_center + group_rotation * local_offset;

			bird_body.model.scaling = group.scale;
			bird_body.model.translation = pos;
			bird_body.model.rotation = group_rotation;
			draw(bird_body, environment);

			bird_left_wing.model.scaling = group.scale;
			bird_right_wing.model.scaling = group.scale;
			bird_left_wing.model.translation = pos;
			bird_right_wing.model.translation = pos;
			draw(bird_left_wing, environment);
			draw(bird_right_wing, environment);
		}
	}

	glDisable(GL_POLYGON_OFFSET_FILL);
	environment.camera_projection = prev_proj;
	environment.camera_view = prev_view;
	shadow_fbo.unbind();
	glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);

	// Bind shadow map texture to unit 5 and set environment uniforms for main pass
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, shadow_fbo.texture.id);
	environment.uniform_generic.uniform_int["shadow_map"] = 5;
	environment.uniform_generic.uniform_mat4["light_view_projection"] = light_view_projection;
	environment.uniform_generic.uniform_float["shadow_bias"] = shadow_bias;
	environment.uniform_generic.uniform_float["shadow_strength"] = shadow_strength;
	// Tell default objects to use shadows
	environment.uniform_generic.uniform_int["use_shadow"] = 1;
	glActiveTexture(GL_TEXTURE0);

	/* Draw grass blades sampled on the mountain
	for (int gi = 0; gi < grass_positions.size(); ++gi) {
		vec3 p = grass_positions[gi];

		// small sway animation based on time and index
		float sway = 0.05f * std::sin(2.0f * Pi * timer.t + gi * 0.37f);

		// random rotation around Z based on index to vary orientation
		float orient = float(gi) * 1.618f;

		grass.model.translation = p;
		grass.model.rotation = rotation_transform::from_axis_angle({0,0,1}, orient);
		float s = 0.5f * (gi < grass_scales.size() ? grass_scales[gi] : 1.0f);
		grass.model.scaling_xyz = { s, s + sway, s };
		draw(grass, environment);
	}*/

	// Animate water mesh vertices to simulate waves
	// Simple wave: sum of two sine waves + Perlin modulation
	float time = timer.t;
	for (int k = 0; k < water_mesh.position.size(); ++k) {
		vec3& p = water_mesh.position[k];
		float wave1 = 0.20f * std::sin(0.6f * p.x + 1.2f * time);
		float wave2 = 0.15f * std::sin(0.8f * p.y + 1.7f * time + 0.5f);
		float noise = 0.15f * noise_perlin({ p.x * 0.2f + time * 0.1f, p.y * 0.2f + time * 0.1f });
		p.z = water_base_z + wave1 + wave2 + noise;
		//gui.sea_level=p.z-0.2;
	}
	water_mesh.normal_update();

	// Update GPU with new water mesh (re-upload)
	water.initialize_data_on_gpu(water_mesh);
	water.material.color = {
	0.25f,
	0.52f,
	0.75f
	};

	water.material.phong.ambient = 0.35f;
	water.material.phong.diffuse = 0.45f;
	water.material.phong.specular = 1.0f;
	water.material.phong.specular_exponent = 128.0f;
	
	// Provide water level to shader for underwater ambient modification
	environment.uniform_generic.uniform_float["water_level_b"] = gui.sea_level;
	// Bind la texture ciel sur l'unité 3 pour la réflexion de l'eau
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, texture_sky.id);
	environment.uniform_generic.uniform_int["sky_texture"] = 3;
	environment.uniform_generic.uniform_float["reflectivity"] = 0.85f; // quasi-parfait
	glActiveTexture(GL_TEXTURE0);

	environment.uniform_generic.uniform_int["use_shadow"] = 0;
	environment.uniform_generic.uniform_int["is_water"] = 1;
	draw(water, environment);
	environment.uniform_generic.uniform_int["is_water"] = 0;
	environment.uniform_generic.uniform_int["use_shadow"] = 1;
	draw(tree, environment);
	draw(cube1, environment);
	draw(sky, environment);


	// Juste avant draw(terrain, environment)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, terrain.texture.id);  // sable sur unité 0

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texture_grass.id);
	environment.uniform_generic.uniform_int["image_texture_grass"] = 1;

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, texture_rock.id);
	environment.uniform_generic.uniform_int["image_texture_rock"] = 2;

	glActiveTexture(GL_TEXTURE0); // toujours remettre l'unité 0 active
	environment.uniform_generic.uniform_int["is_terrain"] = 1;
	draw(terrain, environment);
	environment.uniform_generic.uniform_int["is_terrain"] = 0;
	environment.uniform_generic.uniform_int["reflec"] = 1;
	draw(terrain, environment);
	environment.uniform_generic.uniform_int["reflec"] = 0;
	ship.model.translation = {5.0f, 15.50f + 0.1 * sin(1.2f * time), 0.50f + 0.1f * cos(1.5f * time)};
	draw(ship, environment);

	// --- AFFICHAGE DE LA FORÊT ---
	for (int i = 0; i < tree_positions.size(); ++i) {

		tree.model.translation = tree_positions[i];

		// 1. La rotation pour "Redresser" l'arbre (90° autour de l'axe X)
		rotation_transform redressement = rotation_transform::from_axis_angle({ 1, 0, 0 }, 3.14159f / 2.0f);

		// 2. La rotation pour "Pivoter" aléatoirement le tronc (autour de l'axe Z)
		float angle_aleatoire = float(i) * 1.37f;
		rotation_transform pivot = rotation_transform::from_axis_angle({ 0, 0, 1 }, angle_aleatoire);

		// 3. On multiplie les deux rotations
		tree.model.rotation = pivot * redressement;

		// Apply per-tree scaling if available
		if (i < tree_scales.size())
			tree.model.scaling = tree_scales[i];

		draw(tree, environment);
	}

	for (int i = 0; i < tree2_positions.size(); ++i) {

		rotation_transform redressement = rotation_transform::from_axis_angle({ 1, 0, 0 }, 3.14159f / 2.0f);
		float angle_aleatoire = float(i) * 1.37f;
		rotation_transform pivot = rotation_transform::from_axis_angle({ 0, 0, 1 }, angle_aleatoire);
		rotation_transform rot = pivot * redressement;
		float scale = (i < tree2_scales.size()) ? tree2_scales[i] : 1.0f;

		// Dessiner toutes les parties au même endroit
		for (auto& part : tree2_parts) {
			part.model.translation = tree2_positions[i];
			part.model.rotation = rot;
			part.model.scaling = scale;
			draw(part, environment);
		}
	}




	// Animate the second cube in the water
	cube2.model.translation = { -1.0f, 6.0f+0.1*sin(0.5f*timer.t), -0.8f + 0.1f * cos(0.5f * timer.t)};
	cube2.model.rotation = rotation_transform::from_axis_angle({1,-0.2,0},Pi/12.0f*sin(0.5f*timer.t));
	draw(cube2, environment);



	if (gui.display_wireframe) {
		
		draw_wireframe(terrain, environment);
		draw_wireframe(water, environment);
		// --- AFFICHAGE DE LA FORÊT ---
		for (int i = 0; i < tree_positions.size(); ++i) {

			// On déplace le modèle virtuel aux coordonnées de l'arbre
			tree.model.translation = tree_positions[i];

			// OPTIONNEL : On donne une rotation aléatoire à chaque arbre (basée sur son index)
			float angle_aleatoire = float(i) * 1.37f;
			tree.model.rotation = rotation_transform::from_axis_angle({ 0, 0, 1 }, angle_aleatoire);

			// Apply per-tree scaling if available
			if (i < tree_scales.size())
				tree.model.scaling = tree_scales[i];

			// On dessine l'arbre
			draw(tree, environment);
		}
		draw_wireframe(cube1, environment);
		draw_wireframe(cube2, environment);



	}

	// Désactiver les ombres sous l'eau (optionnel, elles ne sont pas visibles)
	environment.uniform_generic.uniform_int["use_shadow"] = 0;
	draw(sunken_ship, environment);
	draw(treasure_chest, environment);
	environment.uniform_generic.uniform_int["use_shadow"] = 1;

	//Drawing the bird

	float t = timer.t;
	float angle = bird_angular_speed * t;

	bird_position = {
		bird_orbit_radius_x * std::cos(angle),
		bird_orbit_radius_y * std::sin(angle),
		bird_base_altitude + 1.0f * std::sin(0.7f * t)
	};

	bird_direction = normalize(vec3{
		-bird_orbit_radius_x * bird_angular_speed * std::sin(angle),
		 bird_orbit_radius_y * bird_angular_speed * std::cos(angle),
		 0.7f * std::cos(0.7f * t)
		});

	vec3 forward = normalize(bird_direction);

	rotation_transform bird_rotation =
		rotation_transform::from_vector_transform(
			{ 1.0f, 0.0f, 0.0f },
			bird_direction
		);

	// Corps

	// Draw a flock of birds (10) using the same drawable objects
	// Each bird is placed around the leader position with a small formation offset
	const int flock_size = 10;
	const float formation_radius_x = 0.8f;
	const float formation_radius_y = 0.4f;
	for (int bi = 0; bi < flock_size; ++bi) {

		float theta = 2.0f * Pi * float(bi) / float(flock_size);
		// small elliptical formation around the main bird position
		vec3 local_offset = { formation_radius_x * std::cos(theta), formation_radius_y * std::sin(theta), 0.05f * std::sin(1.5f * theta) };

		// per-bird slight altitude jitter and phase-shifted wing
		vec3 this_position = bird_position + bird_rotation * local_offset;
		float wing_phase = 0.5f * std::sin(2.0f * Pi * timer.t + bi * 0.6f);
		float this_wing_angle = bird_wing_angle + wing_phase;

		rotation_transform left_wing_flap = rotation_transform::from_axis_angle({ 1.0f, 0.0f, 0.0f }, this_wing_angle);
		rotation_transform right_wing_flap = rotation_transform::from_axis_angle({ 1.0f, 0.0f, 0.0f }, -this_wing_angle);

		// Body
		bird_body.model.rotation = bird_rotation;
		bird_body.model.translation = this_position;
		draw(bird_body, environment);

		// Head
		vec3 head_local_position = { 0.2f * 1.3f, 0.0f, 0.2f * 0.15f };
		bird_head.model.translation = this_position + bird_rotation * head_local_position;
		bird_head.model.rotation = bird_rotation;
		draw(bird_head, environment);

		// Beak
		vec3 beak_local_position = { 0.2f * 1.75f, 0.0f, 0.2f * 0.15f };
		bird_beak.model.translation = this_position + bird_rotation * beak_local_position;
		bird_beak.model.rotation = bird_rotation;
		draw(bird_beak, environment);

		// Left wing
		bird_left_wing.model.translation = this_position;
		bird_left_wing.model.rotation = bird_rotation * left_wing_flap;
		draw(bird_left_wing, environment);

		// Right wing
		bird_right_wing.model.translation = this_position;
		bird_right_wing.model.rotation = bird_rotation * right_wing_flap;
		draw(bird_right_wing, environment);
	}



	for (auto& group : bird_groups) {

		vec3 group_center;
		vec3 group_dir;

		if (group.forest_bird) {

			float cycle = std::fmod(t, group.tour_duration);
			float u = cycle / group.tour_duration; // 0 → 1 sur toute la durée

			// Phase 0.0→0.1 : sortie du nid (montée verticale)
			// Phase 0.1→0.9 : tour de l'île
			// Phase 0.9→1.0 : retour au nid (descente)

			vec3 tour_pos;
			float angle = 2.0f * Pi * ((u - 0.1f) / 0.8f); // angle sur le tour

			vec3 above_nest = group.nest_position + vec3{ 0, 0, group.base_altitude };

			if (u < 0.1f) {
				// Sortie : interpolation entre nid et début du tour
				float blend = u / 0.1f;
				vec3 tour_start = {
					group.orbit_radius_x,
					0.0f,
					group.base_altitude
				};
				group_center = group.nest_position + blend * (tour_start - group.nest_position);
				group_dir = normalize(tour_start - group.nest_position);

			}
			else if (u < 0.9f) {
				// Tour de l'île
				float a = 2.0f * Pi * ((u - 0.1f) / 0.8f);
				group_center = {
					group.orbit_radius_x * std::cos(a),
					group.orbit_radius_y * std::sin(a),
					group.base_altitude + 0.8f * std::sin(2.0f * a)
				};
				group_dir = normalize(vec3{
					-group.orbit_radius_x * std::sin(a),
					 group.orbit_radius_y * std::cos(a),
					 0.0f
					});

			}
			else {
				// Retour au nid
				float blend = (u - 0.9f) / 0.1f;
				vec3 tour_end = {
					group.orbit_radius_x,
					0.0f,
					group.base_altitude
				};
				group_center = tour_end + blend * (group.nest_position - tour_end);
				group_dir = normalize(group.nest_position - tour_end);
			}

		}
		else {
			// Comportement normal des autres groupes (inchangé)
			float angle = group.angular_speed * t + group.phase;
			group_center = {
				group.orbit_radius_x * std::cos(angle),
				group.orbit_radius_y * std::sin(angle),
				group.base_altitude + 1.2f * std::sin(0.7f * t + group.phase)
			};
			group_dir = normalize(vec3{
				-group.orbit_radius_x * group.angular_speed * std::sin(angle),
				 group.orbit_radius_y * group.angular_speed * std::cos(angle),
				 0.7f * 0.7f * std::cos(0.7f * t + group.phase)
				});
		}

		// Le reste du dessin est identique pour tous les groupes
		rotation_transform group_rotation =
			rotation_transform::from_vector_transform({ 1.0f, 0.0f, 0.0f }, group_dir);

		float fr_x = 0.5f + 0.1f * group.count;
		float fr_y = 0.3f + 0.05f * group.count;

		for (int bi = 0; bi < group.count; ++bi) {
			float theta = 2.0f * Pi * float(bi) / float(group.count);
			vec3 local_offset = {
				fr_x * std::cos(theta),
				fr_y * std::sin(theta),
				0.05f * std::sin(1.5f * theta)
			};
			vec3 pos = group_center + group_rotation * local_offset;

			float wing_phase = bird_wing_amplitude * std::sin(bird_wing_frequency * t + bi * 0.6f + group.phase);
			rotation_transform left_flap = rotation_transform::from_axis_angle({ 1,0,0 }, wing_phase);
			rotation_transform right_flap = rotation_transform::from_axis_angle({ 1,0,0 }, -wing_phase);

			bird_body.material.color = group.color;
			bird_body.model.scaling = group.scale;
			bird_body.model.translation = pos;
			bird_body.model.rotation = group_rotation;
			draw(bird_body, environment);

			bird_head.material.color = group.color * 1.1f;
			bird_head.model.scaling = group.scale;
			bird_head.model.translation = pos + group_rotation * vec3{ group.scale * 0.2f * 1.3f, 0.0f, 0.03f };
			bird_head.model.rotation = group_rotation;
			draw(bird_head, environment);

			bird_beak.model.scaling = group.scale;
			bird_beak.model.translation = pos + group_rotation * vec3{ group.scale*0.2f * 1.75f, 0.0f, 0.03f };
			bird_beak.model.rotation = group_rotation;
			draw(bird_beak, environment);

			bird_left_wing.material.color = group.color * 0.85f;
			bird_right_wing.material.color = group.color * 0.85f;
			bird_left_wing.model.scaling = group.scale;
			bird_right_wing.model.scaling = group.scale;
			bird_left_wing.model.translation = pos;
			bird_right_wing.model.translation = pos;
			bird_left_wing.model.rotation = group_rotation * left_flap;
			bird_right_wing.model.rotation = group_rotation * right_flap;
			draw(bird_left_wing, environment);
			draw(bird_right_wing, environment);
		}
	}


	draw(ship, environment);

	

}



void scene_structure::display_gui()
{
	ImGui::Checkbox("Frame", &gui.display_frame);
	ImGui::Checkbox("Wireframe", &gui.display_wireframe);
}




void scene_structure::mouse_move_event()
{
	if (!inputs.keyboard.shift)
		camera_control.action_mouse_move();
	
}
void scene_structure::mouse_click_event()
{
	camera_control.action_mouse_click();
}
void scene_structure::keyboard_event()
{
	camera_control.action_keyboard();
}
void scene_structure::idle_frame()
{
	camera_control.idle_frame();
	
}

void scene_structure::display_info()
{
	std::cout << "\nCAMERA CONTROL:" << std::endl;
	std::cout << "-----------------------------------------------" << std::endl;
	std::cout << camera_control.doc_usage() << std::endl;
	std::cout << "-----------------------------------------------\n" << std::endl;


	std::cout << "\nSCENE INFO:" << std::endl;
	std::cout << "-----------------------------------------------" << std::endl;
	std::cout << "Example of scene to start a project." << std::endl;
	std::cout << "-----------------------------------------------\n" << std::endl;
}