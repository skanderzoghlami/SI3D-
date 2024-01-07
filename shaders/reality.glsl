#version 330

#ifdef VERTEX_SHADER

layout(location= 0) in vec3 position;
layout(location= 1) in vec2 texcoord;
layout(location= 2) in vec3 normal_direction;

uniform mat4 mvpMatrix;
uniform mat4 lightSpaceMatrix;

out vec2 vertex_texcoord;
out vec3 dir_norml;
out vec4 fragPosLightSpace;

void main()
{
    gl_Position = mvpMatrix * vec4(position, 1);
    fragPosLightSpace = lightSpaceMatrix * vec4(position, 1.0); 
    vertex_texcoord = texcoord;
    dir_norml = normalize(normal_direction);
}

#endif

#ifdef FRAGMENT_SHADER

in vec2 vertex_texcoord;
in vec3 dir_norml;
in vec4 fragPosLightSpace;

uniform sampler2D material_texture;
uniform sampler2DShadow shadowMap;
uniform vec3 lightDir; 
out vec4 fragment_color;

void main()
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;

    float depthInLightSpace = projCoords.z;


    float shadow = textureProj(shadowMap, vec4(projCoords, 1.0));
    float bias =  0.005 +  0.1 * tan(acos(dot(dir_norml, normalize(lightDir))));


    float shadowResult = (fragPosLightSpace.z > shadow + bias) ? 0.6 : 0.0;
	
    vec4 color0 = texture(material_texture, vertex_texcoord);
    fragment_color = vec4(color0.rgb * (1.0 - shadowResult), color0.a);
    //fragment_color = vec4(fragPosLightSpace.z);

}

#endif
