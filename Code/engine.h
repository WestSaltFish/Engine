//
// engine.h: This file contains the types and functions relative to the engine.
//
#pragma once

#include "platform.h"
#include "BufferSupFuncs.h"
#include "ModelLoadingFuncs.h"
#include "Globals.h"

const VertexV3V2 vertices[] = {
    {glm::vec3(-1.0, -1.0, 0.0), glm::vec2(0.0, 0.0)},
    {glm::vec3(1.0, -1.0, 0.0), glm::vec2(1.0, 0.0)},
    {glm::vec3(1.0, 1.0, 0.0), glm::vec2(1.0, 1.0)},
    {glm::vec3(-1.0, 1.0, 0.0), glm::vec2(0.0, 1.0)},
};

const u16 indices[] =
{
    0,1,2,
    0,2,3
};

struct Bloom 
{
    GLuint rtBright; // For blitting brightest pixels and vertical blur
    GLuint rtBloomH; // For first pass horizontal blur
    FrameBuffer fbBloom1;
    FrameBuffer fbBloom2;
    FrameBuffer fbBloom3;
    FrameBuffer fbBloom4;
    FrameBuffer fbBloom5;
};

struct App
{
    void UpdateEntityBuffer();

    void ConfigureFrameBuffer(FrameBuffer& aConfig);

    void RenderGeometry(const Program& aBindedProgram);

    const GLuint CreateTexture(const bool isFloating = false);

    // Loop
    f32  deltaTime;
    bool isRunning;

    // Input
    Input input;

    // Graphics
    char gpuName[64];
    char openGlVersion[64];

    ivec2 displaySize;

    std::vector<Texture>    textures;
    std::vector<Material>   materials;
    std::vector<Mesh>       meshes;
    std::vector<Model>      models;
    std::vector<Program>    programs;

    // program indices
    GLuint renderToBackBufferShader;
    GLuint renderToFrameBufferShader;
    GLuint framebufferToQuadShader;
    GLuint gridRenderShader;
    // for bloom
    GLuint blitBrightestPixelsShader;
    GLuint blurShader;
    GLuint bloomShader;

    GLuint texturedMeshProgram_uTexture;
    
    // texture indices
    u32 diceTexIdx;
    u32 whiteTexIdx;
    u32 blackTexIdx;
    u32 normalTexIdx;
    u32 magentaTexIdx;

    // Mode
    Mode mode;

    // Embedded geometry (in-editor simple meshes such as
    // a screen filling quad, a cube, a sphere...)
    GLuint embeddedVertices;
    GLuint embeddedElements;

    // Location of the texture uniform in the textured quad shader
    GLuint programUniformTexture;

    // VAO object to link our screen filling quad with our textured quad shader
    GLuint vao;

    std::string openglDebugInfo;

    GLint maxUniformBufferSize;
    GLint uniformBlockAligment;
    Buffer localUniformBuffer;
    std::vector<Entity> entities;
    std::vector<Light> lights;

    GLuint globalParamsOffset;
    GLuint globalParamsSize;

    FrameBuffer deferredFrameBuffer;

    Bloom bloom;

    Camera camera;
};

void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);

void UpdateCamera(App* app);

void InitBloomEffect(App* app);