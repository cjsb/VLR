#include "scene_private.h"

#include <random>

namespace VLR {
    static std::string readTxtFile(const std::string& filepath) {
        std::ifstream ifs;
        ifs.open(filepath, std::ios::in);
        if (ifs.fail())
            return "";

        std::stringstream sstream;
        sstream << ifs.rdbuf();

        return std::string(sstream.str());
    };



    Object::Object(Context &context) : m_context(context) {
    }
    
    
    
    uint32_t Context::NextID = 0;
    
    Context::Context() {
        m_ID = getInstanceID();

        m_optixContext = optix::Context::create();

        m_optixContext->setRayTypeCount(Shared::RayType::NumTypes);

        std::string ptx = readTxtFile("resources/ptxes/path_tracing.ptx");

        m_optixCallableProgramNullFetchAlpha = m_optixContext->createProgramFromPTXString(ptx, "VLR::Null_NormalAlphaModifier_fetchAlpha");
        m_optixCallableProgramNullFetchNormal = m_optixContext->createProgramFromPTXString(ptx, "VLR::Null_NormalAlphaModifier_fetchNormal");
        m_optixCallableProgramFetchAlpha = m_optixContext->createProgramFromPTXString(ptx, "VLR::NormalAlphaModifier_fetchAlpha");
        m_optixCallableProgramFetchNormal = m_optixContext->createProgramFromPTXString(ptx, "VLR::NormalAlphaModifier_fetchNormal");

        m_optixProgramStochasticAlphaAnyHit = m_optixContext->createProgramFromPTXString(ptx, "VLR::stochasticAlphaAnyHit");
        m_optixProgramAlphaAnyHit = m_optixContext->createProgramFromPTXString(ptx, "VLR::alphaAnyHit");
        m_optixProgramPathTracingIteration = m_optixContext->createProgramFromPTXString(ptx, "VLR::pathTracingIteration");

        m_optixProgramPathTracing = m_optixContext->createProgramFromPTXString(ptx, "VLR::pathTracing");
        m_optixProgramPathTracingMiss = m_optixContext->createProgramFromPTXString(ptx, "VLR::pathTracingMiss");
        m_optixProgramException = m_optixContext->createProgramFromPTXString(ptx, "VLR::exception");

        m_optixContext->setEntryPointCount(1);
        m_optixContext->setRayGenerationProgram(0, m_optixProgramPathTracing);
        m_optixContext->setMissProgram(0, m_optixProgramPathTracingMiss);
        m_optixContext->setExceptionProgram(0, m_optixProgramException);

        SurfaceNode::initialize(*this);
        SurfaceMaterial::initialize(*this);
        Camera::initialize(*this);

        m_optixContext->setPrintEnabled(true);
        m_optixContext->setPrintBufferSize(4096);
    }

    Context::~Context() {
        if (m_rngBuffer)
            m_rngBuffer->destroy();

        if (m_outputBuffer)
            m_outputBuffer->destroy();

        Camera::finalize(*this);
        SurfaceMaterial::finalize(*this);
        SurfaceNode::finalize(*this);

        m_optixProgramException->destroy();
        m_optixProgramPathTracingMiss->destroy();
        m_optixProgramPathTracing->destroy();

        m_optixProgramPathTracingIteration->destroy();
        m_optixProgramAlphaAnyHit->destroy();
        m_optixProgramStochasticAlphaAnyHit->destroy();

        m_optixCallableProgramFetchNormal->destroy();
        m_optixCallableProgramFetchAlpha->destroy();
        m_optixCallableProgramNullFetchNormal->destroy();
        m_optixCallableProgramNullFetchAlpha->destroy();

        m_optixContext->destroy();
    }

    void Context::bindOpenGLBuffer(uint32_t bufferID, uint32_t width, uint32_t height) {
        if (m_outputBuffer)
            m_outputBuffer->destroy();
        if (m_rngBuffer)
            m_rngBuffer->destroy();

        m_width = width;
        m_height = height;

        m_outputBuffer = m_optixContext->createBufferFromGLBO(RT_BUFFER_INPUT_OUTPUT, bufferID);
        m_outputBuffer->setFormat(RT_FORMAT_USER);
        m_outputBuffer->setElementSize(sizeof(RGBSpectrum));
        m_outputBuffer->setSize(m_width, m_height);
        {
            auto dstData = (RGBSpectrum*)m_outputBuffer->map();
            std::fill_n(dstData, m_width * m_height, RGBSpectrum::Zero());
            m_outputBuffer->unmap();
        }
        m_optixContext["VLR::pv_outputBuffer"]->set(m_outputBuffer);

        m_rngBuffer = m_optixContext->createBuffer(RT_BUFFER_INPUT_OUTPUT, RT_FORMAT_USER, m_width, m_height);
        m_rngBuffer->setElementSize(sizeof(uint64_t));
        {
            std::mt19937_64 rng(591842031321323413);

            auto dstData = (uint64_t*)m_rngBuffer->map();
            for (int y = 0; y < m_height; ++y) {
                for (int x = 0; x < m_width; ++x) {
                    dstData[y * m_width + x] = rng();
                }
            }
            m_rngBuffer->unmap();
        }
        m_optixContext["VLR::pv_rngBuffer"]->set(m_rngBuffer);
    }

    void Context::render(Scene &scene, Camera* camera) {
        SHGroup &shGroup = scene.getSHGroup();

        m_optixContext["VLR::pv_topGroup"]->set(shGroup.getOptiXObject());

        camera->set();

        m_optixContext->validate();

        m_optixContext->launch(0, m_width, m_height);
    }



    // ----------------------------------------------------------------
    // Shallow Hierarchy

    void SHGroup::addChild(SHTransform* transform) {
        TransformStatus status;
        status.hasGeometryDescendant = transform->hasGeometryDescendant();
        m_transforms[transform] = status;
        if (status.hasGeometryDescendant) {
            m_optixGroup->addChild(transform->getOptiXObject());
            m_optixAcceleration->markDirty();
            ++m_numValidTransforms;
        }
    }

    void SHGroup::addChild(SHGeometryGroup* geomGroup) {
        m_geometryGroups.insert(geomGroup);
        m_optixGroup->addChild(geomGroup->getOptiXObject());
        m_optixAcceleration->markDirty();
    }

    void SHGroup::removeChild(SHTransform* transform) {
        VLRAssert(m_transforms.count(transform), "transform 0x%p is not a child.", transform);
        const TransformStatus status = m_transforms.at(transform);
        m_transforms.erase(transform);
        if (status.hasGeometryDescendant) {
            m_optixGroup->removeChild(transform->getOptiXObject());
            m_optixAcceleration->markDirty();
            --m_numValidTransforms;
        }
    }

    void SHGroup::removeChild(SHGeometryGroup* geomGroup) {
        m_optixGroup->removeChild(geomGroup->getOptiXObject());
        m_geometryGroups.erase(geomGroup);
        m_optixAcceleration->markDirty();
    }

