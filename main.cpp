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


static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    layer->setDrawableSize(CGSizeMake(width, height));
}


int main() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Setup style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // dumb fucking apple retina hack. Load font at 2x size and scale down to maintain sharpness.
    ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ProggyClean.ttf", 26.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    io.FontGlobalScale = 0.5f;

    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Create window with graphics context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+Metal example", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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

    // Setup render pipeline
    MTL::RenderPipelineDescriptor* pipelineDescriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDescriptor->setVertexFunction(library->newFunction(NS::String::string("vert_shader", NS::UTF8StringEncoding)));
    pipelineDescriptor->setFragmentFunction(library->newFunction(NS::String::string("frag_shader", NS::UTF8StringEncoding)));
    pipelineDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    MTL::RenderPipelineState* pipelineState = device->newRenderPipelineState(pipelineDescriptor, &error);


    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    // magic sauce, interfaces with obj-c to create a metal layer and attach it to the window
    layer = CA::MetalLayer::layer();
    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setDrawableSize(CGSizeMake(width, height));
    GLFWBridge::AddLayerToWindow(window, layer);


    MTL::RenderPassDescriptor* renderPassDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();

    // imgui state
    bool show_demo_window = false;
    float f = 0;

    while (!glfwWindowShouldClose(window))  {
        NS::AutoreleasePool* pPool = NS::AutoreleasePool::alloc()->init();

        glfwPollEvents();

        CA::MetalDrawable* drawable = layer->nextDrawable();

        MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();
        MTL::RenderPassColorAttachmentDescriptor* cd = renderPassDescriptor->colorAttachments()->object(0);
        cd->setClearColor(MTL::ClearColor(0.45f, 0.55f, 0.60f, 1.00f));
        cd->setTexture(drawable->texture());
        cd->setLoadAction(MTL::LoadActionClear);
        cd->setStoreAction(MTL::StoreActionStore);

        MTL::RenderCommandEncoder* renderCommandEncoder = commandBuffer->renderCommandEncoder(renderPassDescriptor);

        // render our quad
        renderCommandEncoder->setRenderPipelineState(pipelineState);
        renderCommandEncoder->setFragmentBytes(&f, sizeof(float), 0);
        renderCommandEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, static_cast<NS::UInteger>(0), static_cast<NS::UInteger>(6));


        // Start the Dear ImGui frame
        renderCommandEncoder->pushDebugGroup(NS::String::string("ImGui", NS::UTF8StringEncoding));
        ImGui_ImplMetal_NewFrame(renderPassDescriptor);
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        ImGui::PushFont(font);
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        {
            ImGui::Begin("Cobalt");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::SliderFloat("blue", &f, 0, 1); // example parameter
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        ImGui::PopFont();

        // Render ImGui windows
        ImGui::Render();
        ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderCommandEncoder);
        renderCommandEncoder->popDebugGroup();

        // End render command and commit
        renderCommandEncoder->endEncoding();
        commandBuffer->presentDrawable(drawable);
        commandBuffer->commit();

        pPool->release();
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
        
    return 0;
}