#version 130
// the vertex shader operates on each vertex

// input data from the vertex buffer object
// each vertex is two floats
in vec2 vertexPosition;
in vec4 vertexColor;

void main() {
    // set the x,y position on the screen
    gl_Position.xy = vertexPosition;
    // TODO(ERIC) : Modify shader to use z position.
    gl_Position.z = 0.0;
    // indicate that the coordinates are normalized
    gl_Position.w = 1.0;
}