    void SHGroup::updateChild(SHTransform* transform) {
        VLRAssert(m_transforms.count(transform), "transform 0x%p is not a child.", transform);
        TransformStatus &status = m_transforms.at(transform);
        SHGeometryGroup* descendant;
        optix::Transform &optixTransform = transform->getOptiXObject();
        if (status.hasGeometryDescendant) {
            if (!transform->hasGeometryDescendant()) {
                m_optixGroup->removeChild(optixTransform);
                m_optixAcceleration->markDirty();
                status.hasGeometryDescendant = false;
                --m_numValidTransforms;
            }
        }
        else {
            if (transform->hasGeometryDescendant(&descendant)) {
                optixTransform->setChild(descendant->getOptiXObject());

                m_optixGroup->addChild(optixTransform);
                m_optixAcceleration->markDirty();
                status.hasGeometryDescendant = true;
                ++m_numValidTransforms;
            }
        }
    }

    void SHGroup::printOptiXHierarchy() {
        std::stack<RTobject> stackRTObjects;
        std::stack<RTobjecttype> stackRTObjectTypes;

        std::set<RTgroup> groupList;
        std::set<RTtransform> transformList;
        std::set<RTgeometrygroup> geometryGroupList;
        std::set<RTgeometryinstance> geometryInstanceList;

        stackRTObjects.push(m_optixGroup.get()->get());
        stackRTObjectTypes.push(RT_OBJECTTYPE_GROUP);
        while (!stackRTObjects.empty()) {
            RTobject object = stackRTObjects.top();
            RTobjecttype objType = stackRTObjectTypes.top();
            stackRTObjects.pop();
            stackRTObjectTypes.pop();

            VLRDebugPrintf("0x%p: ", object);

            switch (objType) {
            case RT_OBJECTTYPE_GROUP: {
                auto group = (RTgroup)object;
                VLRDebugPrintf("Group\n");

                groupList.insert(group);

                uint32_t numChildren;
                rtGroupGetChildCount(group, &numChildren);
                for (int i = numChildren - 1; i >= 0; --i) {
                    RTobject childObject = nullptr;
                    RTobjecttype childObjType;
                    rtGroupGetChild(group, i, &childObject);
                    rtGroupGetChildType(group, i, &childObjType);

                    VLRDebugPrintf("- %u: 0x%p\n", i, childObject);

                    stackRTObjects.push(childObject);
                    stackRTObjectTypes.push(childObjType);
                }

                break;
            }
            case RT_OBJECTTYPE_TRANSFORM: {
                auto transform = (RTtransform)object;
                VLRDebugPrintf("Transform\n");

                transformList.insert(transform);

                RTobject childObject = nullptr;
                RTobjecttype childObjType;
                rtTransformGetChild(transform, &childObject);
                rtTransformGetChildType(transform, &childObjType);

                VLRDebugPrintf("- 0x%p\n", childObject);

                stackRTObjects.push(childObject);
                stackRTObjectTypes.push(childObjType);

                break;
            }
            case RT_OBJECTTYPE_SELECTOR: {
                VLRAssert_NotImplemented();
                break;
            }
            case RT_OBJECTTYPE_GEOMETRY_GROUP: {
                auto geometryGroup = (RTgeometrygroup)object;
                VLRDebugPrintf("GeometryGroup\n");

                geometryGroupList.insert(geometryGroup);

                uint32_t numChildren;
                rtGeometryGroupGetChildCount(geometryGroup, &numChildren);
                for (int i = numChildren - 1; i >= 0; --i) {
                    RTgeometryinstance childObject = nullptr;
                    rtGeometryGroupGetChild(geometryGroup, i, &childObject);

                    VLRDebugPrintf("- %u: 0x%p\n", i, childObject);

                    stackRTObjects.push(childObject);
                    stackRTObjectTypes.push(RT_OBJECTTYPE_GEOMETRY_INSTANCE);
                }

                break;
            }
            case RT_OBJECTTYPE_GEOMETRY_INSTANCE: {
                auto geometryInstance = (RTgeometryinstance)object;
                VLRDebugPrintf("GeometryInstance\n");

                geometryInstanceList.insert(geometryInstance);

                break;
            }
            default:
                VLRDebugPrintf("\n");
                VLRAssert_ShouldNotBeCalled();
                break;
            }

            VLRDebugPrintf("\n");
        }



        VLRDebugPrintf("Groups:\n");
        for (auto group : groupList) {
            VLRDebugPrintf("  0x%p:\n", group);
            uint32_t numChildren;
            rtGroupGetChildCount(group, &numChildren);
            RTacceleration acceleration;
            rtGroupGetAcceleration(group, &acceleration);
            int32_t isDirty = 0;
            rtAccelerationIsDirty(acceleration, &isDirty);
            VLRDebugPrintf("  Status: %s\n", isDirty ? "dirty" : "");
            for (int i = 0; i < numChildren; ++i) {
                RTobject childObject = nullptr;
                rtGroupGetChild(group, i, &childObject);

                VLRDebugPrintf("  - %u: 0x%p\n", i, childObject);
            }
        }

        VLRDebugPrintf("Transforms:\n");
        for (auto transform : transformList) {
            VLRDebugPrintf("  0x%p:\n", transform);
            RTobject childObject = nullptr;
            rtTransformGetChild(transform, &childObject);
            float mat[16];
            float invMat[16];
            rtTransformGetMatrix(transform, true, mat, invMat);
            VLRDebugPrintf("    Matrix\n");
            VLRDebugPrintf("      %g, %g, %g, %g\n", mat[0], mat[4], mat[8], mat[12]);
            VLRDebugPrintf("      %g, %g, %g, %g\n", mat[1], mat[5], mat[9], mat[13]);
            VLRDebugPrintf("      %g, %g, %g, %g\n", mat[2], mat[6], mat[10], mat[14]);
            VLRDebugPrintf("      %g, %g, %g, %g\n", mat[3], mat[7], mat[11], mat[15]);
            VLRDebugPrintf("    Inverse Matrix\n");
            VLRDebugPrintf("      %g, %g, %g, %g\n", invMat[0], invMat[4], invMat[8], invMat[12]);
            VLRDebugPrintf("      %g, %g, %g, %g\n", invMat[1], invMat[5], invMat[9], invMat[13]);
            VLRDebugPrintf("      %g, %g, %g, %g\n", invMat[2], invMat[6], invMat[10], invMat[14]);
            VLRDebugPrintf("      %g, %g, %g, %g\n", invMat[3], invMat[7], invMat[11], invMat[15]);

            VLRDebugPrintf("  - 0x%p\n", childObject);
        }

        VLRDebugPrintf("GeometryGroups:\n");
        for (auto geometryGroup : geometryGroupList) {
            VLRDebugPrintf("  0x%p:\n", geometryGroup);
            uint32_t numChildren;
            rtGeometryGroupGetChildCount(geometryGroup, &numChildren);
            RTacceleration acceleration;
            rtGeometryGroupGetAcceleration(geometryGroup, &acceleration);
            int32_t isDirty = 0;
            rtAccelerationIsDirty(acceleration, &isDirty);
            VLRDebugPrintf("  Status: %s\n", isDirty ? "dirty" : "");
            for (int i = 0; i < numChildren; ++i) {
                RTgeometryinstance childObject = nullptr;
                rtGeometryGroupGetChild(geometryGroup, i, &childObject);

                VLRDebugPrintf("  - %u: 0x%p\n", i, childObject);
            }
        }

        VLRDebugPrintf("GeometryInstances:\n");
        for (auto geometryInstance : geometryInstanceList) {
            VLRDebugPrintf("  0x%p:\n", geometryInstance);
        }
    }



