﻿#include "kernel_common.cuh"

namespace VLR {
    RT_FUNCTION DirectionType sideTest(const Normal3D &ng, const Vector3D &d0, const Vector3D &d1) {
        bool reflect = dot(Vector3D(ng), d0) * dot(Vector3D(ng), d1) > 0;
        return DirectionType::AllFreq() | (reflect ? DirectionType::Reflection() : DirectionType::Transmission());
    }



    class FresnelConductor {
        RGBSpectrum m_eta;
        RGBSpectrum m_k;

    public:
        RT_FUNCTION FresnelConductor(const RGBSpectrum &eta, const RGBSpectrum &k) : m_eta(eta), m_k(k) {}

        RT_FUNCTION RGBSpectrum evaluate(float cosEnter) const {
            cosEnter = std::fabs(cosEnter);
            float cosEnter2 = cosEnter * cosEnter;
            RGBSpectrum _2EtaCosEnter = 2.0f * m_eta * cosEnter;
            RGBSpectrum tmp_f = m_eta * m_eta + m_k * m_k;
            RGBSpectrum tmp = tmp_f * cosEnter2;
            RGBSpectrum Rparl2 = (tmp - _2EtaCosEnter + 1) / (tmp + _2EtaCosEnter + 1);
            RGBSpectrum Rperp2 = (tmp_f - _2EtaCosEnter + cosEnter2) / (tmp_f + _2EtaCosEnter + cosEnter2);

            return (Rparl2 + Rperp2) / 2.0f;
        }
        RT_FUNCTION float evaluate(float cosEnter, uint32_t wlIdx) const {
            cosEnter = std::fabs(cosEnter);
            float cosEnter2 = cosEnter * cosEnter;
            float _2EtaCosEnter = 2.0f * m_eta[wlIdx] * cosEnter;
            float tmp_f = m_eta[wlIdx] * m_eta[wlIdx] + m_k[wlIdx] * m_k[wlIdx];
            float tmp = tmp_f * cosEnter2;
            float Rparl2 = (tmp - _2EtaCosEnter + 1) / (tmp + _2EtaCosEnter + 1);
            float Rperp2 = (tmp_f - _2EtaCosEnter + cosEnter2) / (tmp_f + _2EtaCosEnter + cosEnter2);

            return (Rparl2 + Rperp2) / 2.0f;
        }
    };



    class FresnelDielectric {
        RGBSpectrum m_etaExt;
        RGBSpectrum m_etaInt;

    public:
        RT_FUNCTION FresnelDielectric(const RGBSpectrum &etaExt, const RGBSpectrum &etaInt) : m_etaExt(etaExt), m_etaInt(etaInt) {}

        RT_FUNCTION RGBSpectrum etaExt() const { return m_etaExt; }
        RT_FUNCTION RGBSpectrum etaInt() const { return m_etaInt; }

        RT_FUNCTION RGBSpectrum evaluate(float cosEnter) const {
            cosEnter = clamp(cosEnter, -1.0f, 1.0f);

            bool entering = cosEnter > 0.0f;
            const RGBSpectrum &eEnter = entering ? m_etaExt : m_etaInt;
            const RGBSpectrum &eExit = entering ? m_etaInt : m_etaExt;

            RGBSpectrum sinExit = eEnter / eExit * std::sqrt(std::fmax(0.0f, 1.0f - cosEnter * cosEnter));
            RGBSpectrum ret = RGBSpectrum::Zero();
            cosEnter = std::fabs(cosEnter);
            for (int i = 0; i < RGBSpectrum::NumComponents(); ++i) {
                if (sinExit[i] >= 1.0f) {
                    ret[i] = 1.0f;
                }
                else {
                    float cosExit = std::sqrt(std::fmax(0.0f, 1.0f - sinExit[i] * sinExit[i]));
                    ret[i] = evalF(eEnter[i], eExit[i], cosEnter, cosExit);
                }
            }
            return ret;
        }
        RT_FUNCTION float evaluate(float cosEnter, uint32_t wlIdx) const {
            cosEnter = clamp(cosEnter, -1.0f, 1.0f);

            bool entering = cosEnter > 0.0f;
            const float &eEnter = entering ? m_etaExt[wlIdx] : m_etaInt[wlIdx];
            const float &eExit = entering ? m_etaInt[wlIdx] : m_etaExt[wlIdx];

            float sinExit = eEnter / eExit * std::sqrt(std::fmax(0.0f, 1.0f - cosEnter * cosEnter));
            cosEnter = std::fabs(cosEnter);
            if (sinExit >= 1.0f) {
                return 1.0f;
            }
            else {
                float cosExit = std::sqrt(std::fmax(0.0f, 1.0f - sinExit * sinExit));
                return evalF(eEnter, eExit, cosEnter, cosExit);
            }
        }

        RT_FUNCTION static float evalF(float etaEnter, float etaExit, float cosEnter, float cosExit);
    };

    RT_FUNCTION float FresnelDielectric::evalF(float etaEnter, float etaExit, float cosEnter, float cosExit) {
        float Rparl = ((etaExit * cosEnter) - (etaEnter * cosExit)) / ((etaExit * cosEnter) + (etaEnter * cosExit));
        float Rperp = ((etaEnter * cosEnter) - (etaExit * cosExit)) / ((etaEnter * cosEnter) + (etaExit * cosExit));
        return (Rparl * Rparl + Rperp * Rperp) / 2.0f;
    }



    class GGXMicrofacetDistribution {
        float m_alpha_gx;
        float m_alpha_gy;

    public:
        RT_FUNCTION GGXMicrofacetDistribution(float alpha_gx, float alpha_gy) :
            m_alpha_gx(alpha_gx), m_alpha_gy(alpha_gy) {}

        RT_FUNCTION float evaluate(const Normal3D &m) {
            if (m.z <= 0)
                return 0.0f;
            float temp = m.x * m.x / (m_alpha_gx * m_alpha_gx) + m.y * m.y / (m_alpha_gy * m_alpha_gy) + m.z * m.z;
            return 1.0f / (M_PIf * m_alpha_gx * m_alpha_gy * temp * temp);
        }

        RT_FUNCTION float evaluateSmithG1(const Vector3D &v, const Normal3D &m) {
            float chi = (dot(v, m) / v.z) > 0 ? 1 : 0;
            float tanTheta_v_alpha_go_2 = (v.x * v.x * m_alpha_gx * m_alpha_gx + v.y * v.y * m_alpha_gy * m_alpha_gy) / (v.z * v.z);
            return chi * 2 / (1 + std::sqrt(1 + tanTheta_v_alpha_go_2));
        }

        RT_FUNCTION float sample(const Vector3D &v, float u0, float u1, Normal3D* m, float* normalPDF) {
            // stretch view
            Vector3D sv = normalize(Vector3D(m_alpha_gx * v.x, m_alpha_gy * v.y, v.z));

            // orthonormal basis
            //        Vector3D T1 = (sv.z < 0.9999f) ? normalize(cross(sv, Vector3D::Ez)) : Vector3D::Ex;
            //        Vector3D T2 = cross(T1, sv);
            float distIn2D = std::sqrt(sv.x * sv.x + sv.y * sv.y);
            float recDistIn2D = 1.0f / distIn2D;
            Vector3D T1 = (sv.z < 0.9999f) ? Vector3D(sv.y * recDistIn2D, -sv.x * recDistIn2D, 0) : Vector3D::Ex();
            Vector3D T2 = Vector3D(T1.y * sv.z, -T1.x * sv.z, distIn2D);

            // sample point with polar coordinates (r, phi)
            float a = 1.0f / (1.0f + sv.z);
            float r = std::sqrt(u0);
            float phi = M_PIf * ((u1 < a) ? u1 / a : 1 + (u1 - a) / (1.0f - a));
            float P1 = r * std::cos(phi);
            float P2 = r * std::sin(phi) * ((u1 < a) ? 1.0 : sv.z);

            // compute normal
            *m = P1 * T1 + P2 * T2 + std::sqrt(1.0f - P1 * P1 - P2 * P2) * sv;

            // unstretch
            *m = normalize(Normal3D(m_alpha_gx * m->x, m_alpha_gy * m->y, m->z));

            float D = evaluate(*m);
            *normalPDF = evaluateSmithG1(v, *m) * absDot(v, *m) * D / std::abs(v.z);

            return D;
        }

