#version 330

#ifdef VERTEX_SHADER
layout(location= 0) in vec3 position;
layout(location= 1) in vec2 texcoord;
layout(location= 2) in vec3 normal;
layout(location= 4) in uint material;

uniform mat4 mvpMatrix;
uniform mat4 mvMatrix;

out vec3 vertex_position;
out vec2 vertex_texcoord;
out vec3 vertex_normal;
flat out uint vertex_material;
/* decoration flat : le varying est un entier, donc pas vraiment interpolable... il faut le declarer explicitement */

void main( )
{
    gl_Position= mvpMatrix * vec4(position, 1);
    
    // position et normale dans le repere camera
    vertex_position= vec3(mvMatrix * vec4(position, 1));
    vertex_texcoord= texcoord;
    vertex_normal= mat3(mvMatrix) * normal;
    // ... comme d'habitude
    
    // et transmet aussi l'indice de la matiere au fragment shader...
    vertex_material= material;
}

#endif


#ifdef FRAGMENT_SHADER
out vec4 fragment_color;

in vec3 vertex_position;
in vec2 vertex_texcoord;
in vec3 vertex_normal;
flat in uint vertex_material;	// !! decoration flat, le varying est marque explicitement comme non interpolable  !!

#define MAX_MATERIALS 256 
// uniform int materials[MAX_MATERIALS];
uniform int textures_diffuse[MAX_MATERIALS];
uniform int textures_specular[MAX_MATERIALS];
uniform int textures_emissive[MAX_MATERIALS];
uniform vec3 lights_positions[MAX_MATERIALS];

uniform sampler2DArray texture_array;    //< acces au tableau de textures...

vec3 calcLights(vec3 lightPos, vec3 normal, vec3 viewDir,
                vec3 diffuseColor) {
    // diffuse
    vec3 lightDir = normalize(lightPos - vertex_position);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * diffuseColor;

    // light attenuation
    float dist = length(vertex_position);
    float constant = 1.0;
    float linear = 0.001;
    float quadratic = 0.0001;
    float attenuation = 1.0 / 
        (constant + linear * dist + quadratic * (dist * dist));
    
    return attenuation * diffuse;
    // return diffuse;

    // // specular
    // vec3 halfwayDir = normalize(lightDir + viewDir);
    // float rugosite = PBRcoefs.y;
    // float shininess = 2 / pow(rugosite, 2.) - 2;
    // float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    // vec3 specular = vec3(0.3) * spec;
}

void main( )
{
    vec3 diffuseColor = vec3(1.);
    vec3 PBRcoefs = vec3(1.);

    int diffuseIdx = textures_diffuse[vertex_material];
    if (diffuseIdx != -1)
        diffuseColor = 
            texture(texture_array, vec3(vertex_texcoord, diffuseIdx)).rgb;
    // if(texColor.a < 0.3)
    //     discard;
    // vec3 color = texColor.rgb;
    int specularIdx = textures_specular[vertex_material];
    if (specularIdx != -1)
        PBRcoefs = 
            texture(texture_array, vec3(vertex_texcoord, specularIdx)).rgb;

    int emissiveIdx = textures_emissive[vertex_material];
    if (emissiveIdx != -1) {
        fragment_color = vec4(1.0, 1.0, 0.0, 1.);
        return;
    }

    // ambient
    const float amb = 0.05;
    vec3 ambient = amb * diffuseColor;

    // diffuse
    vec3 viewDir = normalize(-vertex_position); // repere caméra
    vec3 lightDir = viewDir; // la camera est la source de lumiere.
    vec3 normal = normalize(vertex_normal);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * diffuseColor;

    // specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float rugosite = PBRcoefs.y;
    float shininess = 2 / pow(rugosite, 2.) - 2;
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specular = vec3(0.3) * spec;

    // light attenuation
    float dist = length(vertex_position);
    float constant = 1.0;
    float linear = 0.045;
    float quadratic = 0.0075;
    float attenuation = 1.0 / 
        (constant + linear * dist + quadratic * (dist * dist));

    // fragment_color = vec4(ambient + diffuse + specular, 1.0);
    // fragment_color = vec4(attenuation * (diffuse + specular), 1.0);
    // fragment_color = vec4(attenuation * diffuse, 1.0);
    // fragment_color = vec4(specular, 1.0);
    fragment_color = vec4(calcLights(lights_positions[1], normal, viewDir, diffuseColor), 1.0);

}

#endif
