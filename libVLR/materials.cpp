﻿#include "materials.h"

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



    // ----------------------------------------------------------------
    // Miscellaneous

    template <typename RealType>
    static optix::Buffer createBuffer(optix::Context &context, RTbuffertype type, RTsize width);

    template <>
    static optix::Buffer createBuffer<float>(optix::Context &context, RTbuffertype type, RTsize width) {
        return context->createBuffer(type, RT_FORMAT_FLOAT, width);
    }



    template <typename RealType>
    void DiscreteDistribution1DTemplate<RealType>::initialize(Context &context, const RealType* values, size_t numValues) {
        optix::Context optixContext = context.getOptiXContext();

        m_numValues = (uint32_t)numValues;
        m_PMF = createBuffer<RealType>(optixContext, RT_BUFFER_INPUT, m_numValues);
        m_CDF = createBuffer<RealType>(optixContext, RT_BUFFER_INPUT, m_numValues + 1);

        RealType* PMF = (RealType*)m_PMF->map();
        RealType* CDF = (RealType*)m_CDF->map();
        std::memcpy(PMF, values, sizeof(RealType) * m_numValues);

        CompensatedSum<RealType> sum(0);
        CDF[0] = 0;
        for (int i = 0; i < m_numValues; ++i) {
            sum += PMF[i];
            CDF[i + 1] = sum;
        }
        m_integral = sum;
        for (int i = 0; i < m_numValues; ++i) {
            PMF[i] /= m_integral;
            CDF[i + 1] /= m_integral;
        }

        m_CDF->unmap();
        m_PMF->unmap();
    }

    template <typename RealType>
    void DiscreteDistribution1DTemplate<RealType>::finalize(Context &context) {
        if (m_CDF && m_PMF) {
            m_CDF->destroy();
            m_PMF->destroy();
        }
    }

    template <typename RealType>
    void DiscreteDistribution1DTemplate<RealType>::getInternalType(Shared::DiscreteDistribution1DTemplate<RealType>* instance) {
        if (m_PMF && m_CDF)
            new (instance) Shared::DiscreteDistribution1DTemplate<RealType>(m_PMF->getId(), m_CDF->getId(), m_integral, m_numValues);
    }

    template class DiscreteDistribution1DTemplate<float>;



    template <typename RealType>
    void RegularConstantContinuousDistribution1DTemplate<RealType>::initialize(Context &context, const RealType* values, size_t numValues) {
        optix::Context optixContext = context.getOptiXContext();

        m_numValues = (uint32_t)numValues;
        m_PDF = createBuffer<RealType>(optixContext, RT_BUFFER_INPUT, m_numValues);
        m_CDF = createBuffer<RealType>(optixContext, RT_BUFFER_INPUT, m_numValues + 1);

        RealType* PDF = (RealType*)m_PDF->map();
        RealType* CDF = (RealType*)m_CDF->map();
        std::memcpy(PDF, values, sizeof(RealType) * m_numValues);

        CompensatedSum<RealType> sum{ 0 };
        CDF[0] = 0;
        for (int i = 0; i < m_numValues; ++i) {
            sum += PDF[i] / m_numValues;
            CDF[i + 1] = sum;
        }
        m_integral = sum;
        for (int i = 0; i < m_numValues; ++i) {
            PDF[i] /= sum;
            CDF[i + 1] /= sum;
        }

        m_CDF->unmap();
        m_PDF->unmap();
    }

    template <typename RealType>
    void RegularConstantContinuousDistribution1DTemplate<RealType>::finalize(Context &context) {
        m_CDF->destroy();
        m_PDF->destroy();
    }

    template <typename RealType>
    void RegularConstantContinuousDistribution1DTemplate<RealType>::getInternalType(Shared::RegularConstantContinuousDistribution1DTemplate<RealType>* instance) {
        new (instance) Shared::RegularConstantContinuousDistribution1DTemplate<RealType>(m_PDF->getId(), m_CDF->getId(), m_integral, m_numValues);
    }

    template class RegularConstantContinuousDistribution1DTemplate<float>;



    template <typename RealType>
    void RegularConstantContinuousDistribution2DTemplate<RealType>::initialize(Context &context, const RealType* values, size_t numD1, size_t numD2) {
        optix::Context optixContext = context.getOptiXContext();

        m_num1DDists = numD2;

        // JP: まず各行に関するDistribution1Dを作成する。
        // EN: First, create Distribution1D's for every rows.
        m_1DDists = optixContext->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER, m_num1DDists);
        m_1DDists->setElementSize(sizeof(RegularConstantContinuousDistribution1DTemplate<RealType>));

        RegularConstantContinuousDistribution1DTemplate<RealType>* dists = (RegularConstantContinuousDistribution1DTemplate<RealType>*)m_1DDists->map();

        CompensatedSum<RealType> sum(0);
        for (int i = 0; i < m_num1DDists; ++i) {
            dists[i].initialize(context, values + i * numD1, numD1);
            sum += dists[i].getIntegral();
        }
        m_integral = sum;

        // JP: 各行の積分値を用いてDistribution1Dを作成する。
        // EN: create a Distribution1D using integral values of each row.
        RealType* integrals = new RealType[m_num1DDists];
        for (int i = 0; i < m_num1DDists; ++i)
            integrals[i] = dists[i].getIntegral();
        m_top1DDist.initialize(context, integrals, m_num1DDists);
        delete[] integrals;

        VLRAssert(std::isfinite(m_integral), "invalid integral value.");

        m_1DDists->unmap();
    }

    template <typename RealType>
    void RegularConstantContinuousDistribution2DTemplate<RealType>::finalize(Context &context) {
        m_top1DDist.finalize(context);

        RegularConstantContinuousDistribution1DTemplate<RealType>* dists = (RegularConstantContinuousDistribution1DTemplate<RealType>*)m_1DDists->map();
        for (int i = m_num1DDists - 1; i >= 0; --i) {
            dists[i].finalize(context);
        }
        m_1DDists->unmap();

        m_1DDists->destroy();
    }

    template <typename RealType>
    void RegularConstantContinuousDistribution2DTemplate<RealType>::getInternalType(Shared::RegularConstantContinuousDistribution2DTemplate<RealType>* instance) {
        Shared::RegularConstantContinuousDistribution1DTemplate<RealType> top1DDist;
        m_top1DDist.getInternalType(&top1DDist);
        new (instance) Shared::RegularConstantContinuousDistribution2DTemplate<RealType>(m_1DDists->getId(), m_num1DDists, m_integral, top1DDist);
    }

    template class RegularConstantContinuousDistribution2DTemplate<float>;

    // END: Miscellaneous
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // Material

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
        optix::Context optixContext = context.getOptiXContext();
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

        optix::Buffer buffer = getOptiXObject();
        auto dstData = (uint8_t*)buffer->map();
        {
            std::copy(m_data.cbegin(), m_data.cend(), dstData);
        }
        buffer->unmap();
    }



    FloatTexture::FloatTexture(Context &context) : Object(context) {
        optix::Context optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_REPEAT);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_REPEAT);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    FloatTexture::~FloatTexture() {
        m_optixTextureSampler->destroy();
    }

    void FloatTexture::setTextureFilterMode(TextureFilter minification, TextureFilter magnification, TextureFilter mipmapping) {
        m_optixTextureSampler->setFilteringModes((RTfiltermode)minification, (RTfiltermode)magnification, (RTfiltermode)mipmapping);
    }



    Float2Texture::Float2Texture(Context &context) : Object(context) {
        optix::Context optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_REPEAT);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_REPEAT);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    Float2Texture::~Float2Texture() {
        m_optixTextureSampler->destroy();
    }

    void Float2Texture::setTextureFilterMode(TextureFilter minification, TextureFilter magnification, TextureFilter mipmapping) {
        m_optixTextureSampler->setFilteringModes((RTfiltermode)minification, (RTfiltermode)magnification, (RTfiltermode)mipmapping);
    }



    Float3Texture::Float3Texture(Context &context) : Object(context) {
        optix::Context optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_REPEAT);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_REPEAT);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    Float3Texture::~Float3Texture() {
        m_optixTextureSampler->destroy();
    }

    void Float3Texture::setTextureFilterMode(TextureFilter minification, TextureFilter magnification, TextureFilter mipmapping) {
        m_optixTextureSampler->setFilteringModes((RTfiltermode)minification, (RTfiltermode)magnification, (RTfiltermode)mipmapping);
    }



    ConstantFloat3Texture::ConstantFloat3Texture(Context &context, const float value[3]) :
        Float3Texture(context) {
        float value4[] = { value[0], value[1], value[2], 0 };
        m_image = new LinearImage2D(context, (const uint8_t*)value4, 1, 1, DataFormat::RGBA32Fx4);
        m_optixTextureSampler->setBuffer(m_image->getOptiXObject());
    }

    ConstantFloat3Texture::~ConstantFloat3Texture() {
        delete m_image;
    }



    ImageFloat3Texture::ImageFloat3Texture(Context &context, const Image2D* image) :
        Float3Texture(context), m_image(image) {
        m_optixTextureSampler->setBuffer(m_image->getOptiXObject());
    }



    Float4Texture::Float4Texture(Context &context) : Object(context) {
        optix::Context optixContext = context.getOptiXContext();
        m_optixTextureSampler = optixContext->createTextureSampler();
        m_optixTextureSampler->setWrapMode(0, RT_WRAP_REPEAT);
        m_optixTextureSampler->setWrapMode(1, RT_WRAP_REPEAT);
        m_optixTextureSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
        m_optixTextureSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
        m_optixTextureSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
        m_optixTextureSampler->setMaxAnisotropy(1.0f);
    }

    Float4Texture::~Float4Texture() {
        m_optixTextureSampler->destroy();
    }

    void Float4Texture::setTextureFilterMode(TextureFilter minification, TextureFilter magnification, TextureFilter mipmapping) {
        m_optixTextureSampler->setFilteringModes((RTfiltermode)minification, (RTfiltermode)magnification, (RTfiltermode)mipmapping);
    }



    ConstantFloat4Texture::ConstantFloat4Texture(Context &context, const float value[4]) :
        Float4Texture(context) {
        m_image = new LinearImage2D(context, (const uint8_t*)value, 1, 1, DataFormat::RGBA32Fx4);
        m_optixTextureSampler->setBuffer(m_image->getOptiXObject());
    }

    ConstantFloat4Texture::~ConstantFloat4Texture() {
        delete m_image;
    }



    ImageFloat4Texture::ImageFloat4Texture(Context &context, const Image2D* image) :
        Float4Texture(context), m_image(image) {
        m_optixTextureSampler->setBuffer(m_image->getOptiXObject());
    }



    // static
    void SurfaceMaterial::commonInitializeProcedure(Context &context, const char* identifiers[10], OptiXProgramSet* programSet) {
        std::string ptx = readTxtFile("resources/ptxes/materials.ptx");

        optix::Context optixContext = context.getOptiXContext();

        if (identifiers[0] && identifiers[1] && identifiers[2] && identifiers[3] && identifiers[4] && identifiers[5] && identifiers[6]) {
            programSet->callableProgramSetupBSDF = optixContext->createProgramFromPTXString(ptx, identifiers[0]);

            programSet->callableProgramGetBaseColor = optixContext->createProgramFromPTXString(ptx, identifiers[1]);
            programSet->callableProgramBSDFmatches = optixContext->createProgramFromPTXString(ptx, identifiers[2]);
            programSet->callableProgramSampleBSDFInternal = optixContext->createProgramFromPTXString(ptx, identifiers[3]);
            programSet->callableProgramEvaluateBSDFInternal = optixContext->createProgramFromPTXString(ptx, identifiers[4]);
            programSet->callableProgramEvaluateBSDF_PDFInternal = optixContext->createProgramFromPTXString(ptx, identifiers[5]);
            programSet->callableProgramBSDFWeightInternal = optixContext->createProgramFromPTXString(ptx, identifiers[6]);

            Shared::BSDFProcedureSet bsdfProcSet;
            {
                bsdfProcSet.progGetBaseColor = programSet->callableProgramGetBaseColor->getId();
                bsdfProcSet.progBSDFmatches = programSet->callableProgramBSDFmatches->getId();
                bsdfProcSet.progSampleBSDFInternal = programSet->callableProgramSampleBSDFInternal->getId();
                bsdfProcSet.progEvaluateBSDFInternal = programSet->callableProgramEvaluateBSDFInternal->getId();
                bsdfProcSet.progEvaluateBSDF_PDFInternal = programSet->callableProgramEvaluateBSDF_PDFInternal->getId();
                bsdfProcSet.progWeightInternal = programSet->callableProgramBSDFWeightInternal->getId();
            }
            programSet->bsdfProcedureSetIndex = context.setBSDFProcedureSet(bsdfProcSet);
        }

        if (identifiers[7] && identifiers[8] && identifiers[9]) {
            programSet->callableProgramSetupEDF = optixContext->createProgramFromPTXString(ptx, identifiers[7]);

            programSet->callableProgramEvaluateEmittanceInternal = optixContext->createProgramFromPTXString(ptx, identifiers[8]);
            programSet->callableProgramEvaluateEDFInternal = optixContext->createProgramFromPTXString(ptx, identifiers[9]);

            Shared::EDFProcedureSet edfProcSet;
            {
                edfProcSet.progEvaluateEmittanceInternal = programSet->callableProgramEvaluateEmittanceInternal->getId();
                edfProcSet.progEvaluateEDFInternal = programSet->callableProgramEvaluateEDFInternal->getId();
            }
            programSet->edfProcedureSetIndex = context.setEDFProcedureSet(edfProcSet);
        }
    }

    // static
    void SurfaceMaterial::commonFinalizeProcedure(Context &context, OptiXProgramSet &programSet) {
        if (programSet.callableProgramSetupEDF) {
            context.unsetEDFProcedureSet(programSet.edfProcedureSetIndex);

            programSet.callableProgramEvaluateEDFInternal->destroy();
            programSet.callableProgramEvaluateEmittanceInternal->destroy();

            programSet.callableProgramSetupEDF->destroy();
        }

        if (programSet.callableProgramSetupBSDF) {
            context.unsetBSDFProcedureSet(programSet.bsdfProcedureSetIndex);

            programSet.callableProgramBSDFWeightInternal->destroy();
            programSet.callableProgramEvaluateBSDF_PDFInternal->destroy();
            programSet.callableProgramEvaluateBSDFInternal->destroy();
            programSet.callableProgramSampleBSDFInternal->destroy();
            programSet.callableProgramBSDFmatches->destroy();
            programSet.callableProgramGetBaseColor->destroy();

            programSet.callableProgramSetupBSDF->destroy();
        }
    }

    // static
    uint32_t SurfaceMaterial::setupMaterialDescriptorHead(Context &context, const OptiXProgramSet &progSet, Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) {
        Shared::SurfaceMaterialHead &head = *(Shared::SurfaceMaterialHead*)&matDesc->i1[baseIndex];

        if (progSet.callableProgramSetupBSDF) {
            head.progSetupBSDF = progSet.callableProgramSetupBSDF->getId();
            head.bsdfProcedureSetIndex = progSet.bsdfProcedureSetIndex;
        }
        else {
            head.progSetupBSDF = context.getOptixCallableProgramNullBSDF_setupBSDF()->getId();
            head.bsdfProcedureSetIndex = context.getNullBSDFProcedureSetIndex();
        }

        if (progSet.callableProgramSetupEDF) {
            head.progSetupEDF = progSet.callableProgramSetupEDF->getId();
            head.edfProcedureSetIndex = progSet.edfProcedureSetIndex;
        }
        else {
            head.progSetupEDF = context.getOptixCallableProgramNullEDF_setupEDF()->getId();
            head.edfProcedureSetIndex = context.getNullEDFProcedureSetIndex();
        }

        return baseIndex + sizeof(Shared::SurfaceMaterialHead) / 4;
    }

    // static
    void SurfaceMaterial::initialize(Context &context) {
        MatteSurfaceMaterial::initialize(context);
        SpecularReflectionSurfaceMaterial::initialize(context);
        SpecularScatteringSurfaceMaterial::initialize(context);
        UE4SurfaceMaterial::initialize(context);
        DiffuseEmitterSurfaceMaterial::initialize(context);
        MultiSurfaceMaterial::initialize(context);
    }

    // static
    void SurfaceMaterial::finalize(Context &context) {
        MultiSurfaceMaterial::finalize(context);
        DiffuseEmitterSurfaceMaterial::finalize(context);
        UE4SurfaceMaterial::finalize(context);
        SpecularScatteringSurfaceMaterial::finalize(context);
        SpecularReflectionSurfaceMaterial::finalize(context);
        MatteSurfaceMaterial::finalize(context);
    }

    SurfaceMaterial::SurfaceMaterial(Context &context) : Object(context) {
        optix::Context optixContext = context.getOptiXContext();
        m_optixMaterial = optixContext->createMaterial();

        m_optixMaterial->setClosestHitProgram(Shared::RayType::Primary, context.getOptiXProgramPathTracingIteration());
        m_optixMaterial->setClosestHitProgram(Shared::RayType::Scattered, context.getOptiXProgramPathTracingIteration());
        m_optixMaterial->setAnyHitProgram(Shared::RayType::Primary, context.getOptiXProgramStochasticAlphaAnyHit());
        m_optixMaterial->setAnyHitProgram(Shared::RayType::Scattered, context.getOptiXProgramStochasticAlphaAnyHit());
        m_optixMaterial->setAnyHitProgram(Shared::RayType::Shadow, context.getOptiXProgramAlphaAnyHit());

        m_matIndex = 0xFFFFFFFF;
    }

    SurfaceMaterial::~SurfaceMaterial() {
        if (m_matIndex != 0xFFFFFFFF)
            m_context.unsetSurfaceMaterialDescriptor(m_matIndex);
        m_matIndex = 0xFFFFFFFF;

        m_optixMaterial->destroy();
    }



    std::map<uint32_t, SurfaceMaterial::OptiXProgramSet> MatteSurfaceMaterial::OptiXProgramSets;

    // static
    void MatteSurfaceMaterial::initialize(Context &context) {
        const char* identifiers[] = {
            "VLR::MatteSurfaceMaterial_setupBSDF",
            "VLR::MatteBRDF_getBaseColor",
            "VLR::MatteBRDF_matches",
            "VLR::MatteBRDF_sampleBSDFInternal",
            "VLR::MatteBRDF_evaluateBSDFInternal",
            "VLR::MatteBRDF_evaluateBSDF_PDFInternal",
            "VLR::MatteBRDF_weightInternal",
            nullptr,
            nullptr,
            nullptr
        };
        OptiXProgramSet programSet;
        commonInitializeProcedure(context, identifiers, &programSet);

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void MatteSurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());
        commonFinalizeProcedure(context, programSet);
    }

    MatteSurfaceMaterial::MatteSurfaceMaterial(Context &context, const Float4Texture* texAlbedoRoughness) :
        SurfaceMaterial(context), m_texAlbedoRoughness(texAlbedoRoughness) {
        Shared::SurfaceMaterialDescriptor matDesc;
        setupMaterialDescriptor(&matDesc, 0);

        m_matIndex = m_context.setSurfaceMaterialDescriptor(matDesc);
        //m_optixMaterial["VLR::pv_materialIndex"]->setUint(m_matIndex); // 何故かvalidate()でエラーになる。
        m_optixMaterial["VLR::pv_materialIndex"]->setUserData(sizeof(m_matIndex), &m_matIndex);
    }

    MatteSurfaceMaterial::~MatteSurfaceMaterial() {
    }

    uint32_t MatteSurfaceMaterial::setupMaterialDescriptor(Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) const {
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        baseIndex = setupMaterialDescriptorHead(m_context, progSet, matDesc, baseIndex);
        Shared::MatteSurfaceMaterial &mat = *(Shared::MatteSurfaceMaterial*)&matDesc->i1[baseIndex];
        mat.texAlbedoRoughness = m_texAlbedoRoughness->getOptiXObject()->getId();

        return baseIndex + sizeof(Shared::MatteSurfaceMaterial) / 4;
    }



    std::map<uint32_t, SurfaceMaterial::OptiXProgramSet> SpecularReflectionSurfaceMaterial::OptiXProgramSets;

    // static
    void SpecularReflectionSurfaceMaterial::initialize(Context &context) {
        const char* identifiers[] = {
            "VLR::SpecularReflectionSurfaceMaterial_setupBSDF",
            "VLR::SpecularBRDF_getBaseColor",
            "VLR::SpecularBRDF_matches",
            "VLR::SpecularBRDF_sampleBSDFInternal",
            "VLR::SpecularBRDF_evaluateBSDFInternal",
            "VLR::SpecularBRDF_evaluateBSDF_PDFInternal",
            "VLR::SpecularBRDF_weightInternal",
            nullptr,
            nullptr,
            nullptr
        };
        OptiXProgramSet programSet;
        commonInitializeProcedure(context, identifiers, &programSet);

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void SpecularReflectionSurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());
        commonFinalizeProcedure(context, programSet);
    }

    SpecularReflectionSurfaceMaterial::SpecularReflectionSurfaceMaterial(Context &context, const Float3Texture* texCoeffR, const Float3Texture* texEta, const Float3Texture* tex_k) :
        SurfaceMaterial(context), m_texCoeffR(texCoeffR), m_texEta(texEta), m_tex_k(tex_k) {
        Shared::SurfaceMaterialDescriptor matDesc;
        setupMaterialDescriptor(&matDesc, 0);

        m_matIndex = m_context.setSurfaceMaterialDescriptor(matDesc);
        //m_optixMaterial["VLR::pv_materialIndex"]->setUint(m_matIndex); // 何故かvalidate()でエラーになる。
        m_optixMaterial["VLR::pv_materialIndex"]->setUserData(sizeof(m_matIndex), &m_matIndex);
    }

    SpecularReflectionSurfaceMaterial::~SpecularReflectionSurfaceMaterial() {
    }

    uint32_t SpecularReflectionSurfaceMaterial::setupMaterialDescriptor(Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) const {
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        baseIndex = setupMaterialDescriptorHead(m_context, progSet, matDesc, baseIndex);
        Shared::SpecularReflectionSurfaceMaterial &mat = *(Shared::SpecularReflectionSurfaceMaterial*)&matDesc->i1[baseIndex];
        mat.texCoeffR = m_texCoeffR->getOptiXObject()->getId();
        mat.texEta = m_texEta->getOptiXObject()->getId();
        mat.tex_k = m_tex_k->getOptiXObject()->getId();

        return baseIndex + sizeof(Shared::SpecularReflectionSurfaceMaterial) / 4;
    }



    std::map<uint32_t, SurfaceMaterial::OptiXProgramSet> SpecularScatteringSurfaceMaterial::OptiXProgramSets;

    // static
    void SpecularScatteringSurfaceMaterial::initialize(Context &context) {
        const char* identifiers[] = {
            "VLR::SpecularScatteringSurfaceMaterial_setupBSDF",
            "VLR::SpecularBSDF_getBaseColor",
            "VLR::SpecularBSDF_matches",
            "VLR::SpecularBSDF_sampleBSDFInternal",
            "VLR::SpecularBSDF_evaluateBSDFInternal",
            "VLR::SpecularBSDF_evaluateBSDF_PDFInternal",
            "VLR::SpecularBSDF_weightInternal",
            nullptr,
            nullptr,
            nullptr
        };
        OptiXProgramSet programSet;
        commonInitializeProcedure(context, identifiers, &programSet);

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void SpecularScatteringSurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());
        commonFinalizeProcedure(context, programSet);
    }

    SpecularScatteringSurfaceMaterial::SpecularScatteringSurfaceMaterial(Context &context, const Float3Texture* texCoeff, const Float3Texture* texEtaExt, const Float3Texture* texEtaInt) :
        SurfaceMaterial(context), m_texCoeff(texCoeff), m_texEtaExt(texEtaExt), m_texEtaInt(texEtaInt) {
        Shared::SurfaceMaterialDescriptor matDesc;
        setupMaterialDescriptor(&matDesc, 0);

        m_matIndex = m_context.setSurfaceMaterialDescriptor(matDesc);
        //m_optixMaterial["VLR::pv_materialIndex"]->setUint(m_matIndex); // 何故かvalidate()でエラーになる。
        m_optixMaterial["VLR::pv_materialIndex"]->setUserData(sizeof(m_matIndex), &m_matIndex);
    }

    SpecularScatteringSurfaceMaterial::~SpecularScatteringSurfaceMaterial() {
    }

    uint32_t SpecularScatteringSurfaceMaterial::setupMaterialDescriptor(Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) const {
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        baseIndex = setupMaterialDescriptorHead(m_context, progSet, matDesc, baseIndex);
        Shared::SpecularScatteringSurfaceMaterial &mat = *(Shared::SpecularScatteringSurfaceMaterial*)&matDesc->i1[baseIndex];
        mat.texCoeff = m_texCoeff->getOptiXObject()->getId();
        mat.texEtaExt = m_texEtaExt->getOptiXObject()->getId();
        mat.texEtaInt = m_texEtaInt->getOptiXObject()->getId();

        return baseIndex + sizeof(Shared::SpecularScatteringSurfaceMaterial) / 4;
    }



    std::map<uint32_t, SurfaceMaterial::OptiXProgramSet> UE4SurfaceMaterial::OptiXProgramSets;

    // static
    void UE4SurfaceMaterial::initialize(Context &context) {
        const char* identifiers[] = {
            "VLR::UE4SurfaceMaterial_setupBSDF",
            "VLR::UE4BRDF_getBaseColor",
            "VLR::UE4BRDF_matches",
            "VLR::UE4BRDF_sampleBSDFInternal",
            "VLR::UE4BRDF_evaluateBSDFInternal",
            "VLR::UE4BRDF_evaluateBSDF_PDFInternal",
            "VLR::UE4BRDF_weightInternal",
            nullptr,
            nullptr,
            nullptr
        };
        OptiXProgramSet programSet;
        commonInitializeProcedure(context, identifiers, &programSet);

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void UE4SurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());
        commonFinalizeProcedure(context, programSet);
    }

    UE4SurfaceMaterial::UE4SurfaceMaterial(Context &context, const Float3Texture* texBaseColor, const Float3Texture* texOcclusionRoughnessMetallic) :
        SurfaceMaterial(context), m_texBaseColor(texBaseColor), m_texOcclusionRoughnessMetallic(texOcclusionRoughnessMetallic) {
        Shared::SurfaceMaterialDescriptor matDesc;
        setupMaterialDescriptor(&matDesc, 0);

        m_matIndex = m_context.setSurfaceMaterialDescriptor(matDesc);
        m_optixMaterial["VLR::pv_materialIndex"]->setUserData(sizeof(m_matIndex), &m_matIndex);
    }

    UE4SurfaceMaterial::~UE4SurfaceMaterial() {
    }

    uint32_t UE4SurfaceMaterial::setupMaterialDescriptor(Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) const {
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        baseIndex = setupMaterialDescriptorHead(m_context, progSet, matDesc, baseIndex);
        Shared::UE4SurfaceMaterial &mat = *(Shared::UE4SurfaceMaterial*)&matDesc->i1[baseIndex];
        mat.texBaseColor = m_texBaseColor->getOptiXObject()->getId();
        mat.texOcclusionRoughnessMetallic = m_texOcclusionRoughnessMetallic->getOptiXObject()->getId();

        return baseIndex + sizeof(Shared::UE4SurfaceMaterial) / 4;
    }



    std::map<uint32_t, SurfaceMaterial::OptiXProgramSet> DiffuseEmitterSurfaceMaterial::OptiXProgramSets;

    // static
    void DiffuseEmitterSurfaceMaterial::initialize(Context &context) {
        const char* identifiers[] = {
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            "VLR::DiffuseEmitterSurfaceMaterial_setupEDF",
            "VLR::DiffuseEDF_evaluateEmittanceInternal",
            "VLR::DiffuseEDF_evaluateEDFInternal"
        };
        OptiXProgramSet programSet;
        commonInitializeProcedure(context, identifiers, &programSet);

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void DiffuseEmitterSurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());
        commonFinalizeProcedure(context, programSet);
    }

    DiffuseEmitterSurfaceMaterial::DiffuseEmitterSurfaceMaterial(Context &context, const Float3Texture* texEmittance) :
        SurfaceMaterial(context), m_texEmittance(texEmittance) {
        Shared::SurfaceMaterialDescriptor matDesc;
        setupMaterialDescriptor(&matDesc, 0);

        m_matIndex = m_context.setSurfaceMaterialDescriptor(matDesc);
        //m_optixMaterial["VLR::pv_materialIndex"]->setUint(m_matIndex); // 何故かvalidate()でエラーになる。
        m_optixMaterial["VLR::pv_materialIndex"]->setUserData(sizeof(m_matIndex), &m_matIndex);
    }

    DiffuseEmitterSurfaceMaterial::~DiffuseEmitterSurfaceMaterial() {
    }

    uint32_t DiffuseEmitterSurfaceMaterial::setupMaterialDescriptor(Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) const {
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        baseIndex = setupMaterialDescriptorHead(m_context, progSet, matDesc, baseIndex);
        Shared::DiffuseEmitterSurfaceMaterial &mat = *(Shared::DiffuseEmitterSurfaceMaterial*)&matDesc->i1[baseIndex];
        mat.texEmittance = m_texEmittance->getOptiXObject()->getId();

        return baseIndex + sizeof(Shared::DiffuseEmitterSurfaceMaterial) / 4;
    }



    std::map<uint32_t, SurfaceMaterial::OptiXProgramSet> MultiSurfaceMaterial::OptiXProgramSets;

    // static
    void MultiSurfaceMaterial::initialize(Context &context) {
        const char* identifiers[] = {
            "VLR::MultiSurfaceMaterial_setupBSDF",
            "VLR::MultiBSDF_getBaseColor",
            "VLR::MultiBSDF_matches",
            "VLR::MultiBSDF_sampleBSDFInternal",
            "VLR::MultiBSDF_evaluateBSDFInternal",
            "VLR::MultiBSDF_evaluateBSDF_PDFInternal",
            "VLR::MultiBSDF_weightInternal",
            "VLR::MultiSurfaceMaterial_setupEDF",
            "VLR::MultiEDF_evaluateEmittanceInternal",
            "VLR::MultiEDF_evaluateEDFInternal"
        };
        OptiXProgramSet programSet;
        commonInitializeProcedure(context, identifiers, &programSet);

        OptiXProgramSets[context.getID()] = programSet;
    }

    // static
    void MultiSurfaceMaterial::finalize(Context &context) {
        OptiXProgramSet &programSet = OptiXProgramSets.at(context.getID());
        commonFinalizeProcedure(context, programSet);
    }

    MultiSurfaceMaterial::MultiSurfaceMaterial(Context &context, const SurfaceMaterial** materials, uint32_t numMaterials) :
        SurfaceMaterial(context) {
        VLRAssert(numMaterials <= lengthof(m_materials), "numMaterials should be less than or equal to %u", lengthof(m_materials));
        std::copy_n(materials, numMaterials, m_materials);
        m_numMaterials = numMaterials;

        Shared::SurfaceMaterialDescriptor matDesc;
        setupMaterialDescriptor(&matDesc, 0);

        m_matIndex = m_context.setSurfaceMaterialDescriptor(matDesc);
        //m_optixMaterial["VLR::pv_materialIndex"]->setUint(m_matIndex); // 何故かvalidate()でエラーになる。
        m_optixMaterial["VLR::pv_materialIndex"]->setUserData(sizeof(m_matIndex), &m_matIndex);
    }

    MultiSurfaceMaterial::~MultiSurfaceMaterial() {
    }

    uint32_t MultiSurfaceMaterial::setupMaterialDescriptor(Shared::SurfaceMaterialDescriptor* matDesc, uint32_t baseIndex) const {
        OptiXProgramSet &progSet = OptiXProgramSets.at(m_context.getID());

        baseIndex = setupMaterialDescriptorHead(m_context, progSet, matDesc, baseIndex);
        Shared::MultiSurfaceMaterial &mat = *(Shared::MultiSurfaceMaterial*)&matDesc->i1[baseIndex];
        baseIndex += sizeof(Shared::MultiSurfaceMaterial) / 4;

        uint32_t matOffsets[4] = { 0, 0, 0, 0 };
        VLRAssert(lengthof(matOffsets) == lengthof(m_materials), "Two sizes must match.");
        for (int i = 0; i < m_numMaterials; ++i) {
            const SurfaceMaterial* mat = m_materials[i];
            matOffsets[i] = baseIndex;
            baseIndex = mat->setupMaterialDescriptor(matDesc, baseIndex);
        }
        VLRAssert(baseIndex <= VLR_MAX_NUM_MATERIAL_DESCRIPTOR_SLOTS, "exceeds the size of SurfaceMaterialDescriptor.");

        mat.matOffset0 = matOffsets[0];
        mat.matOffset1 = matOffsets[1];
        mat.matOffset2 = matOffsets[2];
        mat.matOffset3 = matOffsets[3];
        mat.numMaterials = m_numMaterials;

        return baseIndex;
    }

    bool MultiSurfaceMaterial::isEmitting() const {
        for (int i = 0; i < m_numMaterials; ++i) {
            if (m_materials[i]->isEmitting())
                return true;
        }
        return false;
    }

    // END: Material
    // ----------------------------------------------------------------
}
