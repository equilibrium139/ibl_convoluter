#include <algorithm>
#include <iostream>
#include "stb_image.h"
#include "Shader.h"
#include <glad/glad.h>
#include <vector>
#include <GLFW/glfw3.h>
#include "ispc_texcomp.h"
#include <cassert>

void Convolute(const char* hdriPath, int resolution, float maxRadiance, GLuint cubeVAO);

void GLAPIENTRY
MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam);

int TextureSizeBC6(std::uint32_t resolution, std::uint32_t mipmapLevels);

struct CubemapFile
{
    static constexpr std::uint32_t correctMagicNumber = 'PMBC';
    struct Header
    {
        std::uint32_t magicNumber = correctMagicNumber;
        std::uint32_t mipmapLevels;
        std::uint32_t resolution;
        // TODO: specify pixel format?
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
    if (argc < 3)
    {
        std::cout << "Usage: ibl_convoluter hdri1_path resolutionPixels [maxRadiance]\n";
        return 0;
    }

    int resolution = std::atoi(argv[2]);
    if (resolution <= 0)
    {
        std::cout << "Invalid resolution: '" << argv[2] << "'\n";
        return 0;
    }

    float maxRadiance = 0.0f;
    if (argc == 4)
    {
        maxRadiance = std::atof(argv[3]);
        if (maxRadiance <= 0.0f)
        {
            std::cout << "Invalid max radiance: '" << argv[3] << "'\n";
            return 0;
        }
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

    Convolute(argv[1], resolution, maxRadiance, cubeVAO);

    return 0;
}

void Convolute(const char* hdriPath, int resolution, float maxRadiance, GLuint cubeVAO)
{
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    float* data = stbi_loadf(hdriPath, &width, &height, &nrComponents, 0);
    GLuint hdrTexture;
    if (data)
    {
        assert(nrComponents == 3);
        if (maxRadiance > 0.0f)
        {
            for (int i = 0; i < width * height; i++)
            {
                data[i] = std::clamp(data[i], 0.0f, maxRadiance);
            }
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

    GLuint environmentMap;
    glGenTextures(1, &environmentMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        // RGBA used because this is the format expected by the BC6 compressor being used (final result of BC6 compression doesn't include alpha)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F,
            resolution, resolution, 0, GL_RGBA, GL_FLOAT, nullptr);
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

    glViewport(0, 0, resolution, resolution);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environmentMap, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)(i * 6 * sizeof(GLuint)));
    }

     // TODO: look into compressonator mip map generation
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    CubemapFile envMapFile;
    envMapFile.header.resolution = resolution;
    envMapFile.header.mipmapLevels = 1 + (int)std::log2(resolution);
    int bytesPerFace = TextureSizeBC6(resolution, envMapFile.header.mipmapLevels);
    envMapFile.pixels.resize(bytesPerFace * 6);

    std::vector<std::uint8_t> uncompressedPixels(resolution * resolution * 8);

    int faceOffsetBytes = 0;
    for (unsigned int i = 0; i < 6; ++i)
    {
        int mipRes = envMapFile.header.resolution;
        int mipLevelOffsetBytes = 0;
        for (unsigned int j = 0; j < envMapFile.header.mipmapLevels; j++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, environmentMap, j);
            glReadPixels(0, 0, mipRes, mipRes, GL_RGBA, GL_HALF_FLOAT, uncompressedPixels.data());
            rgba_surface surface;
            surface.ptr = uncompressedPixels.data();
            surface.width = mipRes;
            surface.height = mipRes;
            surface.stride = surface.width * 8;
            bc6h_enc_settings settings;
            GetProfile_bc6h_basic(&settings);
            CompressBlocksBC6H(&surface, &envMapFile.pixels[faceOffsetBytes + mipLevelOffsetBytes], &settings);
            mipLevelOffsetBytes += mipRes * mipRes;
            mipRes = std::max(mipRes / 2, 4);
        }
        faceOffsetBytes += bytesPerFace;
    }
     