    void SHTransform::resolveTransform() {
        int32_t stackIdx = 0;
        const SHTransform* stack[5];
        std::fill_n(stack, lengthof(stack), nullptr);
        const SHTransform* nextSHTr = m_childIsTransform ? m_childTransform : nullptr;
        while (nextSHTr) {
            stack[stackIdx++] = nextSHTr;
            nextSHTr = nextSHTr->m_childIsTransform ? nextSHTr->m_childTransform : nullptr;
        }

        StaticTransform res;
        std::string concatenatedName = "";
        --stackIdx;
        while (stackIdx >= 0) {
            const SHTransform* shtr = stack[stackIdx--];
            res = shtr->m_transform * res;
            concatenatedName = "-" + shtr->getName() + concatenatedName;
        }
        res = m_transform * res;
        concatenatedName = m_name + concatenatedName;

        float mat[16], invMat[16];
        res.getArrays(mat, invMat);
        m_optixTransform->setMatrix(true, mat, invMat);

        //if (true/*m_parent*/) {
        //    VLRDebugPrintf("%s:\n", concatenatedName.c_str());
        //    VLRDebugPrintf("%g, %g, %g, %g\n", mat[0], mat[4], mat[8], mat[12]);
        //    VLRDebugPrintf("%g, %g, %g, %g\n", mat[1], mat[5], mat[9], mat[13]);
        //    VLRDebugPrintf("%g, %g, %g, %g\n", mat[2], mat[6], mat[10], mat[14]);
        //    VLRDebugPrintf("%g, %g, %g, %g\n", mat[3], mat[7], mat[11], mat[15]);
        //    VLRDebugPrintf("\n");
        //}
    }

    void SHTransform::setTransform(const StaticTransform &transform) {
        m_transform = transform;
        resolveTransform();
    }

    void SHTransform::update() {
        resolveTransform();
    }

    void SHTransform::setChild(SHGeometryGroup* geomGroup) {
        VLRAssert(!m_childIsTransform, "Transform which doesn't have a child transform can have a geometry group as a child.");
        m_childGeometryGroup = geomGroup;
    }

    bool SHTransform::hasGeometryDescendant(SHGeometryGroup** descendant) const {
        if (descendant)
            *descendant = nullptr;

        const SHTransform* nextSHTr = this;
        while (nextSHTr) {
            if (!nextSHTr->m_childIsTransform && nextSHTr->m_childGeometryGroup != nullptr) {
                if (descendant)
                    *descendant = nextSHTr->m_childGeometryGroup;
                return true;
            }
            else {
                nextSHTr = nextSHTr->m_childIsTransform ? nextSHTr->m_childTransform : nullptr;
            }
        }

        return false;
    }



    void SHGeometryGroup::addGeometryInstance(SHGeometryInstance* instance) {
        m_instances.insert(instance);
        m_optixGeometryGroup->addChild(instance->getOptiXObject());
        m_optixAcceleration->markDirty();
    }

    void SHGeometryGroup::removeGeometryInstance(SHGeometryInstance* instance) {
        m_instances.erase(instance);
        m_optixGeometryGroup->removeChild(instance->getOptiXObject());
        m_optixAcceleration->markDirty();
    }

    // END: Shallow Hierarchy
    // ----------------------------------------------------------------



    constexpr ObjectType::Value::Value(uint64_t v) : field0(v) {
    }

    bool ObjectType::Value::operator==(const Value &v) const {
        return field0 == v.field0;
    }

    ObjectType::Value ObjectType::Value::operator&(const Value &v) const {
        Value ret;
        ret.field0 = field0 & v.field0;
        return ret;
    }

    ObjectType::Value ObjectType::Value::operator|(const Value &v) const {
        Value ret;
        ret.field0 = field0 | v.field0;
        return ret;
    }
    
