#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "GLFWBridge.hpp"

// Metal headers
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalLayer.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <iostream>
#include <fstream>
#include <sstream>



// global layer 
CA::MetalLayer* layer;

// temp texture stuff
MTL::TextureDescriptor* textureDescriptor;
MTL::Texture* computeTexture;

// window size
int width, height;

struct Mesh {
    std::vector<float> vertices;    // x, y, z positions
    std::vector<float> normals;     // x, y, z normals
    std::vector<float> texcoords;   // u, v texture coordinates
    std::vector<unsigned int> indices; // Face indices
};

Mesh loadOBJ(const std::string& filepath, float scale = 1.0) {
    tinyobj::attrib_t attrib;                       // Contains vertex positions, normals, texcoords
    std::vector<tinyobj::shape_t> shapes;           // Contains all shapes (faces)
    std::vector<tinyobj::material_t> materials;     // Contains material data
    std::string warn, err;

    // Load the OBJ file
    bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str());

    if (!warn.empty()) {
        std::cerr << "Warning: " << warn << std::endl;
    }

    if (!success) {
        throw std::runtime_error("Failed to load OBJ file: " + err);
    }

    // Prepare the mesh
    Mesh mesh;

    // Iterate over shapes
    for (const auto& shape : shapes) {
        // Iterate over faces in the shape
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
            int faceVertices = shape.mesh.num_face_vertices[f]; // Typically 3 for triangles

            // Process each vertex in the face
            for (int v = 0; v < faceVertices; ++v) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                // Vertex positions
                if (idx.vertex_index >= 0) {
                    mesh.vertices.push_back(scale * attrib.vertices[3 * idx.vertex_index + 0]);
                    //mesh.vertices.push_back(scale * attrib.vertices[3 * idx.vertex_index + 2]);
                    mesh.vertices.push_back(scale * attrib.vertices[3 * idx.vertex_index + 1]);
                    mesh.vertices.push_back(scale * attrib.vertices[3 * idx.vertex_index + 2]);
                }

                // Normals
                if (idx.normal_index >= 0) {
                    mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 0]);
                    //mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 2]);
                    mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 1]);
                    mesh.normals.push_back(attrib.normals[3 * idx.normal_index + 2]);
                }

                // Texture coordinates
                if (idx.texcoord_index >= 0) {
                    mesh.texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
                    mesh.texcoords.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
                }

                // Add index
                mesh.indices.push_back(indexOffset + v);
            }

            indexOffset += faceVertices;
        }
    }

    return mesh;
}






static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void framebuffer_size_callback(GLFWwindow* window, int _width, int _height)
{
    width = _width;
    height = _height;
    layer->setDrawableSize(CGSizeMake(_width, _height));
    computeTexture->release();
    textureDescriptor->setWidth(_width/2);
    textureDescriptor->setHeight(_height/2);
    computeTexture = layer->device()->newTexture(textureDescriptor);
}


