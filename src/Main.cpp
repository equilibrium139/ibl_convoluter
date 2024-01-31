#include <algorithm>
#include <iostream>
#include "stb_image.h"
#include "Shader.h"
#include <glad/glad.h>
#include <vector>
#include <GLFW/glfw3.h>
#include "ispc_texcomp.h"

void Convolute(const char* hdriPath, GLuint cubeVAO);

void GLAPIENTRY
MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam);

struct CubemapFile
{
    static constexpr std::uint32_t correctMagicNumber = ' SDD';
    struct Header
    {
        std::uint32_t magicNumber = correctMagicNumber;
        std::uint32_t mipmapLevels;
        std::uint32_t width;
        std::uint32_t height;
        // TODO: specify pixel format
    };
    Header header;
    std::vector<std::uint8_t> pixels;
};

inline void WriteCubemapFile(const CubemapFile& cubemap, const std::string& file_path)
{
    std::ofstream file(file_path, std::ios::binary);

    file.write((const char*)&cubemap.header, sizeof(CubemapFile::Header));
    file.write((const char*)cubemap.pixels.data(), cubemap.pixels.size());
}

struct Vec3
{
    float x, y, z;
};

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: ibl_convoluter hdri1_path [hdri2_path] ... [hdri3_path]\n";
    }

    if (!glfwInit())
        return -1;

    const char* glsl_version = "#version 430";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* dummyWindow = glfwCreateWindow(100, 100, "", NULL, NULL);

    if (!dummyWindow)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(dummyWindow);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    Vec3 cubeVertices[] = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 1.0f},  // POSITIVE_X

        {-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, -1.0f},    // NEGATIVE_X

        {-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 1.0f},   // POSITIVE_Y 

        {-1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, -1.0f}, // NEGATIVE_Y

         {-1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 1.0f},   // POSITIVE_Z

        {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, -1.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, 0.0f}, {1.0f, -1.0f, -1.0f}, // NEGATIVE_Z
    };

    GLuint cubeIndices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    GLuint cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glBindVertexArray(cubeVAO);

    GLuint cubeVBO;
    glGenBuffers(1, &cubeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    GLuint cubeIBO;
    glGenBuffers(1, &cubeIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(Vec3), 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(Vec3), (const void*)(sizeof(Vec3)));

    for (int i = 1; i < argc; i++)
    {
        Convolute(argv[i], cubeVAO);
    }

    return 0;
}

void Convolute(const char* hdriPath, GLuint cubeVAO)
{
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    float* data = stbi_loadf(hdriPath, &width, &height, &nrComponents, 0);
    GLuint hdrTexture;
    if (data)
    {
        for (int i = 0; i < width * height * nrComponents; i++)
        {
            data[i] = std::clamp(data[i], 0.0f, 500.0f);
        }
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image at " << hdriPath << std::endl;
        return;
    }
    // TODO: remove
    width = 2048;

    GLuint environmentMap;
    glGenTextures(1, &environmentMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F,
            width, width, 0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLuint captureFBO;
    glGenFramebuffers(1, &captureFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    Shader equirectToCubemapShader = Shader("Shaders/equirectToCubemap.vert", "Shaders/equirectToCubemap.frag");
    equirectToCubemapShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    equirectToCubemapShader.SetInt("equirectangularMap", 0);

    glViewport(0, 0, width, width);

    std::vector<std::uint8_t> pixels(width * width * 8);
    CubemapFile envMapFile;
    envMapFile.header.width = width;
    envMapFile.header.height = width;
    envMapFile.header.mipmapLevels = 1;
    envMapFile.pixels.resize(envMapFile.header.width * envMapFile.header.height * 6, 255);

    for (unsigned int i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environmentMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)(i * 6 * sizeof(GLuint)));
        glReadPixels(0, 0, width, width, GL_RGBA, GL_HALF_FLOAT, pixels.data());
        rgba_surface surface;
        surface.ptr = pixels.data();
        surface.width = width;
        surface.height = width;
        surface.stride = surface.width * 8;
        bc6h_enc_settings settings;
        GetProfile_bc6h_basic(&settings);
        CompressBlocksBC6H(&surface, &envMapFile.pixels[envMapFile.header.width * envMapFile.header.height * i], &settings);
    }

    WriteCubemapFile(envMapFile, "envmap.cubemap");

    // TODO: look into compressonator mip map generation
    // glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    // glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
    }
}