    WriteCubemapFile(envMapFile, "envmap.cbmp");

    const int irradianceRes = 32;
    GLuint irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, irradianceRes, irradianceRes, 0,
            GL_RGBA, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    Shader convolutionShader = Shader("Shaders/equirectToCubemap.vert", "Shaders/convolute.frag");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    convolutionShader.SetInt("environmentMap", 0);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glViewport(0, 0, irradianceRes, irradianceRes);
    uncompressedPixels.resize(irradianceRes * irradianceRes * 8);
    CubemapFile irradianceMapFileData;
    irradianceMapFileData.header.resolution = irradianceRes;
    irradianceMapFileData.header.mipmapLevels = 1;
    irradianceMapFileData.pixels.resize(irradianceRes * irradianceRes * 6);
    int byteOffset = 0;
    for (unsigned int i = 0; i < 6; ++i)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)(i * 6 * sizeof(GLuint)));
        glReadPixels(0, 0, irradianceRes, irradianceRes, GL_RGBA, GL_HALF_FLOAT, uncompressedPixels.data());
        rgba_surface surface;
        surface.ptr = uncompressedPixels.data();
        surface.width = irradianceRes;
        surface.height = irradianceRes;
        surface.stride = surface.width * 8;
        bc6h_enc_settings settings;
        GetProfile_bc6h_basic(&settings);
        CompressBlocksBC6H(&surface, &irradianceMapFileData.pixels[i * irradianceRes * irradianceRes], &settings);
    }
    
    WriteCubemapFile(irradianceMapFileData, "irradiance.cbmp");

    const int prefilterRes = 128;
    GLuint prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, prefilterRes, prefilterRes, 0, GL_RGBA, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    uncompressedPixels.resize(prefilterRes * prefilterRes * 8);
    Shader prefilterShader = Shader("Shaders/equirectToCubemap.vert", "Shaders/prefilter.frag");
    prefilterShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentMap);
    prefilterShader.SetInt("environmentMap", 0);
    prefilterShader.SetFloat("environmentMapResolution", resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    const unsigned int mipLevels = 5;
    CubemapFile prefilterFile;
    prefilterFile.header.mipmapLevels = mipLevels;
    prefilterFile.header.resolution = prefilterRes;
    prefilterFile.pixels.resize(TextureSizeBC6(prefilterRes, mipLevels) * 6);

    byteOffset = 0;
    for (unsigned int i = 0; i < 6; ++i)
    {
        int mipRes = prefilterRes;
        for (unsigned int j = 0; j < mipLevels; j++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, j);
            glViewport(0, 0, mipRes, mipRes);
            glClear(GL_COLOR_BUFFER_BIT);
            float roughness = (float)j / (float)(mipLevels - 1);
            prefilterShader.SetFloat("roughness", roughness);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)(i * 6 * sizeof(GLuint)));
            glReadPixels(0, 0, mipRes, mipRes, GL_RGBA, GL_HALF_FLOAT, uncompressedPixels.data());
            rgba_surface surface;
            surface.ptr = uncompressedPixels.data();
            surface.width = mipRes;
            surface.height = mipRes;
            surface.stride = surface.width * 8;
            bc6h_enc_settings settings;
            GetProfile_bc6h_basic(&settings);
            CompressBlocksBC6H(&surface, &prefilterFile.pixels[byteOffset], &settings);
            byteOffset += mipRes >= 4 ? mipRes * mipRes : 4 * 4;
            mipRes /= 2;
        }
    }

    WriteCubemapFile(prefilterFile, "prefilter.cbmp");
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

int TextureSizeBC6(std::uint32_t resolution, std::uint32_t mipmapLevels)
{
    int bytesNeeded = resolution * resolution;

    for (int i = 1; i < mipmapLevels; i++)
    {
        resolution = std::max(resolution / 2, 4u); // BC6 always works with 4x4 blocks
        bytesNeeded += resolution * resolution;
    }

    return bytesNeeded;
}
