#include <metal_stdlib>
using namespace metal;

// Sampler for fragment shader
constexpr sampler textureSampler (mag_filter::linear, min_filter::linear);

// Vertex output structure
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;             
};

// Vertex function
vertex VertexOut vert_shader(
    uint vertexID [[vertex_id]]
) {
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
fragment float4 frag_shader(
    VertexOut in [[stage_in]],
    texture2d<float, access::sample> texture [[texture(0)]]
) {
    // Sample the texture using the texture coordinates provided by the vertex shader
    float4 color = texture.sample(textureSampler, in.texCoord);
    return color;
}

// Define the compute kernel
kernel void compute_kernel(
    texture2d<float, access::read_write> texture [[texture(0)]],
    uint2 gid [[thread_position_in_grid]]                    
) {
    // Get texture size
    uint width = texture.get_width();
    uint height = texture.get_height();

    // Calculate normalized coordinates (0.0 to 1.0)
    float2 uv = float2(gid) / float2(width, height);

    // Create a simple gradient color
    float3 color = float3(uv.x, uv.y, 1.0 - uv.x);

    // Write the color to the texture
    float4 sample = texture.read(gid);
    texture.write(sample + float4(color, 1.0) * 0.001, gid);
}
