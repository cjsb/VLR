﻿#pragma once

#include <public_types.h>
#include "basic_types_internal.h"
#include "shared.h"

#include "slot_manager.h"

namespace VLR {
    std::string readTxtFile(const std::string& filepath);



    class Scene;
    class Camera;

    class Context {
        static uint32_t NextID;
        static uint32_t getInstanceID() {
            return NextID++;
        }

        uint32_t m_ID;
        optix::Context m_optixContext;

        optix::Program m_optixProgramShadowAnyHitDefault; // ---- Any Hit Program
        optix::Program m_optixProgramAnyHitWithAlpha; // -------- Any Hit Program
        optix::Program m_optixProgramShadowAnyHitWithAlpha; // -- Any Hit Program
        optix::Program m_optixProgramPathTracingIteration; // --- Closest Hit Program

        optix::Program m_optixProgramPathTracing; // ------------ Ray Generation Program
        optix::Program m_optixProgramPathTracingMiss; // -------- Miss Program
        optix::Program m_optixProgramException; // -------------- Exception Program

        optix::Material m_optixMaterialDefault;
        optix::Material m_optixMaterialWithAlpha;

        optix::Buffer m_optixNodeProcedureSetBuffer;
        uint32_t m_maxNumNodeProcSet;
        SlotManager m_nodeProcSetSlotManager;

        optix::Buffer m_optixNodeDescriptorBuffer;
        uint32_t m_maxNumNodeDescriptors;
        SlotManager m_nodeDescSlotManager;

        optix::Buffer m_optixBSDFProcedureSetBuffer;
        uint32_t m_maxNumBSDFProcSet;
        SlotManager m_bsdfProcSetSlotManager;

        optix::Buffer m_optixEDFProcedureSetBuffer;
        uint32_t m_maxNumEDFProcSet;
        SlotManager m_edfProcSetSlotManager;

        optix::Program m_optixCallableProgramNullBSDF_setupBSDF;
        optix::Program m_optixCallableProgramNullBSDF_getBaseColor;
        optix::Program m_optixCallableProgramNullBSDF_matches;
        optix::Program m_optixCallableProgramNullBSDF_sampleInternal;
        optix::Program m_optixCallableProgramNullBSDF_evaluateInternal;
        optix::Program m_optixCallableProgramNullBSDF_evaluatePDFInternal;
        optix::Program m_optixCallableProgramNullBSDF_weightInternal;
        uint32_t m_nullBSDFProcedureSetIndex;

        optix::Program m_optixCallableProgramNullEDF_setupEDF;
        optix::Program m_optixCallableProgramNullEDF_evaluateEmittanceInternal;
        optix::Program m_optixCallableProgramNullEDF_evaluateInternal;
        uint32_t m_nullEDFProcedureSetIndex;

        optix::Buffer m_optixSurfaceMaterialDescriptorBuffer;
        uint32_t m_maxNumSurfaceMaterialDescriptors;
        SlotManager m_surfMatDescSlotManager;

        optix::Buffer m_outputBuffer;
        optix::Buffer m_rngBuffer;
        uint32_t m_width;
        uint32_t m_height;
        uint32_t m_numAccumFrames;

    public:
        Context(bool logging, uint32_t stackSize);
        ~Context();

        uint32_t getID() const {
            return m_ID;
        }

        void setDevices(const int32_t* devices, uint32_t numDevices);

        void bindOutputBuffer(uint32_t width, uint32_t height, uint32_t glBufferID);
        void* mapOutputBuffer();
        void unmapOutputBuffer();
        void getOutputBufferSize(uint32_t* width, uint32_t* height);

        void render(Scene &scene, Camera* camera, uint32_t shrinkCoeff, bool firstFrame, uint32_t* numAccumFrames);

        const optix::Context &getOptiXContext() const {
            return m_optixContext;
        }

        const optix::Material &getOptiXMaterialDefault() const {
            return m_optixMaterialDefault;
        }
        const optix::Material &getOptiXMaterialWithAlpha() const {
            return m_optixMaterialWithAlpha;
        }

        uint32_t allocateNodeProcedureSet();
        void releaseNodeProcedureSet(uint32_t index);
        void updateNodeProcedureSet(uint32_t index, const Shared::NodeProcedureSet &procSet);

        uint32_t allocateNodeDescriptor();
        void releaseNodeDescriptor(uint32_t index);
        void updateNodeDescriptor(uint32_t index, const Shared::NodeDescriptor &nodeDesc);

