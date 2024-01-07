#version 330

#ifdef VERTEX_SHADER

layout(location= 0) in vec3 position;
layout(location= 1) in vec2 texcoord;
layout(location= 2) in vec3 normal_direction;

uniform mat4 mvpMatrix;
// uniform mat4 mvMatrix;

out vec2 vertex_texcoord;
out vec3 dir_norml ;


void main( )
{
    gl_Position= mvpMatrix * vec4(position, 1);
    vertex_texcoord= texcoord;
    dir_norml = normalize(normal_direction) ;
}

#endif

#ifdef FRAGMENT_SHADER

in vec2 vertex_texcoord;
in vec3 dir_norml;

// uniform vec4 material_color ;
uniform sampler2D material_texture;

out vec4 fragment_color;

void main( )
{
    vec4 color0= texture(material_texture, vertex_texcoord);
    fragment_color= color0 ;
}

#endif