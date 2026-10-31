#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "SOIL2/SOIL2.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "AnimationStructs.h"   // BoneInfo (una sola definicion)
#include "AssimpGLMHelpers.h"
#include "Mesh.h"
#include "Shader.h"

using namespace std;

GLint TextureFromFile(const char* path, string directory);
GLint TextureFromEmbedded(const aiTexture* tex);

class Model
{
public:
    // preTransform = true para modelos estaticos descargados: aplica las transformaciones de sus nodos
    // (no usar en modelos con huesos, porque elimina el esqueleto)
    Model(GLchar* path, bool preTransform = false) { this->loadModel(path, preTransform); }

    void Draw(Shader shader) {
        for (GLuint i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

    const vector<Mesh>& GetMeshes() const { return meshes; }

    // Caja envolvente del modelo, para escalarlo y asentarlo en el piso
    glm::vec3 minBounds = glm::vec3(1e9f);
    glm::vec3 maxBounds = glm::vec3(-1e9f);

    // Accesores para Animation::ReadMissingBones
    std::map<std::string, BoneInfo>& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount()                               { return m_BoneCounter; }

private:
    vector<Mesh>    meshes;
    string          directory;
    vector<Texture> textures_loaded;

    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;

    void SetVertexBoneDataToDefault(Vertex& v) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
            v.m_BoneIDs[i] = -1;
            v.m_Weights[i] = 0.0f;
        }
    }

    void SetVertexBoneData(Vertex& v, int boneID, float weight) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
            if (v.m_BoneIDs[i] < 0) {
                v.m_BoneIDs[i] = boneID;
                v.m_Weights[i] = weight;
                break;
            }
        }
    }

    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene*)
    {
        for (int b = 0; b < (int)mesh->mNumBones; b++) {
            int         boneID   = -1;
            std::string boneName = mesh->mBones[b]->mName.C_Str();

            if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end()) {
                BoneInfo newBone;
                newBone.id     = m_BoneCounter;
                newBone.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(
                                     mesh->mBones[b]->mOffsetMatrix);
                m_BoneInfoMap[boneName] = newBone;
                boneID = m_BoneCounter++;
            } else {
                boneID = m_BoneInfoMap[boneName].id;
            }

            auto* weights    = mesh->mBones[b]->mWeights;
            int   numWeights = mesh->mBones[b]->mNumWeights;
            for (int w = 0; w < numWeights; w++) {
                int   vId    = weights[w].mVertexId;
                float weight = weights[w].mWeight;
                if (vId < (int)vertices.size())
                    SetVertexBoneData(vertices[vId], boneID, weight);
            }
        }
    }

    void loadModel(string path, bool preTransform) {
        Assimp::Importer importer;
        // GenSmoothNormals solo genera normales cuando el modelo descargado no las trae
        unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals;
        if (preTransform)
            flags |= aiProcess_PreTransformVertices;
        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            cout << "ERROR::ASSIMP:: " << path << ": " << importer.GetErrorString() << endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('/'));
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode* node, const aiScene* scene) {
        for (GLuint i = 0; i < node->mNumMeshes; i++)
            meshes.push_back(processMesh(scene->mMeshes[node->mMeshes[i]], scene));
        for (GLuint i = 0; i < node->mNumChildren; i++)
            processNode(node->mChildren[i], scene);
    }

    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        vector<Vertex>  vertices;
        vector<GLuint>  indices;
        vector<Texture> textures;

        for (GLuint i = 0; i < mesh->mNumVertices; i++) {
            Vertex v;
            SetVertexBoneDataToDefault(v);

            v.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            minBounds = glm::min(minBounds, v.Position);
            maxBounds = glm::max(maxBounds, v.Position);
            if (mesh->mNormals)
                v.Normal = { mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z  };
            else
                v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);

            if (mesh->mTextureCoords[0])
                v.TexCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
            else
                v.TexCoords = glm::vec2(0.0f);

            vertices.push_back(v);
        }

        for (GLuint i = 0; i < mesh->mNumFaces; i++) {
            aiFace& face = mesh->mFaces[i];
            for (GLuint j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        // Color difuso del material: se usa cuando el modelo no trae textura (modelos low-poly solo con colores)
        glm::vec3 diffuseColor(1.0f);
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiColor4D color;
            if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS)
                diffuseColor = glm::vec3(color.r, color.g, color.b);
            auto diffuse  = loadMaterialTextures(mat, aiTextureType_DIFFUSE,  "texture_diffuse", scene);
            auto specular = loadMaterialTextures(mat, aiTextureType_SPECULAR, "texture_specular", scene);
            textures.insert(textures.end(), diffuse.begin(),  diffuse.end());
            textures.insert(textures.end(), specular.begin(), specular.end());
        }

        ExtractBoneWeightForVertices(vertices, mesh, scene);
        return Mesh(vertices, indices, textures, diffuseColor);
    }

    vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName, const aiScene* scene) {
        vector<Texture> textures;
        for (GLuint i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);
            bool skip = false;
            for (auto& t : textures_loaded) {
                if (t.path == str) { textures.push_back(t); skip = true; break; }
            }
            if (!skip) {
                Texture texture;
                // "*N" = textura incrustada en el archivo (GLB / FBX de Mixamo)
                if (str.C_Str()[0] == '*' && scene->mNumTextures > 0) {
                    unsigned int idx = (unsigned int)atoi(str.C_Str() + 1);
                    texture.id = idx < scene->mNumTextures ? TextureFromEmbedded(scene->mTextures[idx]) : 0;
                } else {
                    texture.id = TextureFromFile(str.C_Str(), directory);
                }
                texture.type = typeName;
                texture.path = str;
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }
};

