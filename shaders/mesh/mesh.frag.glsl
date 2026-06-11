#version 330 core 

// Fragment shader - this code is executed for every pixel/fragment that belongs to a displayed shape
//
// Compute the color using Phong illumination (ambient, diffuse, specular) 
//  There is 3 possible input colors:
//    - fragment_data.color: the per-vertex color defined in the mesh
//    - material.color: the uniform color (constant for the whole shape)
//    - image_texture: color coming from the texture image
//  The color considered is the product of: fragment_data.color x material.color x image_texture
//  The alpha (/transparent) channel is obtained as the product of: material.alpha x image_texture.a
// 

// Inputs coming from the vertex shader
in struct fragment_data
{
    vec3 position; // position in the world space
    vec3 normal;   // normal in the world space
    vec3 color;    // current color on the fragment
    vec2 uv;       // current uv-texture on the fragment

} fragment;

// Output of the fragment shader - output color
layout(location=0) out vec4 FragColor;


// Uniform values that must be send from the C++ code
// ***************************************************** //

uniform sampler2D image_texture;   // Texture image identifiant
uniform sampler2D image_texture_reflection; // optional reflection texture (e.g., sky)
uniform sampler2D image_texture_grass;
uniform sampler2D image_texture_rock;
uniform sampler2D sky_texture;
uniform float reflectivity;
uniform int is_water;
uniform bool use_reflection; // enable reflection sampling
uniform float water_level_b; // world-space z of water surface
uniform float reflec; //will help to know if we will work on reflexion of the water or not.
uniform int is_terrain; //verifier s'il s'agit du terrain avant d'appliquer la double texture


uniform mat4 view;       // View matrix (rigid transform) of the camera - to compute the camera position

uniform vec3 light; // position of the light
// Shadow map uniforms
uniform sampler2D shadow_map;
uniform mat4 light_view_projection;
uniform float shadow_bias;
uniform float shadow_strength;
uniform int use_shadow; // 1 = apply shadows, 0 = skip shadows (useful for water)


// Coefficients of phong illumination model
struct phong_structure {
	float ambient;      
	float diffuse;
	float specular;
	float specular_exponent;
};

// Settings for texture display
struct texture_settings_structure {
	bool use_texture;       // Switch the use of texture on/off
	bool texture_inverse_v; // Reverse the texture in the v component (1-v)
	bool two_sided;         // Display a two-sided illuminated surface (doesn't work on Mac)
};

// Material of the mesh (using a Phong model)
struct material_structure
{
	vec3 color;  // Uniform color of the object
	float alpha; // alpha coefficient

	phong_structure phong;                       // Phong coefficients
	texture_settings_structure texture_settings; // Additional settings for the texture
}; 

uniform material_structure material;


