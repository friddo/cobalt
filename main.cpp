#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"

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
// window size
int width, height;

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void framebuffer_size_callback(GLFWwindow* window, int _width, int _height)
{
    width = _width;
    height = _height;
    layer->setDrawableSize(CGSizeMake(_width, _height));
}


int main() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    // dumb apple retina hack. Load font at 2x size and scale down to maintain sharpness.
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ProggyClean.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    io.FontGlobalScale = 0.5f;


    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Create window with graphics context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // TODO: currently we do not allow window resizing since our texture is fixed size
    //       in the framebuffer_size_callback we should reallocate the texture if the size changes
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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
        std::cerr << "Failed to open file." << std::endl;
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
    pipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState* pipelineRenderState = device->newRenderPipelineState(pipelineDescriptor, &error);
    if (error) {
        std::cerr << "Failed to create render pipeline state: " << error->localizedDescription()->utf8String() << std::endl;
        return -1;
    }
    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();


    // Make texture used for compute and render
    MTL::TextureDescriptor* textureDescriptor = MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatRGBA32Float, width, height, false);
    textureDescriptor->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
    MTL::Texture* computeTexture = device->newTexture(textureDescriptor);


    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);


    // Magic sauce, interfaces with obj-c to create a metal layer and attach it to the window
    layer = CA::MetalLayer::layer();
    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setDrawableSize(CGSizeMake(width, height));
    GLFWBridge::AddLayerToWindow(window, layer);


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
            computeEncoder->setTexture(computeTexture, 0);

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
                ImGui::Text("Frametime average: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                ImGui::Text("Frame-buffer size: %i x %i", width, height);
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