        uint32_t allocateBSDFProcedureSet();
        void releaseBSDFProcedureSet(uint32_t index);
        void updateBSDFProcedureSet(uint32_t index, const Shared::BSDFProcedureSet &procSet);

        uint32_t allocateEDFProcedureSet();
        void releaseEDFProcedureSet(uint32_t index);
        void updateEDFProcedureSet(uint32_t index, const Shared::EDFProcedureSet &procSet);

        const optix::Program &getOptixCallableProgramNullBSDF_setupBSDF() const {
            return m_optixCallableProgramNullBSDF_setupBSDF;
        }
        uint32_t getNullBSDFProcedureSetIndex() const { return m_nullBSDFProcedureSetIndex; }
        const optix::Program &getOptixCallableProgramNullEDF_setupEDF() const {
            return m_optixCallableProgramNullEDF_setupEDF;
        }
        uint32_t getNullEDFProcedureSetIndex() const { return m_nullEDFProcedureSetIndex; }

        uint32_t allocateSurfaceMaterialDescriptor();
        void releaseSurfaceMaterialDescriptor(uint32_t index);
        void updateSurfaceMaterialDescriptor(uint32_t index, const Shared::SurfaceMaterialDescriptor &matDesc);
    };



    class ClassIdentifier {
        ClassIdentifier &operator=(const ClassIdentifier &) = delete;

        const ClassIdentifier* m_baseClass;

    public:
        ClassIdentifier(const ClassIdentifier* baseClass) : m_baseClass(baseClass) {}

        const ClassIdentifier* getBaseClass() const {
            return m_baseClass;
        }
    };



    class TypeAwareClass {
    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        template <class T>
        bool is() const {
            return &getClass() == &T::ClassID;
        }

        template <class T>
        bool isMemberOf() const {
            const ClassIdentifier* curClass = &getClass();
            while (curClass) {
                if (curClass == &T::ClassID)
                    return true;
                curClass = curClass->getBaseClass();
            }
            return false;
        }
    };



    class Object : public TypeAwareClass {
    protected:
        Context &m_context;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        Object(Context &context);
        virtual ~Object() {}

        Context &getContext() {
            return m_context;
        }
    };



    // ----------------------------------------------------------------
    // Miscellaneous

    template <typename RealType>
    class DiscreteDistribution1DTemplate {
        optix::Buffer m_PMF;
        optix::Buffer m_CDF;
        RealType m_integral;
        uint32_t m_numValues;

    public:
        void initialize(Context &context, const RealType* values, size_t numValues);
        void finalize(Context &context);

        void getInternalType(Shared::DiscreteDistribution1DTemplate<RealType>* instance) const;
    };

    using DiscreteDistribution1D = DiscreteDistribution1DTemplate<float>;



    template <typename RealType>
    class RegularConstantContinuousDistribution1DTemplate {
        optix::Buffer m_PDF;
        optix::Buffer m_CDF;
        RealType m_integral;
        uint32_t m_numValues;

    public:
        void initialize(Context &context, const RealType* values, size_t numValues);
        void finalize(Context &context);

        RealType getIntegral() const { return m_integral; }
        uint32_t getNumValues() const { return m_numValues; }

        void getInternalType(Shared::RegularConstantContinuousDistribution1DTemplate<RealType>* instance) const;
    };

    using RegularConstantContinuousDistribution1D = RegularConstantContinuousDistribution1DTemplate<float>;



    template <typename RealType>
    class RegularConstantContinuousDistribution2DTemplate {
        optix::Buffer m_raw1DDists;
        RegularConstantContinuousDistribution1DTemplate<RealType>* m_1DDists;
        RegularConstantContinuousDistribution1DTemplate<RealType> m_top1DDist;

    public:
        RegularConstantContinuousDistribution2DTemplate() : m_1DDists(nullptr) {}

        void initialize(Context &context, const RealType* values, size_t numD1, size_t numD2);
        void finalize(Context &context);

        bool isInitialized() const { return m_1DDists != nullptr; }

        void getInternalType(Shared::RegularConstantContinuousDistribution2DTemplate<RealType>* instance) const;
    };

    using RegularConstantContinuousDistribution2D = RegularConstantContinuousDistribution2DTemplate<float>;

    // END: Miscellaneous
    // ----------------------------------------------------------------
}
