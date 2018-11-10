#pragma once

#include "common.h"

enum VLRDataFormat {
    VLRDataFormat_RGB8x3 = 0,
    VLRDataFormat_RGB_8x4,
    VLRDataFormat_RGBA8x4,
    VLRDataFormat_RGBA16Fx4,
    VLRDataFormat_RGBA32Fx4,
    VLRDataFormat_RG32Fx2,
    VLRDataFormat_Gray32F,
    VLRDataFormat_Gray8,
    NumVLRDataFormats
};



enum VLRShaderNodeSocketType {
    VLRShaderNodeSocketType_float = 0,
    VLRShaderNodeSocketType_float2,
    VLRShaderNodeSocketType_float3,
    VLRShaderNodeSocketType_float4,
    VLRShaderNodeSocketType_Point3D,
    VLRShaderNodeSocketType_Vector3D,
    VLRShaderNodeSocketType_Normal3D,
    VLRShaderNodeSocketType_Spectrum,
    VLRShaderNodeSocketType_TextureCoordinates,
    NumVLRShaderNodeSocketTypes,
    VLRShaderNodeSocketType_Invalid
};

struct VLRShaderNodeSocketInfo {
    uint32_t dummy;
};



enum VLRTextureFilter {
    VLRTextureFilter_Nearest = 0,
    VLRTextureFilter_Linear,
    VLRTextureFilter_None
};



enum VLRTangentType {
    VLRTangentType_VertexAttribute = 0,
    VLRTangentType_RadialX,
    VLRTangentType_RadialY,
    VLRTangentType_RadialZ,
};



struct VLRPoint3D {
    float x, y, z;
};

struct VLRNormal3D {
    float x, y, z;
};

struct VLRVector3D {
    float x, y, z;
};

struct VLRTexCoord2D {
    float u, v;
};

struct VLRQuaternion {
    float x, y, z, w;
};



struct VLRVertex {
    VLRPoint3D position;
    VLRNormal3D normal;
    VLRVector3D tangent;
    VLRTexCoord2D texCoord;
};
