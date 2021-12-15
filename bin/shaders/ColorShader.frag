#version 130
//The fragment shader operates on each pixel in a given polygon

in vec2 fragmentPosition;
in vec4 fragmentColor;
in vec2 fragmentUV;

//This is the 3 component float vector that gets outputted to
//the screen for each pixel

out vec4 color;

uniform float time;
uniform sampler2D mySampler;

void main() {
	vec4 textureColor = texture(mySampler, fragmentUV);

	//color = textureColor * fragmentColor;

	color = vec4(fragmentColor.r * (cos(fragmentPosition.x + time) + 1.0) * 0.5,
		fragmentColor.g * (cos(fragmentPosition.y + time) + 1.0) * 0.5,
		fragmentColor.b * (cos(fragmentPosition.x*0.4 + time) + 1.0) * 0.5,
		fragmentColor.a) * textureColor;
}

