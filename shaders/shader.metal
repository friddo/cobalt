#include <metal_stdlib>
using namespace metal;

// Vertex output structure
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;             
};

// Vertex function
vertex VertexOut vert_shader(uint vertexID [[vertex_id]]) {
    // Define the quad vertices in normalized device coordinates (NDC)
    float4 positions[6] = {
        float4(-1.0, -1.0, 0.0, 1.0), // Bottom-left
        float4( 1.0, -1.0, 0.0, 1.0), // Bottom-right
        float4(-1.0,  1.0, 0.0, 1.0), // Top-left
        float4(-1.0,  1.0, 0.0, 1.0), // Top-left
        float4( 1.0, -1.0, 0.0, 1.0), // Bottom-right
        float4( 1.0,  1.0, 0.0, 1.0)  // Top-right
    };

    float2 texCoords[6] = {
        float2(0.0, 0.0), // Bottom-left
        float2(1.0, 0.0), // Bottom-right
        float2(0.0, 1.0), // Top-left
        float2(0.0, 1.0), // Top-left
        float2(1.0, 0.0), // Bottom-right
        float2(1.0, 1.0)  // Top-right
    };

    // Output the position and texture coordinates
    VertexOut out;
    out.position = positions[vertexID];
    out.texCoord = texCoords[vertexID];
    return out;
}

// Fragment function
fragment float4 frag_shader(VertexOut in [[stage_in]], constant float& blue_val [[buffer(0)]]) {
    // return UV image
    return float4(in.texCoord.x, in.texCoord.y, blue_val, 1.0);
}