    const ObjectType ObjectType::E_Context                 = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0000'0001));
    const ObjectType ObjectType::E_Image2D                 = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0000'0010));
    const ObjectType ObjectType::E_LinearImage2D           = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0000'0110));
    const ObjectType ObjectType::E_FloatTexture            = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0000'1000));
    const ObjectType ObjectType::E_Float2Texture           = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0001'0000));
    const ObjectType ObjectType::E_Float3Texture           = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0010'0000));
    const ObjectType ObjectType::E_Float4Texture           = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'0100'0000));
    const ObjectType ObjectType::E_ConstantFloat4Texture   = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0000'1100'0000));
    const ObjectType ObjectType::E_ImageFloat4Texture      = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0001'0100'0000));
    const ObjectType ObjectType::E_SurfaceMaterial         = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0010'0000'0000));
    const ObjectType ObjectType::E_MatteSurfaceMaterial    = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'0110'0000'0000));
    const ObjectType ObjectType::E_UE4SurfaceMaterial      = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0000'1010'0000'0000));
    const ObjectType ObjectType::E_Node                    = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0001'0000'0000'0000));
    const ObjectType ObjectType::E_SurfaceNode             = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0011'0000'0000'0000));
    const ObjectType ObjectType::E_TriangleMeshSurfaceNode = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'0111'0000'0000'0000));
    const ObjectType ObjectType::E_ParentNode              = ObjectType(ObjectType::Value(0b0000'0000'0000'0000'1001'0000'0000'0000));
    const ObjectType ObjectType::E_InternalNode            = ObjectType(ObjectType::Value(0b0000'0000'0000'0001'1001'0000'0000'0000));
    const ObjectType ObjectType::E_RootNode                = ObjectType(ObjectType::Value(0b0000'0000'0000'0010'1001'0000'0000'0000));
    const ObjectType ObjectType::E_Scene                   = ObjectType(ObjectType::Value(0b0000'0000'0000'0100'0000'0000'0000'0000));
    const ObjectType ObjectType::E_Camera                  = ObjectType(ObjectType::Value(0b0000'0000'0000'1000'0000'0000'0000'0000));
    const ObjectType ObjectType::E_PerspectiveCamera       = ObjectType(ObjectType::Value(0b0000'0000'0001'1000'0000'0000'0000'0000));

    bool ObjectType::is(const ObjectType &v) const {
        return value == v.value;
    }

    bool ObjectType::isMemberOf(const ObjectType &v) const {
        return (value & v.value) == v.value;
    }
    
    
    
    const size_t sizesOfDataFormats[(uint32_t)DataFormat::Num] = {
        sizeof(RGB8x3),
        sizeof(RGB_8x4),
        sizeof(RGBA8x4),
        sizeof(RGBA16Fx4),
        sizeof(RGBA32Fx4),
        sizeof(Gray8),
    };

    DataFormat Image2D::getInternalFormat(DataFormat inputFormat) {
        switch (inputFormat) {
        case DataFormat::RGB8x3:
            return DataFormat::RGBA8x4;
        case DataFormat::RGB_8x4:
            return DataFormat::RGBA8x4;
        case DataFormat::RGBA8x4:
            return DataFormat::RGBA8x4;
        case DataFormat::RGBA16Fx4:
            return DataFormat::RGBA16Fx4;
        case DataFormat::RGBA32Fx4:
            return DataFormat::RGBA32Fx4;
        case DataFormat::Gray8:
            return DataFormat::Gray8;
        default:
            VLRAssert(false, "Data format is invalid.");
            break;
        }
        return DataFormat::RGBA8x4;
    }

    Image2D::Image2D(Context &context, uint32_t width, uint32_t height, DataFormat dataFormat) :
        Object(context), m_width(width), m_height(height), m_dataFormat(dataFormat) {
        optix::Context &optixContext = context.getOptiXContext();
        switch (m_dataFormat) {
        case VLR::DataFormat::RGB8x3:
            m_optixDataBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE3, width, height);
            break;
        case VLR::DataFormat::RGB_8x4:
            m_optixDataBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE4, width, height);
            break;
        case VLR::DataFormat::RGBA8x4:
            m_optixDataBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE4, width, height);
            break;
        case VLR::DataFormat::RGBA16Fx4:
            m_optixDataBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_HALF4, width, height);
            break;
        case VLR::DataFormat::RGBA32Fx4:
            m_optixDataBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, width, height);
            break;
        case VLR::DataFormat::Gray8:
            m_optixDataBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE, width, height);
            break;
        default:
            VLRAssert_ShouldNotBeCalled();
            break;
        }
    }

    Image2D::~Image2D() {
        m_optixDataBuffer->destroy();
    }



    LinearImage2D::LinearImage2D(Context &context, const uint8_t* linearData, uint32_t width, uint32_t height, DataFormat dataFormat) :
        Image2D(context, width, height, Image2D::getInternalFormat(dataFormat)) {
        m_data.resize(getStride() * getWidth() * getHeight());

        switch (dataFormat) {
        case DataFormat::RGB8x3: {
            auto srcHead = (const RGB8x3*)linearData;
            auto dstHead = (RGBA8x4*)m_data.data();
            for (int y = 0; y < height; ++y) {
                auto srcLineHead = srcHead + width * y;
                auto dstLineHead = dstHead + width * y;
                for (int x = 0; x < width; ++x) {
                    const RGB8x3 &src = *(srcLineHead + x);
                    RGBA8x4 &dst = *(dstLineHead + x);
                    dst.r = src.r;
                    dst.g = src.g;
                    dst.b = src.b;
                    dst.a = 255;
                }
            }
            break;
        }
        case DataFormat::RGB_8x4: {
            auto srcHead = (const RGB_8x4*)linearData;
            auto dstHead = (RGBA8x4*)m_data.data();
            for (int y = 0; y < height; ++y) {
                auto srcLineHead = srcHead + width * y;
                auto dstLineHead = dstHead + width * y;
                for (int x = 0; x < width; ++x) {
                    const RGB_8x4 &src = *(srcLineHead + x);
                    RGBA8x4 &dst = *(dstLineHead + x);
                    dst.r = src.r;
                    dst.g = src.g;
                    dst.b = src.b;
                    dst.a = 255;
                }
            }
            break;
        }
        case DataFormat::RGBA8x4: {
            auto srcHead = (const RGBA8x4*)linearData;
            auto dstHead = (RGBA8x4*)m_data.data();
            std::copy_n(srcHead, width * height, dstHead);
            break;
        }
        case DataFormat::RGBA16Fx4: {
            auto srcHead = (const RGBA16Fx4*)linearData;
            auto dstHead = (RGBA16Fx4*)m_data.data();
            std::copy_n(srcHead, width * height, dstHead);
            break;
        }
        case DataFormat::RGBA32Fx4: {
            auto srcHead = (const RGBA32Fx4*)linearData;
            auto dstHead = (RGBA32Fx4*)m_data.data();
            std::copy_n(srcHead, width * height, dstHead);
            break;
        }
        case DataFormat::Gray8: {
            auto srcHead = (const Gray8*)linearData;
            auto dstHead = (Gray8*)m_data.data();
            std::copy_n(srcHead, width * height, dstHead);
            break;
        }
        default:
            VLRAssert(false, "Data format is invalid.");
            break;
        }

        optix::Buffer &buffer = getOptiXObject();
        auto dstData = (uint8_t*)buffer->map();
        {
            std::copy(m_data.cbegin(), m_data.cend(), dstData);
        }
        buffer->unmap();
    }



    FloatTexture::FloatTexture(Context &context) : Object(context) {
        optix::Context &optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_LINEAR);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    FloatTexture::~FloatTexture() {
        m_optixTextureSampler->destroy();
    }



    Float2Texture::Float2Texture(Context &context) : Object(context) {
        optix::Context &optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_LINEAR);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    Float2Texture::~Float2Texture() {
        m_optixTextureSampler->destroy();
    }



    Float3Texture::Float3Texture(Context &context) : Object(context) {
        optix::Context &optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_LINEAR);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    Float3Texture::~Float3Texture() {
        m_optixTextureSampler->destroy();
    }



    Float4Texture::Float4Texture(Context &context) : Object(context) {
        optix::Context &optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_CLAMP_TO_EDGE);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_LINEAR);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    Float4Texture::~Float4Texture() {
        m_optixTextureSampler->destroy();
    }



    ConstantFloat4Texture::ConstantFloat4Texture(Context &context, const float value[4]) :
        Float4Texture(context) {
        m_image = new LinearImage2D(context, (const uint8_t*)value, 1, 1, DataFormat::RGBA32Fx4);
        m_optixTextureSampler->setBuffer(m_image->getOptiXObject());
    }

    ConstantFloat4Texture::~ConstantFloat4Texture() {
        delete m_image;
    }
    
    
    
    ImageFloat4Texture::ImageFloat4Texture(Context &context, Image2D* image) :
        Float4Texture(context), m_image(image) {
        m_optixTextureSampler->setBuffer(m_image->getOptiXObject());
    }



    // static
    void SurfaceMaterial::initialize(Context &context) {
        MatteSurfaceMaterial::initialize(context);
        UE4SurfaceMaterial::initialize(context);
    }

    // static
    void SurfaceMaterial::finalize(Context &context) {
        UE4SurfaceMaterial::finalize(context);
        MatteSurfaceMaterial::finalize(context);
    }

    SurfaceMaterial::SurfaceMaterial(Context &context) : Object(context) {
        optix::Context &optixContext = context.getOptiXContext();
        m_optixMaterial = optixContext->createMaterial();

        m_optixMaterial->setClosestHitProgram(Shared::RayType::Primary, context.getOptiXProgramPathTracingIteration());
        m_optixMaterial->setClosestHitProgram(Shared::RayType::Scattered, context.getOptiXProgramPathTracingIteration());
        m_optixMaterial->setAnyHitProgram(Shared::RayType::Primary, context.getOptiXProgramStochasticAlphaAnyHit());
        m_optixMaterial->setAnyHitProgram(Shared::RayType::Scattered, context.getOptiXProgramStochasticAlphaAnyHit());
        m_optixMaterial->setAnyHitProgram(Shared::RayType::Shadow, context.getOptiXProgramAlphaAnyHit());
    }



    std::map<uint32_t, MatteSurfaceMaterial::OptiXProgramSet> MatteSurfaceMaterial::OptiXProgramSets;

    // static
    void MatteSurfaceMaterial::initialize(Context &context) {
        std::string ptx = readTxtFile("resources/ptxes/materials.ptx");

        OptiXProgramSet programSet;

        optix::Context &optixContext = context.getOptiXContext();

        programSet.callableProgramSetup = optixContext->createProgramFromPTXString(ptx, "VLR::MatteSurfaceMaterial_setup");

        programSet.callableProgramGetBaseColor = optixContext->createProgramFromPTXString(ptx, "VLR::MatteBRDF_getBaseColor");

        programSet.callableProgramBSDFmatches = optixContext->createProgramFromPTXString(ptx, "VLR::MatteBRDF_matches");
        programSet.callableProgramSampleBSDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::MatteBRDF_sampleBSDFInternal");
        programSet.callableProgramEvaluateBSDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::MatteBRDF_evaluateBSDFInternal");
        programSet.callableProgramEvaluateBSDF_PDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::MatteBRDF_evaluateBSDF_PDFInternal");

        programSet.callableProgramEvaluateEmittanceInternal = optixContext->createProgramFromPTXString(ptx, "VLR::NullEDF_evaluateEmittanceInternal");
        programSet.callableProgramEvaluateEDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::NullEDF_evaluateEDFInternal");

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void MatteSurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());

        programSet.callableProgramEvaluateEDFInternal->destroy();
        programSet.callableProgramEvaluateEmittanceInternal->destroy();

        programSet.callableProgramEvaluateBSDF_PDFInternal->destroy();
        programSet.callableProgramEvaluateBSDFInternal->destroy();
        programSet.callableProgramSampleBSDFInternal->destroy();
        programSet.callableProgramBSDFmatches->destroy();

        programSet.callableProgramGetBaseColor->destroy();

        programSet.callableProgramSetup->destroy();

        OptiXProgramSets.erase(context.getID());
    }

    MatteSurfaceMaterial::MatteSurfaceMaterial(Context &context, Float4Texture* texAlbedoRoughness) :
        SurfaceMaterial(context), m_texAlbedoRoughness(texAlbedoRoughness) {
        OptiXProgramSet &progSet = OptiXProgramSets.at(context.getID());

        Shared::MaterialDescriptor matDesc;

        matDesc.progSetup = progSet.callableProgramSetup->getId();

        matDesc.progGetBaseColor = progSet.callableProgramGetBaseColor->getId();

        matDesc.progBSDFmatches = progSet.callableProgramBSDFmatches->getId();
        matDesc.progSampleBSDFInternal = progSet.callableProgramSampleBSDFInternal->getId();
        matDesc.progEvaluateBSDFInternal = progSet.callableProgramEvaluateBSDFInternal->getId();
        matDesc.progEvaluateBSDF_PDFInternal = progSet.callableProgramEvaluateBSDF_PDFInternal->getId();

        matDesc.progEvaluateEmittanceInternal = progSet.callableProgramEvaluateEmittanceInternal->getId();
        matDesc.progEvaluateEDFInternal = progSet.callableProgramEvaluateEDFInternal->getId();

        Shared::MatteSurfaceMaterial &mat = *(Shared::MatteSurfaceMaterial*)&matDesc;
        mat.texAlbedoRoughness = m_texAlbedoRoughness->getOptiXObject()->getId();

        m_optixMaterial["VLR::pv_materialDescriptor"]->setUserData(sizeof(Shared::MaterialDescriptor), &matDesc);
    }



    std::map<uint32_t, UE4SurfaceMaterial::OptiXProgramSet> UE4SurfaceMaterial::OptiXProgramSets;

    // static
    void UE4SurfaceMaterial::initialize(Context &context) {
        std::string ptx = readTxtFile("resources/ptxes/materials.ptx");

        OptiXProgramSet programSet;

        optix::Context &optixContext = context.getOptiXContext();

        programSet.callableProgramSetup = optixContext->createProgramFromPTXString(ptx, "VLR::UE4SurfaceMaterial_setup");

        programSet.callableProgramGetBaseColor = optixContext->createProgramFromPTXString(ptx, "VLR::UE4BRDF_getBaseColor");

        programSet.callableProgramBSDFmatches = optixContext->createProgramFromPTXString(ptx, "VLR::UE4BRDF_matches");
        programSet.callableProgramSampleBSDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::UE4BRDF_sampleBSDFInternal");
        programSet.callableProgramEvaluateBSDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::UE4BRDF_evaluateBSDFInternal");
        programSet.callableProgramEvaluateBSDF_PDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::UE4BRDF_evaluateBSDF_PDFInternal");

        programSet.callableProgramEvaluateEmittanceInternal = optixContext->createProgramFromPTXString(ptx, "VLR::NullEDF_evaluateEmittanceInternal");
        programSet.callableProgramEvaluateEDFInternal = optixContext->createProgramFromPTXString(ptx, "VLR::NullEDF_evaluateEDFInternal");

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void UE4SurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());

        programSet.callableProgramEvaluateEDFInternal->destroy();
        programSet.callableProgramEvaluateEmittanceInternal->destroy();

        programSet.callableProgramEvaluateBSDF_PDFInternal->destroy();
        programSet.callableProgramEvaluateBSDFInternal->destroy();
        programSet.callableProgramSampleBSDFInternal->destroy();
        programSet.callableProgramBSDFmatches->destroy();

        programSet.callableProgramGetBaseColor->destroy();

        programSet.callableProgramSetup->destroy();

        OptiXProgramSets.erase(context.getID());
    }

    UE4SurfaceMaterial::UE4SurfaceMaterial(Context &context, Float3Texture* texBaseColor, Float2Texture* texRoughnessMetallic) :
        SurfaceMaterial(context), m_texBaseColor(texBaseColor), m_texRoughnessMetallic(texRoughnessMetallic) {
        OptiXProgramSet &progSet = OptiXProgramSets.at(context.getID());

        Shared::MaterialDescriptor matDesc;

        matDesc.progSetup = progSet.callableProgramSetup->getId();

        matDesc.progGetBaseColor = progSet.callableProgramGetBaseColor->getId();

        matDesc.progBSDFmatches = progSet.callableProgramBSDFmatches->getId();
        matDesc.progSampleBSDFInternal = progSet.callableProgramSampleBSDFInternal->getId();
        matDesc.progEvaluateBSDFInternal = progSet.callableProgramEvaluateBSDFInternal->getId();
        matDesc.progEvaluateBSDF_PDFInternal = progSet.callableProgramEvaluateBSDF_PDFInternal->getId();

        matDesc.progEvaluateEmittanceInternal = progSet.callableProgramEvaluateEmittanceInternal->getId();
        matDesc.progEvaluateEDFInternal = progSet.callableProgramEvaluateEDFInternal->getId();

        Shared::UE4SurfaceMaterial &mat = *(Shared::UE4SurfaceMaterial*)&matDesc;
        mat.texBaseColor = m_texBaseColor->getOptiXObject()->getId();
        mat.texRoughnessMetallic = m_texRoughnessMetallic->getOptiXObject()->getId();

        m_optixMaterial["VLR::pv_materialDescriptor"]->setUserData(sizeof(Shared::MaterialDescriptor), &matDesc);
    }
    
    
    
    // static
    void SurfaceNode::initialize(Context &context) {
        TriangleMeshSurfaceNode::initialize(context);
    }

    // static
    void SurfaceNode::finalize(Context &context) {
        TriangleMeshSurfaceNode::finalize(context);
    }

    void SurfaceNode::addParent(ParentNode* parent) {
        VLRAssert(parent != nullptr, "parent must be not null.");
        m_parents.insert(parent);
    }

    void SurfaceNode::removeParent(ParentNode* parent) {
        VLRAssert(parent != nullptr, "parent must be not null.");
        m_parents.erase(parent);
    }



    std::map<uint32_t, TriangleMeshSurfaceNode::OptiXProgramSet> TriangleMeshSurfaceNode::OptiXProgramSets;

    // static
    void TriangleMeshSurfaceNode::initialize(Context &context) {
        std::string ptx = readTxtFile("resources/ptxes/triangle_intersection.ptx");

        OptiXProgramSet programSet;

        optix::Context &optixContext = context.getOptiXContext();

        programSet.programIntersectTriangle = optixContext->createProgramFromPTXString(ptx, "VLR::intersectTriangle");
        programSet.programCalcBBoxForTriangle = optixContext->createProgramFromPTXString(ptx, "VLR::calcBBoxForTriangle");

        programSet.callableProgramDecodeHitPointForTriangle = optixContext->createProgramFromPTXString(ptx, "VLR::decodeHitPointForTriangle");
        programSet.callableProgramDecodeTexCoordForTriangle = optixContext->createProgramFromPTXString(ptx, "VLR::decodeTexCoordForTriangle");

        programSet.callableProgramSampleTriangleMesh = optixContext->createProgramFromPTXString(ptx, "VLR::sampleTriangleMesh");

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void TriangleMeshSurfaceNode::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());

        programSet.callableProgramSampleTriangleMesh->destroy();

        programSet.callableProgramDecodeTexCoordForTriangle->destroy();
        programSet.callableProgramDecodeHitPointForTriangle->destroy();

        programSet.programCalcBBoxForTriangle->destroy();
        programSet.programIntersectTriangle->destroy();

        OptiXProgramSets.erase(context.getID());
    }

    TriangleMeshSurfaceNode::TriangleMeshSurfaceNode(Context &context, const std::string &name) : SurfaceNode(context, name) {
    }

    TriangleMeshSurfaceNode::~TriangleMeshSurfaceNode() {
        for (auto it = m_shGeometryInstances.crbegin(); it != m_shGeometryInstances.crend(); ++it)
            delete *it;
        m_shGeometryInstances.clear();

        for (auto it = m_optixGeometries.begin(); it != m_optixGeometries.end(); ++it) {
            OptiXGeometry &geom = *it;
            geom.optixGeometry->destroy();
            geom.optixIndexBuffer->destroy();
        }
        m_optixVertexBuffer->destroy();
    }

    void TriangleMeshSurfaceNode::addParent(ParentNode* parent) {
        SurfaceNode::addParent(parent);

        // JP: �ǉ������e�ɑ΂��ăW�I���g���C���X�^���X�̒ǉ����s�킹��B
        std::set<SHGeometryInstance*> delta;
        for (auto it = m_shGeometryInstances.cbegin(); it != m_shGeometryInstances.cend(); ++it)
            delta.insert(*it);
        parent->childUpdateEvent(ParentNode::UpdateEvent::GeometryAdded, delta);
    }

    void TriangleMeshSurfaceNode::removeParent(ParentNode* parent) {
        SurfaceNode::removeParent(parent);

        // JP: �폜�����e�ɑ΂��ăW�I���g���C���X�^���X�̍폜���s�킹��B
        std::set<SHGeometryInstance*> delta;
        for (auto it = m_shGeometryInstances.cbegin(); it != m_shGeometryInstances.cend(); ++it)
            delta.insert(*it);
        parent->childUpdateEvent(ParentNode::UpdateEvent::GeometryRemoved, delta);
    }

    void TriangleMeshSurfaceNode::setVertices(std::vector<Vertex> &&vertices) {
        m_vertices = vertices;

        optix::Context &optixContext = m_context.getOptiXContext();
        m_optixVertexBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER, m_vertices.size());
        m_optixVertexBuffer->setElementSize(sizeof(Shared::Vertex));
        {
            auto dstVertices = (Shared::Vertex*)m_optixVertexBuffer->map();
            static_assert(sizeof(Vertex) == sizeof(Shared::Vertex), "These two types must match in size (and structure).");
            std::copy_n((Shared::Vertex*)m_vertices.data(), m_vertices.size(), dstVertices);
            m_optixVertexBuffer->unmap();
        }

        // TODO: ���_���X�V���̏����B(IndexBuffer�Ƃ̐������Ȃ�)
    }

    void TriangleMeshSurfaceNode::addMaterialGroup(std::vector<uint32_t> &&indices, SurfaceMaterial* material) {
        optix::Context &optixContext = m_context.getOptiXContext();
        const OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        m_sameMaterialGroups.emplace_back(indices);
        std::vector<uint32_t> &sameMaterialGroup = m_sameMaterialGroups.back();
        uint32_t numTriangles = sameMaterialGroup.size() / 3;

        OptiXGeometry geom;
        {
            geom.optixGeometry = optixContext->createGeometry();
            geom.optixGeometry->setPrimitiveCount(numTriangles);
            geom.optixGeometry->setIntersectionProgram(progSet.programIntersectTriangle);
            geom.optixGeometry->setBoundingBoxProgram(progSet.programCalcBBoxForTriangle);

            geom.optixIndexBuffer = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER, numTriangles);
            geom.optixIndexBuffer->setElementSize(sizeof(Shared::Triangle));
            {
                auto dstTriangles = (Shared::Triangle*)geom.optixIndexBuffer->map();
                for (auto it = sameMaterialGroup.cbegin(); it != sameMaterialGroup.cend(); it += 3)
                    (*dstTriangles++) = Shared::Triangle{ *(it + 0), *(it + 1) , *(it + 2), 0 };
                geom.optixIndexBuffer->unmap();
            }
        }
        m_optixGeometries.push_back(geom);

        m_materials.push_back(material);

        SHGeometryInstance* geomInst = new SHGeometryInstance(m_context);
        {
            optix::GeometryInstance &optixGeomInst = geomInst->getOptiXObject();
            optixGeomInst->setGeometry(geom.optixGeometry);
            optixGeomInst->setMaterialCount(1);
            optixGeomInst->setMaterial(0, material->getOptiXObject());
            optixGeomInst["VLR::pv_vertexBuffer"]->set(m_optixVertexBuffer);
            optixGeomInst["VLR::pv_triangleBuffer"]->set(geom.optixIndexBuffer);
            optixGeomInst["VLR::pv_progDecodeTexCoord"]->set(progSet.callableProgramDecodeTexCoordForTriangle);
            optixGeomInst["VLR::pv_progDecodeHitPoint"]->set(progSet.callableProgramDecodeHitPointForTriangle);
            optixGeomInst["VLR::pv_progFetchAlpha"]->set(m_context.getOptiXCallableProgramNullFetchAlpha());
            optixGeomInst["VLR::pv_progFetchNormal"]->set(m_context.getOptiXCallableProgramNullFetchNormal());
        }
        m_shGeometryInstances.push_back(geomInst);

        // JP: �e�ɃW�I���g���C���X�^���X�̒ǉ����s�킹��B
        std::set<SHGeometryInstance*> delta;
        delta.insert(geomInst);
        for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
            ParentNode* parent = *it;
            parent->childUpdateEvent(ParentNode::UpdateEvent::GeometryAdded, delta);
        }
    }
    
    
    
    ParentNode::ParentNode(Context &context, const std::string &name, const Transform* localToWorld) :
        Node(context, name), m_localToWorld(localToWorld), m_shGeomGroup(context) {
        // JP: �������g��Transform��������SHTransform�𐶐��B
        // EN: 
        if (m_localToWorld->isStatic()) {
            StaticTransform* tr = (StaticTransform*)m_localToWorld;
            m_shTransforms[nullptr] = new SHTransform(name, m_context, *tr, nullptr);
        }
        else {
            VLRAssert_NotImplemented();
        }
    }

    ParentNode::~ParentNode() {
        for (auto it = m_shTransforms.crbegin(); it != m_shTransforms.crend(); ++it)
            delete it->second;
        m_shTransforms.clear();
    }
    
    void ParentNode::addChild(InternalNode* child) {
        m_children.insert(child);
        child->addParent(this);
    }

    void ParentNode::addChild(SurfaceNode* child) {
        m_children.insert(child);
        child->addParent(this);
    }

    void ParentNode::removeChild(InternalNode* child) {
        m_children.erase(child);
        child->removeParent(this);
    }

    void ParentNode::removeChild(SurfaceNode* child) {
        m_children.erase(child);
        child->removeParent(this);
    }

    void ParentNode::setTransform(const Transform* localToWorld) {
        m_localToWorld = localToWorld;

        // JP: �Ǘ�����SHTransform���X�V����B
        for (auto it = m_shTransforms.cbegin(); it != m_shTransforms.cend(); ++it) {
            if (m_localToWorld->isStatic()) {
                StaticTransform* tr = (StaticTransform*)m_localToWorld;
                SHTransform* shtr = it->second;
                shtr->setTransform(*tr);
            }
            else {
                VLRAssert_NotImplemented();
            }
        }
    }



    void InternalNode::childUpdateEvent(UpdateEvent eventType, const std::set<SHTransform*>& childDelta) {
        switch (eventType) {
        case UpdateEvent::TransformAdded: {
            // JP: �������g��Transform�ƎqInternalNode������SHTransform���q����SHTransform�𐶐��B
            //     �q��SHTransform���L�[�Ƃ��Ď����ɕۑ�����B
            std::set<SHTransform*> delta;
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                if (m_localToWorld->isStatic()) {
                    StaticTransform* tr = (StaticTransform*)m_localToWorld;
                    SHTransform* shtr = new SHTransform(m_name, m_context, *tr, *it);
                    m_shTransforms[*it] = shtr;
                    delta.insert(shtr);
                }
                else {
                    VLRAssert_NotImplemented();
                }
            }

            // JP: �e�Ɏ������ێ�����SHTransform�����������Ƃ�ʒm(������ʒm)�B
            for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
                auto parent = *it;
                parent->childUpdateEvent(eventType, delta);
            }

            break;
        }
        case UpdateEvent::TransformRemoved: {
            // JP: �qInternalNode������SHTransform���q�����Ă���SHTransform���폜�B
            std::set<SHTransform*> delta;
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                SHTransform* shtr = m_shTransforms.at(*it);
                m_shTransforms.erase(*it);
                delta.insert(shtr);
            }

            // JP: �e�Ɏ������ێ�����SHTransform�����������Ƃ�ʒm(������ʒm)�B
            for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
                auto parent = *it;
                parent->childUpdateEvent(eventType, delta);
            }

            for (auto it = delta.cbegin(); it != delta.cend(); ++it)
                delete *it;

            break;
        }
        case UpdateEvent::TransformUpdated: {
            // JP: �qInternalNode������SHTransform���q�����Ă���SHTransform���X�V����B
            std::set<SHTransform*> delta;
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                SHTransform* shtr = m_shTransforms.at(*it);
                shtr->update();
                delta.insert(shtr);
            }

            // JP: �e�Ɏ������ێ�����SHTransform���X�V���ꂽ���Ƃ�ʒm(�X�V����ʒm)�B
            for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
                auto parent = *it;
                parent->childUpdateEvent(eventType, delta);
            }

            break;
        }
        case UpdateEvent::GeometryAdded:
        case UpdateEvent::GeometryRemoved: {
            std::set<SHTransform*> delta;
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                SHTransform* shtr = m_shTransforms.at(*it);
                delta.insert(shtr);
            }

            // JP: �e�Ɏ������ێ�����SHTransform���X�V���ꂽ���Ƃ�ʒm(�X�V����ʒm)�B
            for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
                auto parent = *it;
                parent->childUpdateEvent(eventType, delta);
            }

            break;
        }
        default:
            VLRAssert_ShouldNotBeCalled();
            break;
        }
    }

    void InternalNode::childUpdateEvent(UpdateEvent eventType, const std::set<SHGeometryInstance*> &childDelta) {
        switch (eventType) {
        case UpdateEvent::GeometryAdded: {
            // JP: 
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it)
                m_shGeomGroup.addGeometryInstance(*it);

            if (m_shGeomGroup.getNumInstances() > 0) {
                SHTransform* selfTransform = m_shTransforms.at(nullptr);
                selfTransform->setChild(&m_shGeomGroup);
                
                std::set<SHTransform*> delta;
                delta.insert(selfTransform);
                for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
                    ParentNode* parent = *it;
                    parent->childUpdateEvent(eventType, delta);
                }
            }

            break;
        }
        case UpdateEvent::GeometryRemoved: {
            // JP: 
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it)
                m_shGeomGroup.removeGeometryInstance(*it);

            if (m_shGeomGroup.getNumInstances() == 0) {
                SHTransform* selfTransform = m_shTransforms.at(nullptr);
                selfTransform->setChild(nullptr);

                std::set<SHTransform*> delta;
                delta.insert(selfTransform);
                for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
                    ParentNode* parent = *it;
                    parent->childUpdateEvent(eventType, delta);
                }
            }

            break;
        }
        default:
            VLRAssert_ShouldNotBeCalled();
            break;
        }
    }

    InternalNode::InternalNode(Context &context, const std::string &name, const Transform* localToWorld) :
        ParentNode(context, name, localToWorld) {
    }

    void InternalNode::setTransform(const Transform* localToWorld) {
        ParentNode::setTransform(localToWorld);

        // JP: �e�ɕό`��񂪍X�V���ꂽ���Ƃ�ʒm����B
        std::set<SHTransform*> delta;
        for (auto it = m_shTransforms.cbegin(); it != m_shTransforms.cend(); ++it)
            delta.insert(it->second);
        for (auto it = m_parents.cbegin(); it != m_parents.cend(); ++it) {
            ParentNode* parent = *it;
            parent->childUpdateEvent(UpdateEvent::TransformUpdated, delta);
        }
    }

    void InternalNode::addParent(ParentNode* parent) {
        VLRAssert(parent != nullptr, "parent must be not null.");
        m_parents.insert(parent);

        // JP: �ǉ������e�ɑ΂��ĕό`���̒ǉ����s�킹��B
        std::set<SHTransform*> delta;
        for (auto it = m_shTransforms.cbegin(); it != m_shTransforms.cend(); ++it)
            delta.insert(it->second);
        parent->childUpdateEvent(UpdateEvent::TransformAdded, delta);
    }

    void InternalNode::removeParent(ParentNode* parent) {
        VLRAssert(parent != nullptr, "parent must be not null.");
        m_parents.erase(parent);

        // JP: �폜�����e�ɑ΂��ĕό`���̍폜���s�킹��B
        std::set<SHTransform*> delta;
        for (auto it = m_shTransforms.cbegin(); it != m_shTransforms.cend(); ++it)
            delta.insert(it->second);
        parent->childUpdateEvent(UpdateEvent::TransformRemoved, delta);
    }



    void RootNode::childUpdateEvent(UpdateEvent eventType, const std::set<SHTransform*>& childDelta) {
        switch (eventType) {
        case UpdateEvent::TransformAdded: {
            // JP: �������g��Transform�ƎqInternalNode������SHTransform���q����SHTransform�𐶐��B
            //     �q��SHTransform���L�[�Ƃ��Ď����ɕۑ�����B
            std::set<SHTransform*> delta;
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                if (m_localToWorld->isStatic()) {
                    StaticTransform* tr = (StaticTransform*)m_localToWorld;
                    SHTransform* shtr = new SHTransform(m_name, m_context, *tr, *it);
                    m_shTransforms[*it] = shtr;
                    delta.insert(shtr);
                }
                else {
                    VLRAssert_NotImplemented();
                }
            }

            // JP: SHGroup�ɂ�SHTransform��ǉ�����B
            for (auto it = delta.cbegin(); it != delta.cend(); ++it) {
                SHTransform* shtr = *it;
                m_shGroup.addChild(shtr);
            }

            break;
        }
        case UpdateEvent::TransformRemoved: {
            // JP: �qInternalNode������SHTransform���Ȃ����Ă���SHTransform���폜�B
            std::set<SHTransform*> delta;
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                SHTransform* shtr = m_shTransforms.at(*it);
                m_shTransforms.erase(*it);
                delta.insert(shtr);
            }

            // JP: SHGroup�����SHTransform���폜����B
            for (auto it = delta.cbegin(); it != delta.cend(); ++it) {
                SHTransform* shtr = *it;
                m_shGroup.removeChild(shtr);
            }

            for (auto it = delta.cbegin(); it != delta.cend(); ++it)
                delete *it;

            break;
        }
        case UpdateEvent::TransformUpdated: {
            // JP: �qInternalNode������SHTransform���q�����Ă���SHTransform���X�V����B
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                SHTransform* shtr = m_shTransforms.at(*it);
                shtr->update();
            }

            break;
        }
        case UpdateEvent::GeometryAdded:
        case UpdateEvent::GeometryRemoved: {
            // JP: SHGroup�ɑ΂���SHTransform�̖����̃W�I���g����Ԃɕω������������Ƃ�ʒm����B
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it) {
                SHTransform* shtr = m_shTransforms.at(*it);
                m_shGroup.updateChild(shtr);
            }

            break;
        }
        default:
            VLRAssert_ShouldNotBeCalled();
            break;
        }
    }

    void RootNode::childUpdateEvent(UpdateEvent eventType, const std::set<SHGeometryInstance*> &childDelta) {
        switch (eventType) {
        case UpdateEvent::GeometryAdded: {
            // JP: 
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it)
                m_shGeomGroup.addGeometryInstance(*it);

            if (m_shGeomGroup.getNumInstances() > 0) {
                SHTransform* selfTransform = m_shTransforms.at(nullptr);
                selfTransform->setChild(&m_shGeomGroup);
                m_shGroup.updateChild(selfTransform);
            }

            break;
        }
        case UpdateEvent::GeometryRemoved: {
            // JP: 
            for (auto it = childDelta.cbegin(); it != childDelta.cend(); ++it)
                m_shGeomGroup.removeGeometryInstance(*it);

            if (m_shGeomGroup.getNumInstances() == 0) {
                SHTransform* selfTransform = m_shTransforms.at(nullptr);
                selfTransform->setChild(nullptr);
                m_shGroup.updateChild(selfTransform);
            }

            break;
        }
        default:
            VLRAssert_ShouldNotBeCalled();
            break;
        }
    }

    RootNode::RootNode(Context &context, const Transform* localToWorld) :
        ParentNode(context, "Root", localToWorld), m_shGroup(context) {
        SHTransform* shtr = m_shTransforms[0];
        m_shGroup.addChild(shtr);
    }



    Scene::Scene(Context &context, const Transform* localToWorld) : 
    Object(context), m_rootNode(context, localToWorld) {

    }



    // static
    void Camera::initialize(Context &context) {
        PerspectiveCamera::initialize(context);
    }

    // static
    void Camera::finalize(Context &context) {
        PerspectiveCamera::finalize(context);
    }
    
    
    
    std::map<uint32_t, PerspectiveCamera::OptiXProgramSet> PerspectiveCamera::OptiXProgramSets;
    
    // static
    void PerspectiveCamera::initialize(Context &context) {
        std::string ptx = readTxtFile("resources/ptxes/cameras.ptx");

        OptiXProgramSet programSet;

        optix::Context &optixContext = context.getOptiXContext();

        programSet.callableProgramSampleLensPosition = optixContext->createProgramFromPTXString(ptx, "VLR::PerspectiveCamera_sampleLensPosition");
        programSet.callableProgramSampleIDF = optixContext->createProgramFromPTXString(ptx, "VLR::PerspectiveCamera_sampleIDF");

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void PerspectiveCamera::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());

        programSet.callableProgramSampleIDF->destroy();
        programSet.callableProgramSampleLensPosition->destroy();

        OptiXProgramSets.erase(context.getID());
    }
    
    PerspectiveCamera::PerspectiveCamera(Context &context, const Point3D &position, const Quaternion &orientation,
                                         float aspect, float fovY, float lensRadius, float imgPDist, float objPDist) :
        Camera(context), m_data(aspect, fovY, lensRadius, imgPDist, objPDist) {
        m_data.position = position;
        m_data.orientation = orientation;
    }

    void PerspectiveCamera::set() const {
        optix::Context &optixContext = m_context.getOptiXContext();
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        optixContext["VLR::pv_perspectiveCamera"]->setUserData(sizeof(Shared::PerspectiveCamera), &m_data);
        optixContext["VLR::pv_progSampleLensPosition"]->set(progSet.callableProgramSampleLensPosition);
        optixContext["VLR::pv_progSampleIDF"]->set(progSet.callableProgramSampleIDF);
    }
}