GLint UploadTexture(unsigned char* image, int width, int height);

GLint TextureFromFile(const char* path, string directory)
{
    string filename = directory + '/' + string(path);
    int width, height;
    unsigned char* image = SOIL_load_image(filename.c_str(), &width, &height, 0, SOIL_LOAD_RGB);
    if (!image) {
        // Modelos descargados suelen traer rutas absolutas del autor: se intenta solo con el nombre del archivo
        string name = string(path);
        size_t slash = name.find_last_of("/\\");
        string base = (slash != string::npos) ? name.substr(slash + 1) : name;
        filename = directory + '/' + base;
        image = SOIL_load_image(filename.c_str(), &width, &height, 0, SOIL_LOAD_RGB);
        if (!image) {
            // Modelos de Mixamo: texturas en la subcarpeta textures/
            filename = directory + "/textures/" + base;
            image = SOIL_load_image(filename.c_str(), &width, &height, 0, SOIL_LOAD_RGB);
        }
    }
    if (!image) {
        cout << "ERROR::TEXTURE: " << filename << endl;
        cout << "SOIL: " << SOIL_last_result() << endl;
        return 0;
    }
    GLint textureID = UploadTexture(image, width, height);
    SOIL_free_image_data(image);
    return textureID;
}

// Textura incrustada: comprimida (png/jpg, mHeight == 0) o en texeles BGRA sin comprimir
GLint TextureFromEmbedded(const aiTexture* tex)
{
    int width, height, channels;
    if (tex->mHeight == 0) {
        unsigned char* image = SOIL_load_image_from_memory((const unsigned char*)tex->pcData, (int)tex->mWidth,
                                                           &width, &height, &channels, SOIL_LOAD_RGB);
        if (!image) {
            cout << "ERROR::TEXTURE: incrustada (" << SOIL_last_result() << ")" << endl;
            return 0;
        }
        GLint textureID = UploadTexture(image, width, height);
        SOIL_free_image_data(image);
        return textureID;
    }
    width = (int)tex->mWidth;
    height = (int)tex->mHeight;
    vector<unsigned char> image(width * height * 3);
    for (int i = 0; i < width * height; i++) {
        image[i * 3 + 0] = tex->pcData[i].r;
        image[i * 3 + 1] = tex->pcData[i].g;
        image[i * 3 + 2] = tex->pcData[i].b;
    }
    return UploadTexture(image.data(), width, height);
}

GLint UploadTexture(unsigned char* image, int width, int height)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}