void main()
{
	// Compute the position of the center of the camera
	mat3 O = transpose(mat3(view));                   // get the orientation matrix
	vec3 last_col = vec3(view*vec4(0.0, 0.0, 0.0, 1.0)); // get the last column
	vec3 camera_position = -O*last_col;


	// Renormalize normal
	vec3 N = normalize(fragment.normal);

	// Inverse the normal if it is viewed from its back (two-sided surface)
	//  (note: gl_FrontFacing doesn't work on Mac)
	if (material.texture_settings.two_sided && gl_FrontFacing == false) {
		N = -N;
	}

	// Phong coefficient (diffuse, specular)
	// *************************************** //

	// Unit direction toward the light
	vec3 L = normalize(light-fragment.position);

	// Diffuse coefficient
	float diffuse_component = max(dot(N,L),0.0);

	// Specular coefficient
	float specular_component = 0.0;
	if(diffuse_component>0.0){
		vec3 R = reflect(-L,N); // reflection of light vector relative to the normal.
		vec3 V = normalize(camera_position-fragment.position);
		specular_component = pow( max(dot(R,V),0.0), material.phong.specular_exponent );
	}

	// Texture
	// *************************************** //

	// Current uv coordinates
	vec2 uv_image = vec2(fragment.uv.x, fragment.uv.y);
	if(material.texture_settings.texture_inverse_v) {
		uv_image.y = 1.0-uv_image.y;
	}

	// Get the current texture color
	vec4 color_image_texture = texture(image_texture, uv_image);
	if(material.texture_settings.use_texture == false) {
		color_image_texture=vec4(1.0,1.0,1.0,1.0);
	}
	
	// Compute Shading
	// *************************************** //


	// Compute the final shaded color using Phong model
	float Ka = material.phong.ambient;
	float Kd = material.phong.diffuse;
	float Ks = material.phong.specular;

	// Compute the base color of the object based on: vertex color, uniform color, and texture
	if(is_terrain == 1){
    vec4 color_sand  = texture(image_texture,       uv_image * 8.0);
    vec4 color_grass = texture(image_texture_grass, uv_image * 6.0);
    vec4 color_rock  = texture(image_texture_rock,  uv_image * 10.0);

    float z     = fragment.position.z;
    float slope = fragment.normal.z; // 1.0=plat, 0.0=falaise verticale

    // Roche : uniquement sur les fortes pentes (slope faible)
    float w_rock = 1.0 - smoothstep(0.150, 0.20, slope);

    // Sable : uniquement en bord de mer (basse altitude)
    float w_sand = (1.0 - w_rock) * (1.0 - smoothstep(-0.5, 0.05, z));

    // Herbe : tout le reste
    float w_grass = 1.0 - w_rock - w_sand;
    w_grass = clamp(w_grass, 0.0, 1.0);

    color_image_texture = w_rock  * color_rock
                        + w_sand  * color_sand
                        + w_grass * color_grass;
	Ks=0;
}
	
	vec3 color_object  = fragment.color * material.color * color_image_texture.rgb;

	
	

	// Compute ambient contribution color (per requirement: if fragment is below water level,
	// replace ambient color by 0.7*blue + 0.3*effective_ambient_color)
	vec3 ambient_effective = Ka * color_object; // effective ambient color

	//If the camera is under the water we make things looks blue
	if (camera_position.z < water_level_b) {
		ambient_effective = 0.5 * vec3(0.25, 0.52, 0.75) + 0.50 * ambient_effective;
	}

	vec3 color_shading = ambient_effective + Kd * diffuse_component * color_object + Ks * specular_component * vec3(1.0, 1.0, 1.0);

	




	if (is_water == 1) {
    // Vecteur de vue
    vec3 V = normalize(camera_position - fragment.position);
    vec3 N = normalize(fragment.normal);

    // Vecteur de réflexion
    vec3 R = reflect(-V, N);

    // Convertir R en coordonnées UV sphériques (projection équirectangulaire)
    float phi   = atan(R.y, R.x);               // azimut  [-π, π]
    float theta = asin(clamp(R.z, -1.0, 1.0));  // élévation [-π/2, π/2]

    vec2 sky_uv = vec2(
        phi   / (2.0 * 3.14159) + 0.5,   // [0, 1]
        theta / 3.14159 + 0.5             // [0, 1]
    );

    vec4 sky_color = texture(sky_texture, sky_uv);

    // Facteur de Fresnel : plus on regarde rasant, plus c'est réfléchissant
    float fresnel = pow(1.0 - max(dot(V, N), 0.0), 3.0);
    float r = mix(reflectivity * 0.5, reflectivity, fresnel);

    // Mélange eau + réflexion ciel
    color_shading = mix(color_shading, sky_color.rgb, r);
}




	// Reflection sampling (approximate): sample reflection texture using world XZ coordinates
	if(use_reflection) {
		// Build simple 2D UV from world coordinates and wrap
		vec2 refl_uv = vec2(0.5 + fragment.position.x * 0.02, 0.5 + fragment.position.z * 0.02);
		refl_uv = fract(refl_uv);
		vec4 refl_col = texture(image_texture_reflection, refl_uv);
		// Mix reflection into shading using reflectivity and texture alpha
		color_shading = mix(color_shading, refl_col.rgb, reflectivity * refl_col.a);
	}

	// --- Shadow computation (basic PCF) ---
	float shadow_factor = 1.0;
	if(use_shadow == 1) {
		vec4 light_space = light_view_projection * vec4(fragment.position, 1.0);
		vec3 proj = light_space.xyz / light_space.w;
		vec2 shadow_uv = proj.xy * 0.5 + 0.5;
		float current_depth = proj.z * 0.5 + 0.5;
		// Only compute shadow if inside light frustum
		if(shadow_uv.x>=0.0 && shadow_uv.x<=1.0 && shadow_uv.y>=0.0 && shadow_uv.y<=1.0) {
			// simple 3x3 PCF
			float samples = 0.0;
			float occluded = 0.0;
			for(int xo=-1; xo<=1; ++xo){
				for(int yo=-1; yo<=1; ++yo){
					vec2 offset = vec2(float(xo), float(yo)) * (1.0/float(textureSize(shadow_map,0).x));
					float closest = texture(shadow_map, shadow_uv + offset).r;
					if(current_depth - shadow_bias > closest) occluded += 1.0;
					samples += 1.0;
				}
			}
			float occ = occluded / samples;
			shadow_factor = mix(1.0, shadow_strength, occ);
		}
	}

	// Apply shadow to shading
	color_shading *= shadow_factor;

	// Output color, with the alpha component
	FragColor = vec4(color_shading, material.alpha * color_image_texture.a);



	//Let's now study the case where it is water.
	if(reflec==1){
		vec3 V = normalize(camera_position - fragment.position);

		vec3 R = reflect(-V, normalize(fragment.normal));

		

	}

}