        RT_FUNCTION float evaluatePDF(const Vector3D &v, const Normal3D &m) {
            return evaluateSmithG1(v, m) * absDot(v, m) * evaluate(m) / std::abs(v.z);
        }
    };



    // ----------------------------------------------------------------
    // Texture Mappings

    RT_CALLABLE_PROGRAM Point3D OffsetAndScaleUVTextureMap2D_map(const uint32_t* texMapDesc, const SurfacePoint &surfPt) {
        const OffsetAndScaleUVTextureMap2D &texMap = *(const OffsetAndScaleUVTextureMap2D*)texMapDesc;
        return Point3D(texMap.scale[0] * surfPt.texCoord.u + texMap.offset[0],
                       texMap.scale[1] * surfPt.texCoord.v + texMap.offset[1],
                       0.0f);
    }

    RT_FUNCTION Point3D textureMap(uint32_t texMapIndex, const SurfacePoint &surfPt) {
        const TextureMapDescriptor &texMapDesc = pv_textureMapDescriptorBuffer[texMapIndex];
        progSigTextureMap progTexMap = (progSigTextureMap)texMapDesc.progTextureMap;
        return progTexMap(texMapDesc.data, surfPt);
    }

    // END: Texture Mappings
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // NullBSDF

    RT_CALLABLE_PROGRAM uint32_t NullBSDF_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        return 0;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum NullBSDF_getBaseColor(const uint32_t* params) {
        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM bool NullBSDF_matches(const uint32_t* params, DirectionType flags) {
        return false;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum NullBSDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM RGBSpectrum NullBSDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM float NullBSDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        return 0.0f;
    }

    RT_CALLABLE_PROGRAM float NullBSDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        return 0.0f;
    }

    // END: NullBSDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // MatteBRDF

    struct MatteBRDF {
        RGBSpectrum albedo;
        float roughness;
    };

    RT_CALLABLE_PROGRAM uint32_t MatteSurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        MatteBRDF &p = *(MatteBRDF*)params;
        const MatteSurfaceMaterial &mat = *(const MatteSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue = optix::rtTex2D<optix::float4>(mat.texAlbedoRoughness, texCoord.x, texCoord.y);
        p.albedo = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        p.roughness = texValue.w;

        return sizeof(MatteBRDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MatteBRDF_getBaseColor(const uint32_t* params) {
        MatteBRDF &p = *(MatteBRDF*)params;

        return p.albedo;
    }

    RT_CALLABLE_PROGRAM bool MatteBRDF_matches(const uint32_t* params, DirectionType flags) {
        DirectionType m_type = DirectionType::Reflection() | DirectionType::LowFreq();
        return m_type.matches(flags);
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MatteBRDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        MatteBRDF &p = *(MatteBRDF*)params;

        result->dirLocal = cosineSampleHemisphere(uDir[0], uDir[1]);
        result->dirPDF = result->dirLocal.z / M_PIf;
        result->sampledType = DirectionType::Reflection() | DirectionType::LowFreq();
        result->dirLocal.z *= query.dirLocal.z > 0 ? 1 : -1;

        return p.albedo / M_PIf;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MatteBRDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        MatteBRDF &p = *(MatteBRDF*)params;

        if (query.dirLocal.z * dirLocal.z <= 0.0f) {
            RGBSpectrum fs = RGBSpectrum::Zero();
            return fs;
        }
        RGBSpectrum fs = p.albedo / M_PIf;

        return fs;
    }

    RT_CALLABLE_PROGRAM float MatteBRDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        if (query.dirLocal.z * dirLocal.z <= 0.0f) {
            return 0.0f;
        }

        return std::abs(dirLocal.z) / M_PIf;
    }

    RT_CALLABLE_PROGRAM float MatteBRDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        MatteBRDF &p = *(MatteBRDF*)params;
        return p.albedo.importance(query.wlHint);
    }

    // END: MatteBRDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // SpecularBRDF

    struct SpecularBRDF {
        RGBSpectrum coeffR;
        RGBSpectrum eta;
        RGBSpectrum k;
    };

