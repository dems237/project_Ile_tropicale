#version 330 core 

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

uniform sampler2D image_texture;   // Canal 0 : Sable / Herbe
uniform sampler2D image_texture_2; // Canal 1 : Roche

uniform mat4 view;       // View matrix of the camera
uniform vec3 light;      // Position of the light

// Coefficients of phong illumination model
struct phong_structure {
    float ambient;      
    float diffuse;
    float specular;
    float specular_exponent;
};

// Settings for texture display
struct texture_settings_structure {
    bool use_texture;       
    bool texture_inverse_v; 
    bool two_sided;         
};

// Material of the mesh
struct material_structure
{
    vec3 color;  
    float alpha; 
    phong_structure phong;                       
    texture_settings_structure texture_settings; 
}; 

uniform material_structure material;

void main()
{
    // 1. Compute camera position
    mat3 O = transpose(mat3(view));                   
    vec3 last_col = vec3(view * vec4(0.0, 0.0, 0.0, 1.0)); 
    vec3 camera_position = -O * last_col;

    // 2. Renormalize normal
    vec3 N = normalize(fragment.normal);

    // Inverse the normal if it is viewed from its back
    if (material.texture_settings.two_sided && gl_FrontFacing == false) {
        N = -N;
    }

    // 3. Phong coefficients (diffuse, specular)
    vec3 L = normalize(light - fragment.position);
    float diffuse_component = max(dot(N, L), 0.0);

    float specular_component = 0.0;
    if(diffuse_component > 0.0){
        vec3 R = reflect(-L, N); 
        vec3 V = normalize(camera_position - fragment.position);
        specular_component = pow(max(dot(R, V), 0.0), material.phong.specular_exponent);
    }

    // 4. Texture & Splatting (Sable vs Roche)
    vec2 uv_image = vec2(fragment.uv.x, fragment.uv.y);
    if(material.texture_settings.texture_inverse_v) {
        uv_image.y = 1.0 - uv_image.y;
    }

    vec4 color_image_texture = vec4(1.0, 1.0, 1.0, 1.0);
    float final_mix = 1.0; // 1.0 = Sable, 0.0 = Roche

    if(material.texture_settings.use_texture) {
        // Répétition des textures pour le grand terrain
        vec2 uv_sand = uv_image * 15.0;
        vec2 uv_rock = uv_image * 25.0;
        
        vec4 color_sand = texture(image_texture, uv_sand);
        vec4 color_rock = texture(image_texture_2, uv_rock);

        // Analyse de la pente (1.0 = plat, 0.0 = vertical)
        float slope_factor = smoothstep(0.65, 0.90, N.z);
        
        // Analyse de l'altitude (0.0 en bas, 1.0 en haut)
        float height_factor = smoothstep(-0.5, 2.0, fragment.position.z);

        // Calcul du mélange : Sable si c'est plat ET bas. Sinon, Roche.
        final_mix = slope_factor * (1.0 - height_factor);
        
        color_image_texture = mix(color_rock, color_sand, final_mix);
    }
    
    // 5. Compute Shading
    vec3 color_object = fragment.color * material.color * color_image_texture.rgb;

    float Ka = material.phong.ambient;
    float Kd = material.phong.diffuse;
    float Ks = material.phong.specular;
    
    // Ajustement de la brillance : la roche est mate, le sable brille
    if(material.texture_settings.use_texture) {
        Ks *= (0.1 + 0.9 * final_mix); 
    }

    vec3 color_shading = (Ka + Kd * diffuse_component) * color_object + Ks * specular_component * vec3(1.0, 1.0, 1.0);
    
    // 6. Output final
    FragColor = vec4(color_shading, material.alpha * color_image_texture.a);
}