int main() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    // dumb apple retina hack. Load "custom" font at 2x size and scale down to maintain sharpness.
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ProggyClean.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    io.FontGlobalScale = 0.5f;


    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Create window with graphics context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Cobalt", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwGetFramebufferSize(window, &width, &height); // store size early

    MTL::Device* device = static_cast<MTL::Device*>(MTL::CopyAllDevices()->object(0));
    MTL::CommandQueue* commandQueue = device->newCommandQueue();



    // Fetch the shader source code
    std::ifstream file("shaders/shader.metal");
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file." << std::endl;
        return -1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();


    // Create Metal library from source
    NS::Error* error = nullptr;
    NS::String* kernelSource = NS::String::string(buffer.str().c_str(), NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(kernelSource, nullptr, &error);
    if (error) {
        std::cerr << "Failed to compile Metal kernel: " << error->localizedDescription()->utf8String() << std::endl;
        return -1;
    }


    // Setup compute pipeline
    NS::String* str = NS::String::string("compute_kernel", NS::UTF8StringEncoding);
    MTL::ComputePipelineState* pipelineComputeState = device->newComputePipelineState(library->newFunction(str), &error);
    if (error) {
        std::cerr << "Failed to create compute pipeline state: " << error->localizedDescription()->utf8String() << std::endl;
        return -1;
    }


    // Setup render pipeline
    MTL::RenderPipelineDescriptor* pipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDescriptor->setVertexFunction(library->newFunction(NS::String::string("vert_shader", NS::UTF8StringEncoding)));
    pipelineDescriptor->setFragmentFunction(library->newFunction(NS::String::string("frag_shader", NS::UTF8StringEncoding)));
    pipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm); // same as layer, not as texture
    MTL::RenderPipelineState* pipelineRenderState = device->newRenderPipelineState(pipelineDescriptor, &error);
    if (error) {
        std::cerr << "Failed to create render pipeline state: " << error->localizedDescription()->utf8String() << std::endl;
        return -1;
    }
    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();


    // Make texture used for compute and render
    textureDescriptor = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA32Float, width/2, height/2, false);
    textureDescriptor->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
    computeTexture = device->newTexture(textureDescriptor);


    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);


    // Magic sauce, interfaces with obj-c to create a metal layer and attach it to the window
    layer = CA::MetalLayer::layer();
    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setDrawableSize(CGSizeMake(width, height));
    GLFWBridge::AddLayerToWindow(window, layer);



    // load mesh
    Mesh mesh = loadOBJ("models/dragon.obj", 1.0f);
    std::cout << "Loaded mesh with " << mesh.vertices.size() / 3 << " vertices and " << mesh.indices.size() / 3 << " triangles." << std::endl;

    // create buffers
    MTL::Buffer* vertexBuffer = device->newBuffer(mesh.vertices.data(), mesh.vertices.size() * sizeof(float), MTL::ResourceStorageModeShared);
    MTL::Buffer* normalBuffer = device->newBuffer(mesh.normals.data(), mesh.normals.size() * sizeof(float), MTL::ResourceStorageModeShared);
    MTL::Buffer* indexBuffer = device->newBuffer(mesh.indices.data(), mesh.indices.size() * sizeof(uint), MTL::ResourceStorageModeShared);

    // make geometry desciptor
    MTL::AccelerationStructureTriangleGeometryDescriptor* geometryDescriptor = MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init();

    // link vertex data
    geometryDescriptor->setVertexBuffer(vertexBuffer);
    geometryDescriptor->setVertexStride(sizeof(float) * 3);
    // link index data 
    geometryDescriptor->setIndexBuffer(indexBuffer);
    geometryDescriptor->setIndexType(MTL::IndexTypeUInt32);
    // set tri count
    geometryDescriptor->setTriangleCount(mesh.indices.size() / 3);
    // link per-primitive data 
    geometryDescriptor->setPrimitiveDataBuffer(normalBuffer);
    geometryDescriptor->setPrimitiveDataStride(sizeof(MTL::PackedFloat3) * 3);
    geometryDescriptor->setPrimitiveDataElementSize(sizeof(MTL::PackedFloat3) * 3);


    // create AccelerationStructure
    MTL::PrimitiveAccelerationStructureDescriptor* blasDescriptor = MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
    blasDescriptor->setGeometryDescriptors(NS::Array::array(geometryDescriptor));
    MTL::AccelerationStructure* blas = device->newAccelerationStructure(blasDescriptor);
    NS::UInteger scratchBufferSize = device->accelerationStructureSizes(blasDescriptor).buildScratchBufferSize;
    MTL::Buffer* scratchBuffer = device->newBuffer(scratchBufferSize, MTL::ResourceStorageModePrivate);

    // commit data and construct
    MTL::CommandBuffer* accelerationCommandBuffer = commandQueue->commandBuffer();
    MTL::AccelerationStructureCommandEncoder* accelerationEncoder = accelerationCommandBuffer->accelerationStructureCommandEncoder();
    accelerationEncoder->buildAccelerationStructure(blas, blasDescriptor, scratchBuffer, 0);
    accelerationEncoder->endEncoding();
    accelerationCommandBuffer->commit();
    accelerationCommandBuffer->waitUntilCompleted();

    // all data needed is now in the structure, we do not need the buffers anymore
    vertexBuffer->release();
    normalBuffer->release();
    indexBuffer->release();
    scratchBuffer->release();


    // temp imgui stuff
    struct point {
        float x, y, z;
    };
    uint frame = 1;
    point lookFrom = {2.8f, 0.0f, -1.2f};
    point lookAt = {0.0f, 0.1f, 0.0f};
    
    // start main event loop
    while (!glfwWindowShouldClose(window))  {
        NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

        glfwPollEvents();

        CA::MetalDrawable* drawable = layer->nextDrawable();


        // do compute pass
        {
            MTL::CommandBuffer* computeCommandBuffer = commandQueue->commandBuffer();
            MTL::ComputeCommandEncoder* computeEncoder = computeCommandBuffer->computeCommandEncoder();
            computeEncoder->setComputePipelineState(pipelineComputeState);
            
            computeEncoder->setAccelerationStructure(blas, 0);


            computeEncoder->setTexture(computeTexture, 0);
            //computeEncoder->setBytes(&bright, sizeof(float), 0);
            computeEncoder->setBytes(&lookFrom, sizeof(point), 1);
            computeEncoder->setBytes(&lookAt, sizeof(point), 2);
            computeEncoder->setBytes(&frame, sizeof(uint), 3);
            frame++;

            // dispatch compute
            computeEncoder->dispatchThreads(MTL::Size(width, height, 1), MTL::Size(16, 16, 1));
            computeEncoder->endEncoding();
            computeCommandBuffer->commit();
        }

        // do render pass
        { 
            MTL::CommandBuffer* renderCommandBuffer = commandQueue->commandBuffer();
            MTL::RenderPassColorAttachmentDescriptor* cd = renderPassDescriptor->colorAttachments()->object(0);
            cd->setClearColor(MTL::ClearColor(0.45f, 0.55f, 0.60f, 1.00f));
            cd->setTexture(drawable->texture());
            cd->setLoadAction(MTL::LoadActionClear);
            cd->setStoreAction(MTL::StoreActionStore);
            MTL::RenderCommandEncoder* renderEncoder = renderCommandBuffer->renderCommandEncoder(renderPassDescriptor);


            // render our quad
            renderEncoder->setRenderPipelineState(pipelineRenderState);
            // add any parameter values to buffers
            // renderEncoder->setFragmentBytes(&f, sizeof(float), 0);
            // add texture for rendering
            renderEncoder->setFragmentTexture(computeTexture, 0);
            // draw
            renderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0), static_cast<NS::UInteger>(6));


            // render imgui
            // Start the Dear ImGui frame
            renderEncoder->pushDebugGroup(NS::String::string("ImGui", NS::UTF8StringEncoding));
            ImGui_ImplMetal_NewFrame(renderPassDescriptor);
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();


            ImGui::PushFont(font);
            static bool show_demo_window = false;
            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);
            

            {
                ImGui::SetNextWindowPos(ImVec2(10, 10));
                ImGui::Begin("Cobalt");
                ImGui::Checkbox("Demo Window", &show_demo_window);
                ImGui::SliderFloat3("Look From", &lookFrom.x, -5.0f, 5.0f);
                ImGui::SliderFloat3("Look At", &lookAt.x, -1.0f, 1.0f);
                ImGui::Text("Frame-count since last purge: %i", frame);
                if(ImGui::Button("Purge")) {
                    frame = 1;
                }
                ImGui::Text("Frametime average: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                ImGui::Text("Frame-buffer size: %i x %i", width, height);
                ImGui::Text("Comp-Texture size: %i x %i", width/2, height/2);
                ImGui::End();
            }

            ImGui::PopFont();

            // Render ImGui windows
            ImGui::Render();
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), renderCommandBuffer, renderEncoder);
            renderEncoder->popDebugGroup();

            // commit render
            renderEncoder->endEncoding();
            renderCommandBuffer->presentDrawable(drawable);
            renderCommandBuffer->commit();
        }

        pPool->release();
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // TODO: cleanup

    glfwDestroyWindow(window);
    glfwTerminate();
        
    return 0;
}