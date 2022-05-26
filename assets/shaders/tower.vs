#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoords;

// Output data ; will be interpolated for each fragment.
out vec2 uv;
out vec4 c;

// Values that stay constant for the whole mesh.
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

uniform vec4 u_color;

void main(){

	// Output position of the vertex, in clip space : MVP * position
	gl_Position =  u_projection * u_view * u_model * vec4(a_pos,1);
	
	// UV of the vertex. No special space for this one.
	uv = vec2(a_texcoords.x, a_texcoords.y);
	c = u_color;
}