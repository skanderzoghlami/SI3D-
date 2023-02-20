
#version 330

#ifdef VERTEX_SHADER

uniform mat4 mvpMatrix;
//~ uniform mat4 mvMatrix;

layout(location= 0) in vec3 position;
//~ layout(location= 1) in vec2 texcoord;
//~ layout(location= 2) in vec3 normal;

void main( )
{
	gl_Position= mvpMatrix * vec4(position, 1);
	gl_Position.w= -1;
}

#endif