    RT_CALLABLE_PROGRAM uint32_t SpecularReflectionSurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        SpecularBRDF &p = *(SpecularBRDF*)params;
        const SpecularReflectionSurfaceMaterial &mat = *(const SpecularReflectionSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue;
        texValue = optix::rtTex2D<optix::float4>(mat.texCoeffR, texCoord.x, texCoord.y);
        p.coeffR = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.texEta, texCoord.x, texCoord.y);
        p.eta = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.tex_k, texCoord.x, texCoord.y);
        p.k = RGBSpectrum(texValue.x, texValue.y, texValue.z);

        return sizeof(SpecularBRDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum SpecularBRDF_getBaseColor(const uint32_t* params) {
        SpecularBRDF &p = *(SpecularBRDF*)params;

        return p.coeffR;
    }

    RT_CALLABLE_PROGRAM bool SpecularBRDF_matches(const uint32_t* params, DirectionType flags) {
        DirectionType m_type = DirectionType::Reflection() | DirectionType::Delta0D();
        return m_type.matches(flags);
    }

    RT_CALLABLE_PROGRAM RGBSpectrum SpecularBRDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        SpecularBRDF &p = *(SpecularBRDF*)params;

        FresnelConductor fresnel(p.eta, p.k);

        result->dirLocal = Vector3D(-query.dirLocal.x, -query.dirLocal.y, query.dirLocal.z);
        result->dirPDF = 1.0f;
        result->sampledType = DirectionType::Reflection() | DirectionType::Delta0D();
        RGBSpectrum fs = p.coeffR * fresnel.evaluate(query.dirLocal.z) / std::fabs(query.dirLocal.z);

        return fs;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum SpecularBRDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM float SpecularBRDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        return 0.0f;
    }

    RT_CALLABLE_PROGRAM float SpecularBRDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        SpecularBRDF &p = *(SpecularBRDF*)params;

        FresnelDielectric fresnel(p.eta, p.k);

        return (p.coeffR * fresnel.evaluate(query.dirLocal.z)).importance(query.wlHint);
    }

    // END: SpecularBRDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // SpecularBSDF

    struct SpecularBSDF {
        RGBSpectrum coeff;
        RGBSpectrum etaExt;
        RGBSpectrum etaInt;
        bool dispersive;
    };

    RT_CALLABLE_PROGRAM uint32_t SpecularScatteringSurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        SpecularBSDF &p = *(SpecularBSDF*)params;
        const SpecularScatteringSurfaceMaterial &mat = *(const SpecularScatteringSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue;
        texValue = optix::rtTex2D<optix::float4>(mat.texCoeff, texCoord.x, texCoord.y);
        p.coeff = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.texEtaExt, texCoord.x, texCoord.y);
        p.etaExt = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.texEtaInt, texCoord.x, texCoord.y);
        p.etaInt = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        p.dispersive = !wavelengthSelected;

        return sizeof(SpecularBSDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum SpecularBSDF_getBaseColor(const uint32_t* params) {
        SpecularBSDF &p = *(SpecularBSDF*)params;

        return p.coeff;
    }

    RT_CALLABLE_PROGRAM bool SpecularBSDF_matches(const uint32_t* params, DirectionType flags) {
        DirectionType m_type = DirectionType::WholeSphere() | DirectionType::Delta0D();
        return m_type.matches(flags);
    }

    RT_CALLABLE_PROGRAM RGBSpectrum SpecularBSDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        SpecularBSDF &p = *(SpecularBSDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        const RGBSpectrum &eEnter = entering ? p.etaExt : p.etaInt;
        const RGBSpectrum &eExit = entering ? p.etaInt : p.etaExt;
        FresnelDielectric fresnel(eEnter, eExit);

        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        RGBSpectrum F = fresnel.evaluate(dirV.z);
        float reflectProb = F.importance(query.wlHint);
        if (query.dirTypeFilter.isReflection())
            reflectProb = 1.0f;
        if (query.dirTypeFilter.isTransmission())
            reflectProb = 0.0f;
        if (uComponent < reflectProb) {
            if (dirV.z == 0.0f) {
                result->dirPDF = 0.0f;
                return RGBSpectrum::Zero();
            }
            Vector3D dirL = Vector3D(-dirV.x, -dirV.y, dirV.z);
            result->dirLocal = entering ? dirL : -dirL;
            result->dirPDF = reflectProb;
            result->sampledType = DirectionType::Reflection() | DirectionType::Delta0D();
            RGBSpectrum fs = p.coeff * F / std::fabs(dirV.z);

            return fs;
        }
        else {
            float sinEnter2 = 1.0f - dirV.z * dirV.z;
            float recRelIOR = eEnter[query.wlHint] / eExit[query.wlHint];// reciprocal of relative IOR.
            float sinExit2 = recRelIOR * recRelIOR * sinEnter2;

            if (sinExit2 >= 1.0f) {
                result->dirPDF = 0.0f;
                return RGBSpectrum::Zero();
            }
            float cosExit = std::sqrt(std::fmax(0.0f, 1.0f - sinExit2));
            Vector3D dirL = Vector3D(recRelIOR * -dirV.x, recRelIOR * -dirV.y, -cosExit);
            result->dirLocal = entering ? dirL : -dirL;
            result->dirPDF = 1.0f - reflectProb;
            result->sampledType = DirectionType::Transmission() | DirectionType::Delta0D() | (p.dispersive ? DirectionType::Dispersive() : DirectionType());

            RGBSpectrum ret = RGBSpectrum::Zero();
            ret[query.wlHint] = p.coeff[query.wlHint] * (1.0f - F[query.wlHint]);
            RGBSpectrum fs = ret / std::fabs(cosExit);

            return fs;
        }
    }

    RT_CALLABLE_PROGRAM RGBSpectrum SpecularBSDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM float SpecularBSDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        return 0.0f;
    }

    RT_CALLABLE_PROGRAM float SpecularBSDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        SpecularBSDF &p = *(SpecularBSDF*)params;
        return p.coeff.importance(query.wlHint);
    }

    // END: SpecularBSDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // MicrofacetBRDF

    struct MicrofacetBRDF {
        RGBSpectrum eta;
        RGBSpectrum k;
        float roughnessX;
        float roughnessY;
    };

    RT_CALLABLE_PROGRAM uint32_t MicrofacetReflectionSurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        MicrofacetBRDF &p = *(MicrofacetBRDF*)params;
        const MicrofacetReflectionSurfaceMaterial &mat = *(const MicrofacetReflectionSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue;
        optix::float2 texValueF2;
        texValue = optix::rtTex2D<optix::float4>(mat.texEta, texCoord.x, texCoord.y);
        p.eta = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.tex_k, texCoord.x, texCoord.y);
        p.k = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValueF2 = optix::rtTex2D<optix::float2>(mat.texRoughness, texCoord.x, texCoord.y);
        p.roughnessX = texValueF2.x;
        p.roughnessY = texValueF2.y;

        return sizeof(MicrofacetBRDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MicrofacetBRDF_getBaseColor(const uint32_t* params) {
        MicrofacetBRDF &p = *(MicrofacetBRDF*)params;

        FresnelDielectric fresnel(p.eta, p.k);

        return fresnel.evaluate(1.0f);
    }

    RT_CALLABLE_PROGRAM bool MicrofacetBRDF_matches(const uint32_t* params, DirectionType flags) {
        DirectionType m_type = DirectionType::Reflection() | DirectionType::HighFreq();
        return m_type.matches(flags);
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MicrofacetBRDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        MicrofacetBRDF &p = *(MicrofacetBRDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        FresnelDielectric fresnel(p.eta, p.k);

        float alphaX = p.roughnessX * p.roughnessX;
        float alphaY = p.roughnessY * p.roughnessY;
        GGXMicrofacetDistribution ggx(alphaX, alphaY);

        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        // JP: ハーフベクトルをサンプルして、最終的な方向サンプルを生成する。
        // EN: sample a half vector, then generate a resulting direction sample based on it.
        Normal3D m;
        float mPDF;
        float D = ggx.sample(dirV, uDir[0], uDir[1], &m, &mPDF);
        float dotHV = dot(dirV, m);
        if (dotHV <= 0) {
            result->dirPDF = 0.0f;
            return RGBSpectrum::Zero();
        }

        Vector3D dirL = 2 * dotHV * m - dirV;
        result->dirLocal = entering ? dirL : -dirL;
        if (dirL.z * dirV.z <= 0) {
            result->dirPDF = 0.0f;
            return RGBSpectrum::Zero();
        }

        float commonPDFTerm = 1.0f / (4 * dotHV);
        result->dirPDF = commonPDFTerm * mPDF;
        result->sampledType = DirectionType::Reflection() | DirectionType::HighFreq();

        RGBSpectrum F = fresnel.evaluate(dotHV);
        float G = ggx.evaluateSmithG1(dirV, m) * ggx.evaluateSmithG1(dirL, m);
        RGBSpectrum fs = F * D * G / (4 * dirV.z * dirL.z);

        //VLRAssert(fs.allFinite(), "fs: %s, F: %s, G, %g, D: %g, wlIdx: %u, qDir: %s, rDir: %s",
        //          fs.toString().c_str(), F.toString().c_str(), G, D, query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

        return fs;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MicrofacetBRDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        MicrofacetBRDF &p = *(MicrofacetBRDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        FresnelConductor fresnel(p.eta, p.k);

        float alphaX = p.roughnessX * p.roughnessX;
        float alphaY = p.roughnessY * p.roughnessY;
        GGXMicrofacetDistribution ggx(alphaX, alphaY);

        Vector3D dirL = entering ? dirLocal : -dirLocal;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;
        float dotNVdotNL = dirL.z * dirV.z;

        if (dotNVdotNL <= 0)
            return RGBSpectrum::Zero();

        Normal3D m = halfVector(dirV, dirL);
        float dotHV = dot(dirV, m);
        float D = ggx.evaluate(m);

        RGBSpectrum F = fresnel.evaluate(dotHV);
        float G = ggx.evaluateSmithG1(dirV, m) * ggx.evaluateSmithG1(dirL, m);
        RGBSpectrum fs = F * D * G / (4 * dotNVdotNL);

        //VLRAssert(fs.allFinite(), "fs: %s, F: %s, G, %g, D: %g, wlIdx: %u, qDir: %s, dir: %s",
        //          fs.toString().c_str(), F.toString().c_str(), G, D, query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

        return fs;
    }

    RT_CALLABLE_PROGRAM float MicrofacetBRDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        MicrofacetBRDF &p = *(MicrofacetBRDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        FresnelConductor fresnel(p.eta, p.k);

        float alphaX = p.roughnessX * p.roughnessX;
        float alphaY = p.roughnessY * p.roughnessY;
        GGXMicrofacetDistribution ggx(alphaX, alphaY);

        Vector3D dirL = entering ? dirLocal : -dirLocal;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;
        float dotNVdotNL = dirL.z * dirV.z;

        if (dotNVdotNL <= 0.0f)
            return 0.0f;

        Normal3D m = halfVector(dirV, dirL);
        float dotHV = dot(dirV, m);
        if (dotHV <= 0)
            return 0.0f;

        float mPDF = ggx.evaluatePDF(dirV, m);
        float commonPDFTerm = 1.0f / (4 * dotHV);
        float ret = commonPDFTerm * mPDF;

        //VLRAssert(std::isfinite(commonPDFTerm) && std::isfinite(mPDF),
        //          "commonPDFTerm: %g, mPDF: %g, wlIdx: %u, qDir: %s, dir: %s",
        //          commonPDFTerm, mPDF, query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

        return ret;
    }

    RT_CALLABLE_PROGRAM float MicrofacetBRDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        MicrofacetBRDF &p = *(MicrofacetBRDF*)params;

        FresnelDielectric fresnel(p.eta, p.k);

        float expectedDotHV = query.dirLocal.z;

        return fresnel.evaluate(expectedDotHV).importance(query.wlHint);
    }

    // END: MicrofacetBRDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // MicrofacetBSDF

    struct MicrofacetBSDF {
        RGBSpectrum coeff;
        RGBSpectrum etaExt;
        RGBSpectrum etaInt;
        float roughnessX;
        float roughnessY;
    };

    RT_CALLABLE_PROGRAM uint32_t MicrofacetScatteringSurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        MicrofacetBSDF &p = *(MicrofacetBSDF*)params;
        const MicrofacetScatteringSurfaceMaterial &mat = *(const MicrofacetScatteringSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue;
        optix::float2 texValueF2;
        texValue = optix::rtTex2D<optix::float4>(mat.texCoeff, texCoord.x, texCoord.y);
        p.coeff = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.texEtaExt, texCoord.x, texCoord.y);
        p.etaExt = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.texEtaInt, texCoord.x, texCoord.y);
        p.etaInt = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValueF2 = optix::rtTex2D<optix::float2>(mat.texRoughness, texCoord.x, texCoord.y);
        p.roughnessX = texValueF2.x;
        p.roughnessY = texValueF2.y;

        return sizeof(MicrofacetBSDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MicrofacetBSDF_getBaseColor(const uint32_t* params) {
        MicrofacetBSDF &p = *(MicrofacetBSDF*)params;

        return p.coeff;
    }

    RT_CALLABLE_PROGRAM bool MicrofacetBSDF_matches(const uint32_t* params, DirectionType flags) {
        DirectionType m_type = DirectionType::WholeSphere() | DirectionType::HighFreq();
        return m_type.matches(flags);
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MicrofacetBSDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        MicrofacetBSDF &p = *(MicrofacetBSDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        const RGBSpectrum &eEnter = entering ? p.etaExt : p.etaInt;
        const RGBSpectrum &eExit = entering ? p.etaInt : p.etaExt;
        FresnelDielectric fresnel(eEnter, eExit);

        float alphaX = p.roughnessX * p.roughnessX;
        float alphaY = p.roughnessY * p.roughnessY;
        GGXMicrofacetDistribution ggx(alphaX, alphaY);

        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        // JP: ハーフベクトルをサンプルする。
        // EN: sample a half vector.
        Normal3D m;
        float mPDF;
        float D = ggx.sample(dirV, uDir[0], uDir[1], &m, &mPDF);
        float dotHV = dot(dirV, m);
        if (dotHV <= 0 || std::isnan(D)) {
            result->dirPDF = 0.0f;
            return RGBSpectrum::Zero();
        }

        // JP: サンプルしたハーフベクトルからフレネル項の値を計算して、反射か透過を選択する。
        // EN: calculate the Fresnel term using the sampled half vector, then select reflection or transmission.
        RGBSpectrum F = fresnel.evaluate(dotHV);
        float reflectProb = F.importance(query.wlHint);
        if (query.dirTypeFilter.isReflection())
            reflectProb = 1.0f;
        if (query.dirTypeFilter.isTransmission())
            reflectProb = 0.0f;
        if (uComponent < reflectProb) {
            // JP: 最終的な方向サンプルを生成する。
            // EN: calculate a resulting direction.
            Vector3D dirL = 2 * dotHV * m - dirV;
            result->dirLocal = entering ? dirL : -dirL;
            if (dirL.z * dirV.z <= 0) {
                result->dirPDF = 0.0f;
                return RGBSpectrum::Zero();
            }
            float commonPDFTerm = reflectProb / (4 * dotHV);
            result->dirPDF = commonPDFTerm * mPDF;
            result->sampledType = DirectionType::Reflection() | DirectionType::HighFreq();

            float G = ggx.evaluateSmithG1(dirV, m) * ggx.evaluateSmithG1(dirL, m);
            RGBSpectrum fs = F * D * G / (4 * dirV.z * dirL.z);

            //VLRAssert(fs.allFinite(), "fs: %s, F: %g, %g, %g, G, %g, D: %g, wlIdx: %u, qDir: (%g, %g, %g), rDir: (%g, %g, %g)",
            //          fs.toString().c_str(), F.toString().c_str(), G, D, query.wlHint, 
            //          dirV.x, dirV.y, dirV.z, dirL.x, dirL.y, dirL.z);

            return fs;
        }
        else {
            // JP: 最終的な方向サンプルを生成する。
            // EN: calculate a resulting direction.
            float recRelIOR = eEnter[query.wlHint] / eExit[query.wlHint];
            float innerRoot = 1 + recRelIOR * recRelIOR * (dotHV * dotHV - 1);
            if (innerRoot < 0) {
                result->dirPDF = 0.0f;
                return RGBSpectrum::Zero();
            }
            Vector3D dirL = (recRelIOR * dotHV - std::sqrt(innerRoot)) * m - recRelIOR * dirV;
            result->dirLocal = entering ? dirL : -dirL;
            if (dirL.z * dirV.z >= 0) {
                result->dirPDF = 0.0f;
                return RGBSpectrum::Zero();
            }
            float dotHL = dot(dirL, m);
            float commonPDFTerm = (1 - reflectProb) / std::pow(eEnter[query.wlHint] * dotHV + eExit[query.wlHint] * dotHL, 2);
            result->dirPDF = commonPDFTerm * mPDF * eExit[query.wlHint] * eExit[query.wlHint] * std::fabs(dotHL);
            result->sampledType = DirectionType::Transmission() | DirectionType::HighFreq();

            // JP: マイクロファセットBSDFの各項の値を波長成分ごとに計算する。
            // EN: calculate the value of each term of the microfacet BSDF for each wavelength component.
            RGBSpectrum ret = RGBSpectrum::Zero();
            for (int wlIdx = 0; wlIdx < RGBSpectrum::NumComponents(); ++wlIdx) {
                Normal3D m_wl = normalize(-(eEnter[wlIdx] * dirV + eExit[wlIdx] * dirL) * (entering ? 1 : -1));
                float dotHV_wl = dot(dirV, m_wl);
                float dotHL_wl = dot(dirL, m_wl);
                float F_wl = fresnel.evaluate(dotHV_wl, wlIdx);
                float G_wl = ggx.evaluateSmithG1(dirV, m_wl) * ggx.evaluateSmithG1(dirL, m_wl);
                float D_wl = ggx.evaluate(m_wl);
                ret[wlIdx] = std::fabs(dotHV_wl * dotHL_wl) * (1 - F_wl) * G_wl * D_wl / std::pow(eEnter[wlIdx] * dotHV_wl + eExit[wlIdx] * dotHL_wl, 2);

                //VLRAssert(std::isfinite(ret[wlIdx]), "fs: %g, F: %g, G, %g, D: %g, wlIdx: %u, qDir: %s",
                //          ret[wlIdx], F_wl, G_wl, D_wl, query.wlHint, dirV.toString().c_str());
            }
            ret /= std::fabs(dirV.z * dirL.z);
            ret *= eEnter * eEnter;
            //ret *= query.adjoint ? (eExit * eExit) : (eEnter * eEnter);// adjoint: need to cancel eEnter^2 / eExit^2 => eEnter^2 * (eExit^2 / eEnter^2)

            //VLRAssert(ret.allFinite(), "fs: %s, wlIdx: %u, qDir: %s, rDir: %s",
            //          ret.toString().c_str(), query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

            return ret;
        }
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MicrofacetBSDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        MicrofacetBSDF &p = *(MicrofacetBSDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        const RGBSpectrum &eEnter = entering ? p.etaExt : p.etaInt;
        const RGBSpectrum &eExit = entering ? p.etaInt : p.etaExt;
        FresnelDielectric fresnel(eEnter, eExit);

        float alphaX = p.roughnessX * p.roughnessX;
        float alphaY = p.roughnessY * p.roughnessY;
        GGXMicrofacetDistribution ggx(alphaX, alphaY);

        Vector3D dirL = entering ? dirLocal : -dirLocal;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;
        float dotNVdotNL = dirL.z * dirV.z;

        if (dotNVdotNL > 0 && query.dirTypeFilter.matches(DirectionType::Reflection() | DirectionType::AllFreq())) {
            Normal3D m = halfVector(dirV, dirL);
            float dotHV = dot(dirV, m);
            float D = ggx.evaluate(m);

            RGBSpectrum F = fresnel.evaluate(dotHV);
            float G = ggx.evaluateSmithG1(dirV, m) * ggx.evaluateSmithG1(dirL, m);
            RGBSpectrum fs = F * D * G / (4 * dotNVdotNL);

            //VLRAssert(fs.allFinite(), "fs: %s, F: %s, G, %g, D: %g, wlIdx: %u, qDir: %s, dir: %s",
            //          fs.toString().c_str(), F.toString().c_str(), G, D, query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

            return fs;
        }
        else if (dotNVdotNL < 0 && query.dirTypeFilter.matches(DirectionType::Transmission() | DirectionType::AllFreq())) {
            RGBSpectrum ret = RGBSpectrum::Zero();
            for (int wlIdx = 0; wlIdx < RGBSpectrum::NumComponents(); ++wlIdx) {
                Normal3D m_wl = normalize(-(eEnter[wlIdx] * dirV + eExit[wlIdx] * dirL) * (entering ? 1 : -1));
                float dotHV_wl = dot(dirV, m_wl);
                float dotHL_wl = dot(dirL, m_wl);
                float F_wl = fresnel.evaluate(dotHV_wl, wlIdx);
                float G_wl = ggx.evaluateSmithG1(dirV, m_wl) * ggx.evaluateSmithG1(dirL, m_wl);
                float D_wl = ggx.evaluate(m_wl);
                ret[wlIdx] = std::fabs(dotHV_wl * dotHL_wl) * (1 - F_wl) * G_wl * D_wl / std::pow(eEnter[wlIdx] * dotHV_wl + eExit[wlIdx] * dotHL_wl, 2);

                //VLRAssert(std::isfinite(ret[wlIdx]), "fs: %g, F: %g, G, %g, D: %g, wlIdx: %u, qDir: %s, dir: %s",
                //          ret[wlIdx], F_wl, G_wl, D_wl, query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());
            }
            ret /= std::fabs(dotNVdotNL);
            ret *= eEnter * eEnter;
            //ret *= query.adjoint ? (eExit * eExit) : (eEnter * eEnter);// !adjoint: eExit^2 * (eEnter / eExit)^2

            //VLRAssert(ret.allFinite(), "fs: %s, wlIdx: %u, qDir: %s, dir: %s",
            //          ret.toString().c_str(), query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

            return ret;
        }

        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM float MicrofacetBSDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        MicrofacetBSDF &p = *(MicrofacetBSDF*)params;

        bool entering = query.dirLocal.z >= 0.0f;

        const RGBSpectrum &eEnter = entering ? p.etaExt : p.etaInt;
        const RGBSpectrum &eExit = entering ? p.etaInt : p.etaExt;
        FresnelDielectric fresnel(eEnter, eExit);

        float alphaX = p.roughnessX * p.roughnessX;
        float alphaY = p.roughnessY * p.roughnessY;
        GGXMicrofacetDistribution ggx(alphaX, alphaY);

        Vector3D dirL = entering ? dirLocal : -dirLocal;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;
        float dotNVdotNL = dirL.z * dirV.z;
        if (dotNVdotNL == 0)
            return 0.0f;

        Normal3D m;
        if (dotNVdotNL > 0)
            m = halfVector(dirV, dirL);
        else
            m = normalize(-(eEnter[query.wlHint] * dirV + eExit[query.wlHint] * dirL));
        float dotHV = dot(dirV, m);
        if (dotHV <= 0)
            return 0.0f;
        float mPDF = ggx.evaluatePDF(dirV, m);

        RGBSpectrum F = fresnel.evaluate(dotHV);
        float reflectProb = F.importance(query.wlHint);
        if (query.dirTypeFilter.isReflection())
            reflectProb = 1.0f;
        if (query.dirTypeFilter.isTransmission())
            reflectProb = 0.0f;
        if (dotNVdotNL > 0) {
            float commonPDFTerm = reflectProb / (4 * dotHV);

            //VLRAssert(std::isfinite(commonPDFTerm) && std::isfinite(mPDF),
            //          "commonPDFTerm: %g, mPDF: %g, F: %s, wlIdx: %u, qDir: %s, dir: %s",
            //          commonPDFTerm, mPDF, F.toString().c_str(), query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

            return commonPDFTerm * mPDF;
        }
        else {
            float dotHL = dot(dirL, m);
            float commonPDFTerm = (1 - reflectProb) / std::pow(eEnter[query.wlHint] * dotHV + eExit[query.wlHint] * dotHL, 2);

            //VLRAssert(std::isfinite(commonPDFTerm) && std::isfinite(mPDF),
            //          "commonPDFTerm: %g, mPDF: %g, F: %s, wlIdx: %u, qDir: %s, dir: %s",
            //          commonPDFTerm, mPDF, F.toString().c_str(), query.wlHint, dirV.toString().c_str(), dirL.toString().c_str());

            return commonPDFTerm * mPDF * eExit[query.wlHint] * eExit[query.wlHint] * std::fabs(dotHL);
        }
    }

    RT_CALLABLE_PROGRAM float MicrofacetBSDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        MicrofacetBSDF &p = *(MicrofacetBSDF*)params;
        return p.coeff.importance(query.wlHint);
    }

    // END: MicrofacetBSDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // UE4 (Modified) BRDF

    struct UE4BRDF {
        RGBSpectrum baseColor;
        float roughness;
        float metallic;
    };

    RT_CALLABLE_PROGRAM uint32_t UE4SurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        UE4BRDF &p = *(UE4BRDF*)params;
        const UE4SurfaceMaterial &mat = *(const UE4SurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue;
        texValue = optix::rtTex2D<optix::float4>(mat.texBaseColor, texCoord.x, texCoord.y);
        p.baseColor = RGBSpectrum(texValue.x, texValue.y, texValue.z);
        texValue = optix::rtTex2D<optix::float4>(mat.texOcclusionRoughnessMetallic, texCoord.x, texCoord.y);
        p.roughness = texValue.y;
        p.metallic = texValue.z;

        return sizeof(UE4BRDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum UE4BRDF_getBaseColor(const uint32_t* params) {
        UE4BRDF &p = *(UE4BRDF*)params;

        return p.baseColor;
    }

    RT_CALLABLE_PROGRAM bool UE4BRDF_matches(const uint32_t* params, DirectionType flags) {
        DirectionType m_type = DirectionType::Reflection() | DirectionType::LowFreq() | DirectionType::HighFreq();
        return m_type.matches(flags);
    }

    RT_CALLABLE_PROGRAM RGBSpectrum UE4BRDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        UE4BRDF &p = *(UE4BRDF*)params;

        const float specular = 0.5f;
        float alpha = p.roughness * p.roughness;
        GGXMicrofacetDistribution ggx(alpha, alpha);

        bool entering = query.dirLocal.z >= 0.0f;
        Vector3D dirL;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        float expectedF_D90 = 0.5f * p.roughness + 2 * p.roughness * query.dirLocal.z * query.dirLocal.z;
        float oneMinusDotVN5 = std::pow(1 - dirV.z, 5);
        float expectedDiffuseFresnel = lerp(1.0f, expectedF_D90, oneMinusDotVN5);
        float iBaseColor = p.baseColor.importance(query.wlHint) * expectedDiffuseFresnel * expectedDiffuseFresnel * lerp(1.0f, 1.0f / 1.51f, p.roughness);

        RGBSpectrum specularF0Color = lerp(0.08f * specular * RGBSpectrum::One(), p.baseColor, p.metallic);
        float expectedOneMinusDotVH5 = std::pow(1 - dirV.z, 5);
        float iSpecularF0 = specularF0Color.importance(query.wlHint);

        float diffuseWeight = iBaseColor * (1 - p.metallic);
        float specularWeight = lerp(iSpecularF0, 1.0f, expectedOneMinusDotVH5);

        float weights[] = { diffuseWeight, specularWeight };
        float probSelection;
        float sumWeights = 0.0f;
        uint32_t component = sampleDiscrete(weights, 2, uComponent, &probSelection, &sumWeights, &uComponent);

        float diffuseDirPDF, specularDirPDF;
        RGBSpectrum fs;
        Normal3D m;
        float dotLH;
        float D;
        if (component == 0) {
            result->sampledType = DirectionType::Reflection() | DirectionType::LowFreq();

            // JP: コサイン分布からサンプルする。
            // EN: sample based on cosine distribution.
            dirL = cosineSampleHemisphere(uDir[0], uDir[1]);
            diffuseDirPDF = dirL.z / M_PIf;

            // JP: 同じ方向サンプルを別の要素からサンプルする確率密度を求める。
            // EN: calculate PDFs to generate the sampled direction from the other distributions.
            m = halfVector(dirL, dirV);
            dotLH = dot(dirL, m);
            float commonPDFTerm = 1.0f / (4 * dotLH);
            specularDirPDF = commonPDFTerm * ggx.evaluatePDF(dirV, m);

            D = ggx.evaluate(m);
        }
        else if (component == 1) {
            result->sampledType = DirectionType::Reflection() | DirectionType::HighFreq();

            // ----------------------------------------------------------------
            // JP: ベーススペキュラー層のマイクロファセット分布からサンプルする。
            // EN: sample based on the base specular microfacet distribution.
            float mPDF;
            D = ggx.sample(dirV, uDir[0], uDir[1], &m, &mPDF);
            float dotVH = dot(dirV, m);
            dotLH = dotVH;
            dirL = 2 * dotVH * m - dirV;
            if (dirL.z * dirV.z <= 0) {
                result->dirPDF = 0.0f;
                return RGBSpectrum::Zero();
            }
            float commonPDFTerm = 1.0f / (4 * dotLH);
            specularDirPDF = commonPDFTerm * mPDF;
            // ----------------------------------------------------------------

            // JP: 同じ方向サンプルを別の要素からサンプルする確率密度を求める。
            // EN: calculate PDFs to generate the sampled direction from the other distributions.
            diffuseDirPDF = dirL.z / M_PIf;
        }

        float oneMinusDotLH5 = std::pow(1 - dotLH, 5);

        float G = ggx.evaluateSmithG1(dirL, m) * ggx.evaluateSmithG1(dirV, m);
        RGBSpectrum F = lerp(specularF0Color, RGBSpectrum::One(), oneMinusDotLH5);

        float microfacetDenom = 4 * dirL.z * dirV.z;
        RGBSpectrum specularValue = F * ((D * G) / microfacetDenom);

        float F_D90 = 0.5f * p.roughness + 2 * p.roughness * dotLH * dotLH;
        float oneMinusDotLN5 = std::pow(1 - dirL.z, 5);
        float diffuseFresnelOut = lerp(1.0f, F_D90, oneMinusDotVN5);
        float diffuseFresnelIn = lerp(1.0f, F_D90, oneMinusDotLN5);
        RGBSpectrum diffuseValue = p.baseColor * ((diffuseFresnelOut * diffuseFresnelIn * lerp(1.0f, 1.0f / 1.51f, p.roughness) / M_PIf) * (1 - p.metallic));

        RGBSpectrum ret = diffuseValue + specularValue;

        result->dirLocal = entering ? dirL : -dirL;

        // PDF based on the single-sample model MIS.
        result->dirPDF = (diffuseDirPDF * diffuseWeight + specularDirPDF * specularWeight) / sumWeights;

        return ret;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum UE4BRDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        UE4BRDF &p = *(UE4BRDF*)params;

        const float specular = 0.5f;
        float alpha = p.roughness * p.roughness;
        GGXMicrofacetDistribution ggx(alpha, alpha);

        if (dirLocal.z * query.dirLocal.z <= 0) {
            return RGBSpectrum::Zero();
        }

        bool entering = query.dirLocal.z >= 0.0f;
        Vector3D dirL = entering ? dirLocal : -dirLocal;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        Normal3D m = halfVector(dirL, dirV);
        float dotLH = dot(dirL, m);

        float oneMinusDotLH5 = std::pow(1 - dotLH, 5);

        RGBSpectrum specularF0Color = lerp(0.08f * specular * RGBSpectrum::One(), p.baseColor, p.metallic);

        float D = ggx.evaluate(m);
        float G = ggx.evaluateSmithG1(dirL, m) * ggx.evaluateSmithG1(dirV, m);
        RGBSpectrum F = lerp(specularF0Color, RGBSpectrum::One(), oneMinusDotLH5);

        float microfacetDenom = 4 * dirL.z * dirV.z;
        RGBSpectrum specularValue = F * ((D * G) / microfacetDenom);

        float F_D90 = 0.5f * p.roughness + 2 * p.roughness * dotLH * dotLH;
        float oneMinusDotVN5 = std::pow(1 - dirV.z, 5);
        float oneMinusDotLN5 = std::pow(1 - dirL.z, 5);
        float diffuseFresnelOut = lerp(1.0f, F_D90, oneMinusDotVN5);
        float diffuseFresnelIn = lerp(1.0f, F_D90, oneMinusDotLN5);

        RGBSpectrum diffuseValue = p.baseColor * ((diffuseFresnelOut * diffuseFresnelIn * lerp(1.0f, 1.0f / 1.51f, p.roughness) / M_PIf) * (1 - p.metallic));

        RGBSpectrum ret = diffuseValue + specularValue;

        return ret;
    }

    RT_CALLABLE_PROGRAM float UE4BRDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        UE4BRDF &p = *(UE4BRDF*)params;

        const float specular = 0.5f;
        float alpha = p.roughness * p.roughness;
        GGXMicrofacetDistribution ggx(alpha, alpha);

        bool entering = query.dirLocal.z >= 0.0f;
        Vector3D dirL = entering ? dirLocal : -dirLocal;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        Normal3D m = halfVector(dirL, dirV);
        float dotLH = dot(dirL, m);
        float commonPDFTerm = 1.0f / (4 * dotLH);

        float expectedF_D90 = 0.5f * p.roughness + 2 * p.roughness * query.dirLocal.z * query.dirLocal.z;
        float oneMinusDotVN5 = std::pow(1 - dirV.z, 5);
        float expectedDiffuseFresnel = lerp(1.0f, expectedF_D90, oneMinusDotVN5);
        float iBaseColor = p.baseColor.importance(query.wlHint) * expectedDiffuseFresnel * expectedDiffuseFresnel * lerp(1.0f, 1.0f / 1.51f, p.roughness);

        RGBSpectrum specularF0Color = lerp(0.08f * specular * RGBSpectrum::One(), p.baseColor, p.metallic);
        float expectedOneMinusDotVH5 = std::pow(1 - dirV.z, 5);
        float iSpecularF0 = specularF0Color.importance(query.wlHint);

        float diffuseWeight = iBaseColor * (1 - p.metallic);
        float specularWeight = lerp(iSpecularF0, 1.0f, expectedOneMinusDotVH5);

        float sumWeights = diffuseWeight + specularWeight;

        float diffuseDirPDF = dirL.z / M_PIf;
        float specularDirPDF = commonPDFTerm * ggx.evaluatePDF(dirV, m);

        float ret = (diffuseDirPDF * diffuseWeight + specularDirPDF * specularWeight) / sumWeights;

        return ret;
    }

    RT_CALLABLE_PROGRAM float UE4BRDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        UE4BRDF &p = *(UE4BRDF*)params;

        const float specular = 0.5f;

        bool entering = query.dirLocal.z >= 0.0f;
        Vector3D dirV = entering ? query.dirLocal : -query.dirLocal;

        float expectedF_D90 = 0.5f * p.roughness + 2 * p.roughness * query.dirLocal.z * query.dirLocal.z;
        float oneMinusDotVN5 = std::pow(1 - dirV.z, 5);
        float expectedDiffuseFresnel = lerp(1.0f, expectedF_D90, oneMinusDotVN5);
        float iBaseColor = p.baseColor.importance(query.wlHint) * expectedDiffuseFresnel * expectedDiffuseFresnel * lerp(1.0f, 1.0f / 1.51f, p.roughness);

        RGBSpectrum specularF0Color = lerp(0.08f * specular * RGBSpectrum::One(), p.baseColor, p.metallic);
        float expectedOneMinusDotVH5 = std::pow(1 - dirV.z, 5);
        float iSpecularF0 = specularF0Color.importance(query.wlHint);

        float diffuseWeight = iBaseColor * (1 - p.metallic);
        float specularWeight = lerp(iSpecularF0, 1.0f, expectedOneMinusDotVH5);

        return diffuseWeight + specularWeight;
    }

    // END: UE4 (Modified) BRDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // NullEDF

    RT_CALLABLE_PROGRAM uint32_t NullEDF_setupEDF(const uint32_t* matDesc, const SurfacePoint &surfPt, uint32_t* params) {
        return 0;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum NullEDF_evaluateEmittanceInternal(const uint32_t* params) {
        return RGBSpectrum::Zero();
    }

    RT_CALLABLE_PROGRAM RGBSpectrum NullEDF_evaluateEDFInternal(const uint32_t* params, const EDFQuery &query, const Vector3D &dirLocal) {
        return RGBSpectrum::Zero();
    }

    // END: NullEDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // DiffuseEDF

    struct DiffuseEDF {
        RGBSpectrum emittance;
    };

    RT_CALLABLE_PROGRAM uint32_t DiffuseEmitterSurfaceMaterial_setupEDF(const uint32_t* matDesc, const SurfacePoint &surfPt, uint32_t* params) {
        DiffuseEDF &p = *(DiffuseEDF*)params;
        const DiffuseEmitterSurfaceMaterial &mat = *(const DiffuseEmitterSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        Point3D texCoord = textureMap(mat.texMap, surfPt);

        optix::float4 texValue = optix::rtTex2D<optix::float4>(mat.texEmittance, texCoord.x, texCoord.y);
        p.emittance = RGBSpectrum(texValue.x, texValue.y, texValue.z);

        return sizeof(DiffuseEDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum DiffuseEDF_evaluateEmittanceInternal(const uint32_t* params) {
        DiffuseEDF &p = *(DiffuseEDF*)params;
        return p.emittance;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum DiffuseEDF_evaluateEDFInternal(const uint32_t* params, const EDFQuery &query, const Vector3D &dirLocal) {
        return RGBSpectrum(dirLocal.z > 0.0f ? 1.0f / M_PIf : 0.0f);
    }

    // END: DiffuseEDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // MultiBSDF / MultiEDF

    // bsdf0-3: param offsets
    // numBSDFs
    // --------------------------------
    // BSDF0 procedure set index
    // BSDF0 params
    // ...
    // BSDF3 procedure set index
    // BSDF3 params
    struct MultiBSDF {
        struct {
            unsigned int bsdf0 : 6;
            unsigned int bsdf1 : 6;
            unsigned int bsdf2 : 6;
            unsigned int bsdf3 : 6;
            unsigned int numBSDFs : 8;
        };
    };

    RT_CALLABLE_PROGRAM uint32_t MultiSurfaceMaterial_setupBSDF(const uint32_t* matDesc, const SurfacePoint &surfPt, bool wavelengthSelected, uint32_t* params) {
        MultiBSDF &p = *(MultiBSDF*)params;
        const MultiSurfaceMaterial &mat = *(const MultiSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        uint32_t baseIndex = sizeof(MultiBSDF) / 4;
        const uint32_t matOffsets[] = { mat.matOffset0, mat.matOffset1, mat.matOffset2, mat.matOffset3 };
        uint32_t bsdfOffsets[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < mat.numMaterials; ++i) {
            bsdfOffsets[i] = baseIndex;

            const SurfaceMaterialHead &matHead = *(const SurfaceMaterialHead*)(matDesc + matOffsets[i]);
            //rtPrintf("%d: %u, %u, %u, %u\n", i, matHead.progSetupBSDF, matHead.bsdfProcedureSetIndex, matHead.progSetupEDF, matHead.edfProcedureSetIndex);
            progSigSetupBSDF setupBSDF = (progSigSetupBSDF)matHead.progSetupBSDF;
            *(uint32_t*)(params + baseIndex++) = matHead.bsdfProcedureSetIndex;
            baseIndex += setupBSDF((const uint32_t*)&matHead, surfPt, wavelengthSelected, params + baseIndex);
        }

        p.bsdf0 = bsdfOffsets[0];
        p.bsdf1 = bsdfOffsets[1];
        p.bsdf2 = bsdfOffsets[2];
        p.bsdf3 = bsdfOffsets[3];
        p.numBSDFs = mat.numMaterials;

        //rtPrintf("%u, %u, %u, %u, %u mats\n", p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3, p.numBSDFs);

        return baseIndex;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MultiBSDF_getBaseColor(const uint32_t* params) {
        const MultiBSDF &p = *(const MultiBSDF*)params;

        uint32_t bsdfOffsets[4] = { p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3 };

        RGBSpectrum ret;
        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigGetBaseColor getBaseColor = (progSigGetBaseColor)procSet.progGetBaseColor;

            ret += getBaseColor(bsdf + 1);
        }

        return ret;
    }

    RT_CALLABLE_PROGRAM bool MultiBSDF_matches(const uint32_t* params, DirectionType flags) {
        const MultiBSDF &p = *(const MultiBSDF*)params;

        uint32_t bsdfOffsets[4] = { p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3 };

        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigBSDFmatches matches = (progSigBSDFmatches)procSet.progBSDFmatches;

            if (matches(bsdf + 1, flags))
                return true;
        }

        return false;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MultiBSDF_sampleBSDFInternal(const uint32_t* params, const BSDFQuery &query, float uComponent, const float uDir[2], BSDFQueryResult* result) {
        const MultiBSDF &p = *(const MultiBSDF*)params;

        uint32_t bsdfOffsets[4] = { p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3 };

        float weights[4];
        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigBSDFWeightInternal weightInternal = (progSigBSDFWeightInternal)procSet.progWeightInternal;

            weights[i] = weightInternal(bsdf + 1, query);
        }

        // JP: 各BSDFのウェイトに基づいて方向のサンプルを行うBSDFを選択する。
        // EN: Based on the weight of each BSDF, select a BSDF from which direction sampling.
        float tempProb;
        float sumWeights;
        uint32_t idx = sampleDiscrete(weights, p.numBSDFs, uComponent, &tempProb, &sumWeights, &uComponent);
        if (sumWeights == 0.0f) {
            result->dirPDF = 0.0f;
            return RGBSpectrum::Zero();
        }

        const uint32_t* selectedBSDF = params + bsdfOffsets[idx];
        uint32_t selProcIdx = *(const uint32_t*)selectedBSDF;
        const BSDFProcedureSet selProcSet = pv_bsdfProcedureSetBuffer[selProcIdx];
        progSigSampleBSDFInternal sampleInternal = (progSigSampleBSDFInternal)selProcSet.progSampleBSDFInternal;

        // JP: 選択したBSDFから方向をサンプリングする。
        // EN: sample a direction from the selected BSDF.
        RGBSpectrum value = sampleInternal(selectedBSDF + 1, query, uComponent, uDir, result);
        result->dirPDF *= weights[idx];
        if (result->dirPDF == 0.0f) {
            result->dirPDF = 0.0f;
            return RGBSpectrum::Zero();
        }

        // JP: サンプルした方向に関するBSDFの値の合計と、single-sample model MISに基づいた確率密度を計算する。
        // EN: calculate the total of BSDF values and a PDF based on the single-sample model MIS for the sampled direction.
        if (!result->sampledType.isDelta()) {
            for (int i = 0; i < p.numBSDFs; ++i) {
                const uint32_t* bsdf = params + bsdfOffsets[i];
                uint32_t procIdx = *(const uint32_t*)bsdf;
                const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
                progSigBSDFmatches matches = (progSigBSDFmatches)procSet.progBSDFmatches;
                progSigEvaluateBSDF_PDFInternal evaluatePDFInternal = (progSigEvaluateBSDF_PDFInternal)procSet.progEvaluateBSDF_PDFInternal;

                if (i != idx && matches(bsdf + 1, query.dirTypeFilter))
                    result->dirPDF += evaluatePDFInternal(bsdf + 1, query, result->dirLocal) * weights[i];
            }

            BSDFQuery mQuery = query;
            mQuery.dirTypeFilter &= sideTest(query.geometricNormalLocal, query.dirLocal, result->dirLocal);
            value = RGBSpectrum::Zero();
            for (int i = 0; i < p.numBSDFs; ++i) {
                const uint32_t* bsdf = params + bsdfOffsets[i];
                uint32_t procIdx = *(const uint32_t*)bsdf;
                const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
                progSigBSDFmatches matches = (progSigBSDFmatches)procSet.progBSDFmatches;
                progSigEvaluateBSDFInternal evaluateBSDFInternal = (progSigEvaluateBSDFInternal)procSet.progEvaluateBSDFInternal;

                if (!matches(bsdf + 1, mQuery.dirTypeFilter))
                    continue;
                value += evaluateBSDFInternal(bsdf + 1, mQuery, result->dirLocal);
            }
        }
        result->dirPDF /= sumWeights;

        return value;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MultiBSDF_evaluateBSDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        const MultiBSDF &p = *(const MultiBSDF*)params;

        uint32_t bsdfOffsets[4] = { p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3 };

        RGBSpectrum retValue = RGBSpectrum::Zero();
        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigBSDFmatches matches = (progSigBSDFmatches)procSet.progBSDFmatches;
            progSigEvaluateBSDFInternal evaluateBSDFInternal = (progSigEvaluateBSDFInternal)procSet.progEvaluateBSDFInternal;

            if (!matches(bsdf + 1, query.dirTypeFilter))
                continue;
            retValue += evaluateBSDFInternal(bsdf + 1, query, dirLocal);
        }
        return retValue;
    }

    RT_CALLABLE_PROGRAM float MultiBSDF_evaluateBSDF_PDFInternal(const uint32_t* params, const BSDFQuery &query, const Vector3D &dirLocal) {
        const MultiBSDF &p = *(const MultiBSDF*)params;

        uint32_t bsdfOffsets[4] = { p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3 };

        float sumWeights = 0.0f;
        float weights[4];
        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigBSDFWeightInternal weightInternal = (progSigBSDFWeightInternal)procSet.progWeightInternal;

            weights[i] = weightInternal(bsdf + 1, query);
            sumWeights += weights[i];
        }
        if (sumWeights == 0.0f)
            return 0.0f;

        float retPDF = 0.0f;
        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigEvaluateBSDF_PDFInternal evaluatePDFInternal = (progSigEvaluateBSDF_PDFInternal)procSet.progEvaluateBSDF_PDFInternal;

            if (weights[i] > 0)
                retPDF += evaluatePDFInternal(bsdf + 1, query, dirLocal) * weights[i];
        }
        retPDF /= sumWeights;

        return retPDF;
    }

    RT_CALLABLE_PROGRAM float MultiBSDF_weightInternal(const uint32_t* params, const BSDFQuery &query) {
        const MultiBSDF &p = *(const MultiBSDF*)params;

        uint32_t bsdfOffsets[4] = { p.bsdf0, p.bsdf1, p.bsdf2, p.bsdf3 };

        float ret = 0.0f;
        for (int i = 0; i < p.numBSDFs; ++i) {
            const uint32_t* bsdf = params + bsdfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)bsdf;
            const BSDFProcedureSet procSet = pv_bsdfProcedureSetBuffer[procIdx];
            progSigBSDFWeightInternal weightInternal = (progSigBSDFWeightInternal)procSet.progWeightInternal;

            ret += weightInternal(bsdf + 1, query);
        }

        return ret;
    }

    // edf0-3: param offsets
    // numEDFs
    // --------------------------------
    // EDF0 procedure set index
    // EDF0 params
    // ...
    // EDF3 procedure set index
    // EDF3 params
    struct MultiEDF {
        struct {
            unsigned int edf0 : 6;
            unsigned int edf1 : 6;
            unsigned int edf2 : 6;
            unsigned int edf3 : 6;
            unsigned int numEDFs : 8;
        };
    };

    RT_CALLABLE_PROGRAM uint32_t MultiSurfaceMaterial_setupEDF(const uint32_t* matDesc, const SurfacePoint &surfPt, uint32_t* params) {
        MultiEDF &p = *(MultiEDF*)params;
        const MultiSurfaceMaterial &mat = *(const MultiSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        uint32_t baseIndex = sizeof(MultiEDF) / 4;
        const uint32_t matOffsets[] = { mat.matOffset0, mat.matOffset1, mat.matOffset2, mat.matOffset3 };
        uint32_t edfOffsets[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < mat.numMaterials; ++i) {
            edfOffsets[i] = baseIndex;

            const SurfaceMaterialHead &matHead = *(const SurfaceMaterialHead*)(matDesc + matOffsets[i]);
            progSigSetupEDF setupEDF = (progSigSetupEDF)matHead.progSetupEDF;
            *(uint32_t*)(params + baseIndex++) = matHead.edfProcedureSetIndex;
            baseIndex += setupEDF((const uint32_t*)&matHead, surfPt, params + baseIndex);
        }

        p.edf0 = edfOffsets[0];
        p.edf1 = edfOffsets[1];
        p.edf2 = edfOffsets[2];
        p.edf3 = edfOffsets[3];
        p.numEDFs = mat.numMaterials;

        return baseIndex;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MultiEDF_evaluateEmittanceInternal(const uint32_t* params) {
        const MultiEDF &p = *(const MultiEDF*)params;

        uint32_t edfOffsets[4] = { p.edf0, p.edf1, p.edf2, p.edf3 };

        RGBSpectrum ret = RGBSpectrum::Zero();
        for (int i = 0; i < p.numEDFs; ++i) {
            const uint32_t* edf = params + edfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)edf;
            const EDFProcedureSet procSet = pv_edfProcedureSetBuffer[procIdx];
            progSigEvaluateEmittanceInternal evaluateEmittanceInternal = (progSigEvaluateEmittanceInternal)procSet.progEvaluateEmittanceInternal;

            ret += evaluateEmittanceInternal(edf + 1);
        }

        return ret;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum MultiEDF_evaluateEDFInternal(const uint32_t* params, const EDFQuery &query, const Vector3D &dirLocal) {
        const MultiEDF &p = *(const MultiEDF*)params;

        uint32_t edfOffsets[4] = { p.edf0, p.edf1, p.edf2, p.edf3 };

        RGBSpectrum ret = RGBSpectrum::Zero();
        RGBSpectrum sumEmittance = RGBSpectrum::Zero();
        for (int i = 0; i < p.numEDFs; ++i) {
            const uint32_t* edf = params + edfOffsets[i];
            uint32_t procIdx = *(const uint32_t*)edf;
            const EDFProcedureSet procSet = pv_edfProcedureSetBuffer[procIdx];
            progSigEvaluateEmittanceInternal evaluateEmittanceInternal = (progSigEvaluateEmittanceInternal)procSet.progEvaluateEmittanceInternal;
            progSigEvaluateEDFInternal evaluateEDFInternal = (progSigEvaluateEDFInternal)procSet.progEvaluateEDFInternal;

            RGBSpectrum emittance = evaluateEmittanceInternal(edf + 1);
            sumEmittance += emittance;
            ret += emittance * evaluateEDFInternal(edf + 1, query, dirLocal);
        }
        ret.safeDivide(sumEmittance);

        return ret;
    }

    // END: MultiBSDF / MultiEDF
    // ----------------------------------------------------------------



    // ----------------------------------------------------------------
    // EnvironmentEDF

    struct EnvironmentEDF {
        RGBSpectrum emittance;
    };

    RT_CALLABLE_PROGRAM uint32_t EnvironmentEmitterSurfaceMaterial_setupEDF(const uint32_t* matDesc, const SurfacePoint &surfPt, uint32_t* params) {
        EnvironmentEDF &p = *(EnvironmentEDF*)params;
        const EnvironmentEmitterSurfaceMaterial &mat = *(const EnvironmentEmitterSurfaceMaterial*)(matDesc + sizeof(SurfaceMaterialHead) / 4);

        optix::float4 texValue = optix::rtTex2D<optix::float4>(mat.texEmittance, surfPt.texCoord.u, surfPt.texCoord.v);
        p.emittance = RGBSpectrum(texValue.x, texValue.y, texValue.z);

        return sizeof(EnvironmentEDF) / 4;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum EnvironmentEDF_evaluateEmittanceInternal(const uint32_t* params) {
        EnvironmentEDF &p = *(EnvironmentEDF*)params;
        return M_PIf * p.emittance;
    }

    RT_CALLABLE_PROGRAM RGBSpectrum EnvironmentEDF_evaluateEDFInternal(const uint32_t* params, const EDFQuery &query, const Vector3D &dirLocal) {
        return RGBSpectrum(dirLocal.z > 0.0f ? 1.0f / M_PIf : 0.0f);
    }

    // END: EnvironmentEDF
    // ----------------------------------------------------------------
}
