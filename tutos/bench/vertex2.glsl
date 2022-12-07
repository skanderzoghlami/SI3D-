
#version 330

#ifdef VERTEX_SHADER

uniform mat4 mvp;
uniform mat4 mv;

layout(location= 0) in vec3 position;
layout(location= 1) in vec2 texcoord;
layout(location= 2) in vec3 normal;

out vec2 vertex_texcoord;
out vec3 vertex_normal;
out vec3 vertex_position;

void main( )
{
	gl_Position= mvp * vec4(position, 1);
	
	vertex_texcoord= texcoord;
	vertex_normal= mat3(mv) * normal;
	vertex_position= vec3(mv * vec4(position, 1));
}

#endif

#ifdef FRAGMENT_SHADER

in vec2 vertex_texcoord;
in vec3 vertex_normal;
in vec3 vertex_position;

uniform sampler2D grid;
layout(early_fragment_tests) in ;

void main( )
{
	vec3 color= texture(grid, vertex_texcoord).rgb;
	vec3 l= vec3(0, 0, 1) - vertex_position;
	float ndotl= dot(normalize(vertex_normal), normalize(l));
	gl_FragColor= vec4(color * ndotl, 1);
	
	//~ gl_FragColor= vec4(1);
}

#endif
