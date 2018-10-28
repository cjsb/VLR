﻿#pragma once

#include "textures.h"

namespace VLR {
    // ----------------------------------------------------------------
    // Material

    class SurfaceMaterial : public Object {
    protected:
        struct OptiXProgramSet {
            optix::Program callableProgramSetupBSDF;
            optix::Program callableProgramBSDFGetBaseColor;
            optix::Program callableProgramBSDFmatches;
            optix::Program callableProgramBSDFSampleInternal;
            optix::Program callableProgramBSDFEvaluateInternal;
            optix::Program callableProgramBSDFEvaluatePDFInternal;
            optix::Program callableProgramBSDFWeightInternal;
            uint32_t bsdfProcedureSetIndex;

            optix::Program callableProgramSetupEDF;
            optix::Program callableProgramEDFEvaluateEmittanceInternal;
            optix::Program callableProgramEDFEvaluateInternal;
            uint32_t edfProcedureSetIndex;
        };

        uint32_t m_matIndex;

        static void commonInitializeProcedure(Context &context, const char* identifiers[10], OptiXProgramSet* programSet);
        static void commonFinalizeProcedure(Context &context, OptiXProgramSet &programSet);
        static void setupMaterialDescriptorHead(Context &context, const OptiXProgramSet &progSet, Shared::SurfaceMaterialDescriptor* matDesc);

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        SurfaceMaterial(Context &context);
        virtual ~SurfaceMaterial();

        uint32_t getMaterialIndex() const {
            return m_matIndex;
        }

        virtual bool isEmitting() const { return false; }
    };



    class MatteSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeAlbedo;
        RGBSpectrum m_immAlbedo;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        MatteSurfaceMaterial(Context &context);
        ~MatteSurfaceMaterial();

        bool setNodeAlbedo(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueAlbedo(const RGBSpectrum &value);
    };



    class SpecularReflectionSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeCoeffR;
        ShaderNodeSocketIdentifier m_nodeEta;
        ShaderNodeSocketIdentifier m_node_k;
        RGBSpectrum m_immCoeffR;
        RGBSpectrum m_immEta;
        RGBSpectrum m_imm_k;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        SpecularReflectionSurfaceMaterial(Context &context);
        ~SpecularReflectionSurfaceMaterial();

        bool setNodeCoeffR(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueCoeffR(const RGBSpectrum &value);
        bool setNodeEta(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEta(const RGBSpectrum &value);
        bool setNode_k(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValue_k(const RGBSpectrum &value);
    };



    class SpecularScatteringSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeCoeff;
        ShaderNodeSocketIdentifier m_nodeEtaExt;
        ShaderNodeSocketIdentifier m_nodeEtaInt;
        RGBSpectrum m_immCoeff;
        RGBSpectrum m_immEtaExt;
        RGBSpectrum m_immEtaInt;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        SpecularScatteringSurfaceMaterial(Context &context);
        ~SpecularScatteringSurfaceMaterial();

        bool setNodeCoeff(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueCoeff(const RGBSpectrum &value);
        bool setNodeEtaExt(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEtaExt(const RGBSpectrum &value);
        bool setNodeEtaInt(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEtaInt(const RGBSpectrum &value);
    };



    class MicrofacetReflectionSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeEta;
        ShaderNodeSocketIdentifier m_node_k;
        ShaderNodeSocketIdentifier m_nodeRoughness;
        RGBSpectrum m_immEta;
        RGBSpectrum m_imm_k;
        float m_immRoughness[2];

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        MicrofacetReflectionSurfaceMaterial(Context &context);
        ~MicrofacetReflectionSurfaceMaterial();

        bool setNodeEta(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEta(const RGBSpectrum &value);
        bool setNode_k(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValue_k(const RGBSpectrum &value);
        bool setNodeRoughness(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueRoughness(const float value[2]);
    };



    class MicrofacetScatteringSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeCoeff;
        ShaderNodeSocketIdentifier m_nodeEtaExt;
        ShaderNodeSocketIdentifier m_nodeEtaInt;
        ShaderNodeSocketIdentifier m_nodeRoughness;
        RGBSpectrum m_immCoeff;
        RGBSpectrum m_immEtaExt;
        RGBSpectrum m_immEtaInt;
        float m_immRoughness[2];

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        MicrofacetScatteringSurfaceMaterial(Context &context);
        ~MicrofacetScatteringSurfaceMaterial();

        bool setNodeCoeff(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueCoeff(const RGBSpectrum &value);
        bool setNodeEtaExt(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEtaExt(const RGBSpectrum &value);
        bool setNodeEtaInt(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEtaInt(const RGBSpectrum &value);
        bool setNodeRoughness(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueRoughness(const float value[2]);
    };



    class LambertianScatteringSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeCoeff;
        ShaderNodeSocketIdentifier m_nodeF0;
        RGBSpectrum m_immCoeff;
        float m_immF0;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        LambertianScatteringSurfaceMaterial(Context &context);
        ~LambertianScatteringSurfaceMaterial();

        bool setNodeCoeff(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueCoeff(const RGBSpectrum &value);
        bool setNodeF0(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueF0(float value);
    };



    class UE4SurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeBaseColor;
        ShaderNodeSocketIdentifier m_nodeOcclusionRoughnessMetallic;
        RGBSpectrum m_immBaseColor;
        float m_immOcculusion;
        float m_immRoughness;
        float m_immMetallic;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        UE4SurfaceMaterial(Context &context);
        ~UE4SurfaceMaterial();

        bool setNodeBaseColor(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueBaseColor(const RGBSpectrum &value);
        bool setNodeOcclusionRoughnessMetallic(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueOcclusion(float value);
        void setImmediateValueRoughness(float value);
        void setImmediateValueMetallic(float value);
    };



    class DiffuseEmitterSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        ShaderNodeSocketIdentifier m_nodeEmittance;
        RGBSpectrum m_immEmittance;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        DiffuseEmitterSurfaceMaterial(Context &context);
        ~DiffuseEmitterSurfaceMaterial();

        bool isEmitting() const override { return true; }

        bool setNodeEmittance(const ShaderNodeSocketIdentifier &outputSocket);
        void setImmediateValueEmittance(const RGBSpectrum &value);
    };



    class MultiSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        const SurfaceMaterial* m_subMaterials[4];
        uint32_t m_numSubMaterials;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        MultiSurfaceMaterial(Context &context);
        ~MultiSurfaceMaterial();

        bool isEmitting() const override;

        void setSubMaterial(uint32_t index, const SurfaceMaterial* mat);
    };



    class EnvironmentEmitterSurfaceMaterial : public SurfaceMaterial {
        static std::map<uint32_t, OptiXProgramSet> OptiXProgramSets;

        const EnvironmentTextureShaderNode* m_nodeEmittance;
        RGBSpectrum m_immEmittance;
        RegularConstantContinuousDistribution2D m_importanceMap;

        void setupMaterialDescriptor() const;

    public:
        static const ClassIdentifier ClassID;
        virtual const ClassIdentifier &getClass() const { return ClassID; }

        static void initialize(Context &context);
        static void finalize(Context &context);

        EnvironmentEmitterSurfaceMaterial(Context &context);
        ~EnvironmentEmitterSurfaceMaterial();

        bool isEmitting() const override { return true; }

        bool setNodeEmittance(const EnvironmentTextureShaderNode* node);
        void setImmediateValueEmittance(const RGBSpectrum &value);

        const RegularConstantContinuousDistribution2D &getImportanceMap() const {
            return m_importanceMap;
        }
    };

    // END: Material
    // ----------------------------------------------------------------
}
