#version 330

#ifdef VERTEX_SHADER

layout(location = 0) in vec3 position;

uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * vec4(position, 1.0);
}

#endif

#ifdef FRAGMENT_SHADER

out vec4 FragColor;

void main()
{
}

#endif
