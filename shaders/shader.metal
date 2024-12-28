#include <metal_stdlib>
using namespace metal;
using namespace raytracing;

// Sampler for fragment shader
constexpr sampler textureSampler (mag_filter::nearest, min_filter::nearest);

// Vertex output structure
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;             
};

struct Triangle {
    packed_float3 n0;
    packed_float3 n1;
    packed_float3 n2;
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
    float3 color = texture.sample(textureSampler, in.texCoord).xyz;
    color = color / (1.0 + color);
    return float4(color, 1.0);
}



template<typename T>
inline T interpolateVertexAttribute(thread T* attributes, float2 uv) {
    // Look up the value for each vertex.
    const T T0 = attributes[0];
    const T T1 = attributes[1];
    const T T2 = attributes[2];
    // Compute the sum of the vertex attributes weighted by the barycentric coordinates.
    // The barycentric coordinates sum to one.
    return (1.0f - uv.x - uv.y) * T0 + uv.x * T1 + uv.y * T2;
}



// Define the compute kernel
kernel void compute_kernel(
    texture2d<float, access::read_write> texture [[texture(0)]],
    constant packed_float3 &lookFrom [[buffer(1)]],
    constant packed_float3 &lookAt [[buffer(2)]],
    constant uint &frame [[buffer(3)]],
    primitive_acceleration_structure accelStructure [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]]                    
) {
    // Get texture size
    uint width = texture.get_width();
    uint height = texture.get_height();
    float aspect = float(width) / float(height);

    // Field of View to viewport scale
    float theta = 0.3;
    float half_height = tan(theta / 2.0);
    float half_width = aspect * half_height;

    // Compute camera basis vectors
    float3 forward = normalize(lookAt - lookFrom); // Camera forward direction
    float3 right = normalize(cross(forward, float3(0,1,0))); // Horizontal axis
    float3 adjustedUp = cross(right, forward);    // Vertical axis

    // Compute the viewport corners
    float3 horizontal = 2.0 * half_width * right;
    float3 vertical = 2.0 * half_height * adjustedUp;
    float3 lower_left_corner = lookFrom + forward - half_width * right - half_height * adjustedUp;

    // Calculate normalized coordinates (0.0 to 1.0)
    float2 uv = float2(gid) / float2(width, height);

    // Compute the ray direction for the pixel
    float3 ray_direction = normalize(lower_left_corner + uv.x * horizontal + uv.y * vertical - lookFrom);

    ray r;
    r.origin = lookFrom;
    r.direction = ray_direction;
    r.min_distance = 0.0001;
    r.max_distance = INFINITY;

    // Create an intersector to test for intersection between the ray and the geometry in the scene.
    intersector<triangle_data> i;
    i.assume_geometry_type(geometry_type::triangle);
    i.force_opacity(forced_opacity::opaque);

    intersection_result<triangle_data> intersection;
    i.accept_any_intersection(false);

    // get possible intersection
    intersection = i.intersect(r, accelStructure);
    
    // shade based on intersection
    float4 color;
    if (intersection.type == intersection_type::none) {
        // if nothing hit, shade the background
        if(r.direction.y < 0.0) {
            color = float4(0.7,0.7,0.7, 1.0);
        } else {
            color = float4(0.68, 0.96, 0.96, 1.0);
        }
    } else {
        // if we hit a triangle, shade it based on its normal
        const device Triangle *data;
        data = (const device Triangle*)intersection.primitive_data;

        float3 n[3];
        n[0] = data->n0;
        n[1] = data->n1;
        n[2] = data->n2;

        float2 uv = intersection.triangle_barycentric_coord;
        float3 norm = interpolateVertexAttribute(n, uv);

        // calculate the light intensity
        float3 light_dir = normalize(float3(1.0, 1.0, -1.0));
        float3 light_intensity = float3(dot(norm, light_dir));

        color = float4(light_intensity, 1.0);
    }

    // blend the color with the previous frame
    // float4 prev_color = texture.read(gid);
    // float4 new_color = (prev_color * float(frame - 1) + color) / float(frame); 
    texture.write(color, gid);
}
