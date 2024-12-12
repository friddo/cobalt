#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

int main() {
    // for testing purposes
    const size_t vectorSize = 1024;
    const size_t bufferSize = vectorSize * sizeof(float);

    // Initialize Metal device
    // WHY DOES CreateSystemDefaultDevice() NOT RETURN A DEVICE?
    MTL::Device* device = static_cast<MTL::Device*>(MTL::CopyAllDevices()->object(0));//MTL::CreateSystemDefaultDevice();
    std::cout << "Metal device: " << device->name()->utf8String() << std::endl;
    if (!device) {
        std::cerr << "Metal is not supported on this device." << std::endl;
        return -1;
    }

    // Fetch the kernel source code
    std::ifstream file("kernels/vector_add.metal");
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

    // Retrieve the compute function
    MTL::Function* function = library->newFunction(NS::String::string("vector_add", NS::UTF8StringEncoding));
    if (!function) {
        std::cerr << "Failed to retrieve function 'vector_add'." << std::endl;
        return -1;
    }

    // Create compute pipeline
    MTL::ComputePipelineState* pipelineState = device->newComputePipelineState(function, &error);
    if (error) {
        std::cerr << "Failed to create pipeline state: " << error->localizedDescription()->utf8String() << std::endl;
        return -1;
    }

    // Create input and output buffers
    std::vector<float> inputA(vectorSize, 1.0f); // fill first with 1s
    std::vector<float> inputB(vectorSize, 2.0f); // fill second with 2s
    std::vector<float> output(vectorSize);

    MTL::Buffer* bufferA = device->newBuffer(bufferSize, MTL::ResourceStorageModeShared);
    MTL::Buffer* bufferB = device->newBuffer(bufferSize, MTL::ResourceStorageModeShared);
    MTL::Buffer* bufferOut = device->newBuffer(bufferSize, MTL::ResourceStorageModeShared);

    // Copy data to GPU buffers
    memcpy(bufferA->contents(), inputA.data(), bufferSize);
    memcpy(bufferB->contents(), inputB.data(), bufferSize);

    // Create command queue and buffer
    MTL::CommandQueue* commandQueue = device->newCommandQueue();
    MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();

    // Create compute command encoder
    MTL::ComputeCommandEncoder* encoder = commandBuffer->computeCommandEncoder();
    encoder->setComputePipelineState(pipelineState);
    encoder->setBuffer(bufferA, 0, 0);
    encoder->setBuffer(bufferB, 0, 1);
    encoder->setBuffer(bufferOut, 0, 2);

    // Dispatch threads
    MTL::Size gridSize(vectorSize, 1, 1);
    MTL::Size threadgroupSize(64, 1, 1);
    encoder->dispatchThreads(gridSize, threadgroupSize);

    encoder->endEncoding();

    // Commit and wait for completion
    commandBuffer->commit();
    commandBuffer->waitUntilCompleted();

    // Retrieve output data
    memcpy(output.data(), bufferOut->contents(), bufferSize);

    // Validate results
    bool success = true;
    for (size_t i = 0; i < vectorSize; ++i) {
        if (output[i] != 3.0f) { // 1.0 + 2.0 = 3.0
            success = false;
            break;
        }
    }

    if (success) {
        std::cout << "Vector addition succeeded!" << std::endl;
    } else {
        std::cout << "Vector addition failed!" << std::endl;
    }

    // Cleanup
    bufferA->release();
    bufferB->release();
    bufferOut->release();
    pipelineState->release();
    function->release();
    library->release();
    commandQueue->release();
    device->release();

    return 0;
}
