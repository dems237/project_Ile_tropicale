#version 330 core

layout (location = 0) in vec3 vertex_position;

uniform mat4 light_view_projection;
uniform mat4 model;

void main()
{
	gl_Position = light_view_projection * model * vec4(vertex_position, 1.0);
}
