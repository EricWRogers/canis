#version 330 core

// Interpolated values from the vertex shaders
in vec2 uv;
in vec4 c;

// Ouput data
out vec4 color;

// Values that stay constant for the whole mesh.
uniform sampler2D diffuse;

void main(){

	// Output color = color of the texture at the specified UV
	color = c * texture( diffuse, uv );
}