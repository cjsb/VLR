﻿#pragma once

#include <VLR.h>
#include "scene.h"



VLR_API VLRResult vlrCreateContext(VLRContext* context) {
    *context = new VLR::Context;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrDestroyContext(VLRContext context) {
    delete context;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrContextBindOpenGLBuffer(VLRContext context, uint32_t bufferID, uint32_t width, uint32_t height) {
    context->bindOpenGLBuffer(bufferID, width, height);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrContextRender(VLRContext context, VLRScene scene, VLRCamera camera, uint32_t shrinkCoeff, bool firstFrame) {
    if (!scene->is<VLR::Scene>() || !camera->isMemberOf<VLR::Camera>())
        return VLR_ERROR_INVALID_TYPE;
    context->render(*scene, camera, shrinkCoeff, firstFrame);

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrLinearImage2DCreate(VLRContext context, VLRLinearImage2D* image,
                                         uint32_t width, uint32_t height, VLRDataFormat format, uint8_t* linearData) {
    *image = new VLR::LinearImage2D(*context, linearData, width, height, format);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrLinearImage2DDestroy(VLRContext context, VLRLinearImage2D image) {
    if (!image->is<VLR::LinearImage2D>())
        return VLR_ERROR_INVALID_TYPE;
    delete image;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrLinearImage2DGetWidth(VLRLinearImage2D image, uint32_t* width) {
    if (!image->is<VLR::LinearImage2D>())
        return VLR_ERROR_INVALID_TYPE;
    *width = image->getWidth();

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrLinearImage2DGetHeight(VLRLinearImage2D image, uint32_t* height) {
    if (!image->is<VLR::LinearImage2D>())
        return VLR_ERROR_INVALID_TYPE;
    *height = image->getHeight();

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrLinearImage2DGetStride(VLRLinearImage2D image, uint32_t* stride) {
    if (!image->is<VLR::LinearImage2D>())
        return VLR_ERROR_INVALID_TYPE;
    *stride = image->getStride();

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrConstantFloat3TextureCreate(VLRContext context, VLRConstantFloat3Texture* texture,
                                                 const float value[3]) {
    *texture = new VLR::ConstantFloat3Texture(*context, value);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrConstantFloat3TextureDestroy(VLRContext context, VLRConstantFloat3Texture texture) {
    if (!texture->is<VLR::ConstantFloat3Texture>())
        return VLR_ERROR_INVALID_TYPE;
    delete texture;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrImageFloat3TextureCreate(VLRContext context, VLRImageFloat3Texture* texture,
                                              VLRImage2D image) {
    *texture = new VLR::ImageFloat3Texture(*context, image);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrImageFloat3TextureDestroy(VLRContext context, VLRImageFloat3Texture texture) {
    if (!texture->is<VLR::ImageFloat3Texture>())
        return VLR_ERROR_INVALID_TYPE;
    delete texture;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrConstantFloat4TextureCreate(VLRContext context, VLRConstantFloat4Texture* texture,
                                                 const float value[4]) {
    *texture = new VLR::ConstantFloat4Texture(*context, value);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrConstantFloat4TextureDestroy(VLRContext context, VLRConstantFloat4Texture texture) {
    if (!texture->is<VLR::ConstantFloat4Texture>())
        return VLR_ERROR_INVALID_TYPE;
    delete texture;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrImageFloat4TextureCreate(VLRContext context, VLRImageFloat4Texture* texture,
                                              VLRImage2D image) {
    *texture = new VLR::ImageFloat4Texture(*context, image);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrImageFloat4TextureDestroy(VLRContext context, VLRImageFloat4Texture texture) {
    if (!texture->is<VLR::ImageFloat4Texture>())
        return VLR_ERROR_INVALID_TYPE;
    delete texture;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrMatteSurfaceMaterialCreate(VLRContext context, VLRMatteSurfaceMaterial* material,
                                                VLRFloat4Texture texAlbedoRoughness) {
    *material = new VLR::MatteSurfaceMaterial(*context, texAlbedoRoughness);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrMatteSurfaceMaterialDestroy(VLRContext context, VLRMatteSurfaceMaterial material) {
    if (!material->is<VLR::MatteSurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;
    delete material;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrSpecularReflectionSurfaceMaterialCreate(VLRContext context, VLRSpecularReflectionSurfaceMaterial* material,
                                                             VLRFloat3Texture texCoeffR, VLRFloat3Texture texEta, VLRFloat3Texture tex_k) {
    *material = new VLR::SpecularReflectionSurfaceMaterial(*context, texCoeffR, texEta, tex_k);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrSpecularReflectionSurfaceMaterialDestroy(VLRContext context, VLRSpecularReflectionSurfaceMaterial material) {
    if (!material->is<VLR::SpecularReflectionSurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;
    delete material;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrSpecularScatteringSurfaceMaterialCreate(VLRContext context, VLRSpecularScatteringSurfaceMaterial* material,
                                                             VLRFloat3Texture texCoeff, VLRFloat3Texture texEtaExt, VLRFloat3Texture texEtaInt) {
    *material = new VLR::SpecularScatteringSurfaceMaterial(*context, texCoeff, texEtaExt, texEtaInt);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrSpecularScatteringSurfaceMaterialDestroy(VLRContext context, VLRSpecularScatteringSurfaceMaterial material) {
    if (!material->is<VLR::SpecularScatteringSurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;
    delete material;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrUE4SurfaceMaterialCreate(VLRContext context, VLRUE4SurfaceMaterial* material,
                                              VLRFloat3Texture texBaseColor, VLRFloat2Texture texRoughnessMetallic) {
    *material = new VLR::UE4SurfaceMaterial(*context, texBaseColor, texRoughnessMetallic);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrUE4SurfaceMaterialDestroy(VLRContext context, VLRUE4SurfaceMaterial material) {
    if (!material->is<VLR::UE4SurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;
    delete material;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrDiffuseEmitterSurfaceMaterialCreate(VLRContext context, VLRDiffuseEmitterSurfaceMaterial* material,
                                                         VLRFloat3Texture texEmittance) {
    *material = new VLR::DiffuseEmitterSurfaceMaterial(*context, texEmittance);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrDiffuseEmitterSurfaceMaterialDestroy(VLRContext context, VLRDiffuseEmitterSurfaceMaterial material) {
    if (!material->is<VLR::DiffuseEmitterSurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;
    delete material;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrMultiSurfaceMaterialCreate(VLRContext context, VLRMultiSurfaceMaterial* material,
                                                const VLRSurfaceMaterial* materials, uint32_t numMaterials) {
    *material = new VLR::MultiSurfaceMaterial(*context, (const VLR::SurfaceMaterial**)materials, numMaterials);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrMultiSurfaceMaterialDestroy(VLRContext context, VLRMultiSurfaceMaterial material) {
    if (!material->is<VLR::MultiSurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;
    delete material;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrTriangleMeshSurfaceNodeCreate(VLRContext context, VLRTriangleMeshSurfaceNode* surfaceNode, 
                                                   const char* name) {
    *surfaceNode = new VLR::TriangleMeshSurfaceNode(*context, name);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrTriangleMeshSurfaceNodeDestroy(VLRContext context, VLRTriangleMeshSurfaceNode surfaceNode) {
    if (!surfaceNode->is<VLR::TriangleMeshSurfaceNode>())
        return VLR_ERROR_INVALID_TYPE;
    delete surfaceNode;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrTriangleMeshSurfaceNodeSetName(VLRTriangleMeshSurfaceNode node, const char* name) {
    if (!node->is<VLR::TriangleMeshSurfaceNode>())
        return VLR_ERROR_INVALID_TYPE;
    node->setName(name);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrTriangleMeshSurfaceNodeGetName(VLRTriangleMeshSurfaceNode node, const char** name) {
    if (!node->is<VLR::TriangleMeshSurfaceNode>())
        return VLR_ERROR_INVALID_TYPE;
    *name = node->getName().c_str();

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrTriangleMeshSurfaceNodeSetVertices(VLRTriangleMeshSurfaceNode surfaceNode, VLRVertex* vertices, uint32_t numVertices) {
    if (!surfaceNode->is<VLR::TriangleMeshSurfaceNode>())
        return VLR_ERROR_INVALID_TYPE;

    std::vector<VLR::Vertex> vecVertices;
    vecVertices.resize(numVertices);
    std::copy_n(vertices, numVertices, vecVertices.data());

    surfaceNode->setVertices(std::move(vecVertices));

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrTriangleMeshSurfaceNodeAddMaterialGroup(VLRTriangleMeshSurfaceNode surfaceNode, uint32_t* indices, uint32_t numIndices, VLRSurfaceMaterial material) {
    if (!surfaceNode->is<VLR::TriangleMeshSurfaceNode>())
        return VLR_ERROR_INVALID_TYPE;

    std::vector<uint32_t> vecIndices;
    vecIndices.resize(numIndices);
    std::copy_n(indices, numIndices, vecIndices.data());

    if (!material->isMemberOf<VLR::SurfaceMaterial>())
        return VLR_ERROR_INVALID_TYPE;

    surfaceNode->addMaterialGroup(std::move(vecIndices), material);

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrInternalNodeCreate(VLRContext context, VLRInternalNode* node,
                                        const char* name, const VLR::Transform* transform) {
    *node = new VLR::InternalNode(*context, name, transform);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeDestroy(VLRContext context, VLRInternalNode node) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;
    delete node;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeSetName(VLRInternalNode node, const char* name) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;
    node->setName(name);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeGetName(VLRInternalNode node, const char** name) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;
    *name = node->getName().c_str();

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeSetTransform(VLRInternalNode node, const VLR::Transform* localToWorld) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;
    node->setTransform(localToWorld);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeGetTransform(VLRInternalNode node, const VLR::Transform** localToWorld) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;
    *localToWorld = node->getTransform();

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeAddChild(VLRInternalNode node, VLRObject child) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;

    if (child->isMemberOf<VLR::InternalNode>())
        node->addChild((VLR::InternalNode*)child);
    else if (child->isMemberOf<VLR::SurfaceNode>())
        node->addChild((VLR::SurfaceNode*)child);
    else
        return VLR_ERROR_INVALID_TYPE;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrInternalNodeRemoveChild(VLRInternalNode node, VLRObject child) {
    if (!node->is<VLR::InternalNode>())
        return VLR_ERROR_INVALID_TYPE;

    if (child->isMemberOf<VLR::InternalNode>())
        node->removeChild((VLR::InternalNode*)child);
    else if (child->isMemberOf<VLR::SurfaceNode>())
        node->removeChild((VLR::SurfaceNode*)child);
    else
        return VLR_ERROR_INVALID_TYPE;

    return VLR_ERROR_NO_ERROR;
}



VLR_API VLRResult vlrSceneCreate(VLRContext context, VLRScene* scene,
                                 const VLR::Transform* transform) {
    *scene = new VLR::Scene(*context, transform);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrSceneDestroy(VLRContext context, VLRScene scene) {
    if (!scene->is<VLR::Scene>())
        return VLR_ERROR_INVALID_TYPE;
    delete scene;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrSceneSetTransform(VLRScene scene, const VLR::Transform* localToWorld) {
    if (!scene->is<VLR::Scene>())
        return VLR_ERROR_INVALID_TYPE;
    scene->setTransform(localToWorld);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrSceneAddChild(VLRScene scene, VLRObject child) {
    if (!scene->is<VLR::Scene>())
        return VLR_ERROR_INVALID_TYPE;

    if (child->isMemberOf<VLR::InternalNode>())
        scene->addChild((VLR::InternalNode*)child);
    else if (child->isMemberOf<VLR::SurfaceNode>())
        scene->addChild((VLR::SurfaceNode*)child);
    else
        return VLR_ERROR_INVALID_TYPE;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrSceneRemoveChild(VLRScene scene, VLRObject child) {
    if (!scene->is<VLR::Scene>())
        return VLR_ERROR_INVALID_TYPE;

    if (child->isMemberOf<VLR::InternalNode>())
        scene->removeChild((VLR::InternalNode*)child);
    else if (child->isMemberOf<VLR::SurfaceNode>())
        scene->removeChild((VLR::SurfaceNode*)child);
    else
        return VLR_ERROR_INVALID_TYPE;

    return VLR_ERROR_NO_ERROR;
}




VLR_API VLRResult vlrPerspectiveCameraCreate(VLRContext context, VLRPerspectiveCamera* camera,
                                             const VLR::Point3D &position, const VLR::Quaternion &orientation,
                                             float sensitivity, float aspect, float fovY, float lensRadius, float imgPDist, float objPDist) {
    *camera = new VLR::PerspectiveCamera(*context, position, orientation, sensitivity, aspect, fovY, lensRadius, imgPDist, objPDist);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrPerspectiveCameraDestroy(VLRContext context, VLRPerspectiveCamera camera) {
    if (!camera->is<VLR::PerspectiveCamera>())
        return VLR_ERROR_INVALID_TYPE;
    delete camera;

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrPerspectiveCameraSetPosition(VLRPerspectiveCamera camera, const VLR::Point3D &position) {
    if (!camera->is<VLR::PerspectiveCamera>())
        return VLR_ERROR_INVALID_TYPE;
    camera->setPosition(position);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrPerspectiveCameraSetOrientation(VLRPerspectiveCamera camera, const VLR::Quaternion &orientation) {
    if (!camera->is<VLR::PerspectiveCamera>())
        return VLR_ERROR_INVALID_TYPE;
    camera->setOrientation(orientation);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrPerspectiveCameraSetSensitivity(VLRPerspectiveCamera camera, float sensitivity) {
    if (!camera->is<VLR::PerspectiveCamera>())
        return VLR_ERROR_INVALID_TYPE;
    camera->setSensitivity(sensitivity);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrPerspectiveCameraSetLensRadius(VLRPerspectiveCamera camera, float lensRadius) {
    if (!camera->is<VLR::PerspectiveCamera>())
        return VLR_ERROR_INVALID_TYPE;
    camera->setLensRadius(lensRadius);

    return VLR_ERROR_NO_ERROR;
}

VLR_API VLRResult vlrPerspectiveCameraSetObjectPlaneDistance(VLRPerspectiveCamera camera, float distance) {
    if (!camera->is<VLR::PerspectiveCamera>())
        return VLR_ERROR_INVALID_TYPE;
    camera->setObjectPlaneDistance(distance);

    return VLR_ERROR_NO_ERROR;
}
