
#version 330

#ifdef VERTEX_SHADER

uniform mat4 mvpMatrix;
//~ uniform mat4 mvMatrix;

layout(location= 0) in vec3 position;
//~ layout(location= 1) in vec2 texcoord;
//~ layout(location= 2) in vec3 normal;

//~ out vec2 vertex_texcoord;
//~ out vec3 vertex_normal;
//~ out vec3 vertex_position;

void main( )
{
	gl_Position= mvpMatrix * vec4(position, 1);
	
	//~ vertex_texcoord= texcoord;
	//~ vertex_normal= mat3(mvMatrix) * normal;
	//~ vertex_position= vec3(mvMatrix * vec4(position, 1));
}

#endif
