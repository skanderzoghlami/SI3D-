
#version 450

#extension GL_ARB_shader_ballot : require
#extension GL_ARB_gpu_shader_int64 : require


#ifdef VERTEX_SHADER

in vec3 position;

uniform mat4 mvpMatrix;

void main( )
{
    gl_Position= mvpMatrix * vec4(position, 1);
}
#endif


#ifdef FRAGMENT_SHADER

layout(binding= 0, r32ui) coherent uniform uimage2D image;

layout(binding= 0) coherent buffer triangleData1
{
    uint tiles[];
};

layout(binding= 1) coherent buffer triangleData2
{
    uint fragments[];
};

out vec4 fragment_color;

//~ layout(early_fragment_tests) in;
void main( )
{
    ivec2 tile= ivec2(gl_FragCoord.xy) / 8;
    int tile_id= tile.y * 1024 + tile.x;

    int n= 0;
    // tiles par triangle
    while(true)
    {
        n++;
        // selectionne un thread du groupe
        uint first= readFirstInvocationARB(gl_SubGroupInvocationARB);
        if(gl_SubGroupInvocationARB == first)
        {
            imageAtomicAdd(image, tile, 1);
            atomicAdd(tiles[gl_PrimitiveID], 1);
        }
        
        // termine tous les threads du tile + triangle.
        int first_primitive_id= readFirstInvocationARB(gl_PrimitiveID);
        int first_tile_id= readFirstInvocationARB(tile_id);
        if(tile_id == first_tile_id && gl_PrimitiveID == first_primitive_id)
            break;
    }

    // fragments par triangle
    atomicAdd(fragments[gl_PrimitiveID], 1);
    
    int first_n= readFirstInvocationARB(n);
    fragment_color= vec4(float(first_n), 0, 0, 1);
}
#endif
