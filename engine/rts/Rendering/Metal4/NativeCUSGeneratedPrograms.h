/* Whole selected30770 translation with source-authored selected31657 shader delta.
 * Exact new original stage identities and source construction: this revision ASSEMBLY-SOURCES.json.
 * No compiler was run for this delta; inactive native padding and actual bindings remain. */
#pragma once
#include "NativeSampleShadingShaders.h"
#include "Rendering/Backend/NativeCUSProgramData.h"
namespace rendering::metal4 {
inline constexpr NativeCUSGeneratedField CUS_0_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,true},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_0_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_840_pieceVertexPosOrig [[user(locn0)]];
    float4 m_840_worldVertexPos [[user(locn1)]];
    float3 m_840_worldTangent_VS [[user(locn2)]];
    float3 m_840_worldNormal_VS [[user(locn3)]];
    float4 m_840_uvCoords [[user(locn4)]];
    float4 m_840_teamCol [[user(locn5)]];
    float4 m_840_shadowVertexPos [[user(locn6)]];
    float4 m_840_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _585, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_585.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _701 = mVP;
    float3 _704 = _701.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _704.x;
    mVP.y = _704.y;
    mVP.z = _704.z;
    float4 _717 = mVP;
    float2 _719 = _717.xz + (float2(diff) + (wind * diff2));
    mVP.x = _719.x;
    mVP.z = _719.y;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float Perlin3D(thread const float3& P)
{
    float3 Pi = floor(P);
    float3 Pf = P - Pi;
    float3 Pf_min1 = Pf - float3(1.0);
    Pi -= (floor(Pi * 0.0144927538931369781494140625) * 69.0);
    float3 Pi_inc1 = step(Pi, float3(67.5)) * (Pi + float3(1.0));
    float4 Pt = float4(Pi.xy, Pi_inc1.xy) + float4(50.0, 161.0, 50.0, 161.0);
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    float3 lowz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float3 highz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi_inc1.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float4 hashx0 = fract(Pt * lowz_mod.xxxx);
    float4 hashx1 = fract(Pt * highz_mod.xxxx);
    float4 hashy0 = fract(Pt * lowz_mod.yyyy);
    float4 hashy1 = fract(Pt * highz_mod.yyyy);
    float4 hashz0 = fract(Pt * lowz_mod.zzzz);
    float4 hashz1 = fract(Pt * highz_mod.zzzz);
    float4 grad_x0 = hashx0 - float4(0.499989986419677734375);
    float4 grad_y0 = hashy0 - float4(0.499989986419677734375);
    float4 grad_z0 = hashz0 - float4(0.499989986419677734375);
    float4 grad_x1 = hashx1 - float4(0.499989986419677734375);
    float4 grad_y1 = hashy1 - float4(0.499989986419677734375);
    float4 grad_z1 = hashz1 - float4(0.499989986419677734375);
    float4 grad_results_0 = rsqrt(((grad_x0 * grad_x0) + (grad_y0 * grad_y0)) + (grad_z0 * grad_z0)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x0) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y0)) + (Pf.zzzz * grad_z0));
    float4 grad_results_1 = rsqrt(((grad_x1 * grad_x1) + (grad_y1 * grad_y1)) + (grad_z1 * grad_z1)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x1) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y1)) + (Pf_min1.zzzz * grad_z1));
    float3 blend = ((Pf * Pf) * Pf) * ((Pf * ((Pf * 6.0) - float3(15.0))) + float3(10.0));
    float4 res0 = mix(grad_results_0, grad_results_1, float4(blend.z));
    float4 blend2 = float4(blend.xy, float2(1.0) - blend.xy);
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    return final * 1.1547005176544189453125;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _214 = Lerp(param, param_1, param_2);
    return Transform{ _214, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _110, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _760, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _764 = instData.x + bID0;
        tx.quat = _760.transforms[_764].quat;
        tx.trSc = _760.transforms[_764].trSc;
    }
    else
    {
        uint _779 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _786 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _760.transforms[_779].quat;
        param_4.trSc = _760.transforms[_779].trSc;
        Transform param_5;
        param_5.quat = _760.transforms[_786].quat;
        param_5.trSc = _760.transforms[_786].trSc;
        float param_6 = _110.timeInfo.w;
        Transform _804 = Lerp(param_4, param_5, param_6);
        tx = _804;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _81, thread float& simFrame, constant UniformParamsBuffer& _110, const device UniformsBuffer& _585, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _760, thread float3& normal, thread Data& _840, constant NativeCUSDrawUniforms& _853, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _840.uvCoords.z = float(_585.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_853.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _585.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _853.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _853.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _853.featureFadeDistSq) / (_853.featureDrawDistSq - _853.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _840.uvCoords.x = uv.xy.x;
    _840.uvCoords.y = uv.xy.y;
    _840.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_585.uni[instData.y].health / _585.uni[instData.y].maxHealth, 0.0, 1.0);
    float buildProgress = ((device float*)&_585.uni[instData.y].userDefined[0])[0u];
    if (buildProgress > (-0.5))
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_840.aoterm_fogFactor_selfIllumMod_healthFraction.w / fast::max(9.9999999392252902907785028219223e-09, buildProgress), 0.0, 1.0);
        if (buildProgress < 0.999000012874603271484375)
        {
            piecePos.y += (0.5 * (1.0 - smoothstep(0.949999988079071044921875, 1.0, buildProgress)));
        }
    }
    float4 _966 = piecePos;
    float3 _968 = _966.xyz * featureFadeFactor;
    piecePos.x = _968.x;
    piecePos.y = _968.y;
    piecePos.z = _968.z;
    _840.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_853.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _585, instData);
        piecePos = param;
    }
    if ((uint(_853.bitOptions) & 256u) != 0u)
    {
        bool _1002 = _840.aoterm_fogFactor_selfIllumMod_healthFraction.w < 0.949999988079071044921875;
        bool _1011;
        if (!_1002)
        {
            _1011 = _853.baseVertexDisplacement > 0.00999999977648258209228515625;
        }
        else
        {
            _1011 = _1002;
        }
        if (_1011)
        {
            float3 seedVec = piecePos.xyz * 0.100000001490116119384765625;
            float param_1 = float(_585.uni[instData.y].composite >> uint(16));
            seedVec.y += (1024.0 * hash11(param_1));
            float damageAmount = (1.0 - _840.aoterm_fogFactor_selfIllumMod_healthFraction.w) * 0.800000011920928955078125;
            float3 param_2 = seedVec;
            float4 _1048 = piecePos;
            float4 _1052 = piecePos;
            float3 _1054 = _1052.xyz + (fast::normalize(_1048.xyz) * ((fast::max(damageAmount, _853.baseVertexDisplacement) * 6.0) * Perlin3D(param_2)));
            piecePos.x = _1054.x;
            piecePos.y = _1054.y;
            piecePos.z = _1054.z;
        }
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1076 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1084;
    if (!_1076)
    {
        _1084 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1084 = _1076;
    }
    if (_1084)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_3 = piecePos.xyz;
    float3 param_6 = safeT;
    float3 param_8 = safeB;
    float4 param_4;
    float3 param_5;
    float3 param_7;
    float3 param_9;
    GetModelSpaceVertex(param_3, param_4, param_5, param_6, param_7, param_8, param_9, _110, instData, bonesInfo, _760, normal);
    modelPos = param_4;
    modelVertexNormal = param_5;
    float3 modelSpaceTangent = param_7;
    float3 modelSpaceBitangent = param_9;
    uint _1110 = instData.x + 0u;
    uint _1113 = instData.x + 1u;
    Transform param_10;
    param_10.quat = _760.transforms[_1110].quat;
    param_10.trSc = _760.transforms[_1110].trSc;
    Transform param_11;
    param_11.quat = _760.transforms[_1113].quat;
    param_11.trSc = _760.transforms[_1113].trSc;
    float param_12 = _110.timeInfo.w;
    Transform _1131 = Lerp(param_10, param_11, param_12);
    Transform modelToWorldTX = _1131;
    Transform param_13 = modelToWorldTX;
    float4 param_14 = modelPos;
    float4 worldPos = ApplyTransform(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelSpaceTangent;
    _840.worldTangent_VS = RotateByQuaternion(param_15, param_16);
    float4 param_17 = modelToWorldTX.quat;
    float3 param_18 = modelVertexNormal;
    _840.worldNormal_VS = RotateByQuaternion(param_17, param_18);
    _840.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_585.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _840.teamCol = ((teamIndex == 255u) ? float4(0.0) : _110.teamColor[teamIndex]);
    _840.teamCol.w = ((device float*)&_585.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_585.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_110.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _840.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    bool _1181 = (uint(_853.bitOptions) & 32u) != 0u;
    bool _1191;
    if (!_1181)
    {
        _1191 = (uint(_853.bitOptions) & 64u) != 0u;
    }
    else
    {
        _1191 = _1181;
    }
    bool _1201;
    if (!_1191)
    {
        _1201 = (uint(_853.bitOptions) & 8192u) != 0u;
    }
    else
    {
        _1201 = _1191;
    }
    if (_1201)
    {
        float unitSpeed = ((device float*)&_585.uni[instData.y].speed)[3u];
        if (unitSpeed > 0.5)
        {
            unitSpeed = fast::max(3.0, unitSpeed);
        }
        float loopingFrameCount = mod(simFrame, 8.0);
        float baseOffset = loopingFrameCount * unitSpeed;
        if ((uint(_853.bitOptions) & 32u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.628173828125, _840.uvCoords.y <= 0.6220703125, _840.uvCoords.x <= 0.749755859375, _840.uvCoords.y >= 0.60205078125)))
            {
                _840.uvCoords.x += (mod(baseOffset * 4.0, 12.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_840.uvCoords.x >= 0.6279296875, _840.uvCoords.y <= 0.601806640625, _840.uvCoords.x <= 0.75, _840.uvCoords.y >= 0.569580078125)))
                {
                    _840.uvCoords.x += (mod(baseOffset * 4.0, 20.0) / 4096.0);
                }
            }
        }
        if ((uint(_853.bitOptions) & 64u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.74267578125, _840.uvCoords.y <= 0.062744140625, _840.uvCoords.x <= 1.0, _840.uvCoords.y >= 0.0)))
            {
                _840.uvCoords.x += (mod(baseOffset * (-6.0), 56.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_840.uvCoords.x >= 0.046875, _840.uvCoords.y <= 0.8447265625, _840.uvCoords.x <= 0.171630859375, _840.uvCoords.y >= 0.828125)))
                {
                    _840.uvCoords.x += (mod(baseOffset * 3.0, 24.0) / 4096.0);
                }
                else
                {
                    if (all(bool4(_840.uvCoords.x >= 0.046142578125, _840.uvCoords.y <= 0.827880859375, _840.uvCoords.x <= 0.169677734375, _840.uvCoords.y >= 0.804931640625)))
                    {
                        _840.uvCoords.x += (mod(baseOffset * 3.0, 28.0) / 4096.0);
                    }
                }
            }
        }
        if ((uint(_853.bitOptions) & 8192u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.637939453125, _840.uvCoords.y <= 0.8125, _840.uvCoords.x <= 0.799072265625, _840.uvCoords.y >= 0.7138671875)))
            {
                _840.uvCoords.x += (mod(baseOffset * (-6.0), 55.0) / 4096.0);
            }
        }
    }
    _840.worldVertexPos = worldPos;
    _840.shadowVertexPos = _81.shadowView * worldPos;
    float4 _1434 = _840.shadowVertexPos;
    float2 _1436 = _1434.xy + float2(0.5);
    _840.shadowVertexPos.x = _1436.x;
    _840.shadowVertexPos.y = _1436.y;
    _840.shadowVertexPos.w = ((device float*)&_585.uni[instData.y].userDefined[0])[0u];
    if ((uint(_853.bitOptions) & 8u) != 0u)
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_853.bitOptions) & 16u) != 0u)
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_585.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_853.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _81.cameraView * _840.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_110.fogParams.y - fogCoord) * _110.fogParams.w;
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_840.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_853.drawPass) & 4u) == 4u)
    {
        gl_Position = _81.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _853.clipPlane0);
    }
    else
    {
        gl_Position = _81.cameraViewProj * worldPos;
    }
    _840.worldVertexPos.w = ((device float*)&_585.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _81 [[buffer(2)]], constant UniformParamsBuffer& _110 [[buffer(3)]], const device TransformBuffer& _760 [[buffer(4)]], const device UniformsBuffer& _585 [[buffer(5)]], constant NativeCUSDrawUniforms& _853 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _840 = {};
    float3 cameraPos = _81.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_81.cameraView[0].z, _81.cameraView[1].z, _81.cameraView[2].z) * (-1.0);
    float simFrame = _110.timeInfo.x + _110.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _81, simFrame, _110, _585, in.instData, in.bonesInfo, _760, in.normal, _840, _853, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_853.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_853.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_853.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_840_pieceVertexPosOrig = _840.pieceVertexPosOrig;
    out.m_840_worldVertexPos = _840.worldVertexPos;
    out.m_840_worldTangent_VS = _840.worldTangent_VS;
    out.m_840_worldNormal_VS = _840.worldNormal_VS;
    out.m_840_uvCoords = _840.uvCoords;
    out.m_840_teamCol = _840.teamCol;
    out.m_840_shadowVertexPos = _840.shadowVertexPos;
    out.m_840_aoterm_fogFactor_selfIllumMod_healthFraction = _840.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_0_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,true},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_0_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _194 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _476 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_455_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_teamCol [[user(locn5), flat]];
    float4 m_455_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _200)
{
    return (_200.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _200)
{
    if ((_200.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _200)
{
    uint code = _200.NativeCUSSampleCodes[slot].x;
    if ((_200.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _266;
        if (mode == 0u)
        {
            _266 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _278;
            if (mode == 1u)
            {
                _278 = float4(value.x);
            }
            else
            {
                _278 = float4(0.0, 0.0, 0.0, value.x);
            }
            _266 = _278;
        }
        value = _266;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _333 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _333);
    float4 _338 = NativeCUSChannels(param_3, param_4, _200);
    return _338;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture3d<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, level(lod));
    float4 _426 = NativeCUSChannels(param_1, param_2, _200);
    return _426;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _352 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _352, bias(bias0));
    float4 _358 = NativeCUSChannels(param_3, param_4, _200);
    return _358;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_200.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _382 = NativeCUSUV(param, param_1, _200);
    uv.x = _382.x;
    uv.y = _382.y;
    if ((_200.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _200, thread Data& _455, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _455.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _476[presetIndex].samplingRandomness;
    float samplingDistance = _476[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _521 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _200);
        shadow = _521;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _404 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _404, level(lod));
    float4 _410 = NativeCUSChannels(param_3, param_4, _200);
    return _410;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    int2 _433;
    if (NativeCUSEmpty(param, _200))
    {
        _433 = int2(0);
    }
    else
    {
        _433 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _433;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _374 = NativeCUSChannels(param_1, param_2, _200);
    return _374;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _200)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _200);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _200).xyz;
    return SampleReflectionMapMod(param_3, _200);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _200).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _200, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _200.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _200)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _200);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1168 [[buffer(2)]], constant UniformParamsBuffer& _175 [[buffer(3)]], constant NativeCUSDrawUniforms& _200 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> texture1w [[texture(3)]], texture2d<float> texture2w [[texture(4)]], texture2d<float> normalTexw [[texture(5)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> brdfLUT [[texture(9)]], texture3d<float> noisetex3dcube [[texture(10)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler texture1wSmplr [[sampler(3)]], sampler texture2wSmplr [[sampler(4)]], sampler normalTexwSmplr [[sampler(5)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler brdfLUTSmplr [[sampler(9)]], sampler noisetex3dcubeSmplr [[sampler(10)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _455 = {};
    _455.pieceVertexPosOrig = in.m_455_pieceVertexPosOrig;
    _455.worldVertexPos = in.m_455_worldVertexPos;
    _455.worldTangent_VS = in.m_455_worldTangent_VS;
    _455.worldNormal_VS = in.m_455_worldNormal_VS;
    _455.uvCoords = in.m_455_uvCoords;
    _455.teamCol = in.m_455_teamCol;
    _455.shadowVertexPos = in.m_455_shadowVertexPos;
    _455.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_455_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _175.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _455.uvCoords.xy;
    float unitID = floor(_455.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_455.worldNormal_VS);
    float3 worldTangent = fast::normalize(_455.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _833 = (uint(_200.bitOptions) & 2u) != 0u;
    bool _843;
    if (!_833)
    {
        _843 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _843 = _833;
    }
    float4 normalTexVal;
    if (_843)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _200);
    }
    float healthMix = 0.0;
    float4 myPerlin = float4(0.0);
    bool _860 = (uint(_200.bitOptions) & 128u) != 0u;
    bool _869;
    if (!_860)
    {
        _869 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _869 = _860;
    }
    if (_869)
    {
        float3 seedVec = _455.pieceVertexPosOrig.xyz * 0.60000002384185791015625;
        float param_2 = unitID;
        seedVec.y += ((1024.0 * hash11(param_2)) + dot(seedVec.xz, float2(0.100000001490116119384765625)));
        int param_3 = 10;
        float3 param_4 = fract(seedVec * 0.100000001490116119384765625);
        float param_5 = 0.0;
        myPerlin = (NativeCUSLod(noisetex3dcube, noisetex3dcubeSmplr, param_3, param_4, param_5, _200) * 2.0) - float4(1.0);
        healthMix = ((myPerlin.x * 0.5) + 0.5) * (2.0 - _200.baseVertexDisplacement);
        healthMix = smoothstep(0.0, healthMix, fast::max((1.0 - _455.aoterm_fogFactor_selfIllumMod_healthFraction.w) + _200.baseVertexDisplacement, 0.0)) * 0.800000011920928955078125;
    }
    float3 tbnNormal;
    if ((uint(_200.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        bool _942 = (uint(_200.bitOptions) & 128u) != 0u;
        bool _951;
        if (!_942)
        {
            _951 = (uint(_200.bitOptions) & 512u) != 0u;
        }
        else
        {
            _951 = _942;
        }
        if (_951)
        {
            if (healthMix > 0.0500000007450580596923828125)
            {
                int param_6 = 5;
                float2 param_7 = myUV;
                float param_8 = textureLODBias;
                float3 tbnNormalw = (NativeCUSTexture(normalTexw, normalTexwSmplr, param_6, param_7, param_8, _200).xyz * 2.0) - float3(1.0);
                float3 wrecknormal = tbnNormalw;
                tbnNormal = mix(tbnNormal, tbnNormalw, float3(healthMix));
            }
        }
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_9 = 0;
    float2 param_10 = myUV;
    float param_11 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_9, param_10, param_11, _200);
    int param_12 = 1;
    float2 param_13 = myUV;
    float param_14 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_12, param_13, param_14, _200);
    if ((uint(_200.bitOptions) & 128u) != 0u)
    {
        if (healthMix > 0.0500000007450580596923828125)
        {
            int param_15 = 3;
            float2 param_16 = myUV;
            float4 texColor1w = NativeCUSTexture(texture1w, texture1wSmplr, param_15, param_16, _200);
            int param_17 = 4;
            float2 param_18 = myUV;
            float4 texColor2w = NativeCUSTexture(texture2w, texture2wSmplr, param_17, param_18, _200);
            healthMix *= (1.0 - (0.89999997615814208984375 * texColor2.x));
            texColor1 = mix(texColor1, texColor1w, float4(healthMix));
            float4 _1034 = texColor2;
            float3 _1040 = mix(_1034.xyz, texColor2w.xyz, float3(healthMix));
            texColor2.x = _1040.x;
            texColor2.y = _1040.y;
            texColor2.z = _1040.z;
            texColor2.z += (0.5 * healthMix);
        }
    }
    float4 teamColor = _455.teamCol;
    float3 param_19 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_19, _200);
    if ((uint(_200.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_455.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_200.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_20 = roughness;
    float3 param_21 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_20, param_21), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_175.sunDir.xyz);
    float3 V = fast::normalize(_1168.cameraViewInv[3].xyz - _455.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_200.bitOptions) & 1u) != 0u)
    {
        float param_22 = NdotL;
        gShadow = GetShadowPCFRandom(param_22, shadowsQuality, _200, _455, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _175.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_23 = 9;
    float2 param_24 = float2(NdotV, roughness);
    float param_25 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_23, param_24, param_25, _200).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_26 = F0;
        float3 param_27 = F90;
        float param_28 = VdotH;
        float3 F = FresnelSchlick(param_26, param_27, param_28);
        float param_29 = NdotL;
        float param_30 = NdotV;
        float param_31 = roughness2;
        float Vis = VisibilityOcclusionFast(param_29, param_30, param_31);
        float param_32 = NdotH;
        float param_33 = roughness4;
        float D = MicrofacetDistribution(param_32, param_33);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_175.sunSpecularModel.xyz, _175.sunDiffuseModel.xyz, float3(step(dot(_175.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_175.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_34 = F0;
    float3 param_35 = F90;
    float param_36 = VdotH;
    float param_37 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_34, param_35, param_36, param_37);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_175.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_38 = Rv;
    float param_39 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_38, param_39, _200, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_40 = NdotV;
    float param_41 = _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_42 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_40, param_41, param_42);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _200);
    if ((uint(_200.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_175.fogColor.xyz, outColor, float3(_455.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _200.brightnessFactor;
    float buildProgress = _455.shadowVertexPos.w;
    if (buildProgress > (-0.5))
    {
        myPerlin = myPerlin;
        float height = fast::clamp(_455.pieceVertexPosOrig.w / 1.0, 0.0, 1.0);
        float sintime = 0.5 + (0.5 * sin(simFrame * 0.100000001490116119384765625));
        float sintimefast = 0.5 + (0.5 * sin(simFrame * 0.1500000059604644775390625));
        float sinprogress = 0.5 + (0.5 * sin((buildProgress * 0.00999999977648258209228515625) * 3.141499996185302734375));
        float4 progressLevels = float4(buildProgress);
        progressLevels = powr(progressLevels, float4(3.0, 1.5, 0.699999988079071044921875, 0.3499999940395355224609375));
        progressLevels = mix(progressLevels, float4(myPerlin.y + (myPerlin.z * sin(simFrame * 0.042341999709606170654296875))), float4(0.0500000007450580596923828125 * smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress)));
        float4 levelLines = fast::clamp(float4(1.0) - (abs(progressLevels - float4(height)) * 100.0), float4(0.0), float4(1.0));
        float levelFactor = dot(levelLines, float4(1.0));
        float3 fragSize = fwidth(_455.worldVertexPos.xyz);
        float fragSizeFactor = 1.0 / dot(float3(1.0), fragSize);
        float3 buildGrid = (abs(fract((_455.worldVertexPos.xyz / float3(8.0)) - float3(0.5)) - float3(0.5)) * 8.0) / fragSize;
        float line = 1.0 - fast::min(fast::min(buildGrid.x, buildGrid.y), buildGrid.z);
        line = fast::clamp(line * smoothstep(-0.5, 1.0, fragSizeFactor), 0.0, 1.0);
        float gridSize = fast::clamp(((1.0 - buildProgress) * 10.0) + 2.0, 2.0, 12.0);
        float3 grid = step(float3(0.5), fast::clamp(float3(1.0) - (fract(_455.pieceVertexPosOrig.xyz / float3(gridSize)) * 10.0), float3(0.0), float3(1.0)));
        float3 pulseTeamColor = mix(_455.teamCol.xyz, _455.teamCol.xyz * 1.7799999713897705078125, float3(sintimefast));
        if (height > progressLevels.x)
        {
            float unemissive = 1.0 - smoothstep(progressLevels.x, progressLevels.y, height);
            outSpecularColor *= unemissive;
            outColor -= ((albedoColor * emissiveness) * (1.0 - unemissive));
            emissiveness *= unemissive;
        }
        if (height > progressLevels.y)
        {
            outColor = mix(outColor, pulseTeamColor, float3(smoothstep(progressLevels.y, progressLevels.z, height)));
        }
        if (height > progressLevels.z)
        {
            outColor = pulseTeamColor * fast::clamp(NdotL, 0.300000011920928955078125, 1.0);
        }
        if (height > progressLevels.w)
        {
            float heightProgress = smoothstep(progressLevels.w, 1.0, height);
            float flatlight = 0.300000011920928955078125 + (0.699999988079071044921875 * smoothstep(0.0, 1.0, fast::clamp((dot(worldNormal, L) * 0.5) + 0.5, heightProgress * 0.20000000298023223876953125, 1.0)));
            outColor = pulseTeamColor * flatlight;
            N = worldNormal;
            texColor2.w *= fast::max(0.4000000059604644775390625, fast::max(fast::clamp(line, buildProgress, 1.0), 1.0 - heightProgress));
        }
        float last5percent = smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress);
        outColor = mix(outColor, pulseTeamColor, float3(line * last5percent));
        outColor += float3(levelFactor);
        outSpecularColor += float3(levelFactor);
        outSpecularColor += (((pulseTeamColor * line) * sintimefast) * 2.0);
    }
    float selectedness = _455.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_455.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1815 = fast::clamp(_455.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1815.x;
        selectionHighlight.y = _1815.y;
        selectionHighlight.z = _1815.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _455.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float cloakTime = _455.worldVertexPos.w;
    if (abs(cloakTime) > 0.5)
    {
        float cloakedness = 0.0;
        if (cloakTime > 0.0)
        {
            cloakedness = fast::clamp((_175.timeInfo.x - cloakTime) / 15.0, 0.0, 1.0);
        }
        if (cloakTime < 0.0)
        {
            cloakedness = 1.0 - fast::clamp((_175.timeInfo.x + cloakTime) / 15.0, 0.0, 1.0);
        }
        float sintime_1 = fract(simFrame * 0.0199999995529651641845703125);
        myPerlin.y = (myPerlin.y * 0.5) + 0.5;
        texColor2.w = 1.0 - fast::clamp(cloakedness * 0.4900000095367431640625, 0.0, 0.4900000095367431640625);
        float perlinline1 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract(simFrame * 0.004999999888241291046142578125))), 0.0, 1.0);
        float perlinline2 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract((simFrame * 0.004999999888241291046142578125) + 0.5))), 0.0, 1.0);
        float cloaknoise = (cloakedness * perlinline1) + (cloakedness * perlinline2);
        outColor += float3(cloaknoise * 0.85000002384185791015625);
        float dotcamera_1 = dot(worldNormal, V);
        float highLightOpacity_1 = fast::clamp(1.0 - dotcamera_1, 0.0, 1.0);
        highLightOpacity_1 *= highLightOpacity_1;
        outColor = mix(outColor, _455.teamCol.xyz * 3.0, float3(highLightOpacity_1 * cloakedness));
        outSpecularColor += float3(fast::clamp(cloaknoise * 0.75, 0.0, 1.0));
    }
    fragData[0] = float4(outColor, texColor2.w);
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_0_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"baseVertexDisplacement",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture1w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTexw",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"noisetex3dcube",35679,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_1_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,false},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,false},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,false},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_1_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, thread float3& pos, thread float4& uv, constant NativeCUSDrawUniforms& _608, thread float3& T, thread float3& B, thread float4& gl_Position, thread float (&gl_ClipDistance)[1])
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float4 _592 = piecePos;
    float3 _594 = _592.xyz * featureFadeFactor;
    piecePos.x = _594.x;
    piecePos.y = _594.y;
    piecePos.z = _594.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_608.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _638 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _646;
    if (!_638)
    {
        _646 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _646 = _638;
    }
    if (_646)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _672 = instData.x + 0u;
    uint _675 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_672].quat;
    param_8.trSc = _473.transforms[_672].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_675].quat;
    param_9.trSc = _473.transforms[_675].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _693 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _693;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    float4 lightVertexPos = _73.shadowView * worldPos;
    float3 lightVertexNormal = normal;
    float NdotL = fast::clamp(dot(lightVertexNormal, float3(0.0, 0.0, 1.0)), 0.0, 1.0);
    float bias0 = 4.9999998736893758177757263183594e-05 * tan(acos(NdotL));
    bias0 = fast::clamp(bias0, 0.0, 0.00025000001187436282634735107421875);
    float4 _761 = lightVertexPos;
    float2 _763 = _761.xy + float2(0.5);
    lightVertexPos.x = _763.x;
    lightVertexPos.y = _763.y;
    lightVertexPos.z += bias0;
    gl_Position = _73.shadowProj * lightVertexPos;
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _608 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(_73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, in.pos, in.uv, _608, in.T, in.B, out.gl_Position, out.gl_ClipDistance);
    if ((_608.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_608.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_608.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_1_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,false},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,false},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,false},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,false},
};
inline constexpr char CUS_1_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

constant spvUnsafeArray<float, 8> _40 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });

fragment void NativeCUSFragment(constant UniformParamsBuffer& _21 [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    spvUnsafeArray<ShadowQuality, 4> _91 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });
    
    int shadowsQuality = 0;
    float simFrame = _21.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_1_defaults[]={{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_2_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,true},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_2_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_840_pieceVertexPosOrig [[user(locn0)]];
    float4 m_840_worldVertexPos [[user(locn1)]];
    float3 m_840_worldTangent_VS [[user(locn2)]];
    float3 m_840_worldNormal_VS [[user(locn3)]];
    float4 m_840_uvCoords [[user(locn4)]];
    float4 m_840_teamCol [[user(locn5)]];
    float4 m_840_shadowVertexPos [[user(locn6)]];
    float4 m_840_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _585, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_585.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _701 = mVP;
    float3 _704 = _701.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _704.x;
    mVP.y = _704.y;
    mVP.z = _704.z;
    float4 _717 = mVP;
    float2 _719 = _717.xz + (float2(diff) + (wind * diff2));
    mVP.x = _719.x;
    mVP.z = _719.y;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float Perlin3D(thread const float3& P)
{
    float3 Pi = floor(P);
    float3 Pf = P - Pi;
    float3 Pf_min1 = Pf - float3(1.0);
    Pi -= (floor(Pi * 0.0144927538931369781494140625) * 69.0);
    float3 Pi_inc1 = step(Pi, float3(67.5)) * (Pi + float3(1.0));
    float4 Pt = float4(Pi.xy, Pi_inc1.xy) + float4(50.0, 161.0, 50.0, 161.0);
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    float3 lowz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float3 highz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi_inc1.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float4 hashx0 = fract(Pt * lowz_mod.xxxx);
    float4 hashx1 = fract(Pt * highz_mod.xxxx);
    float4 hashy0 = fract(Pt * lowz_mod.yyyy);
    float4 hashy1 = fract(Pt * highz_mod.yyyy);
    float4 hashz0 = fract(Pt * lowz_mod.zzzz);
    float4 hashz1 = fract(Pt * highz_mod.zzzz);
    float4 grad_x0 = hashx0 - float4(0.499989986419677734375);
    float4 grad_y0 = hashy0 - float4(0.499989986419677734375);
    float4 grad_z0 = hashz0 - float4(0.499989986419677734375);
    float4 grad_x1 = hashx1 - float4(0.499989986419677734375);
    float4 grad_y1 = hashy1 - float4(0.499989986419677734375);
    float4 grad_z1 = hashz1 - float4(0.499989986419677734375);
    float4 grad_results_0 = rsqrt(((grad_x0 * grad_x0) + (grad_y0 * grad_y0)) + (grad_z0 * grad_z0)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x0) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y0)) + (Pf.zzzz * grad_z0));
    float4 grad_results_1 = rsqrt(((grad_x1 * grad_x1) + (grad_y1 * grad_y1)) + (grad_z1 * grad_z1)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x1) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y1)) + (Pf_min1.zzzz * grad_z1));
    float3 blend = ((Pf * Pf) * Pf) * ((Pf * ((Pf * 6.0) - float3(15.0))) + float3(10.0));
    float4 res0 = mix(grad_results_0, grad_results_1, float4(blend.z));
    float4 blend2 = float4(blend.xy, float2(1.0) - blend.xy);
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    return final * 1.1547005176544189453125;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _214 = Lerp(param, param_1, param_2);
    return Transform{ _214, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _110, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _760, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _764 = instData.x + bID0;
        tx.quat = _760.transforms[_764].quat;
        tx.trSc = _760.transforms[_764].trSc;
    }
    else
    {
        uint _779 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _786 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _760.transforms[_779].quat;
        param_4.trSc = _760.transforms[_779].trSc;
        Transform param_5;
        param_5.quat = _760.transforms[_786].quat;
        param_5.trSc = _760.transforms[_786].trSc;
        float param_6 = _110.timeInfo.w;
        Transform _804 = Lerp(param_4, param_5, param_6);
        tx = _804;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _81, thread float& simFrame, constant UniformParamsBuffer& _110, const device UniformsBuffer& _585, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _760, thread float3& normal, thread Data& _840, constant NativeCUSDrawUniforms& _853, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _840.uvCoords.z = float(_585.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_853.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _585.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _853.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _853.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _853.featureFadeDistSq) / (_853.featureDrawDistSq - _853.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _840.uvCoords.x = uv.xy.x;
    _840.uvCoords.y = uv.xy.y;
    _840.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_585.uni[instData.y].health / _585.uni[instData.y].maxHealth, 0.0, 1.0);
    float buildProgress = ((device float*)&_585.uni[instData.y].userDefined[0])[0u];
    if (buildProgress > (-0.5))
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_840.aoterm_fogFactor_selfIllumMod_healthFraction.w / fast::max(9.9999999392252902907785028219223e-09, buildProgress), 0.0, 1.0);
        if (buildProgress < 0.999000012874603271484375)
        {
            piecePos.y += (0.5 * (1.0 - smoothstep(0.949999988079071044921875, 1.0, buildProgress)));
        }
    }
    float4 _966 = piecePos;
    float3 _968 = _966.xyz * featureFadeFactor;
    piecePos.x = _968.x;
    piecePos.y = _968.y;
    piecePos.z = _968.z;
    _840.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_853.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _585, instData);
        piecePos = param;
    }
    if ((uint(_853.bitOptions) & 256u) != 0u)
    {
        bool _1002 = _840.aoterm_fogFactor_selfIllumMod_healthFraction.w < 0.949999988079071044921875;
        bool _1011;
        if (!_1002)
        {
            _1011 = _853.baseVertexDisplacement > 0.00999999977648258209228515625;
        }
        else
        {
            _1011 = _1002;
        }
        if (_1011)
        {
            float3 seedVec = piecePos.xyz * 0.100000001490116119384765625;
            float param_1 = float(_585.uni[instData.y].composite >> uint(16));
            seedVec.y += (1024.0 * hash11(param_1));
            float damageAmount = (1.0 - _840.aoterm_fogFactor_selfIllumMod_healthFraction.w) * 0.800000011920928955078125;
            float3 param_2 = seedVec;
            float4 _1048 = piecePos;
            float4 _1052 = piecePos;
            float3 _1054 = _1052.xyz + (fast::normalize(_1048.xyz) * ((fast::max(damageAmount, _853.baseVertexDisplacement) * 6.0) * Perlin3D(param_2)));
            piecePos.x = _1054.x;
            piecePos.y = _1054.y;
            piecePos.z = _1054.z;
        }
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1076 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1084;
    if (!_1076)
    {
        _1084 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1084 = _1076;
    }
    if (_1084)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_3 = piecePos.xyz;
    float3 param_6 = safeT;
    float3 param_8 = safeB;
    float4 param_4;
    float3 param_5;
    float3 param_7;
    float3 param_9;
    GetModelSpaceVertex(param_3, param_4, param_5, param_6, param_7, param_8, param_9, _110, instData, bonesInfo, _760, normal);
    modelPos = param_4;
    modelVertexNormal = param_5;
    float3 modelSpaceTangent = param_7;
    float3 modelSpaceBitangent = param_9;
    uint _1110 = instData.x + 0u;
    uint _1113 = instData.x + 1u;
    Transform param_10;
    param_10.quat = _760.transforms[_1110].quat;
    param_10.trSc = _760.transforms[_1110].trSc;
    Transform param_11;
    param_11.quat = _760.transforms[_1113].quat;
    param_11.trSc = _760.transforms[_1113].trSc;
    float param_12 = _110.timeInfo.w;
    Transform _1131 = Lerp(param_10, param_11, param_12);
    Transform modelToWorldTX = _1131;
    Transform param_13 = modelToWorldTX;
    float4 param_14 = modelPos;
    float4 worldPos = ApplyTransform(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelSpaceTangent;
    _840.worldTangent_VS = RotateByQuaternion(param_15, param_16);
    float4 param_17 = modelToWorldTX.quat;
    float3 param_18 = modelVertexNormal;
    _840.worldNormal_VS = RotateByQuaternion(param_17, param_18);
    _840.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_585.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _840.teamCol = ((teamIndex == 255u) ? float4(0.0) : _110.teamColor[teamIndex]);
    _840.teamCol.w = ((device float*)&_585.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_585.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_110.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _840.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    bool _1181 = (uint(_853.bitOptions) & 32u) != 0u;
    bool _1191;
    if (!_1181)
    {
        _1191 = (uint(_853.bitOptions) & 64u) != 0u;
    }
    else
    {
        _1191 = _1181;
    }
    bool _1201;
    if (!_1191)
    {
        _1201 = (uint(_853.bitOptions) & 8192u) != 0u;
    }
    else
    {
        _1201 = _1191;
    }
    if (_1201)
    {
        float unitSpeed = ((device float*)&_585.uni[instData.y].speed)[3u];
        if (unitSpeed > 0.5)
        {
            unitSpeed = fast::max(3.0, unitSpeed);
        }
        float loopingFrameCount = mod(simFrame, 8.0);
        float baseOffset = loopingFrameCount * unitSpeed;
        if ((uint(_853.bitOptions) & 32u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.628173828125, _840.uvCoords.y <= 0.6220703125, _840.uvCoords.x <= 0.749755859375, _840.uvCoords.y >= 0.60205078125)))
            {
                _840.uvCoords.x += (mod(baseOffset * 4.0, 12.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_840.uvCoords.x >= 0.6279296875, _840.uvCoords.y <= 0.601806640625, _840.uvCoords.x <= 0.75, _840.uvCoords.y >= 0.569580078125)))
                {
                    _840.uvCoords.x += (mod(baseOffset * 4.0, 20.0) / 4096.0);
                }
            }
        }
        if ((uint(_853.bitOptions) & 64u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.74267578125, _840.uvCoords.y <= 0.062744140625, _840.uvCoords.x <= 1.0, _840.uvCoords.y >= 0.0)))
            {
                _840.uvCoords.x += (mod(baseOffset * (-6.0), 56.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_840.uvCoords.x >= 0.046875, _840.uvCoords.y <= 0.8447265625, _840.uvCoords.x <= 0.171630859375, _840.uvCoords.y >= 0.828125)))
                {
                    _840.uvCoords.x += (mod(baseOffset * 3.0, 24.0) / 4096.0);
                }
                else
                {
                    if (all(bool4(_840.uvCoords.x >= 0.046142578125, _840.uvCoords.y <= 0.827880859375, _840.uvCoords.x <= 0.169677734375, _840.uvCoords.y >= 0.804931640625)))
                    {
                        _840.uvCoords.x += (mod(baseOffset * 3.0, 28.0) / 4096.0);
                    }
                }
            }
        }
        if ((uint(_853.bitOptions) & 8192u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.637939453125, _840.uvCoords.y <= 0.8125, _840.uvCoords.x <= 0.799072265625, _840.uvCoords.y >= 0.7138671875)))
            {
                _840.uvCoords.x += (mod(baseOffset * (-6.0), 55.0) / 4096.0);
            }
        }
    }
    _840.worldVertexPos = worldPos;
    _840.shadowVertexPos = _81.shadowView * worldPos;
    float4 _1434 = _840.shadowVertexPos;
    float2 _1436 = _1434.xy + float2(0.5);
    _840.shadowVertexPos.x = _1436.x;
    _840.shadowVertexPos.y = _1436.y;
    _840.shadowVertexPos.w = ((device float*)&_585.uni[instData.y].userDefined[0])[0u];
    if ((uint(_853.bitOptions) & 8u) != 0u)
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_853.bitOptions) & 16u) != 0u)
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_585.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_853.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _81.cameraView * _840.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_110.fogParams.y - fogCoord) * _110.fogParams.w;
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_840.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_853.drawPass) & 4u) == 4u)
    {
        gl_Position = _81.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _853.clipPlane0);
    }
    else
    {
        gl_Position = _81.cameraViewProj * worldPos;
    }
    _840.worldVertexPos.w = ((device float*)&_585.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _81 [[buffer(2)]], constant UniformParamsBuffer& _110 [[buffer(3)]], const device TransformBuffer& _760 [[buffer(4)]], const device UniformsBuffer& _585 [[buffer(5)]], constant NativeCUSDrawUniforms& _853 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _840 = {};
    float3 cameraPos = _81.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_81.cameraView[0].z, _81.cameraView[1].z, _81.cameraView[2].z) * (-1.0);
    float simFrame = _110.timeInfo.x + _110.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _81, simFrame, _110, _585, in.instData, in.bonesInfo, _760, in.normal, _840, _853, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_853.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_853.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_853.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_840_pieceVertexPosOrig = _840.pieceVertexPosOrig;
    out.m_840_worldVertexPos = _840.worldVertexPos;
    out.m_840_worldTangent_VS = _840.worldTangent_VS;
    out.m_840_worldNormal_VS = _840.worldNormal_VS;
    out.m_840_uvCoords = _840.uvCoords;
    out.m_840_teamCol = _840.teamCol;
    out.m_840_shadowVertexPos = _840.shadowVertexPos;
    out.m_840_aoterm_fogFactor_selfIllumMod_healthFraction = _840.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_2_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,true},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_2_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _166 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _416 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
    float4 fragData_1 [[color(1)]];
    float4 fragData_2 [[color(2)]];
    float4 fragData_3 [[color(3)]];
    float4 fragData_4 [[color(4)]];
};

struct NativeCUSFragment_in
{
    float4 m_395_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_395_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_395_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_teamCol [[user(locn5), flat]];
    float4 m_395_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _172)
{
    return (_172.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _172)
{
    if ((_172.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _172)
{
    uint code = _172.NativeCUSSampleCodes[slot].x;
    if ((_172.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _238;
        if (mode == 0u)
        {
            _238 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _250;
            if (mode == 1u)
            {
                _250 = float4(value.x);
            }
            else
            {
                _250 = float4(0.0, 0.0, 0.0, value.x);
            }
            _238 = _250;
        }
        value = _238;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _305 = NativeCUSUV(param_1, param_2, _172);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _305);
    float4 _310 = NativeCUSChannels(param_3, param_4, _172);
    return _310;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture3d<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& lod, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, level(lod));
    float4 _382 = NativeCUSChannels(param_1, param_2, _172);
    return _382;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _324 = NativeCUSUV(param_1, param_2, _172);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _324, bias(bias0));
    float4 _330 = NativeCUSChannels(param_3, param_4, _172);
    return _330;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _172)
{
    if (_172.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_172.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _338 = NativeCUSUV(param, param_1, _172);
    uv.x = _338.x;
    uv.y = _338.y;
    if ((_172.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _172, thread Data& _395, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _395.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _416[presetIndex].samplingRandomness;
    float samplingDistance = _416[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _461 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _172);
        shadow = _461;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _360 = NativeCUSUV(param_1, param_2, _172);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _360, level(lod));
    float4 _366 = NativeCUSChannels(param_3, param_4, _172);
    return _366;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _172)
{
    if (_172.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _172.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _172)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _172);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1002 [[buffer(2)]], constant UniformParamsBuffer& _147 [[buffer(3)]], constant NativeCUSDrawUniforms& _172 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> normalTexw [[texture(5)]], texture2d<float> shadowTex [[texture(6)]], texture2d<float> brdfLUT [[texture(9)]], texture3d<float> noisetex3dcube [[texture(10)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler normalTexwSmplr [[sampler(5)]], sampler shadowTexSmplr [[sampler(6)]], sampler brdfLUTSmplr [[sampler(9)]], sampler noisetex3dcubeSmplr [[sampler(10)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 5> fragData = {};
    Data _395 = {};
    _395.pieceVertexPosOrig = in.m_395_pieceVertexPosOrig;
    _395.worldVertexPos = in.m_395_worldVertexPos;
    _395.worldTangent_VS = in.m_395_worldTangent_VS;
    _395.worldNormal_VS = in.m_395_worldNormal_VS;
    _395.uvCoords = in.m_395_uvCoords;
    _395.teamCol = in.m_395_teamCol;
    _395.shadowVertexPos = in.m_395_shadowVertexPos;
    _395.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_395_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _147.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _395.uvCoords.xy;
    float unitID = floor(_395.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_395.worldNormal_VS);
    float3 worldTangent = fast::normalize(_395.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _720 = (uint(_172.bitOptions) & 2u) != 0u;
    bool _730;
    if (!_720)
    {
        _730 = (uint(_172.bitOptions) & 512u) != 0u;
    }
    else
    {
        _730 = _720;
    }
    float4 normalTexVal;
    if (_730)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _172);
    }
    float healthMix = 0.0;
    float4 myPerlin = float4(0.0);
    bool _747 = (uint(_172.bitOptions) & 128u) != 0u;
    bool _756;
    if (!_747)
    {
        _756 = (uint(_172.bitOptions) & 512u) != 0u;
    }
    else
    {
        _756 = _747;
    }
    if (_756)
    {
        float3 seedVec = _395.pieceVertexPosOrig.xyz * 0.60000002384185791015625;
        float param_2 = unitID;
        seedVec.y += ((1024.0 * hash11(param_2)) + dot(seedVec.xz, float2(0.100000001490116119384765625)));
        int param_3 = 10;
        float3 param_4 = fract(seedVec * 0.100000001490116119384765625);
        float param_5 = 0.0;
        myPerlin = (NativeCUSLod(noisetex3dcube, noisetex3dcubeSmplr, param_3, param_4, param_5, _172) * 2.0) - float4(1.0);
        healthMix = ((myPerlin.x * 0.5) + 0.5) * (2.0 - _172.baseVertexDisplacement);
        healthMix = smoothstep(0.0, healthMix, fast::max((1.0 - _395.aoterm_fogFactor_selfIllumMod_healthFraction.w) + _172.baseVertexDisplacement, 0.0)) * 0.800000011920928955078125;
    }
    float3 tbnNormal;
    if ((uint(_172.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        bool _830 = (uint(_172.bitOptions) & 128u) != 0u;
        bool _839;
        if (!_830)
        {
            _839 = (uint(_172.bitOptions) & 512u) != 0u;
        }
        else
        {
            _839 = _830;
        }
        if (_839)
        {
            if (healthMix > 0.0500000007450580596923828125)
            {
                int param_6 = 5;
                float2 param_7 = myUV;
                float param_8 = textureLODBias;
                float3 tbnNormalw = (NativeCUSTexture(normalTexw, normalTexwSmplr, param_6, param_7, param_8, _172).xyz * 2.0) - float3(1.0);
                float3 wrecknormal = tbnNormalw;
                tbnNormal = mix(tbnNormal, tbnNormalw, float3(healthMix));
            }
        }
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_9 = 0;
    float2 param_10 = myUV;
    float param_11 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_9, param_10, param_11, _172);
    int param_12 = 1;
    float2 param_13 = myUV;
    float param_14 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_12, param_13, param_14, _172);
    float4 teamColor = _395.teamCol;
    float3 param_15 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_15, _172);
    if ((uint(_172.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_395.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_172.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_16 = roughness;
    float3 param_17 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_16, param_17), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_147.sunDir.xyz);
    float3 V = fast::normalize(_1002.cameraViewInv[3].xyz - _395.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_172.bitOptions) & 1u) != 0u)
    {
        float param_18 = NdotL;
        gShadow = GetShadowPCFRandom(param_18, shadowsQuality, _172, _395, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _147.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_19 = 9;
    float2 param_20 = float2(NdotV, roughness);
    float param_21 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_19, param_20, param_21, _172).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_22 = F0;
        float3 param_23 = F90;
        float param_24 = VdotH;
        float3 F = FresnelSchlick(param_22, param_23, param_24);
        float param_25 = NdotL;
        float param_26 = NdotV;
        float param_27 = roughness2;
        float Vis = VisibilityOcclusionFast(param_25, param_26, param_27);
        float param_28 = NdotH;
        float param_29 = roughness4;
        float D = MicrofacetDistribution(param_28, param_29);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_147.sunSpecularModel.xyz, _147.sunDiffuseModel.xyz, float3(step(dot(_147.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_147.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_30 = F0;
    float3 param_31 = F90;
    float param_32 = VdotH;
    float param_33 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_30, param_31, param_32, param_33);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_147.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _395.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 reflectionColor = float3(0.0);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_34 = NdotV;
    float param_35 = _395.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_36 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_34, param_35, param_36);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _172);
    if ((uint(_172.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_147.fogColor.xyz, outColor, float3(_395.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _172.brightnessFactor;
    float buildProgress = _395.shadowVertexPos.w;
    if (buildProgress > (-0.5))
    {
        myPerlin = myPerlin;
        float height = fast::clamp(_395.pieceVertexPosOrig.w / 1.0, 0.0, 1.0);
        float sintime = 0.5 + (0.5 * sin(simFrame * 0.100000001490116119384765625));
        float sintimefast = 0.5 + (0.5 * sin(simFrame * 0.1500000059604644775390625));
        float sinprogress = 0.5 + (0.5 * sin((buildProgress * 0.00999999977648258209228515625) * 3.141499996185302734375));
        float4 progressLevels = float4(buildProgress);
        progressLevels = powr(progressLevels, float4(3.0, 1.5, 0.699999988079071044921875, 0.3499999940395355224609375));
        progressLevels = mix(progressLevels, float4(myPerlin.y + (myPerlin.z * sin(simFrame * 0.042341999709606170654296875))), float4(0.0500000007450580596923828125 * smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress)));
        float4 levelLines = fast::clamp(float4(1.0) - (abs(progressLevels - float4(height)) * 100.0), float4(0.0), float4(1.0));
        float levelFactor = dot(levelLines, float4(1.0));
        float3 fragSize = fwidth(_395.worldVertexPos.xyz);
        float fragSizeFactor = 1.0 / dot(float3(1.0), fragSize);
        float3 buildGrid = (abs(fract((_395.worldVertexPos.xyz / float3(8.0)) - float3(0.5)) - float3(0.5)) * 8.0) / fragSize;
        float line = 1.0 - fast::min(fast::min(buildGrid.x, buildGrid.y), buildGrid.z);
        line = fast::clamp(line * smoothstep(-0.5, 1.0, fragSizeFactor), 0.0, 1.0);
        float gridSize = fast::clamp(((1.0 - buildProgress) * 10.0) + 2.0, 2.0, 12.0);
        float3 grid = step(float3(0.5), fast::clamp(float3(1.0) - (fract(_395.pieceVertexPosOrig.xyz / float3(gridSize)) * 10.0), float3(0.0), float3(1.0)));
        float3 pulseTeamColor = mix(_395.teamCol.xyz, _395.teamCol.xyz * 1.7799999713897705078125, float3(sintimefast));
        if (height > progressLevels.x)
        {
            float unemissive = 1.0 - smoothstep(progressLevels.x, progressLevels.y, height);
            outSpecularColor *= unemissive;
            outColor -= ((albedoColor * emissiveness) * (1.0 - unemissive));
            emissiveness *= unemissive;
        }
        if (height > progressLevels.y)
        {
            outColor = mix(outColor, pulseTeamColor, float3(smoothstep(progressLevels.y, progressLevels.z, height)));
        }
        if (height > progressLevels.z)
        {
            outColor = pulseTeamColor * fast::clamp(NdotL, 0.300000011920928955078125, 1.0);
        }
        if (height > progressLevels.w)
        {
            float heightProgress = smoothstep(progressLevels.w, 1.0, height);
            float flatlight = 0.300000011920928955078125 + (0.699999988079071044921875 * smoothstep(0.0, 1.0, fast::clamp((dot(worldNormal, L) * 0.5) + 0.5, heightProgress * 0.20000000298023223876953125, 1.0)));
            outColor = pulseTeamColor * flatlight;
            N = worldNormal;
        }
        float last5percent = smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress);
        outColor = mix(outColor, pulseTeamColor, float3(line * last5percent));
        outColor += float3(levelFactor);
        outSpecularColor += float3(levelFactor);
        outSpecularColor += (((pulseTeamColor * line) * sintimefast) * 2.0);
    }
    float selectedness = _395.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_395.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1637 = fast::clamp(_395.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1637.x;
        selectionHighlight.y = _1637.y;
        selectionHighlight.z = _1637.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _395.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float cloakTime = _395.worldVertexPos.w;
    if (abs(cloakTime) > 0.5)
    {
        float cloakedness = 0.0;
        if (cloakTime > 0.0)
        {
            cloakedness = fast::clamp((_147.timeInfo.x - cloakTime) / 15.0, 0.0, 1.0);
        }
        if (cloakTime < 0.0)
        {
            cloakedness = 1.0 - fast::clamp((_147.timeInfo.x + cloakTime) / 15.0, 0.0, 1.0);
        }
        float sintime_1 = fract(simFrame * 0.0199999995529651641845703125);
        myPerlin.y = (myPerlin.y * 0.5) + 0.5;
        texColor2.w = 1.0 - fast::clamp(cloakedness * 0.4900000095367431640625, 0.0, 0.4900000095367431640625);
        float perlinline1 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract(simFrame * 0.004999999888241291046142578125))), 0.0, 1.0);
        float perlinline2 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract((simFrame * 0.004999999888241291046142578125) + 0.5))), 0.0, 1.0);
        float cloaknoise = (cloakedness * perlinline1) + (cloakedness * perlinline2);
        outColor += float3(cloaknoise * 0.85000002384185791015625);
        float dotcamera_1 = dot(worldNormal, V);
        float highLightOpacity_1 = fast::clamp(1.0 - dotcamera_1, 0.0, 1.0);
        highLightOpacity_1 *= highLightOpacity_1;
        outColor = mix(outColor, _395.teamCol.xyz * 3.0, float3(highLightOpacity_1 * cloakedness));
        outSpecularColor += float3(fast::clamp(cloaknoise * 0.75, 0.0, 1.0));
    }
    float alphaBin = (texColor2.w < 0.5) ? 0.0 : 1.0;
    outSpecularColor = CustomTM(outSpecularColor, pbrParams, _172);
    fragData[0] = float4((N * 0.5) + float3(0.5), alphaBin);
    fragData[1] = float4(outColor, alphaBin);
    fragData[2] = float4(outSpecularColor, alphaBin);
    fragData[3] = float4(((albedoColor * emissiveness) * 2.0) + (outSpecularColor * 0.300000011920928955078125), alphaBin);
    // ENABLE_OPTION_HEALTH_TEXTURING: original unit outline identity channels.
    uint outlineID = uint(unitID) + 1u;
    fragData[4] = float4(float(outlineID & 255u) / 255.0, 1.0, float(outlineID >> 8u) / 255.0, alphaBin);
    out.fragData_0 = fragData[0];
    out.fragData_1 = fragData[1];
    out.fragData_2 = fragData[2];
    out.fragData_3 = fragData[3];
    out.fragData_4 = fragData[4];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_2_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"baseVertexDisplacement",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTexw",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"noisetex3dcube",35679,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_3_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,true},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_3_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_840_pieceVertexPosOrig [[user(locn0)]];
    float4 m_840_worldVertexPos [[user(locn1)]];
    float3 m_840_worldTangent_VS [[user(locn2)]];
    float3 m_840_worldNormal_VS [[user(locn3)]];
    float4 m_840_uvCoords [[user(locn4)]];
    float4 m_840_teamCol [[user(locn5)]];
    float4 m_840_shadowVertexPos [[user(locn6)]];
    float4 m_840_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _585, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_585.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _701 = mVP;
    float3 _704 = _701.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _704.x;
    mVP.y = _704.y;
    mVP.z = _704.z;
    float4 _717 = mVP;
    float2 _719 = _717.xz + (float2(diff) + (wind * diff2));
    mVP.x = _719.x;
    mVP.z = _719.y;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float Perlin3D(thread const float3& P)
{
    float3 Pi = floor(P);
    float3 Pf = P - Pi;
    float3 Pf_min1 = Pf - float3(1.0);
    Pi -= (floor(Pi * 0.0144927538931369781494140625) * 69.0);
    float3 Pi_inc1 = step(Pi, float3(67.5)) * (Pi + float3(1.0));
    float4 Pt = float4(Pi.xy, Pi_inc1.xy) + float4(50.0, 161.0, 50.0, 161.0);
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    float3 lowz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float3 highz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi_inc1.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float4 hashx0 = fract(Pt * lowz_mod.xxxx);
    float4 hashx1 = fract(Pt * highz_mod.xxxx);
    float4 hashy0 = fract(Pt * lowz_mod.yyyy);
    float4 hashy1 = fract(Pt * highz_mod.yyyy);
    float4 hashz0 = fract(Pt * lowz_mod.zzzz);
    float4 hashz1 = fract(Pt * highz_mod.zzzz);
    float4 grad_x0 = hashx0 - float4(0.499989986419677734375);
    float4 grad_y0 = hashy0 - float4(0.499989986419677734375);
    float4 grad_z0 = hashz0 - float4(0.499989986419677734375);
    float4 grad_x1 = hashx1 - float4(0.499989986419677734375);
    float4 grad_y1 = hashy1 - float4(0.499989986419677734375);
    float4 grad_z1 = hashz1 - float4(0.499989986419677734375);
    float4 grad_results_0 = rsqrt(((grad_x0 * grad_x0) + (grad_y0 * grad_y0)) + (grad_z0 * grad_z0)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x0) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y0)) + (Pf.zzzz * grad_z0));
    float4 grad_results_1 = rsqrt(((grad_x1 * grad_x1) + (grad_y1 * grad_y1)) + (grad_z1 * grad_z1)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x1) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y1)) + (Pf_min1.zzzz * grad_z1));
    float3 blend = ((Pf * Pf) * Pf) * ((Pf * ((Pf * 6.0) - float3(15.0))) + float3(10.0));
    float4 res0 = mix(grad_results_0, grad_results_1, float4(blend.z));
    float4 blend2 = float4(blend.xy, float2(1.0) - blend.xy);
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    return final * 1.1547005176544189453125;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _214 = Lerp(param, param_1, param_2);
    return Transform{ _214, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _110, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _760, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _764 = instData.x + bID0;
        tx.quat = _760.transforms[_764].quat;
        tx.trSc = _760.transforms[_764].trSc;
    }
    else
    {
        uint _779 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _786 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _760.transforms[_779].quat;
        param_4.trSc = _760.transforms[_779].trSc;
        Transform param_5;
        param_5.quat = _760.transforms[_786].quat;
        param_5.trSc = _760.transforms[_786].trSc;
        float param_6 = _110.timeInfo.w;
        Transform _804 = Lerp(param_4, param_5, param_6);
        tx = _804;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _81, thread float& simFrame, constant UniformParamsBuffer& _110, const device UniformsBuffer& _585, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _760, thread float3& normal, thread Data& _840, constant NativeCUSDrawUniforms& _853, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _840.uvCoords.z = float(_585.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_853.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _585.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _853.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _853.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _853.featureFadeDistSq) / (_853.featureDrawDistSq - _853.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _840.uvCoords.x = uv.xy.x;
    _840.uvCoords.y = uv.xy.y;
    _840.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_585.uni[instData.y].health / _585.uni[instData.y].maxHealth, 0.0, 1.0);
    float buildProgress = ((device float*)&_585.uni[instData.y].userDefined[0])[0u];
    if (buildProgress > (-0.5))
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_840.aoterm_fogFactor_selfIllumMod_healthFraction.w / fast::max(9.9999999392252902907785028219223e-09, buildProgress), 0.0, 1.0);
        if (buildProgress < 0.999000012874603271484375)
        {
            piecePos.y += (0.5 * (1.0 - smoothstep(0.949999988079071044921875, 1.0, buildProgress)));
        }
    }
    float4 _966 = piecePos;
    float3 _968 = _966.xyz * featureFadeFactor;
    piecePos.x = _968.x;
    piecePos.y = _968.y;
    piecePos.z = _968.z;
    _840.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_853.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _585, instData);
        piecePos = param;
    }
    if ((uint(_853.bitOptions) & 256u) != 0u)
    {
        bool _1002 = _840.aoterm_fogFactor_selfIllumMod_healthFraction.w < 0.949999988079071044921875;
        bool _1011;
        if (!_1002)
        {
            _1011 = _853.baseVertexDisplacement > 0.00999999977648258209228515625;
        }
        else
        {
            _1011 = _1002;
        }
        if (_1011)
        {
            float3 seedVec = piecePos.xyz * 0.100000001490116119384765625;
            float param_1 = float(_585.uni[instData.y].composite >> uint(16));
            seedVec.y += (1024.0 * hash11(param_1));
            float damageAmount = (1.0 - _840.aoterm_fogFactor_selfIllumMod_healthFraction.w) * 0.800000011920928955078125;
            float3 param_2 = seedVec;
            float4 _1048 = piecePos;
            float4 _1052 = piecePos;
            float3 _1054 = _1052.xyz + (fast::normalize(_1048.xyz) * ((fast::max(damageAmount, _853.baseVertexDisplacement) * 6.0) * Perlin3D(param_2)));
            piecePos.x = _1054.x;
            piecePos.y = _1054.y;
            piecePos.z = _1054.z;
        }
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1076 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1084;
    if (!_1076)
    {
        _1084 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1084 = _1076;
    }
    if (_1084)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_3 = piecePos.xyz;
    float3 param_6 = safeT;
    float3 param_8 = safeB;
    float4 param_4;
    float3 param_5;
    float3 param_7;
    float3 param_9;
    GetModelSpaceVertex(param_3, param_4, param_5, param_6, param_7, param_8, param_9, _110, instData, bonesInfo, _760, normal);
    modelPos = param_4;
    modelVertexNormal = param_5;
    float3 modelSpaceTangent = param_7;
    float3 modelSpaceBitangent = param_9;
    uint _1110 = instData.x + 0u;
    uint _1113 = instData.x + 1u;
    Transform param_10;
    param_10.quat = _760.transforms[_1110].quat;
    param_10.trSc = _760.transforms[_1110].trSc;
    Transform param_11;
    param_11.quat = _760.transforms[_1113].quat;
    param_11.trSc = _760.transforms[_1113].trSc;
    float param_12 = _110.timeInfo.w;
    Transform _1131 = Lerp(param_10, param_11, param_12);
    Transform modelToWorldTX = _1131;
    Transform param_13 = modelToWorldTX;
    float4 param_14 = modelPos;
    float4 worldPos = ApplyTransform(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelSpaceTangent;
    _840.worldTangent_VS = RotateByQuaternion(param_15, param_16);
    float4 param_17 = modelToWorldTX.quat;
    float3 param_18 = modelVertexNormal;
    _840.worldNormal_VS = RotateByQuaternion(param_17, param_18);
    _840.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_585.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _840.teamCol = ((teamIndex == 255u) ? float4(0.0) : _110.teamColor[teamIndex]);
    _840.teamCol.w = ((device float*)&_585.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_585.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_110.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _840.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    bool _1181 = (uint(_853.bitOptions) & 32u) != 0u;
    bool _1191;
    if (!_1181)
    {
        _1191 = (uint(_853.bitOptions) & 64u) != 0u;
    }
    else
    {
        _1191 = _1181;
    }
    bool _1201;
    if (!_1191)
    {
        _1201 = (uint(_853.bitOptions) & 8192u) != 0u;
    }
    else
    {
        _1201 = _1191;
    }
    if (_1201)
    {
        float unitSpeed = ((device float*)&_585.uni[instData.y].speed)[3u];
        if (unitSpeed > 0.5)
        {
            unitSpeed = fast::max(3.0, unitSpeed);
        }
        float loopingFrameCount = mod(simFrame, 8.0);
        float baseOffset = loopingFrameCount * unitSpeed;
        if ((uint(_853.bitOptions) & 32u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.628173828125, _840.uvCoords.y <= 0.6220703125, _840.uvCoords.x <= 0.749755859375, _840.uvCoords.y >= 0.60205078125)))
            {
                _840.uvCoords.x += (mod(baseOffset * 4.0, 12.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_840.uvCoords.x >= 0.6279296875, _840.uvCoords.y <= 0.601806640625, _840.uvCoords.x <= 0.75, _840.uvCoords.y >= 0.569580078125)))
                {
                    _840.uvCoords.x += (mod(baseOffset * 4.0, 20.0) / 4096.0);
                }
            }
        }
        if ((uint(_853.bitOptions) & 64u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.74267578125, _840.uvCoords.y <= 0.062744140625, _840.uvCoords.x <= 1.0, _840.uvCoords.y >= 0.0)))
            {
                _840.uvCoords.x += (mod(baseOffset * (-6.0), 56.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_840.uvCoords.x >= 0.046875, _840.uvCoords.y <= 0.8447265625, _840.uvCoords.x <= 0.171630859375, _840.uvCoords.y >= 0.828125)))
                {
                    _840.uvCoords.x += (mod(baseOffset * 3.0, 24.0) / 4096.0);
                }
                else
                {
                    if (all(bool4(_840.uvCoords.x >= 0.046142578125, _840.uvCoords.y <= 0.827880859375, _840.uvCoords.x <= 0.169677734375, _840.uvCoords.y >= 0.804931640625)))
                    {
                        _840.uvCoords.x += (mod(baseOffset * 3.0, 28.0) / 4096.0);
                    }
                }
            }
        }
        if ((uint(_853.bitOptions) & 8192u) != 0u)
        {
            if (all(bool4(_840.uvCoords.x >= 0.637939453125, _840.uvCoords.y <= 0.8125, _840.uvCoords.x <= 0.799072265625, _840.uvCoords.y >= 0.7138671875)))
            {
                _840.uvCoords.x += (mod(baseOffset * (-6.0), 55.0) / 4096.0);
            }
        }
    }
    _840.worldVertexPos = worldPos;
    _840.shadowVertexPos = _81.shadowView * worldPos;
    float4 _1434 = _840.shadowVertexPos;
    float2 _1436 = _1434.xy + float2(0.5);
    _840.shadowVertexPos.x = _1436.x;
    _840.shadowVertexPos.y = _1436.y;
    _840.shadowVertexPos.w = ((device float*)&_585.uni[instData.y].userDefined[0])[0u];
    if ((uint(_853.bitOptions) & 8u) != 0u)
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_853.bitOptions) & 16u) != 0u)
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_585.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_853.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _81.cameraView * _840.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_110.fogParams.y - fogCoord) * _110.fogParams.w;
        _840.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_840.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_853.drawPass) & 4u) == 4u)
    {
        gl_Position = _81.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _853.clipPlane0);
    }
    else
    {
        gl_Position = _81.cameraViewProj * worldPos;
    }
    _840.worldVertexPos.w = ((device float*)&_585.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _81 [[buffer(2)]], constant UniformParamsBuffer& _110 [[buffer(3)]], const device TransformBuffer& _760 [[buffer(4)]], const device UniformsBuffer& _585 [[buffer(5)]], constant NativeCUSDrawUniforms& _853 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _840 = {};
    float3 cameraPos = _81.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_81.cameraView[0].z, _81.cameraView[1].z, _81.cameraView[2].z) * (-1.0);
    float simFrame = _110.timeInfo.x + _110.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _81, simFrame, _110, _585, in.instData, in.bonesInfo, _760, in.normal, _840, _853, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_853.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_853.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_853.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_840_pieceVertexPosOrig = _840.pieceVertexPosOrig;
    out.m_840_worldVertexPos = _840.worldVertexPos;
    out.m_840_worldTangent_VS = _840.worldTangent_VS;
    out.m_840_worldNormal_VS = _840.worldNormal_VS;
    out.m_840_uvCoords = _840.uvCoords;
    out.m_840_teamCol = _840.teamCol;
    out.m_840_shadowVertexPos = _840.shadowVertexPos;
    out.m_840_aoterm_fogFactor_selfIllumMod_healthFraction = _840.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_3_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,true},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_3_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _194 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _476 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_455_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_teamCol [[user(locn5), flat]];
    float4 m_455_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _200)
{
    return (_200.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _200)
{
    if ((_200.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _200)
{
    uint code = _200.NativeCUSSampleCodes[slot].x;
    if ((_200.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _266;
        if (mode == 0u)
        {
            _266 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _278;
            if (mode == 1u)
            {
                _278 = float4(value.x);
            }
            else
            {
                _278 = float4(0.0, 0.0, 0.0, value.x);
            }
            _266 = _278;
        }
        value = _266;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _333 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _333);
    float4 _338 = NativeCUSChannels(param_3, param_4, _200);
    return _338;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture3d<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, level(lod));
    float4 _426 = NativeCUSChannels(param_1, param_2, _200);
    return _426;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _352 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _352, bias(bias0));
    float4 _358 = NativeCUSChannels(param_3, param_4, _200);
    return _358;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_200.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _382 = NativeCUSUV(param, param_1, _200);
    uv.x = _382.x;
    uv.y = _382.y;
    if ((_200.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _200, thread Data& _455, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _455.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _476[presetIndex].samplingRandomness;
    float samplingDistance = _476[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _521 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _200);
        shadow = _521;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _404 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _404, level(lod));
    float4 _410 = NativeCUSChannels(param_3, param_4, _200);
    return _410;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    int2 _433;
    if (NativeCUSEmpty(param, _200))
    {
        _433 = int2(0);
    }
    else
    {
        _433 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _433;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _374 = NativeCUSChannels(param_1, param_2, _200);
    return _374;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _200)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _200);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _200).xyz;
    return SampleReflectionMapMod(param_3, _200);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _200).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _200, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _200.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _200)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _200);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1168 [[buffer(2)]], constant UniformParamsBuffer& _175 [[buffer(3)]], constant NativeCUSDrawUniforms& _200 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> texture1w [[texture(3)]], texture2d<float> texture2w [[texture(4)]], texture2d<float> normalTexw [[texture(5)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> brdfLUT [[texture(9)]], texture3d<float> noisetex3dcube [[texture(10)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler texture1wSmplr [[sampler(3)]], sampler texture2wSmplr [[sampler(4)]], sampler normalTexwSmplr [[sampler(5)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler brdfLUTSmplr [[sampler(9)]], sampler noisetex3dcubeSmplr [[sampler(10)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _455 = {};
    _455.pieceVertexPosOrig = in.m_455_pieceVertexPosOrig;
    _455.worldVertexPos = in.m_455_worldVertexPos;
    _455.worldTangent_VS = in.m_455_worldTangent_VS;
    _455.worldNormal_VS = in.m_455_worldNormal_VS;
    _455.uvCoords = in.m_455_uvCoords;
    _455.teamCol = in.m_455_teamCol;
    _455.shadowVertexPos = in.m_455_shadowVertexPos;
    _455.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_455_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _175.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _455.uvCoords.xy;
    float unitID = floor(_455.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_455.worldNormal_VS);
    float3 worldTangent = fast::normalize(_455.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _833 = (uint(_200.bitOptions) & 2u) != 0u;
    bool _843;
    if (!_833)
    {
        _843 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _843 = _833;
    }
    float4 normalTexVal;
    if (_843)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _200);
    }
    float healthMix = 0.0;
    float4 myPerlin = float4(0.0);
    bool _860 = (uint(_200.bitOptions) & 128u) != 0u;
    bool _869;
    if (!_860)
    {
        _869 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _869 = _860;
    }
    if (_869)
    {
        float3 seedVec = _455.pieceVertexPosOrig.xyz * 0.60000002384185791015625;
        float param_2 = unitID;
        seedVec.y += ((1024.0 * hash11(param_2)) + dot(seedVec.xz, float2(0.100000001490116119384765625)));
        int param_3 = 10;
        float3 param_4 = fract(seedVec * 0.100000001490116119384765625);
        float param_5 = 0.0;
        myPerlin = (NativeCUSLod(noisetex3dcube, noisetex3dcubeSmplr, param_3, param_4, param_5, _200) * 2.0) - float4(1.0);
        healthMix = ((myPerlin.x * 0.5) + 0.5) * (2.0 - _200.baseVertexDisplacement);
        healthMix = smoothstep(0.0, healthMix, fast::max((1.0 - _455.aoterm_fogFactor_selfIllumMod_healthFraction.w) + _200.baseVertexDisplacement, 0.0)) * 0.800000011920928955078125;
    }
    float3 tbnNormal;
    if ((uint(_200.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        bool _942 = (uint(_200.bitOptions) & 128u) != 0u;
        bool _951;
        if (!_942)
        {
            _951 = (uint(_200.bitOptions) & 512u) != 0u;
        }
        else
        {
            _951 = _942;
        }
        if (_951)
        {
            if (healthMix > 0.0500000007450580596923828125)
            {
                int param_6 = 5;
                float2 param_7 = myUV;
                float param_8 = textureLODBias;
                float3 tbnNormalw = (NativeCUSTexture(normalTexw, normalTexwSmplr, param_6, param_7, param_8, _200).xyz * 2.0) - float3(1.0);
                float3 wrecknormal = tbnNormalw;
                tbnNormal = mix(tbnNormal, tbnNormalw, float3(healthMix));
            }
        }
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_9 = 0;
    float2 param_10 = myUV;
    float param_11 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_9, param_10, param_11, _200);
    int param_12 = 1;
    float2 param_13 = myUV;
    float param_14 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_12, param_13, param_14, _200);
    if ((uint(_200.bitOptions) & 128u) != 0u)
    {
        if (healthMix > 0.0500000007450580596923828125)
        {
            int param_15 = 3;
            float2 param_16 = myUV;
            float4 texColor1w = NativeCUSTexture(texture1w, texture1wSmplr, param_15, param_16, _200);
            int param_17 = 4;
            float2 param_18 = myUV;
            float4 texColor2w = NativeCUSTexture(texture2w, texture2wSmplr, param_17, param_18, _200);
            healthMix *= (1.0 - (0.89999997615814208984375 * texColor2.x));
            texColor1 = mix(texColor1, texColor1w, float4(healthMix));
            float4 _1034 = texColor2;
            float3 _1040 = mix(_1034.xyz, texColor2w.xyz, float3(healthMix));
            texColor2.x = _1040.x;
            texColor2.y = _1040.y;
            texColor2.z = _1040.z;
            texColor2.z += (0.5 * healthMix);
        }
    }
    float4 teamColor = _455.teamCol;
    float3 param_19 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_19, _200);
    if ((uint(_200.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_455.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_200.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_20 = roughness;
    float3 param_21 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_20, param_21), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_175.sunDir.xyz);
    float3 V = fast::normalize(_1168.cameraViewInv[3].xyz - _455.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_200.bitOptions) & 1u) != 0u)
    {
        float param_22 = NdotL;
        gShadow = GetShadowPCFRandom(param_22, shadowsQuality, _200, _455, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _175.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_23 = 9;
    float2 param_24 = float2(NdotV, roughness);
    float param_25 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_23, param_24, param_25, _200).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_26 = F0;
        float3 param_27 = F90;
        float param_28 = VdotH;
        float3 F = FresnelSchlick(param_26, param_27, param_28);
        float param_29 = NdotL;
        float param_30 = NdotV;
        float param_31 = roughness2;
        float Vis = VisibilityOcclusionFast(param_29, param_30, param_31);
        float param_32 = NdotH;
        float param_33 = roughness4;
        float D = MicrofacetDistribution(param_32, param_33);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_175.sunSpecularModel.xyz, _175.sunDiffuseModel.xyz, float3(step(dot(_175.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_175.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_34 = F0;
    float3 param_35 = F90;
    float param_36 = VdotH;
    float param_37 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_34, param_35, param_36, param_37);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_175.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_38 = Rv;
    float param_39 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_38, param_39, _200, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_40 = NdotV;
    float param_41 = _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_42 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_40, param_41, param_42);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _200);
    if ((uint(_200.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_175.fogColor.xyz, outColor, float3(_455.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _200.brightnessFactor;
    float buildProgress = _455.shadowVertexPos.w;
    if (buildProgress > (-0.5))
    {
        myPerlin = myPerlin;
        float height = fast::clamp(_455.pieceVertexPosOrig.w / 1.0, 0.0, 1.0);
        float sintime = 0.5 + (0.5 * sin(simFrame * 0.100000001490116119384765625));
        float sintimefast = 0.5 + (0.5 * sin(simFrame * 0.1500000059604644775390625));
        float sinprogress = 0.5 + (0.5 * sin((buildProgress * 0.00999999977648258209228515625) * 3.141499996185302734375));
        float4 progressLevels = float4(buildProgress);
        progressLevels = powr(progressLevels, float4(3.0, 1.5, 0.699999988079071044921875, 0.3499999940395355224609375));
        progressLevels = mix(progressLevels, float4(myPerlin.y + (myPerlin.z * sin(simFrame * 0.042341999709606170654296875))), float4(0.0500000007450580596923828125 * smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress)));
        float4 levelLines = fast::clamp(float4(1.0) - (abs(progressLevels - float4(height)) * 100.0), float4(0.0), float4(1.0));
        float levelFactor = dot(levelLines, float4(1.0));
        float3 fragSize = fwidth(_455.worldVertexPos.xyz);
        float fragSizeFactor = 1.0 / dot(float3(1.0), fragSize);
        float3 buildGrid = (abs(fract((_455.worldVertexPos.xyz / float3(8.0)) - float3(0.5)) - float3(0.5)) * 8.0) / fragSize;
        float line = 1.0 - fast::min(fast::min(buildGrid.x, buildGrid.y), buildGrid.z);
        line = fast::clamp(line * smoothstep(-0.5, 1.0, fragSizeFactor), 0.0, 1.0);
        float gridSize = fast::clamp(((1.0 - buildProgress) * 10.0) + 2.0, 2.0, 12.0);
        float3 grid = step(float3(0.5), fast::clamp(float3(1.0) - (fract(_455.pieceVertexPosOrig.xyz / float3(gridSize)) * 10.0), float3(0.0), float3(1.0)));
        float3 pulseTeamColor = mix(_455.teamCol.xyz, _455.teamCol.xyz * 1.7799999713897705078125, float3(sintimefast));
        if (height > progressLevels.x)
        {
            float unemissive = 1.0 - smoothstep(progressLevels.x, progressLevels.y, height);
            outSpecularColor *= unemissive;
            outColor -= ((albedoColor * emissiveness) * (1.0 - unemissive));
            emissiveness *= unemissive;
        }
        if (height > progressLevels.y)
        {
            outColor = mix(outColor, pulseTeamColor, float3(smoothstep(progressLevels.y, progressLevels.z, height)));
        }
        if (height > progressLevels.z)
        {
            outColor = pulseTeamColor * fast::clamp(NdotL, 0.300000011920928955078125, 1.0);
        }
        if (height > progressLevels.w)
        {
            float heightProgress = smoothstep(progressLevels.w, 1.0, height);
            float flatlight = 0.300000011920928955078125 + (0.699999988079071044921875 * smoothstep(0.0, 1.0, fast::clamp((dot(worldNormal, L) * 0.5) + 0.5, heightProgress * 0.20000000298023223876953125, 1.0)));
            outColor = pulseTeamColor * flatlight;
            N = worldNormal;
            texColor2.w *= fast::max(0.4000000059604644775390625, fast::max(fast::clamp(line, buildProgress, 1.0), 1.0 - heightProgress));
        }
        float last5percent = smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress);
        outColor = mix(outColor, pulseTeamColor, float3(line * last5percent));
        outColor += float3(levelFactor);
        outSpecularColor += float3(levelFactor);
        outSpecularColor += (((pulseTeamColor * line) * sintimefast) * 2.0);
    }
    float selectedness = _455.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_455.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1815 = fast::clamp(_455.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1815.x;
        selectionHighlight.y = _1815.y;
        selectionHighlight.z = _1815.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _455.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float cloakTime = _455.worldVertexPos.w;
    if (abs(cloakTime) > 0.5)
    {
        float cloakedness = 0.0;
        if (cloakTime > 0.0)
        {
            cloakedness = fast::clamp((_175.timeInfo.x - cloakTime) / 15.0, 0.0, 1.0);
        }
        if (cloakTime < 0.0)
        {
            cloakedness = 1.0 - fast::clamp((_175.timeInfo.x + cloakTime) / 15.0, 0.0, 1.0);
        }
        float sintime_1 = fract(simFrame * 0.0199999995529651641845703125);
        myPerlin.y = (myPerlin.y * 0.5) + 0.5;
        texColor2.w = 1.0 - fast::clamp(cloakedness * 0.4900000095367431640625, 0.0, 0.4900000095367431640625);
        float perlinline1 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract(simFrame * 0.004999999888241291046142578125))), 0.0, 1.0);
        float perlinline2 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract((simFrame * 0.004999999888241291046142578125) + 0.5))), 0.0, 1.0);
        float cloaknoise = (cloakedness * perlinline1) + (cloakedness * perlinline2);
        outColor += float3(cloaknoise * 0.85000002384185791015625);
        float dotcamera_1 = dot(worldNormal, V);
        float highLightOpacity_1 = fast::clamp(1.0 - dotcamera_1, 0.0, 1.0);
        highLightOpacity_1 *= highLightOpacity_1;
        outColor = mix(outColor, _455.teamCol.xyz * 3.0, float3(highLightOpacity_1 * cloakedness));
        outSpecularColor += float3(fast::clamp(cloaknoise * 0.75, 0.0, 1.0));
    }
    fragData[0] = float4(outColor, texColor2.w);
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_3_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"baseVertexDisplacement",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture1w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTexw",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"noisetex3dcube",35679,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_4_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,true},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_4_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_1252_pieceVertexPosOrig [[user(locn0)]];
    float4 m_1252_worldVertexPos [[user(locn1)]];
    float3 m_1252_worldTangent_VS [[user(locn2)]];
    float3 m_1252_worldNormal_VS [[user(locn3)]];
    float4 m_1252_uvCoords [[user(locn4)]];
    float4 m_1252_teamCol [[user(locn5)]];
    float4 m_1252_shadowVertexPos [[user(locn6)]];
    float4 m_1252_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _733, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_733.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _849 = mVP;
    float3 _852 = _849.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _852.x;
    mVP.y = _852.y;
    mVP.z = _852.z;
    float4 _865 = mVP;
    float2 _867 = _865.xz + (float2(diff) + (wind * diff2));
    mVP.x = _867.x;
    mVP.z = _867.y;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float Perlin3D(thread const float3& P)
{
    float3 Pi = floor(P);
    float3 Pf = P - Pi;
    float3 Pf_min1 = Pf - float3(1.0);
    Pi -= (floor(Pi * 0.0144927538931369781494140625) * 69.0);
    float3 Pi_inc1 = step(Pi, float3(67.5)) * (Pi + float3(1.0));
    float4 Pt = float4(Pi.xy, Pi_inc1.xy) + float4(50.0, 161.0, 50.0, 161.0);
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    float3 lowz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float3 highz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi_inc1.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float4 hashx0 = fract(Pt * lowz_mod.xxxx);
    float4 hashx1 = fract(Pt * highz_mod.xxxx);
    float4 hashy0 = fract(Pt * lowz_mod.yyyy);
    float4 hashy1 = fract(Pt * highz_mod.yyyy);
    float4 hashz0 = fract(Pt * lowz_mod.zzzz);
    float4 hashz1 = fract(Pt * highz_mod.zzzz);
    float4 grad_x0 = hashx0 - float4(0.499989986419677734375);
    float4 grad_y0 = hashy0 - float4(0.499989986419677734375);
    float4 grad_z0 = hashz0 - float4(0.499989986419677734375);
    float4 grad_x1 = hashx1 - float4(0.499989986419677734375);
    float4 grad_y1 = hashy1 - float4(0.499989986419677734375);
    float4 grad_z1 = hashz1 - float4(0.499989986419677734375);
    float4 grad_results_0 = rsqrt(((grad_x0 * grad_x0) + (grad_y0 * grad_y0)) + (grad_z0 * grad_z0)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x0) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y0)) + (Pf.zzzz * grad_z0));
    float4 grad_results_1 = rsqrt(((grad_x1 * grad_x1) + (grad_y1 * grad_y1)) + (grad_z1 * grad_z1)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x1) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y1)) + (Pf_min1.zzzz * grad_z1));
    float3 blend = ((Pf * Pf) * Pf) * ((Pf * ((Pf * 6.0) - float3(15.0))) + float3(10.0));
    float4 res0 = mix(grad_results_0, grad_results_1, float4(blend.z));
    float4 blend2 = float4(blend.xy, float2(1.0) - blend.xy);
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    return final * 1.1547005176544189453125;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _363 = Lerp(param, param_1, param_2);
    return Transform{ _363, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
float4 InvertNormalizedQuaternion(thread const float4& q)
{
    return float4(-q.x, -q.y, -q.z, q.w);
}

static inline __attribute__((always_inline))
Transform InvertTransformAffine(thread const Transform& tra)
{
    float4 param = tra.quat;
    float4 invR = InvertNormalizedQuaternion(param);
    float invS = 1.0 / tra.trSc.w;
    float4 param_1 = invR;
    float3 param_2 = (-tra.trSc.xyz) * invS;
    return Transform{ invR, float4(RotateByQuaternion(param_1, param_2), invS) };
}

static inline __attribute__((always_inline))
float4 MultiplyQuat(thread const float4& a, thread const float4& b)
{
    return float4(((b.xyz * a.w) + (a.xyz * b.w)) + cross(a.xyz, b.xyz), (a.w * b.w) - dot(a.xyz, b.xyz));
}

static inline __attribute__((always_inline))
Transform ApplyTransform(thread const Transform& parentTra, thread const Transform& childTra)
{
    float4 param = parentTra.quat;
    float4 param_1 = childTra.quat;
    float4 param_2 = parentTra.quat;
    float3 param_3 = childTra.trSc.xyz * parentTra.trSc.w;
    return Transform{ MultiplyQuat(param, param_1), float4(parentTra.trSc.xyz + RotateByQuaternion(param_2, param_3), parentTra.trSc.w * childTra.trSc.w) };
}

static inline __attribute__((always_inline))
float3 ApplyTransform(thread const Transform& tra, thread const float3& v)
{
    float4 param = tra.quat;
    float3 param_1 = v * tra.trSc.w;
    return RotateByQuaternion(param, param_1) + tra.trSc.xyz;
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _133, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _908, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _912 = instData.x + bID0;
        tx.quat = _908.transforms[_912].quat;
        tx.trSc = _908.transforms[_912].trSc;
    }
    else
    {
        uint _927 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _934 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _908.transforms[_927].quat;
        param_4.trSc = _908.transforms[_927].trSc;
        Transform param_5;
        param_5.quat = _908.transforms[_934].quat;
        param_5.trSc = _908.transforms[_934].trSc;
        float param_6 = _133.timeInfo.w;
        Transform _952 = Lerp(param_4, param_5, param_6);
        tx = _952;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
    bool _993;
    if (!staticModel)
    {
        _993 = weights.x == 1.0;
    }
    else
    {
        _993 = staticModel;
    }
    if (_993)
    {
        return;
    }
    uint param_15 = bonesInfo.y;
    uint param_16 = 1u;
    uint param_17 = bonesInfo.y;
    uint param_18 = 2u;
    uint param_19 = bonesInfo.y;
    uint param_20 = 3u;
    float3 _1018 = float3(float(GetUnpackedValue(param_15, param_16)) / 255.0, float(GetUnpackedValue(param_17, param_18)) / 255.0, float(GetUnpackedValue(param_19, param_20)) / 255.0);
    weights.y = _1018.x;
    weights.z = _1018.y;
    weights.w = _1018.z;
    float wSum = 0.0;
    msPosition *= weights.x;
    msNormal *= weights.x;
    msTangent *= weights.x;
    msBitangent *= weights.x;
    wSum += weights.x;
    uint _1050 = instData.w + bID0;
    Transform bposeTra;
    bposeTra.quat = _908.transforms[_1050].quat;
    bposeTra.trSc = _908.transforms[_1050].trSc;
    Transform param_25;
    Transform param_26;
    Transform param_27;
    for (uint bi = 1u; bi < 3u; bi++)
    {
        uint param_21 = bonesInfo.x;
        uint param_22 = bi;
        uint param_23 = bonesInfo.z;
        uint param_24 = bi;
        uint bID = GetUnpackedValue(param_21, param_22) + (GetUnpackedValue(param_23, param_24) << 8u);
        bool _1082 = bID == 65535u;
        bool _1090;
        if (!_1082)
        {
            _1090 = weights[bi] == 0.0;
        }
        else
        {
            _1090 = _1082;
        }
        if (_1090)
        {
            continue;
        }
        uint _1098 = instData.w + bID;
        param_25.quat = _908.transforms[_1098].quat;
        param_25.trSc = _908.transforms[_1098].trSc;
        Transform bposeInvTra = InvertTransformAffine(param_25);
        uint _1114 = (instData.x + (2u * (1u + bID))) + 0u;
        uint _1121 = (instData.x + (2u * (1u + bID))) + 1u;
        param_26.quat = _908.transforms[_1114].quat;
        param_26.trSc = _908.transforms[_1114].trSc;
        param_27.quat = _908.transforms[_1121].quat;
        param_27.trSc = _908.transforms[_1121].trSc;
        float param_28 = _133.timeInfo.w;
        Transform _1139 = Lerp(param_26, param_27, param_28);
        Transform boneTx = _1139;
        weights[bi] *= float(boneTx.trSc.w > 0.0);
        Transform param_29 = bposeInvTra;
        Transform param_30 = bposeTra;
        Transform param_31 = boneTx;
        Transform param_32 = ApplyTransform(param_29, param_30);
        Transform param_33 = ApplyTransform(param_31, param_32);
        float4 param_34 = piecePos;
        float4 txPiecePos = ApplyTransform(param_33, param_34);
        tx.trSc = float4(0.0, 0.0, 0.0, 1.0);
        Transform param_35 = bposeInvTra;
        Transform param_36 = bposeTra;
        Transform param_37 = boneTx;
        Transform param_38 = ApplyTransform(param_35, param_36);
        Transform param_39 = ApplyTransform(param_37, param_38);
        float3 param_40 = normal;
        float3 txPieceNormal = ApplyTransform(param_39, param_40);
        Transform param_41 = bposeInvTra;
        Transform param_42 = bposeTra;
        Transform param_43 = boneTx;
        Transform param_44 = ApplyTransform(param_41, param_42);
        Transform param_45 = ApplyTransform(param_43, param_44);
        float3 param_46 = safeTangent;
        float3 txPieceTangent = ApplyTransform(param_45, param_46);
        Transform param_47 = bposeInvTra;
        Transform param_48 = bposeTra;
        Transform param_49 = boneTx;
        Transform param_50 = ApplyTransform(param_47, param_48);
        Transform param_51 = ApplyTransform(param_49, param_50);
        float3 param_52 = safeBitangent;
        float3 txPieceBitangent = ApplyTransform(param_51, param_52);
        msPosition += (txPiecePos * weights[bi]);
        msNormal += (txPieceNormal * weights[bi]);
        msTangent += (txPieceTangent * weights[bi]);
        msBitangent += (txPieceBitangent * weights[bi]);
        wSum += weights[bi];
    }
    msPosition /= float4(wSum);
    msNormal /= float3(wSum);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _104, thread float& simFrame, constant UniformParamsBuffer& _133, const device UniformsBuffer& _733, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _908, thread float3& normal, thread Data& _1252, constant NativeCUSDrawUniforms& _1265, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _1252.uvCoords.z = float(_733.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_1265.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _733.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _1265.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _1265.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _1265.featureFadeDistSq) / (_1265.featureDrawDistSq - _1265.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _1252.uvCoords.x = uv.xy.x;
    _1252.uvCoords.y = uv.xy.y;
    _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_733.uni[instData.y].health / _733.uni[instData.y].maxHealth, 0.0, 1.0);
    float buildProgress = ((device float*)&_733.uni[instData.y].userDefined[0])[0u];
    if (buildProgress > (-0.5))
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_1252.aoterm_fogFactor_selfIllumMod_healthFraction.w / fast::max(9.9999999392252902907785028219223e-09, buildProgress), 0.0, 1.0);
        if (buildProgress < 0.999000012874603271484375)
        {
            piecePos.y += (0.5 * (1.0 - smoothstep(0.949999988079071044921875, 1.0, buildProgress)));
        }
    }
    float4 _1378 = piecePos;
    float3 _1380 = _1378.xyz * featureFadeFactor;
    piecePos.x = _1380.x;
    piecePos.y = _1380.y;
    piecePos.z = _1380.z;
    _1252.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_1265.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _733, instData);
        piecePos = param;
    }
    if ((uint(_1265.bitOptions) & 256u) != 0u)
    {
        bool _1414 = _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w < 0.949999988079071044921875;
        bool _1423;
        if (!_1414)
        {
            _1423 = _1265.baseVertexDisplacement > 0.00999999977648258209228515625;
        }
        else
        {
            _1423 = _1414;
        }
        if (_1423)
        {
            float3 seedVec = piecePos.xyz * 0.100000001490116119384765625;
            float param_1 = float(_733.uni[instData.y].composite >> uint(16));
            seedVec.y += (1024.0 * hash11(param_1));
            float damageAmount = (1.0 - _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w) * 0.800000011920928955078125;
            float3 param_2 = seedVec;
            float4 _1460 = piecePos;
            float4 _1464 = piecePos;
            float3 _1466 = _1464.xyz + (fast::normalize(_1460.xyz) * ((fast::max(damageAmount, _1265.baseVertexDisplacement) * 6.0) * Perlin3D(param_2)));
            piecePos.x = _1466.x;
            piecePos.y = _1466.y;
            piecePos.z = _1466.z;
        }
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1488 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1496;
    if (!_1488)
    {
        _1496 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1496 = _1488;
    }
    if (_1496)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_3 = piecePos.xyz;
    float3 param_6 = safeT;
    float3 param_8 = safeB;
    float4 param_4;
    float3 param_5;
    float3 param_7;
    float3 param_9;
    GetModelSpaceVertex(param_3, param_4, param_5, param_6, param_7, param_8, param_9, _133, instData, bonesInfo, _908, normal);
    modelPos = param_4;
    modelVertexNormal = param_5;
    float3 modelSpaceTangent = param_7;
    float3 modelSpaceBitangent = param_9;
    uint _1522 = instData.x + 0u;
    uint _1525 = instData.x + 1u;
    Transform param_10;
    param_10.quat = _908.transforms[_1522].quat;
    param_10.trSc = _908.transforms[_1522].trSc;
    Transform param_11;
    param_11.quat = _908.transforms[_1525].quat;
    param_11.trSc = _908.transforms[_1525].trSc;
    float param_12 = _133.timeInfo.w;
    Transform _1543 = Lerp(param_10, param_11, param_12);
    Transform modelToWorldTX = _1543;
    Transform param_13 = modelToWorldTX;
    float4 param_14 = modelPos;
    float4 worldPos = ApplyTransform(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelSpaceTangent;
    _1252.worldTangent_VS = RotateByQuaternion(param_15, param_16);
    float4 param_17 = modelToWorldTX.quat;
    float3 param_18 = modelVertexNormal;
    _1252.worldNormal_VS = RotateByQuaternion(param_17, param_18);
    _1252.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_733.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _1252.teamCol = ((teamIndex == 255u) ? float4(0.0) : _133.teamColor[teamIndex]);
    _1252.teamCol.w = ((device float*)&_733.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_733.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_133.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _1252.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    bool _1593 = (uint(_1265.bitOptions) & 32u) != 0u;
    bool _1603;
    if (!_1593)
    {
        _1603 = (uint(_1265.bitOptions) & 64u) != 0u;
    }
    else
    {
        _1603 = _1593;
    }
    bool _1613;
    if (!_1603)
    {
        _1613 = (uint(_1265.bitOptions) & 8192u) != 0u;
    }
    else
    {
        _1613 = _1603;
    }
    if (_1613)
    {
        float unitSpeed = ((device float*)&_733.uni[instData.y].speed)[3u];
        if (unitSpeed > 0.5)
        {
            unitSpeed = fast::max(3.0, unitSpeed);
        }
        float loopingFrameCount = mod(simFrame, 8.0);
        float baseOffset = loopingFrameCount * unitSpeed;
        if ((uint(_1265.bitOptions) & 32u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.628173828125, _1252.uvCoords.y <= 0.6220703125, _1252.uvCoords.x <= 0.749755859375, _1252.uvCoords.y >= 0.60205078125)))
            {
                _1252.uvCoords.x += (mod(baseOffset * 4.0, 12.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_1252.uvCoords.x >= 0.6279296875, _1252.uvCoords.y <= 0.601806640625, _1252.uvCoords.x <= 0.75, _1252.uvCoords.y >= 0.569580078125)))
                {
                    _1252.uvCoords.x += (mod(baseOffset * 4.0, 20.0) / 4096.0);
                }
            }
        }
        if ((uint(_1265.bitOptions) & 64u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.74267578125, _1252.uvCoords.y <= 0.062744140625, _1252.uvCoords.x <= 1.0, _1252.uvCoords.y >= 0.0)))
            {
                _1252.uvCoords.x += (mod(baseOffset * (-6.0), 56.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_1252.uvCoords.x >= 0.046875, _1252.uvCoords.y <= 0.8447265625, _1252.uvCoords.x <= 0.171630859375, _1252.uvCoords.y >= 0.828125)))
                {
                    _1252.uvCoords.x += (mod(baseOffset * 3.0, 24.0) / 4096.0);
                }
                else
                {
                    if (all(bool4(_1252.uvCoords.x >= 0.046142578125, _1252.uvCoords.y <= 0.827880859375, _1252.uvCoords.x <= 0.169677734375, _1252.uvCoords.y >= 0.804931640625)))
                    {
                        _1252.uvCoords.x += (mod(baseOffset * 3.0, 28.0) / 4096.0);
                    }
                }
            }
        }
        if ((uint(_1265.bitOptions) & 8192u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.637939453125, _1252.uvCoords.y <= 0.8125, _1252.uvCoords.x <= 0.799072265625, _1252.uvCoords.y >= 0.7138671875)))
            {
                _1252.uvCoords.x += (mod(baseOffset * (-6.0), 55.0) / 4096.0);
            }
        }
    }
    _1252.worldVertexPos = worldPos;
    _1252.shadowVertexPos = _104.shadowView * worldPos;
    float4 _1846 = _1252.shadowVertexPos;
    float2 _1848 = _1846.xy + float2(0.5);
    _1252.shadowVertexPos.x = _1848.x;
    _1252.shadowVertexPos.y = _1848.y;
    _1252.shadowVertexPos.w = ((device float*)&_733.uni[instData.y].userDefined[0])[0u];
    if ((uint(_1265.bitOptions) & 8u) != 0u)
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_1265.bitOptions) & 16u) != 0u)
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_733.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_1265.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _104.cameraView * _1252.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_133.fogParams.y - fogCoord) * _133.fogParams.w;
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_1252.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_1265.drawPass) & 4u) == 4u)
    {
        gl_Position = _104.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _1265.clipPlane0);
    }
    else
    {
        gl_Position = _104.cameraViewProj * worldPos;
    }
    _1252.worldVertexPos.w = ((device float*)&_733.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _104 [[buffer(2)]], constant UniformParamsBuffer& _133 [[buffer(3)]], const device TransformBuffer& _908 [[buffer(4)]], const device UniformsBuffer& _733 [[buffer(5)]], constant NativeCUSDrawUniforms& _1265 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _1252 = {};
    float3 cameraPos = _104.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_104.cameraView[0].z, _104.cameraView[1].z, _104.cameraView[2].z) * (-1.0);
    float simFrame = _133.timeInfo.x + _133.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _104, simFrame, _133, _733, in.instData, in.bonesInfo, _908, in.normal, _1252, _1265, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_1265.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_1265.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_1265.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_1252_pieceVertexPosOrig = _1252.pieceVertexPosOrig;
    out.m_1252_worldVertexPos = _1252.worldVertexPos;
    out.m_1252_worldTangent_VS = _1252.worldTangent_VS;
    out.m_1252_worldNormal_VS = _1252.worldNormal_VS;
    out.m_1252_uvCoords = _1252.uvCoords;
    out.m_1252_teamCol = _1252.teamCol;
    out.m_1252_shadowVertexPos = _1252.shadowVertexPos;
    out.m_1252_aoterm_fogFactor_selfIllumMod_healthFraction = _1252.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_4_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,true},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_4_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _194 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _476 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_455_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_teamCol [[user(locn5), flat]];
    float4 m_455_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _200)
{
    return (_200.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _200)
{
    if ((_200.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _200)
{
    uint code = _200.NativeCUSSampleCodes[slot].x;
    if ((_200.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _266;
        if (mode == 0u)
        {
            _266 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _278;
            if (mode == 1u)
            {
                _278 = float4(value.x);
            }
            else
            {
                _278 = float4(0.0, 0.0, 0.0, value.x);
            }
            _266 = _278;
        }
        value = _266;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _333 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _333);
    float4 _338 = NativeCUSChannels(param_3, param_4, _200);
    return _338;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture3d<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, level(lod));
    float4 _426 = NativeCUSChannels(param_1, param_2, _200);
    return _426;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _352 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _352, bias(bias0));
    float4 _358 = NativeCUSChannels(param_3, param_4, _200);
    return _358;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_200.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _382 = NativeCUSUV(param, param_1, _200);
    uv.x = _382.x;
    uv.y = _382.y;
    if ((_200.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _200, thread Data& _455, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _455.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _476[presetIndex].samplingRandomness;
    float samplingDistance = _476[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _521 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _200);
        shadow = _521;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _404 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _404, level(lod));
    float4 _410 = NativeCUSChannels(param_3, param_4, _200);
    return _410;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    int2 _433;
    if (NativeCUSEmpty(param, _200))
    {
        _433 = int2(0);
    }
    else
    {
        _433 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _433;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _374 = NativeCUSChannels(param_1, param_2, _200);
    return _374;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _200)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _200);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _200).xyz;
    return SampleReflectionMapMod(param_3, _200);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _200).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _200, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _200.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _200)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _200);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1168 [[buffer(2)]], constant UniformParamsBuffer& _175 [[buffer(3)]], constant NativeCUSDrawUniforms& _200 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> texture1w [[texture(3)]], texture2d<float> texture2w [[texture(4)]], texture2d<float> normalTexw [[texture(5)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> brdfLUT [[texture(9)]], texture3d<float> noisetex3dcube [[texture(10)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler texture1wSmplr [[sampler(3)]], sampler texture2wSmplr [[sampler(4)]], sampler normalTexwSmplr [[sampler(5)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler brdfLUTSmplr [[sampler(9)]], sampler noisetex3dcubeSmplr [[sampler(10)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _455 = {};
    _455.pieceVertexPosOrig = in.m_455_pieceVertexPosOrig;
    _455.worldVertexPos = in.m_455_worldVertexPos;
    _455.worldTangent_VS = in.m_455_worldTangent_VS;
    _455.worldNormal_VS = in.m_455_worldNormal_VS;
    _455.uvCoords = in.m_455_uvCoords;
    _455.teamCol = in.m_455_teamCol;
    _455.shadowVertexPos = in.m_455_shadowVertexPos;
    _455.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_455_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _175.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _455.uvCoords.xy;
    float unitID = floor(_455.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_455.worldNormal_VS);
    float3 worldTangent = fast::normalize(_455.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _833 = (uint(_200.bitOptions) & 2u) != 0u;
    bool _843;
    if (!_833)
    {
        _843 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _843 = _833;
    }
    float4 normalTexVal;
    if (_843)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _200);
    }
    float healthMix = 0.0;
    float4 myPerlin = float4(0.0);
    bool _860 = (uint(_200.bitOptions) & 128u) != 0u;
    bool _869;
    if (!_860)
    {
        _869 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _869 = _860;
    }
    if (_869)
    {
        float3 seedVec = _455.pieceVertexPosOrig.xyz * 0.60000002384185791015625;
        float param_2 = unitID;
        seedVec.y += ((1024.0 * hash11(param_2)) + dot(seedVec.xz, float2(0.100000001490116119384765625)));
        int param_3 = 10;
        float3 param_4 = fract(seedVec * 0.100000001490116119384765625);
        float param_5 = 0.0;
        myPerlin = (NativeCUSLod(noisetex3dcube, noisetex3dcubeSmplr, param_3, param_4, param_5, _200) * 2.0) - float4(1.0);
        healthMix = ((myPerlin.x * 0.5) + 0.5) * (2.0 - _200.baseVertexDisplacement);
        healthMix = smoothstep(0.0, healthMix, fast::max((1.0 - _455.aoterm_fogFactor_selfIllumMod_healthFraction.w) + _200.baseVertexDisplacement, 0.0)) * 0.800000011920928955078125;
    }
    float3 tbnNormal;
    if ((uint(_200.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        bool _942 = (uint(_200.bitOptions) & 128u) != 0u;
        bool _951;
        if (!_942)
        {
            _951 = (uint(_200.bitOptions) & 512u) != 0u;
        }
        else
        {
            _951 = _942;
        }
        if (_951)
        {
            if (healthMix > 0.0500000007450580596923828125)
            {
                int param_6 = 5;
                float2 param_7 = myUV;
                float param_8 = textureLODBias;
                float3 tbnNormalw = (NativeCUSTexture(normalTexw, normalTexwSmplr, param_6, param_7, param_8, _200).xyz * 2.0) - float3(1.0);
                float3 wrecknormal = tbnNormalw;
                tbnNormal = mix(tbnNormal, tbnNormalw, float3(healthMix));
            }
        }
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_9 = 0;
    float2 param_10 = myUV;
    float param_11 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_9, param_10, param_11, _200);
    int param_12 = 1;
    float2 param_13 = myUV;
    float param_14 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_12, param_13, param_14, _200);
    if ((uint(_200.bitOptions) & 128u) != 0u)
    {
        if (healthMix > 0.0500000007450580596923828125)
        {
            int param_15 = 3;
            float2 param_16 = myUV;
            float4 texColor1w = NativeCUSTexture(texture1w, texture1wSmplr, param_15, param_16, _200);
            int param_17 = 4;
            float2 param_18 = myUV;
            float4 texColor2w = NativeCUSTexture(texture2w, texture2wSmplr, param_17, param_18, _200);
            healthMix *= (1.0 - (0.89999997615814208984375 * texColor2.x));
            texColor1 = mix(texColor1, texColor1w, float4(healthMix));
            float4 _1034 = texColor2;
            float3 _1040 = mix(_1034.xyz, texColor2w.xyz, float3(healthMix));
            texColor2.x = _1040.x;
            texColor2.y = _1040.y;
            texColor2.z = _1040.z;
            texColor2.z += (0.5 * healthMix);
        }
    }
    float4 teamColor = _455.teamCol;
    float3 param_19 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_19, _200);
    if ((uint(_200.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_455.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_200.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_20 = roughness;
    float3 param_21 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_20, param_21), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_175.sunDir.xyz);
    float3 V = fast::normalize(_1168.cameraViewInv[3].xyz - _455.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_200.bitOptions) & 1u) != 0u)
    {
        float param_22 = NdotL;
        gShadow = GetShadowPCFRandom(param_22, shadowsQuality, _200, _455, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _175.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_23 = 9;
    float2 param_24 = float2(NdotV, roughness);
    float param_25 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_23, param_24, param_25, _200).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_26 = F0;
        float3 param_27 = F90;
        float param_28 = VdotH;
        float3 F = FresnelSchlick(param_26, param_27, param_28);
        float param_29 = NdotL;
        float param_30 = NdotV;
        float param_31 = roughness2;
        float Vis = VisibilityOcclusionFast(param_29, param_30, param_31);
        float param_32 = NdotH;
        float param_33 = roughness4;
        float D = MicrofacetDistribution(param_32, param_33);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_175.sunSpecularModel.xyz, _175.sunDiffuseModel.xyz, float3(step(dot(_175.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_175.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_34 = F0;
    float3 param_35 = F90;
    float param_36 = VdotH;
    float param_37 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_34, param_35, param_36, param_37);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_175.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_38 = Rv;
    float param_39 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_38, param_39, _200, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_40 = NdotV;
    float param_41 = _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_42 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_40, param_41, param_42);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _200);
    if ((uint(_200.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_175.fogColor.xyz, outColor, float3(_455.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _200.brightnessFactor;
    float buildProgress = _455.shadowVertexPos.w;
    if (buildProgress > (-0.5))
    {
        myPerlin = myPerlin;
        float height = fast::clamp(_455.pieceVertexPosOrig.w / 1.0, 0.0, 1.0);
        float sintime = 0.5 + (0.5 * sin(simFrame * 0.100000001490116119384765625));
        float sintimefast = 0.5 + (0.5 * sin(simFrame * 0.1500000059604644775390625));
        float sinprogress = 0.5 + (0.5 * sin((buildProgress * 0.00999999977648258209228515625) * 3.141499996185302734375));
        float4 progressLevels = float4(buildProgress);
        progressLevels = powr(progressLevels, float4(3.0, 1.5, 0.699999988079071044921875, 0.3499999940395355224609375));
        progressLevels = mix(progressLevels, float4(myPerlin.y + (myPerlin.z * sin(simFrame * 0.042341999709606170654296875))), float4(0.0500000007450580596923828125 * smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress)));
        float4 levelLines = fast::clamp(float4(1.0) - (abs(progressLevels - float4(height)) * 100.0), float4(0.0), float4(1.0));
        float levelFactor = dot(levelLines, float4(1.0));
        float3 fragSize = fwidth(_455.worldVertexPos.xyz);
        float fragSizeFactor = 1.0 / dot(float3(1.0), fragSize);
        float3 buildGrid = (abs(fract((_455.worldVertexPos.xyz / float3(8.0)) - float3(0.5)) - float3(0.5)) * 8.0) / fragSize;
        float line = 1.0 - fast::min(fast::min(buildGrid.x, buildGrid.y), buildGrid.z);
        line = fast::clamp(line * smoothstep(-0.5, 1.0, fragSizeFactor), 0.0, 1.0);
        float gridSize = fast::clamp(((1.0 - buildProgress) * 10.0) + 2.0, 2.0, 12.0);
        float3 grid = step(float3(0.5), fast::clamp(float3(1.0) - (fract(_455.pieceVertexPosOrig.xyz / float3(gridSize)) * 10.0), float3(0.0), float3(1.0)));
        float3 pulseTeamColor = mix(_455.teamCol.xyz, _455.teamCol.xyz * 1.7799999713897705078125, float3(sintimefast));
        if (height > progressLevels.x)
        {
            float unemissive = 1.0 - smoothstep(progressLevels.x, progressLevels.y, height);
            outSpecularColor *= unemissive;
            outColor -= ((albedoColor * emissiveness) * (1.0 - unemissive));
            emissiveness *= unemissive;
        }
        if (height > progressLevels.y)
        {
            outColor = mix(outColor, pulseTeamColor, float3(smoothstep(progressLevels.y, progressLevels.z, height)));
        }
        if (height > progressLevels.z)
        {
            outColor = pulseTeamColor * fast::clamp(NdotL, 0.300000011920928955078125, 1.0);
        }
        if (height > progressLevels.w)
        {
            float heightProgress = smoothstep(progressLevels.w, 1.0, height);
            float flatlight = 0.300000011920928955078125 + (0.699999988079071044921875 * smoothstep(0.0, 1.0, fast::clamp((dot(worldNormal, L) * 0.5) + 0.5, heightProgress * 0.20000000298023223876953125, 1.0)));
            outColor = pulseTeamColor * flatlight;
            N = worldNormal;
            texColor2.w *= fast::max(0.4000000059604644775390625, fast::max(fast::clamp(line, buildProgress, 1.0), 1.0 - heightProgress));
        }
        float last5percent = smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress);
        outColor = mix(outColor, pulseTeamColor, float3(line * last5percent));
        outColor += float3(levelFactor);
        outSpecularColor += float3(levelFactor);
        outSpecularColor += (((pulseTeamColor * line) * sintimefast) * 2.0);
    }
    float selectedness = _455.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_455.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1815 = fast::clamp(_455.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1815.x;
        selectionHighlight.y = _1815.y;
        selectionHighlight.z = _1815.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _455.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float cloakTime = _455.worldVertexPos.w;
    if (abs(cloakTime) > 0.5)
    {
        float cloakedness = 0.0;
        if (cloakTime > 0.0)
        {
            cloakedness = fast::clamp((_175.timeInfo.x - cloakTime) / 15.0, 0.0, 1.0);
        }
        if (cloakTime < 0.0)
        {
            cloakedness = 1.0 - fast::clamp((_175.timeInfo.x + cloakTime) / 15.0, 0.0, 1.0);
        }
        float sintime_1 = fract(simFrame * 0.0199999995529651641845703125);
        myPerlin.y = (myPerlin.y * 0.5) + 0.5;
        texColor2.w = 1.0 - fast::clamp(cloakedness * 0.4900000095367431640625, 0.0, 0.4900000095367431640625);
        float perlinline1 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract(simFrame * 0.004999999888241291046142578125))), 0.0, 1.0);
        float perlinline2 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract((simFrame * 0.004999999888241291046142578125) + 0.5))), 0.0, 1.0);
        float cloaknoise = (cloakedness * perlinline1) + (cloakedness * perlinline2);
        outColor += float3(cloaknoise * 0.85000002384185791015625);
        float dotcamera_1 = dot(worldNormal, V);
        float highLightOpacity_1 = fast::clamp(1.0 - dotcamera_1, 0.0, 1.0);
        highLightOpacity_1 *= highLightOpacity_1;
        outColor = mix(outColor, _455.teamCol.xyz * 3.0, float3(highLightOpacity_1 * cloakedness));
        outSpecularColor += float3(fast::clamp(cloaknoise * 0.75, 0.0, 1.0));
    }
    fragData[0] = float4(outColor, texColor2.w);
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_4_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"baseVertexDisplacement",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture1w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTexw",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"noisetex3dcube",35679,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_5_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,false},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,false},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,false},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_5_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_965_pieceVertexPosOrig [[user(locn0)]];
    float4 m_965_worldVertexPos [[user(locn1)]];
    float3 m_965_worldTangent_VS [[user(locn2)]];
    float3 m_965_worldNormal_VS [[user(locn3)]];
    float4 m_965_uvCoords [[user(locn4)]];
    float4 m_965_teamCol [[user(locn5)]];
    float4 m_965_shadowVertexPos [[user(locn6)]];
    float4 m_965_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _446, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_446.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _562 = mVP;
    float3 _565 = _562.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _565.x;
    mVP.y = _565.y;
    mVP.z = _565.z;
    float4 _578 = mVP;
    float2 _580 = _578.xz + (float2(diff) + (wind * diff2));
    mVP.x = _580.x;
    mVP.z = _580.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _355 = Lerp(param, param_1, param_2);
    return Transform{ _355, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
float4 InvertNormalizedQuaternion(thread const float4& q)
{
    return float4(-q.x, -q.y, -q.z, q.w);
}

static inline __attribute__((always_inline))
Transform InvertTransformAffine(thread const Transform& tra)
{
    float4 param = tra.quat;
    float4 invR = InvertNormalizedQuaternion(param);
    float invS = 1.0 / tra.trSc.w;
    float4 param_1 = invR;
    float3 param_2 = (-tra.trSc.xyz) * invS;
    return Transform{ invR, float4(RotateByQuaternion(param_1, param_2), invS) };
}

static inline __attribute__((always_inline))
float4 MultiplyQuat(thread const float4& a, thread const float4& b)
{
    return float4(((b.xyz * a.w) + (a.xyz * b.w)) + cross(a.xyz, b.xyz), (a.w * b.w) - dot(a.xyz, b.xyz));
}

static inline __attribute__((always_inline))
Transform ApplyTransform(thread const Transform& parentTra, thread const Transform& childTra)
{
    float4 param = parentTra.quat;
    float4 param_1 = childTra.quat;
    float4 param_2 = parentTra.quat;
    float3 param_3 = childTra.trSc.xyz * parentTra.trSc.w;
    return Transform{ MultiplyQuat(param, param_1), float4(parentTra.trSc.xyz + RotateByQuaternion(param_2, param_3), parentTra.trSc.w * childTra.trSc.w) };
}

static inline __attribute__((always_inline))
float3 ApplyTransform(thread const Transform& tra, thread const float3& v)
{
    float4 param = tra.quat;
    float3 param_1 = v * tra.trSc.w;
    return RotateByQuaternion(param, param_1) + tra.trSc.xyz;
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _125, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _621, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _625 = instData.x + bID0;
        tx.quat = _621.transforms[_625].quat;
        tx.trSc = _621.transforms[_625].trSc;
    }
    else
    {
        uint _640 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _647 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _621.transforms[_640].quat;
        param_4.trSc = _621.transforms[_640].trSc;
        Transform param_5;
        param_5.quat = _621.transforms[_647].quat;
        param_5.trSc = _621.transforms[_647].trSc;
        float param_6 = _125.timeInfo.w;
        Transform _665 = Lerp(param_4, param_5, param_6);
        tx = _665;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
    bool _706;
    if (!staticModel)
    {
        _706 = weights.x == 1.0;
    }
    else
    {
        _706 = staticModel;
    }
    if (_706)
    {
        return;
    }
    uint param_15 = bonesInfo.y;
    uint param_16 = 1u;
    uint param_17 = bonesInfo.y;
    uint param_18 = 2u;
    uint param_19 = bonesInfo.y;
    uint param_20 = 3u;
    float3 _731 = float3(float(GetUnpackedValue(param_15, param_16)) / 255.0, float(GetUnpackedValue(param_17, param_18)) / 255.0, float(GetUnpackedValue(param_19, param_20)) / 255.0);
    weights.y = _731.x;
    weights.z = _731.y;
    weights.w = _731.z;
    float wSum = 0.0;
    msPosition *= weights.x;
    msNormal *= weights.x;
    msTangent *= weights.x;
    msBitangent *= weights.x;
    wSum += weights.x;
    uint _763 = instData.w + bID0;
    Transform bposeTra;
    bposeTra.quat = _621.transforms[_763].quat;
    bposeTra.trSc = _621.transforms[_763].trSc;
    Transform param_25;
    Transform param_26;
    Transform param_27;
    for (uint bi = 1u; bi < 3u; bi++)
    {
        uint param_21 = bonesInfo.x;
        uint param_22 = bi;
        uint param_23 = bonesInfo.z;
        uint param_24 = bi;
        uint bID = GetUnpackedValue(param_21, param_22) + (GetUnpackedValue(param_23, param_24) << 8u);
        bool _795 = bID == 65535u;
        bool _803;
        if (!_795)
        {
            _803 = weights[bi] == 0.0;
        }
        else
        {
            _803 = _795;
        }
        if (_803)
        {
            continue;
        }
        uint _811 = instData.w + bID;
        param_25.quat = _621.transforms[_811].quat;
        param_25.trSc = _621.transforms[_811].trSc;
        Transform bposeInvTra = InvertTransformAffine(param_25);
        uint _827 = (instData.x + (2u * (1u + bID))) + 0u;
        uint _834 = (instData.x + (2u * (1u + bID))) + 1u;
        param_26.quat = _621.transforms[_827].quat;
        param_26.trSc = _621.transforms[_827].trSc;
        param_27.quat = _621.transforms[_834].quat;
        param_27.trSc = _621.transforms[_834].trSc;
        float param_28 = _125.timeInfo.w;
        Transform _852 = Lerp(param_26, param_27, param_28);
        Transform boneTx = _852;
        weights[bi] *= float(boneTx.trSc.w > 0.0);
        Transform param_29 = bposeInvTra;
        Transform param_30 = bposeTra;
        Transform param_31 = boneTx;
        Transform param_32 = ApplyTransform(param_29, param_30);
        Transform param_33 = ApplyTransform(param_31, param_32);
        float4 param_34 = piecePos;
        float4 txPiecePos = ApplyTransform(param_33, param_34);
        tx.trSc = float4(0.0, 0.0, 0.0, 1.0);
        Transform param_35 = bposeInvTra;
        Transform param_36 = bposeTra;
        Transform param_37 = boneTx;
        Transform param_38 = ApplyTransform(param_35, param_36);
        Transform param_39 = ApplyTransform(param_37, param_38);
        float3 param_40 = normal;
        float3 txPieceNormal = ApplyTransform(param_39, param_40);
        Transform param_41 = bposeInvTra;
        Transform param_42 = bposeTra;
        Transform param_43 = boneTx;
        Transform param_44 = ApplyTransform(param_41, param_42);
        Transform param_45 = ApplyTransform(param_43, param_44);
        float3 param_46 = safeTangent;
        float3 txPieceTangent = ApplyTransform(param_45, param_46);
        Transform param_47 = bposeInvTra;
        Transform param_48 = bposeTra;
        Transform param_49 = boneTx;
        Transform param_50 = ApplyTransform(param_47, param_48);
        Transform param_51 = ApplyTransform(param_49, param_50);
        float3 param_52 = safeBitangent;
        float3 txPieceBitangent = ApplyTransform(param_51, param_52);
        msPosition += (txPiecePos * weights[bi]);
        msNormal += (txPieceNormal * weights[bi]);
        msTangent += (txPieceTangent * weights[bi]);
        msBitangent += (txPieceBitangent * weights[bi]);
        wSum += weights[bi];
    }
    msPosition /= float4(wSum);
    msNormal /= float3(wSum);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(constant UniformMatrixBuffer& _96, thread float& simFrame, constant UniformParamsBuffer& _125, const device UniformsBuffer& _446, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _621, thread float3& normal, thread Data& _965, thread float3& pos, thread float4& uv, constant NativeCUSDrawUniforms& _1020, thread float3& T, thread float3& B, thread float4& gl_Position, thread float (&gl_ClipDistance)[1])
{
    _965.uvCoords.z = float(_446.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    float4 piecePos = float4(pos, 1.0);
    _965.uvCoords.x = uv.xy.x;
    _965.uvCoords.y = uv.xy.y;
    _965.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_446.uni[instData.y].health / _446.uni[instData.y].maxHealth, 0.0, 1.0);
    float4 _1004 = piecePos;
    float3 _1006 = _1004.xyz * featureFadeFactor;
    piecePos.x = _1006.x;
    piecePos.y = _1006.y;
    piecePos.z = _1006.z;
    _965.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_1020.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _446, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1050 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1058;
    if (!_1050)
    {
        _1058 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1058 = _1050;
    }
    if (_1058)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _125, instData, bonesInfo, _621, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _1084 = instData.x + 0u;
    uint _1087 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _621.transforms[_1084].quat;
    param_8.trSc = _621.transforms[_1084].trSc;
    Transform param_9;
    param_9.quat = _621.transforms[_1087].quat;
    param_9.trSc = _621.transforms[_1087].trSc;
    float param_10 = _125.timeInfo.w;
    Transform _1105 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _1105;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _965.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _965.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _965.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_446.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _965.teamCol = ((teamIndex == 255u) ? float4(0.0) : _125.teamColor[teamIndex]);
    _965.teamCol.w = ((device float*)&_446.uni[instData.y].userDefined[1])[2u];
    float4 lightVertexPos = _96.shadowView * worldPos;
    float3 lightVertexNormal = normal;
    float NdotL = fast::clamp(dot(lightVertexNormal, float3(0.0, 0.0, 1.0)), 0.0, 1.0);
    float bias0 = 4.9999998736893758177757263183594e-05 * tan(acos(NdotL));
    bias0 = fast::clamp(bias0, 0.0, 0.00025000001187436282634735107421875);
    float4 _1173 = lightVertexPos;
    float2 _1175 = _1173.xy + float2(0.5);
    lightVertexPos.x = _1175.x;
    lightVertexPos.y = _1175.y;
    lightVertexPos.z += bias0;
    gl_Position = _96.shadowProj * lightVertexPos;
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _96 [[buffer(2)]], constant UniformParamsBuffer& _125 [[buffer(3)]], const device TransformBuffer& _621 [[buffer(4)]], const device UniformsBuffer& _446 [[buffer(5)]], constant NativeCUSDrawUniforms& _1020 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _965 = {};
    float3 cameraPos = _96.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_96.cameraView[0].z, _96.cameraView[1].z, _96.cameraView[2].z) * (-1.0);
    float simFrame = _125.timeInfo.x + _125.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(_96, simFrame, _125, _446, in.instData, in.bonesInfo, _621, in.normal, _965, in.pos, in.uv, _1020, in.T, in.B, out.gl_Position, out.gl_ClipDistance);
    if ((_1020.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_1020.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_1020.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_965_pieceVertexPosOrig = _965.pieceVertexPosOrig;
    out.m_965_worldVertexPos = _965.worldVertexPos;
    out.m_965_worldTangent_VS = _965.worldTangent_VS;
    out.m_965_worldNormal_VS = _965.worldNormal_VS;
    out.m_965_uvCoords = _965.uvCoords;
    out.m_965_teamCol = _965.teamCol;
    out.m_965_shadowVertexPos = _965.shadowVertexPos;
    out.m_965_aoterm_fogFactor_selfIllumMod_healthFraction = _965.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_5_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,false},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,false},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,false},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,false},
};
inline constexpr char CUS_5_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

constant spvUnsafeArray<float, 8> _40 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });

fragment void NativeCUSFragment(constant UniformParamsBuffer& _21 [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    spvUnsafeArray<ShadowQuality, 4> _91 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });
    
    int shadowsQuality = 0;
    float simFrame = _21.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_5_defaults[]={{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_6_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,true},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_6_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_1252_pieceVertexPosOrig [[user(locn0)]];
    float4 m_1252_worldVertexPos [[user(locn1)]];
    float3 m_1252_worldTangent_VS [[user(locn2)]];
    float3 m_1252_worldNormal_VS [[user(locn3)]];
    float4 m_1252_uvCoords [[user(locn4)]];
    float4 m_1252_teamCol [[user(locn5)]];
    float4 m_1252_shadowVertexPos [[user(locn6)]];
    float4 m_1252_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _733, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_733.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _849 = mVP;
    float3 _852 = _849.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _852.x;
    mVP.y = _852.y;
    mVP.z = _852.z;
    float4 _865 = mVP;
    float2 _867 = _865.xz + (float2(diff) + (wind * diff2));
    mVP.x = _867.x;
    mVP.z = _867.y;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float Perlin3D(thread const float3& P)
{
    float3 Pi = floor(P);
    float3 Pf = P - Pi;
    float3 Pf_min1 = Pf - float3(1.0);
    Pi -= (floor(Pi * 0.0144927538931369781494140625) * 69.0);
    float3 Pi_inc1 = step(Pi, float3(67.5)) * (Pi + float3(1.0));
    float4 Pt = float4(Pi.xy, Pi_inc1.xy) + float4(50.0, 161.0, 50.0, 161.0);
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    float3 lowz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float3 highz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi_inc1.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float4 hashx0 = fract(Pt * lowz_mod.xxxx);
    float4 hashx1 = fract(Pt * highz_mod.xxxx);
    float4 hashy0 = fract(Pt * lowz_mod.yyyy);
    float4 hashy1 = fract(Pt * highz_mod.yyyy);
    float4 hashz0 = fract(Pt * lowz_mod.zzzz);
    float4 hashz1 = fract(Pt * highz_mod.zzzz);
    float4 grad_x0 = hashx0 - float4(0.499989986419677734375);
    float4 grad_y0 = hashy0 - float4(0.499989986419677734375);
    float4 grad_z0 = hashz0 - float4(0.499989986419677734375);
    float4 grad_x1 = hashx1 - float4(0.499989986419677734375);
    float4 grad_y1 = hashy1 - float4(0.499989986419677734375);
    float4 grad_z1 = hashz1 - float4(0.499989986419677734375);
    float4 grad_results_0 = rsqrt(((grad_x0 * grad_x0) + (grad_y0 * grad_y0)) + (grad_z0 * grad_z0)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x0) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y0)) + (Pf.zzzz * grad_z0));
    float4 grad_results_1 = rsqrt(((grad_x1 * grad_x1) + (grad_y1 * grad_y1)) + (grad_z1 * grad_z1)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x1) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y1)) + (Pf_min1.zzzz * grad_z1));
    float3 blend = ((Pf * Pf) * Pf) * ((Pf * ((Pf * 6.0) - float3(15.0))) + float3(10.0));
    float4 res0 = mix(grad_results_0, grad_results_1, float4(blend.z));
    float4 blend2 = float4(blend.xy, float2(1.0) - blend.xy);
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    return final * 1.1547005176544189453125;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _363 = Lerp(param, param_1, param_2);
    return Transform{ _363, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
float4 InvertNormalizedQuaternion(thread const float4& q)
{
    return float4(-q.x, -q.y, -q.z, q.w);
}

static inline __attribute__((always_inline))
Transform InvertTransformAffine(thread const Transform& tra)
{
    float4 param = tra.quat;
    float4 invR = InvertNormalizedQuaternion(param);
    float invS = 1.0 / tra.trSc.w;
    float4 param_1 = invR;
    float3 param_2 = (-tra.trSc.xyz) * invS;
    return Transform{ invR, float4(RotateByQuaternion(param_1, param_2), invS) };
}

static inline __attribute__((always_inline))
float4 MultiplyQuat(thread const float4& a, thread const float4& b)
{
    return float4(((b.xyz * a.w) + (a.xyz * b.w)) + cross(a.xyz, b.xyz), (a.w * b.w) - dot(a.xyz, b.xyz));
}

static inline __attribute__((always_inline))
Transform ApplyTransform(thread const Transform& parentTra, thread const Transform& childTra)
{
    float4 param = parentTra.quat;
    float4 param_1 = childTra.quat;
    float4 param_2 = parentTra.quat;
    float3 param_3 = childTra.trSc.xyz * parentTra.trSc.w;
    return Transform{ MultiplyQuat(param, param_1), float4(parentTra.trSc.xyz + RotateByQuaternion(param_2, param_3), parentTra.trSc.w * childTra.trSc.w) };
}

static inline __attribute__((always_inline))
float3 ApplyTransform(thread const Transform& tra, thread const float3& v)
{
    float4 param = tra.quat;
    float3 param_1 = v * tra.trSc.w;
    return RotateByQuaternion(param, param_1) + tra.trSc.xyz;
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _133, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _908, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _912 = instData.x + bID0;
        tx.quat = _908.transforms[_912].quat;
        tx.trSc = _908.transforms[_912].trSc;
    }
    else
    {
        uint _927 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _934 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _908.transforms[_927].quat;
        param_4.trSc = _908.transforms[_927].trSc;
        Transform param_5;
        param_5.quat = _908.transforms[_934].quat;
        param_5.trSc = _908.transforms[_934].trSc;
        float param_6 = _133.timeInfo.w;
        Transform _952 = Lerp(param_4, param_5, param_6);
        tx = _952;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
    bool _993;
    if (!staticModel)
    {
        _993 = weights.x == 1.0;
    }
    else
    {
        _993 = staticModel;
    }
    if (_993)
    {
        return;
    }
    uint param_15 = bonesInfo.y;
    uint param_16 = 1u;
    uint param_17 = bonesInfo.y;
    uint param_18 = 2u;
    uint param_19 = bonesInfo.y;
    uint param_20 = 3u;
    float3 _1018 = float3(float(GetUnpackedValue(param_15, param_16)) / 255.0, float(GetUnpackedValue(param_17, param_18)) / 255.0, float(GetUnpackedValue(param_19, param_20)) / 255.0);
    weights.y = _1018.x;
    weights.z = _1018.y;
    weights.w = _1018.z;
    float wSum = 0.0;
    msPosition *= weights.x;
    msNormal *= weights.x;
    msTangent *= weights.x;
    msBitangent *= weights.x;
    wSum += weights.x;
    uint _1050 = instData.w + bID0;
    Transform bposeTra;
    bposeTra.quat = _908.transforms[_1050].quat;
    bposeTra.trSc = _908.transforms[_1050].trSc;
    Transform param_25;
    Transform param_26;
    Transform param_27;
    for (uint bi = 1u; bi < 3u; bi++)
    {
        uint param_21 = bonesInfo.x;
        uint param_22 = bi;
        uint param_23 = bonesInfo.z;
        uint param_24 = bi;
        uint bID = GetUnpackedValue(param_21, param_22) + (GetUnpackedValue(param_23, param_24) << 8u);
        bool _1082 = bID == 65535u;
        bool _1090;
        if (!_1082)
        {
            _1090 = weights[bi] == 0.0;
        }
        else
        {
            _1090 = _1082;
        }
        if (_1090)
        {
            continue;
        }
        uint _1098 = instData.w + bID;
        param_25.quat = _908.transforms[_1098].quat;
        param_25.trSc = _908.transforms[_1098].trSc;
        Transform bposeInvTra = InvertTransformAffine(param_25);
        uint _1114 = (instData.x + (2u * (1u + bID))) + 0u;
        uint _1121 = (instData.x + (2u * (1u + bID))) + 1u;
        param_26.quat = _908.transforms[_1114].quat;
        param_26.trSc = _908.transforms[_1114].trSc;
        param_27.quat = _908.transforms[_1121].quat;
        param_27.trSc = _908.transforms[_1121].trSc;
        float param_28 = _133.timeInfo.w;
        Transform _1139 = Lerp(param_26, param_27, param_28);
        Transform boneTx = _1139;
        weights[bi] *= float(boneTx.trSc.w > 0.0);
        Transform param_29 = bposeInvTra;
        Transform param_30 = bposeTra;
        Transform param_31 = boneTx;
        Transform param_32 = ApplyTransform(param_29, param_30);
        Transform param_33 = ApplyTransform(param_31, param_32);
        float4 param_34 = piecePos;
        float4 txPiecePos = ApplyTransform(param_33, param_34);
        tx.trSc = float4(0.0, 0.0, 0.0, 1.0);
        Transform param_35 = bposeInvTra;
        Transform param_36 = bposeTra;
        Transform param_37 = boneTx;
        Transform param_38 = ApplyTransform(param_35, param_36);
        Transform param_39 = ApplyTransform(param_37, param_38);
        float3 param_40 = normal;
        float3 txPieceNormal = ApplyTransform(param_39, param_40);
        Transform param_41 = bposeInvTra;
        Transform param_42 = bposeTra;
        Transform param_43 = boneTx;
        Transform param_44 = ApplyTransform(param_41, param_42);
        Transform param_45 = ApplyTransform(param_43, param_44);
        float3 param_46 = safeTangent;
        float3 txPieceTangent = ApplyTransform(param_45, param_46);
        Transform param_47 = bposeInvTra;
        Transform param_48 = bposeTra;
        Transform param_49 = boneTx;
        Transform param_50 = ApplyTransform(param_47, param_48);
        Transform param_51 = ApplyTransform(param_49, param_50);
        float3 param_52 = safeBitangent;
        float3 txPieceBitangent = ApplyTransform(param_51, param_52);
        msPosition += (txPiecePos * weights[bi]);
        msNormal += (txPieceNormal * weights[bi]);
        msTangent += (txPieceTangent * weights[bi]);
        msBitangent += (txPieceBitangent * weights[bi]);
        wSum += weights[bi];
    }
    msPosition /= float4(wSum);
    msNormal /= float3(wSum);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _104, thread float& simFrame, constant UniformParamsBuffer& _133, const device UniformsBuffer& _733, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _908, thread float3& normal, thread Data& _1252, constant NativeCUSDrawUniforms& _1265, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _1252.uvCoords.z = float(_733.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_1265.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _733.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _1265.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _1265.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _1265.featureFadeDistSq) / (_1265.featureDrawDistSq - _1265.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _1252.uvCoords.x = uv.xy.x;
    _1252.uvCoords.y = uv.xy.y;
    _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_733.uni[instData.y].health / _733.uni[instData.y].maxHealth, 0.0, 1.0);
    float buildProgress = ((device float*)&_733.uni[instData.y].userDefined[0])[0u];
    if (buildProgress > (-0.5))
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_1252.aoterm_fogFactor_selfIllumMod_healthFraction.w / fast::max(9.9999999392252902907785028219223e-09, buildProgress), 0.0, 1.0);
        if (buildProgress < 0.999000012874603271484375)
        {
            piecePos.y += (0.5 * (1.0 - smoothstep(0.949999988079071044921875, 1.0, buildProgress)));
        }
    }
    float4 _1378 = piecePos;
    float3 _1380 = _1378.xyz * featureFadeFactor;
    piecePos.x = _1380.x;
    piecePos.y = _1380.y;
    piecePos.z = _1380.z;
    _1252.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_1265.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _733, instData);
        piecePos = param;
    }
    if ((uint(_1265.bitOptions) & 256u) != 0u)
    {
        bool _1414 = _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w < 0.949999988079071044921875;
        bool _1423;
        if (!_1414)
        {
            _1423 = _1265.baseVertexDisplacement > 0.00999999977648258209228515625;
        }
        else
        {
            _1423 = _1414;
        }
        if (_1423)
        {
            float3 seedVec = piecePos.xyz * 0.100000001490116119384765625;
            float param_1 = float(_733.uni[instData.y].composite >> uint(16));
            seedVec.y += (1024.0 * hash11(param_1));
            float damageAmount = (1.0 - _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w) * 0.800000011920928955078125;
            float3 param_2 = seedVec;
            float4 _1460 = piecePos;
            float4 _1464 = piecePos;
            float3 _1466 = _1464.xyz + (fast::normalize(_1460.xyz) * ((fast::max(damageAmount, _1265.baseVertexDisplacement) * 6.0) * Perlin3D(param_2)));
            piecePos.x = _1466.x;
            piecePos.y = _1466.y;
            piecePos.z = _1466.z;
        }
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1488 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1496;
    if (!_1488)
    {
        _1496 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1496 = _1488;
    }
    if (_1496)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_3 = piecePos.xyz;
    float3 param_6 = safeT;
    float3 param_8 = safeB;
    float4 param_4;
    float3 param_5;
    float3 param_7;
    float3 param_9;
    GetModelSpaceVertex(param_3, param_4, param_5, param_6, param_7, param_8, param_9, _133, instData, bonesInfo, _908, normal);
    modelPos = param_4;
    modelVertexNormal = param_5;
    float3 modelSpaceTangent = param_7;
    float3 modelSpaceBitangent = param_9;
    uint _1522 = instData.x + 0u;
    uint _1525 = instData.x + 1u;
    Transform param_10;
    param_10.quat = _908.transforms[_1522].quat;
    param_10.trSc = _908.transforms[_1522].trSc;
    Transform param_11;
    param_11.quat = _908.transforms[_1525].quat;
    param_11.trSc = _908.transforms[_1525].trSc;
    float param_12 = _133.timeInfo.w;
    Transform _1543 = Lerp(param_10, param_11, param_12);
    Transform modelToWorldTX = _1543;
    Transform param_13 = modelToWorldTX;
    float4 param_14 = modelPos;
    float4 worldPos = ApplyTransform(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelSpaceTangent;
    _1252.worldTangent_VS = RotateByQuaternion(param_15, param_16);
    float4 param_17 = modelToWorldTX.quat;
    float3 param_18 = modelVertexNormal;
    _1252.worldNormal_VS = RotateByQuaternion(param_17, param_18);
    _1252.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_733.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _1252.teamCol = ((teamIndex == 255u) ? float4(0.0) : _133.teamColor[teamIndex]);
    _1252.teamCol.w = ((device float*)&_733.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_733.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_133.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _1252.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    bool _1593 = (uint(_1265.bitOptions) & 32u) != 0u;
    bool _1603;
    if (!_1593)
    {
        _1603 = (uint(_1265.bitOptions) & 64u) != 0u;
    }
    else
    {
        _1603 = _1593;
    }
    bool _1613;
    if (!_1603)
    {
        _1613 = (uint(_1265.bitOptions) & 8192u) != 0u;
    }
    else
    {
        _1613 = _1603;
    }
    if (_1613)
    {
        float unitSpeed = ((device float*)&_733.uni[instData.y].speed)[3u];
        if (unitSpeed > 0.5)
        {
            unitSpeed = fast::max(3.0, unitSpeed);
        }
        float loopingFrameCount = mod(simFrame, 8.0);
        float baseOffset = loopingFrameCount * unitSpeed;
        if ((uint(_1265.bitOptions) & 32u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.628173828125, _1252.uvCoords.y <= 0.6220703125, _1252.uvCoords.x <= 0.749755859375, _1252.uvCoords.y >= 0.60205078125)))
            {
                _1252.uvCoords.x += (mod(baseOffset * 4.0, 12.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_1252.uvCoords.x >= 0.6279296875, _1252.uvCoords.y <= 0.601806640625, _1252.uvCoords.x <= 0.75, _1252.uvCoords.y >= 0.569580078125)))
                {
                    _1252.uvCoords.x += (mod(baseOffset * 4.0, 20.0) / 4096.0);
                }
            }
        }
        if ((uint(_1265.bitOptions) & 64u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.74267578125, _1252.uvCoords.y <= 0.062744140625, _1252.uvCoords.x <= 1.0, _1252.uvCoords.y >= 0.0)))
            {
                _1252.uvCoords.x += (mod(baseOffset * (-6.0), 56.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_1252.uvCoords.x >= 0.046875, _1252.uvCoords.y <= 0.8447265625, _1252.uvCoords.x <= 0.171630859375, _1252.uvCoords.y >= 0.828125)))
                {
                    _1252.uvCoords.x += (mod(baseOffset * 3.0, 24.0) / 4096.0);
                }
                else
                {
                    if (all(bool4(_1252.uvCoords.x >= 0.046142578125, _1252.uvCoords.y <= 0.827880859375, _1252.uvCoords.x <= 0.169677734375, _1252.uvCoords.y >= 0.804931640625)))
                    {
                        _1252.uvCoords.x += (mod(baseOffset * 3.0, 28.0) / 4096.0);
                    }
                }
            }
        }
        if ((uint(_1265.bitOptions) & 8192u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.637939453125, _1252.uvCoords.y <= 0.8125, _1252.uvCoords.x <= 0.799072265625, _1252.uvCoords.y >= 0.7138671875)))
            {
                _1252.uvCoords.x += (mod(baseOffset * (-6.0), 55.0) / 4096.0);
            }
        }
    }
    _1252.worldVertexPos = worldPos;
    _1252.shadowVertexPos = _104.shadowView * worldPos;
    float4 _1846 = _1252.shadowVertexPos;
    float2 _1848 = _1846.xy + float2(0.5);
    _1252.shadowVertexPos.x = _1848.x;
    _1252.shadowVertexPos.y = _1848.y;
    _1252.shadowVertexPos.w = ((device float*)&_733.uni[instData.y].userDefined[0])[0u];
    if ((uint(_1265.bitOptions) & 8u) != 0u)
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_1265.bitOptions) & 16u) != 0u)
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_733.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_1265.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _104.cameraView * _1252.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_133.fogParams.y - fogCoord) * _133.fogParams.w;
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_1252.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_1265.drawPass) & 4u) == 4u)
    {
        gl_Position = _104.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _1265.clipPlane0);
    }
    else
    {
        gl_Position = _104.cameraViewProj * worldPos;
    }
    _1252.worldVertexPos.w = ((device float*)&_733.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _104 [[buffer(2)]], constant UniformParamsBuffer& _133 [[buffer(3)]], const device TransformBuffer& _908 [[buffer(4)]], const device UniformsBuffer& _733 [[buffer(5)]], constant NativeCUSDrawUniforms& _1265 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _1252 = {};
    float3 cameraPos = _104.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_104.cameraView[0].z, _104.cameraView[1].z, _104.cameraView[2].z) * (-1.0);
    float simFrame = _133.timeInfo.x + _133.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _104, simFrame, _133, _733, in.instData, in.bonesInfo, _908, in.normal, _1252, _1265, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_1265.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_1265.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_1265.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_1252_pieceVertexPosOrig = _1252.pieceVertexPosOrig;
    out.m_1252_worldVertexPos = _1252.worldVertexPos;
    out.m_1252_worldTangent_VS = _1252.worldTangent_VS;
    out.m_1252_worldNormal_VS = _1252.worldNormal_VS;
    out.m_1252_uvCoords = _1252.uvCoords;
    out.m_1252_teamCol = _1252.teamCol;
    out.m_1252_shadowVertexPos = _1252.shadowVertexPos;
    out.m_1252_aoterm_fogFactor_selfIllumMod_healthFraction = _1252.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_6_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,true},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_6_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _166 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _416 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
    float4 fragData_1 [[color(1)]];
    float4 fragData_2 [[color(2)]];
    float4 fragData_3 [[color(3)]];
    float4 fragData_4 [[color(4)]];
};

struct NativeCUSFragment_in
{
    float4 m_395_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_395_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_395_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_teamCol [[user(locn5), flat]];
    float4 m_395_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_395_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _172)
{
    return (_172.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _172)
{
    if ((_172.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _172)
{
    uint code = _172.NativeCUSSampleCodes[slot].x;
    if ((_172.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _238;
        if (mode == 0u)
        {
            _238 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _250;
            if (mode == 1u)
            {
                _250 = float4(value.x);
            }
            else
            {
                _250 = float4(0.0, 0.0, 0.0, value.x);
            }
            _238 = _250;
        }
        value = _238;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _305 = NativeCUSUV(param_1, param_2, _172);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _305);
    float4 _310 = NativeCUSChannels(param_3, param_4, _172);
    return _310;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture3d<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& lod, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, level(lod));
    float4 _382 = NativeCUSChannels(param_1, param_2, _172);
    return _382;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _324 = NativeCUSUV(param_1, param_2, _172);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _324, bias(bias0));
    float4 _330 = NativeCUSChannels(param_3, param_4, _172);
    return _330;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _172)
{
    if (_172.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_172.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _338 = NativeCUSUV(param, param_1, _172);
    uv.x = _338.x;
    uv.y = _338.y;
    if ((_172.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _172, thread Data& _395, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _395.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _416[presetIndex].samplingRandomness;
    float samplingDistance = _416[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _461 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _172);
        shadow = _461;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _172)
{
    int param = slot;
    if (NativeCUSEmpty(param, _172))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _360 = NativeCUSUV(param_1, param_2, _172);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _360, level(lod));
    float4 _366 = NativeCUSChannels(param_3, param_4, _172);
    return _366;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _172)
{
    if (_172.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _172.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _172)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _172);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1002 [[buffer(2)]], constant UniformParamsBuffer& _147 [[buffer(3)]], constant NativeCUSDrawUniforms& _172 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> normalTexw [[texture(5)]], texture2d<float> shadowTex [[texture(6)]], texture2d<float> brdfLUT [[texture(9)]], texture3d<float> noisetex3dcube [[texture(10)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler normalTexwSmplr [[sampler(5)]], sampler shadowTexSmplr [[sampler(6)]], sampler brdfLUTSmplr [[sampler(9)]], sampler noisetex3dcubeSmplr [[sampler(10)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 5> fragData = {};
    Data _395 = {};
    _395.pieceVertexPosOrig = in.m_395_pieceVertexPosOrig;
    _395.worldVertexPos = in.m_395_worldVertexPos;
    _395.worldTangent_VS = in.m_395_worldTangent_VS;
    _395.worldNormal_VS = in.m_395_worldNormal_VS;
    _395.uvCoords = in.m_395_uvCoords;
    _395.teamCol = in.m_395_teamCol;
    _395.shadowVertexPos = in.m_395_shadowVertexPos;
    _395.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_395_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _147.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _395.uvCoords.xy;
    float unitID = floor(_395.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_395.worldNormal_VS);
    float3 worldTangent = fast::normalize(_395.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _720 = (uint(_172.bitOptions) & 2u) != 0u;
    bool _730;
    if (!_720)
    {
        _730 = (uint(_172.bitOptions) & 512u) != 0u;
    }
    else
    {
        _730 = _720;
    }
    float4 normalTexVal;
    if (_730)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _172);
    }
    float healthMix = 0.0;
    float4 myPerlin = float4(0.0);
    bool _747 = (uint(_172.bitOptions) & 128u) != 0u;
    bool _756;
    if (!_747)
    {
        _756 = (uint(_172.bitOptions) & 512u) != 0u;
    }
    else
    {
        _756 = _747;
    }
    if (_756)
    {
        float3 seedVec = _395.pieceVertexPosOrig.xyz * 0.60000002384185791015625;
        float param_2 = unitID;
        seedVec.y += ((1024.0 * hash11(param_2)) + dot(seedVec.xz, float2(0.100000001490116119384765625)));
        int param_3 = 10;
        float3 param_4 = fract(seedVec * 0.100000001490116119384765625);
        float param_5 = 0.0;
        myPerlin = (NativeCUSLod(noisetex3dcube, noisetex3dcubeSmplr, param_3, param_4, param_5, _172) * 2.0) - float4(1.0);
        healthMix = ((myPerlin.x * 0.5) + 0.5) * (2.0 - _172.baseVertexDisplacement);
        healthMix = smoothstep(0.0, healthMix, fast::max((1.0 - _395.aoterm_fogFactor_selfIllumMod_healthFraction.w) + _172.baseVertexDisplacement, 0.0)) * 0.800000011920928955078125;
    }
    float3 tbnNormal;
    if ((uint(_172.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        bool _830 = (uint(_172.bitOptions) & 128u) != 0u;
        bool _839;
        if (!_830)
        {
            _839 = (uint(_172.bitOptions) & 512u) != 0u;
        }
        else
        {
            _839 = _830;
        }
        if (_839)
        {
            if (healthMix > 0.0500000007450580596923828125)
            {
                int param_6 = 5;
                float2 param_7 = myUV;
                float param_8 = textureLODBias;
                float3 tbnNormalw = (NativeCUSTexture(normalTexw, normalTexwSmplr, param_6, param_7, param_8, _172).xyz * 2.0) - float3(1.0);
                float3 wrecknormal = tbnNormalw;
                tbnNormal = mix(tbnNormal, tbnNormalw, float3(healthMix));
            }
        }
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_9 = 0;
    float2 param_10 = myUV;
    float param_11 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_9, param_10, param_11, _172);
    int param_12 = 1;
    float2 param_13 = myUV;
    float param_14 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_12, param_13, param_14, _172);
    float4 teamColor = _395.teamCol;
    float3 param_15 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_15, _172);
    if ((uint(_172.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_395.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_172.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_16 = roughness;
    float3 param_17 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_16, param_17), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_147.sunDir.xyz);
    float3 V = fast::normalize(_1002.cameraViewInv[3].xyz - _395.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_172.bitOptions) & 1u) != 0u)
    {
        float param_18 = NdotL;
        gShadow = GetShadowPCFRandom(param_18, shadowsQuality, _172, _395, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _147.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_19 = 9;
    float2 param_20 = float2(NdotV, roughness);
    float param_21 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_19, param_20, param_21, _172).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_22 = F0;
        float3 param_23 = F90;
        float param_24 = VdotH;
        float3 F = FresnelSchlick(param_22, param_23, param_24);
        float param_25 = NdotL;
        float param_26 = NdotV;
        float param_27 = roughness2;
        float Vis = VisibilityOcclusionFast(param_25, param_26, param_27);
        float param_28 = NdotH;
        float param_29 = roughness4;
        float D = MicrofacetDistribution(param_28, param_29);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_147.sunSpecularModel.xyz, _147.sunDiffuseModel.xyz, float3(step(dot(_147.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_147.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_30 = F0;
    float3 param_31 = F90;
    float param_32 = VdotH;
    float param_33 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_30, param_31, param_32, param_33);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_147.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _395.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 reflectionColor = float3(0.0);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_34 = NdotV;
    float param_35 = _395.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_36 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_34, param_35, param_36);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _172);
    if ((uint(_172.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_147.fogColor.xyz, outColor, float3(_395.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _172.brightnessFactor;
    float buildProgress = _395.shadowVertexPos.w;
    if (buildProgress > (-0.5))
    {
        myPerlin = myPerlin;
        float height = fast::clamp(_395.pieceVertexPosOrig.w / 1.0, 0.0, 1.0);
        float sintime = 0.5 + (0.5 * sin(simFrame * 0.100000001490116119384765625));
        float sintimefast = 0.5 + (0.5 * sin(simFrame * 0.1500000059604644775390625));
        float sinprogress = 0.5 + (0.5 * sin((buildProgress * 0.00999999977648258209228515625) * 3.141499996185302734375));
        float4 progressLevels = float4(buildProgress);
        progressLevels = powr(progressLevels, float4(3.0, 1.5, 0.699999988079071044921875, 0.3499999940395355224609375));
        progressLevels = mix(progressLevels, float4(myPerlin.y + (myPerlin.z * sin(simFrame * 0.042341999709606170654296875))), float4(0.0500000007450580596923828125 * smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress)));
        float4 levelLines = fast::clamp(float4(1.0) - (abs(progressLevels - float4(height)) * 100.0), float4(0.0), float4(1.0));
        float levelFactor = dot(levelLines, float4(1.0));
        float3 fragSize = fwidth(_395.worldVertexPos.xyz);
        float fragSizeFactor = 1.0 / dot(float3(1.0), fragSize);
        float3 buildGrid = (abs(fract((_395.worldVertexPos.xyz / float3(8.0)) - float3(0.5)) - float3(0.5)) * 8.0) / fragSize;
        float line = 1.0 - fast::min(fast::min(buildGrid.x, buildGrid.y), buildGrid.z);
        line = fast::clamp(line * smoothstep(-0.5, 1.0, fragSizeFactor), 0.0, 1.0);
        float gridSize = fast::clamp(((1.0 - buildProgress) * 10.0) + 2.0, 2.0, 12.0);
        float3 grid = step(float3(0.5), fast::clamp(float3(1.0) - (fract(_395.pieceVertexPosOrig.xyz / float3(gridSize)) * 10.0), float3(0.0), float3(1.0)));
        float3 pulseTeamColor = mix(_395.teamCol.xyz, _395.teamCol.xyz * 1.7799999713897705078125, float3(sintimefast));
        if (height > progressLevels.x)
        {
            float unemissive = 1.0 - smoothstep(progressLevels.x, progressLevels.y, height);
            outSpecularColor *= unemissive;
            outColor -= ((albedoColor * emissiveness) * (1.0 - unemissive));
            emissiveness *= unemissive;
        }
        if (height > progressLevels.y)
        {
            outColor = mix(outColor, pulseTeamColor, float3(smoothstep(progressLevels.y, progressLevels.z, height)));
        }
        if (height > progressLevels.z)
        {
            outColor = pulseTeamColor * fast::clamp(NdotL, 0.300000011920928955078125, 1.0);
        }
        if (height > progressLevels.w)
        {
            float heightProgress = smoothstep(progressLevels.w, 1.0, height);
            float flatlight = 0.300000011920928955078125 + (0.699999988079071044921875 * smoothstep(0.0, 1.0, fast::clamp((dot(worldNormal, L) * 0.5) + 0.5, heightProgress * 0.20000000298023223876953125, 1.0)));
            outColor = pulseTeamColor * flatlight;
            N = worldNormal;
        }
        float last5percent = smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress);
        outColor = mix(outColor, pulseTeamColor, float3(line * last5percent));
        outColor += float3(levelFactor);
        outSpecularColor += float3(levelFactor);
        outSpecularColor += (((pulseTeamColor * line) * sintimefast) * 2.0);
    }
    float selectedness = _395.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_395.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1637 = fast::clamp(_395.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1637.x;
        selectionHighlight.y = _1637.y;
        selectionHighlight.z = _1637.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _395.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float cloakTime = _395.worldVertexPos.w;
    if (abs(cloakTime) > 0.5)
    {
        float cloakedness = 0.0;
        if (cloakTime > 0.0)
        {
            cloakedness = fast::clamp((_147.timeInfo.x - cloakTime) / 15.0, 0.0, 1.0);
        }
        if (cloakTime < 0.0)
        {
            cloakedness = 1.0 - fast::clamp((_147.timeInfo.x + cloakTime) / 15.0, 0.0, 1.0);
        }
        float sintime_1 = fract(simFrame * 0.0199999995529651641845703125);
        myPerlin.y = (myPerlin.y * 0.5) + 0.5;
        texColor2.w = 1.0 - fast::clamp(cloakedness * 0.4900000095367431640625, 0.0, 0.4900000095367431640625);
        float perlinline1 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract(simFrame * 0.004999999888241291046142578125))), 0.0, 1.0);
        float perlinline2 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract((simFrame * 0.004999999888241291046142578125) + 0.5))), 0.0, 1.0);
        float cloaknoise = (cloakedness * perlinline1) + (cloakedness * perlinline2);
        outColor += float3(cloaknoise * 0.85000002384185791015625);
        float dotcamera_1 = dot(worldNormal, V);
        float highLightOpacity_1 = fast::clamp(1.0 - dotcamera_1, 0.0, 1.0);
        highLightOpacity_1 *= highLightOpacity_1;
        outColor = mix(outColor, _395.teamCol.xyz * 3.0, float3(highLightOpacity_1 * cloakedness));
        outSpecularColor += float3(fast::clamp(cloaknoise * 0.75, 0.0, 1.0));
    }
    float alphaBin = (texColor2.w < 0.5) ? 0.0 : 1.0;
    outSpecularColor = CustomTM(outSpecularColor, pbrParams, _172);
    fragData[0] = float4((N * 0.5) + float3(0.5), alphaBin);
    fragData[1] = float4(outColor, alphaBin);
    fragData[2] = float4(outSpecularColor, alphaBin);
    fragData[3] = float4(((albedoColor * emissiveness) * 2.0) + (outSpecularColor * 0.300000011920928955078125), alphaBin);
    // ENABLE_OPTION_HEALTH_TEXTURING: original unit outline identity channels.
    uint outlineID = uint(unitID) + 1u;
    fragData[4] = float4(float(outlineID & 255u) / 255.0, 1.0, float(outlineID >> 8u) / 255.0, alphaBin);
    out.fragData_0 = fragData[0];
    out.fragData_1 = fragData[1];
    out.fragData_2 = fragData[2];
    out.fragData_3 = fragData[3];
    out.fragData_4 = fragData[4];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_6_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"baseVertexDisplacement",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTexw",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"noisetex3dcube",35679,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_7_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,true},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_7_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of the GLSL mod() function, which is slightly different than Metal fmod()
template<typename Tx, typename Ty>
inline Tx mod(Tx x, Ty y)
{
    return x - y * floor(x / y);
}

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_1252_pieceVertexPosOrig [[user(locn0)]];
    float4 m_1252_worldVertexPos [[user(locn1)]];
    float3 m_1252_worldTangent_VS [[user(locn2)]];
    float3 m_1252_worldNormal_VS [[user(locn3)]];
    float4 m_1252_uvCoords [[user(locn4)]];
    float4 m_1252_teamCol [[user(locn5)]];
    float4 m_1252_shadowVertexPos [[user(locn6)]];
    float4 m_1252_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _733, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_733.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _849 = mVP;
    float3 _852 = _849.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _852.x;
    mVP.y = _852.y;
    mVP.z = _852.z;
    float4 _865 = mVP;
    float2 _867 = _865.xz + (float2(diff) + (wind * diff2));
    mVP.x = _867.x;
    mVP.z = _867.y;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float Perlin3D(thread const float3& P)
{
    float3 Pi = floor(P);
    float3 Pf = P - Pi;
    float3 Pf_min1 = Pf - float3(1.0);
    Pi -= (floor(Pi * 0.0144927538931369781494140625) * 69.0);
    float3 Pi_inc1 = step(Pi, float3(67.5)) * (Pi + float3(1.0));
    float4 Pt = float4(Pi.xy, Pi_inc1.xy) + float4(50.0, 161.0, 50.0, 161.0);
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    float3 lowz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float3 highz_mod = float3(1.0) / (float3(635.2987060546875, 682.35748291015625, 668.926513671875) + (Pi_inc1.zzz * float3(48.50038909912109375, 65.29412078857421875, 63.934600830078125)));
    float4 hashx0 = fract(Pt * lowz_mod.xxxx);
    float4 hashx1 = fract(Pt * highz_mod.xxxx);
    float4 hashy0 = fract(Pt * lowz_mod.yyyy);
    float4 hashy1 = fract(Pt * highz_mod.yyyy);
    float4 hashz0 = fract(Pt * lowz_mod.zzzz);
    float4 hashz1 = fract(Pt * highz_mod.zzzz);
    float4 grad_x0 = hashx0 - float4(0.499989986419677734375);
    float4 grad_y0 = hashy0 - float4(0.499989986419677734375);
    float4 grad_z0 = hashz0 - float4(0.499989986419677734375);
    float4 grad_x1 = hashx1 - float4(0.499989986419677734375);
    float4 grad_y1 = hashy1 - float4(0.499989986419677734375);
    float4 grad_z1 = hashz1 - float4(0.499989986419677734375);
    float4 grad_results_0 = rsqrt(((grad_x0 * grad_x0) + (grad_y0 * grad_y0)) + (grad_z0 * grad_z0)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x0) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y0)) + (Pf.zzzz * grad_z0));
    float4 grad_results_1 = rsqrt(((grad_x1 * grad_x1) + (grad_y1 * grad_y1)) + (grad_z1 * grad_z1)) * (((float2(Pf.x, Pf_min1.x).xyxy * grad_x1) + (float2(Pf.y, Pf_min1.y).xxyy * grad_y1)) + (Pf_min1.zzzz * grad_z1));
    float3 blend = ((Pf * Pf) * Pf) * ((Pf * ((Pf * 6.0) - float3(15.0))) + float3(10.0));
    float4 res0 = mix(grad_results_0, grad_results_1, float4(blend.z));
    float4 blend2 = float4(blend.xy, float2(1.0) - blend.xy);
    float final = dot(res0, blend2.zxzx * blend2.wwyy);
    return final * 1.1547005176544189453125;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _363 = Lerp(param, param_1, param_2);
    return Transform{ _363, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
float4 InvertNormalizedQuaternion(thread const float4& q)
{
    return float4(-q.x, -q.y, -q.z, q.w);
}

static inline __attribute__((always_inline))
Transform InvertTransformAffine(thread const Transform& tra)
{
    float4 param = tra.quat;
    float4 invR = InvertNormalizedQuaternion(param);
    float invS = 1.0 / tra.trSc.w;
    float4 param_1 = invR;
    float3 param_2 = (-tra.trSc.xyz) * invS;
    return Transform{ invR, float4(RotateByQuaternion(param_1, param_2), invS) };
}

static inline __attribute__((always_inline))
float4 MultiplyQuat(thread const float4& a, thread const float4& b)
{
    return float4(((b.xyz * a.w) + (a.xyz * b.w)) + cross(a.xyz, b.xyz), (a.w * b.w) - dot(a.xyz, b.xyz));
}

static inline __attribute__((always_inline))
Transform ApplyTransform(thread const Transform& parentTra, thread const Transform& childTra)
{
    float4 param = parentTra.quat;
    float4 param_1 = childTra.quat;
    float4 param_2 = parentTra.quat;
    float3 param_3 = childTra.trSc.xyz * parentTra.trSc.w;
    return Transform{ MultiplyQuat(param, param_1), float4(parentTra.trSc.xyz + RotateByQuaternion(param_2, param_3), parentTra.trSc.w * childTra.trSc.w) };
}

static inline __attribute__((always_inline))
float3 ApplyTransform(thread const Transform& tra, thread const float3& v)
{
    float4 param = tra.quat;
    float3 param_1 = v * tra.trSc.w;
    return RotateByQuaternion(param, param_1) + tra.trSc.xyz;
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _133, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _908, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _912 = instData.x + bID0;
        tx.quat = _908.transforms[_912].quat;
        tx.trSc = _908.transforms[_912].trSc;
    }
    else
    {
        uint _927 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _934 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _908.transforms[_927].quat;
        param_4.trSc = _908.transforms[_927].trSc;
        Transform param_5;
        param_5.quat = _908.transforms[_934].quat;
        param_5.trSc = _908.transforms[_934].trSc;
        float param_6 = _133.timeInfo.w;
        Transform _952 = Lerp(param_4, param_5, param_6);
        tx = _952;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
    bool _993;
    if (!staticModel)
    {
        _993 = weights.x == 1.0;
    }
    else
    {
        _993 = staticModel;
    }
    if (_993)
    {
        return;
    }
    uint param_15 = bonesInfo.y;
    uint param_16 = 1u;
    uint param_17 = bonesInfo.y;
    uint param_18 = 2u;
    uint param_19 = bonesInfo.y;
    uint param_20 = 3u;
    float3 _1018 = float3(float(GetUnpackedValue(param_15, param_16)) / 255.0, float(GetUnpackedValue(param_17, param_18)) / 255.0, float(GetUnpackedValue(param_19, param_20)) / 255.0);
    weights.y = _1018.x;
    weights.z = _1018.y;
    weights.w = _1018.z;
    float wSum = 0.0;
    msPosition *= weights.x;
    msNormal *= weights.x;
    msTangent *= weights.x;
    msBitangent *= weights.x;
    wSum += weights.x;
    uint _1050 = instData.w + bID0;
    Transform bposeTra;
    bposeTra.quat = _908.transforms[_1050].quat;
    bposeTra.trSc = _908.transforms[_1050].trSc;
    Transform param_25;
    Transform param_26;
    Transform param_27;
    for (uint bi = 1u; bi < 3u; bi++)
    {
        uint param_21 = bonesInfo.x;
        uint param_22 = bi;
        uint param_23 = bonesInfo.z;
        uint param_24 = bi;
        uint bID = GetUnpackedValue(param_21, param_22) + (GetUnpackedValue(param_23, param_24) << 8u);
        bool _1082 = bID == 65535u;
        bool _1090;
        if (!_1082)
        {
            _1090 = weights[bi] == 0.0;
        }
        else
        {
            _1090 = _1082;
        }
        if (_1090)
        {
            continue;
        }
        uint _1098 = instData.w + bID;
        param_25.quat = _908.transforms[_1098].quat;
        param_25.trSc = _908.transforms[_1098].trSc;
        Transform bposeInvTra = InvertTransformAffine(param_25);
        uint _1114 = (instData.x + (2u * (1u + bID))) + 0u;
        uint _1121 = (instData.x + (2u * (1u + bID))) + 1u;
        param_26.quat = _908.transforms[_1114].quat;
        param_26.trSc = _908.transforms[_1114].trSc;
        param_27.quat = _908.transforms[_1121].quat;
        param_27.trSc = _908.transforms[_1121].trSc;
        float param_28 = _133.timeInfo.w;
        Transform _1139 = Lerp(param_26, param_27, param_28);
        Transform boneTx = _1139;
        weights[bi] *= float(boneTx.trSc.w > 0.0);
        Transform param_29 = bposeInvTra;
        Transform param_30 = bposeTra;
        Transform param_31 = boneTx;
        Transform param_32 = ApplyTransform(param_29, param_30);
        Transform param_33 = ApplyTransform(param_31, param_32);
        float4 param_34 = piecePos;
        float4 txPiecePos = ApplyTransform(param_33, param_34);
        tx.trSc = float4(0.0, 0.0, 0.0, 1.0);
        Transform param_35 = bposeInvTra;
        Transform param_36 = bposeTra;
        Transform param_37 = boneTx;
        Transform param_38 = ApplyTransform(param_35, param_36);
        Transform param_39 = ApplyTransform(param_37, param_38);
        float3 param_40 = normal;
        float3 txPieceNormal = ApplyTransform(param_39, param_40);
        Transform param_41 = bposeInvTra;
        Transform param_42 = bposeTra;
        Transform param_43 = boneTx;
        Transform param_44 = ApplyTransform(param_41, param_42);
        Transform param_45 = ApplyTransform(param_43, param_44);
        float3 param_46 = safeTangent;
        float3 txPieceTangent = ApplyTransform(param_45, param_46);
        Transform param_47 = bposeInvTra;
        Transform param_48 = bposeTra;
        Transform param_49 = boneTx;
        Transform param_50 = ApplyTransform(param_47, param_48);
        Transform param_51 = ApplyTransform(param_49, param_50);
        float3 param_52 = safeBitangent;
        float3 txPieceBitangent = ApplyTransform(param_51, param_52);
        msPosition += (txPiecePos * weights[bi]);
        msNormal += (txPieceNormal * weights[bi]);
        msTangent += (txPieceTangent * weights[bi]);
        msBitangent += (txPieceBitangent * weights[bi]);
        wSum += weights[bi];
    }
    msPosition /= float4(wSum);
    msNormal /= float3(wSum);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _104, thread float& simFrame, constant UniformParamsBuffer& _133, const device UniformsBuffer& _733, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _908, thread float3& normal, thread Data& _1252, constant NativeCUSDrawUniforms& _1265, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _1252.uvCoords.z = float(_733.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_1265.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _733.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _1265.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _1265.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _1265.featureFadeDistSq) / (_1265.featureDrawDistSq - _1265.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _1252.uvCoords.x = uv.xy.x;
    _1252.uvCoords.y = uv.xy.y;
    _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_733.uni[instData.y].health / _733.uni[instData.y].maxHealth, 0.0, 1.0);
    float buildProgress = ((device float*)&_733.uni[instData.y].userDefined[0])[0u];
    if (buildProgress > (-0.5))
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_1252.aoterm_fogFactor_selfIllumMod_healthFraction.w / fast::max(9.9999999392252902907785028219223e-09, buildProgress), 0.0, 1.0);
        if (buildProgress < 0.999000012874603271484375)
        {
            piecePos.y += (0.5 * (1.0 - smoothstep(0.949999988079071044921875, 1.0, buildProgress)));
        }
    }
    float4 _1378 = piecePos;
    float3 _1380 = _1378.xyz * featureFadeFactor;
    piecePos.x = _1380.x;
    piecePos.y = _1380.y;
    piecePos.z = _1380.z;
    _1252.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_1265.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _733, instData);
        piecePos = param;
    }
    if ((uint(_1265.bitOptions) & 256u) != 0u)
    {
        bool _1414 = _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w < 0.949999988079071044921875;
        bool _1423;
        if (!_1414)
        {
            _1423 = _1265.baseVertexDisplacement > 0.00999999977648258209228515625;
        }
        else
        {
            _1423 = _1414;
        }
        if (_1423)
        {
            float3 seedVec = piecePos.xyz * 0.100000001490116119384765625;
            float param_1 = float(_733.uni[instData.y].composite >> uint(16));
            seedVec.y += (1024.0 * hash11(param_1));
            float damageAmount = (1.0 - _1252.aoterm_fogFactor_selfIllumMod_healthFraction.w) * 0.800000011920928955078125;
            float3 param_2 = seedVec;
            float4 _1460 = piecePos;
            float4 _1464 = piecePos;
            float3 _1466 = _1464.xyz + (fast::normalize(_1460.xyz) * ((fast::max(damageAmount, _1265.baseVertexDisplacement) * 6.0) * Perlin3D(param_2)));
            piecePos.x = _1466.x;
            piecePos.y = _1466.y;
            piecePos.z = _1466.z;
        }
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _1488 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _1496;
    if (!_1488)
    {
        _1496 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _1496 = _1488;
    }
    if (_1496)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_3 = piecePos.xyz;
    float3 param_6 = safeT;
    float3 param_8 = safeB;
    float4 param_4;
    float3 param_5;
    float3 param_7;
    float3 param_9;
    GetModelSpaceVertex(param_3, param_4, param_5, param_6, param_7, param_8, param_9, _133, instData, bonesInfo, _908, normal);
    modelPos = param_4;
    modelVertexNormal = param_5;
    float3 modelSpaceTangent = param_7;
    float3 modelSpaceBitangent = param_9;
    uint _1522 = instData.x + 0u;
    uint _1525 = instData.x + 1u;
    Transform param_10;
    param_10.quat = _908.transforms[_1522].quat;
    param_10.trSc = _908.transforms[_1522].trSc;
    Transform param_11;
    param_11.quat = _908.transforms[_1525].quat;
    param_11.trSc = _908.transforms[_1525].trSc;
    float param_12 = _133.timeInfo.w;
    Transform _1543 = Lerp(param_10, param_11, param_12);
    Transform modelToWorldTX = _1543;
    Transform param_13 = modelToWorldTX;
    float4 param_14 = modelPos;
    float4 worldPos = ApplyTransform(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelSpaceTangent;
    _1252.worldTangent_VS = RotateByQuaternion(param_15, param_16);
    float4 param_17 = modelToWorldTX.quat;
    float3 param_18 = modelVertexNormal;
    _1252.worldNormal_VS = RotateByQuaternion(param_17, param_18);
    _1252.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_733.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _1252.teamCol = ((teamIndex == 255u) ? float4(0.0) : _133.teamColor[teamIndex]);
    _1252.teamCol.w = ((device float*)&_733.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_733.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_133.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _1252.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    bool _1593 = (uint(_1265.bitOptions) & 32u) != 0u;
    bool _1603;
    if (!_1593)
    {
        _1603 = (uint(_1265.bitOptions) & 64u) != 0u;
    }
    else
    {
        _1603 = _1593;
    }
    bool _1613;
    if (!_1603)
    {
        _1613 = (uint(_1265.bitOptions) & 8192u) != 0u;
    }
    else
    {
        _1613 = _1603;
    }
    if (_1613)
    {
        float unitSpeed = ((device float*)&_733.uni[instData.y].speed)[3u];
        if (unitSpeed > 0.5)
        {
            unitSpeed = fast::max(3.0, unitSpeed);
        }
        float loopingFrameCount = mod(simFrame, 8.0);
        float baseOffset = loopingFrameCount * unitSpeed;
        if ((uint(_1265.bitOptions) & 32u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.628173828125, _1252.uvCoords.y <= 0.6220703125, _1252.uvCoords.x <= 0.749755859375, _1252.uvCoords.y >= 0.60205078125)))
            {
                _1252.uvCoords.x += (mod(baseOffset * 4.0, 12.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_1252.uvCoords.x >= 0.6279296875, _1252.uvCoords.y <= 0.601806640625, _1252.uvCoords.x <= 0.75, _1252.uvCoords.y >= 0.569580078125)))
                {
                    _1252.uvCoords.x += (mod(baseOffset * 4.0, 20.0) / 4096.0);
                }
            }
        }
        if ((uint(_1265.bitOptions) & 64u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.74267578125, _1252.uvCoords.y <= 0.062744140625, _1252.uvCoords.x <= 1.0, _1252.uvCoords.y >= 0.0)))
            {
                _1252.uvCoords.x += (mod(baseOffset * (-6.0), 56.0) / 4096.0);
            }
            else
            {
                if (all(bool4(_1252.uvCoords.x >= 0.046875, _1252.uvCoords.y <= 0.8447265625, _1252.uvCoords.x <= 0.171630859375, _1252.uvCoords.y >= 0.828125)))
                {
                    _1252.uvCoords.x += (mod(baseOffset * 3.0, 24.0) / 4096.0);
                }
                else
                {
                    if (all(bool4(_1252.uvCoords.x >= 0.046142578125, _1252.uvCoords.y <= 0.827880859375, _1252.uvCoords.x <= 0.169677734375, _1252.uvCoords.y >= 0.804931640625)))
                    {
                        _1252.uvCoords.x += (mod(baseOffset * 3.0, 28.0) / 4096.0);
                    }
                }
            }
        }
        if ((uint(_1265.bitOptions) & 8192u) != 0u)
        {
            if (all(bool4(_1252.uvCoords.x >= 0.637939453125, _1252.uvCoords.y <= 0.8125, _1252.uvCoords.x <= 0.799072265625, _1252.uvCoords.y >= 0.7138671875)))
            {
                _1252.uvCoords.x += (mod(baseOffset * (-6.0), 55.0) / 4096.0);
            }
        }
    }
    _1252.worldVertexPos = worldPos;
    _1252.shadowVertexPos = _104.shadowView * worldPos;
    float4 _1846 = _1252.shadowVertexPos;
    float2 _1848 = _1846.xy + float2(0.5);
    _1252.shadowVertexPos.x = _1848.x;
    _1252.shadowVertexPos.y = _1848.y;
    _1252.shadowVertexPos.w = ((device float*)&_733.uni[instData.y].userDefined[0])[0u];
    if ((uint(_1265.bitOptions) & 8u) != 0u)
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_1265.bitOptions) & 16u) != 0u)
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_733.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_1265.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _104.cameraView * _1252.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_133.fogParams.y - fogCoord) * _133.fogParams.w;
        _1252.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_1252.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_1265.drawPass) & 4u) == 4u)
    {
        gl_Position = _104.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _1265.clipPlane0);
    }
    else
    {
        gl_Position = _104.cameraViewProj * worldPos;
    }
    _1252.worldVertexPos.w = ((device float*)&_733.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _104 [[buffer(2)]], constant UniformParamsBuffer& _133 [[buffer(3)]], const device TransformBuffer& _908 [[buffer(4)]], const device UniformsBuffer& _733 [[buffer(5)]], constant NativeCUSDrawUniforms& _1265 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _1252 = {};
    float3 cameraPos = _104.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_104.cameraView[0].z, _104.cameraView[1].z, _104.cameraView[2].z) * (-1.0);
    float simFrame = _133.timeInfo.x + _133.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _104, simFrame, _133, _733, in.instData, in.bonesInfo, _908, in.normal, _1252, _1265, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_1265.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_1265.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_1265.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_1252_pieceVertexPosOrig = _1252.pieceVertexPosOrig;
    out.m_1252_worldVertexPos = _1252.worldVertexPos;
    out.m_1252_worldTangent_VS = _1252.worldTangent_VS;
    out.m_1252_worldNormal_VS = _1252.worldNormal_VS;
    out.m_1252_uvCoords = _1252.uvCoords;
    out.m_1252_teamCol = _1252.teamCol;
    out.m_1252_shadowVertexPos = _1252.shadowVertexPos;
    out.m_1252_aoterm_fogFactor_selfIllumMod_healthFraction = _1252.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_7_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,true},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_7_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _194 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _476 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_455_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_455_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_teamCol [[user(locn5), flat]];
    float4 m_455_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_455_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _200)
{
    return (_200.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _200)
{
    if ((_200.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _200)
{
    uint code = _200.NativeCUSSampleCodes[slot].x;
    if ((_200.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _266;
        if (mode == 0u)
        {
            _266 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _278;
            if (mode == 1u)
            {
                _278 = float4(value.x);
            }
            else
            {
                _278 = float4(0.0, 0.0, 0.0, value.x);
            }
            _266 = _278;
        }
        value = _266;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _333 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _333);
    float4 _338 = NativeCUSChannels(param_3, param_4, _200);
    return _338;
}

static inline __attribute__((always_inline))
float hash11(thread const float& p)
{
    float3 p3 = fract(float3(p) * 0.103100001811981201171875);
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.x + p3.y) * p3.z);
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture3d<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, level(lod));
    float4 _426 = NativeCUSChannels(param_1, param_2, _200);
    return _426;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _352 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _352, bias(bias0));
    float4 _358 = NativeCUSChannels(param_3, param_4, _200);
    return _358;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_200.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _382 = NativeCUSUV(param, param_1, _200);
    uv.x = _382.x;
    uv.y = _382.y;
    if ((_200.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _200, thread Data& _455, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _455.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _476[presetIndex].samplingRandomness;
    float samplingDistance = _476[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _521 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _200);
        shadow = _521;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _404 = NativeCUSUV(param_1, param_2, _200);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _404, level(lod));
    float4 _410 = NativeCUSChannels(param_3, param_4, _200);
    return _410;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    int2 _433;
    if (NativeCUSEmpty(param, _200))
    {
        _433 = int2(0);
    }
    else
    {
        _433 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _433;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _200)
{
    int param = slot;
    if (NativeCUSEmpty(param, _200))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _374 = NativeCUSChannels(param_1, param_2, _200);
    return _374;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _200)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _200);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _200).xyz;
    return SampleReflectionMapMod(param_3, _200);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _200, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _200).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _200, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _200)
{
    if (_200.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _200.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _200)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _200);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1168 [[buffer(2)]], constant UniformParamsBuffer& _175 [[buffer(3)]], constant NativeCUSDrawUniforms& _200 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> texture1w [[texture(3)]], texture2d<float> texture2w [[texture(4)]], texture2d<float> normalTexw [[texture(5)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> brdfLUT [[texture(9)]], texture3d<float> noisetex3dcube [[texture(10)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler texture1wSmplr [[sampler(3)]], sampler texture2wSmplr [[sampler(4)]], sampler normalTexwSmplr [[sampler(5)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler brdfLUTSmplr [[sampler(9)]], sampler noisetex3dcubeSmplr [[sampler(10)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _455 = {};
    _455.pieceVertexPosOrig = in.m_455_pieceVertexPosOrig;
    _455.worldVertexPos = in.m_455_worldVertexPos;
    _455.worldTangent_VS = in.m_455_worldTangent_VS;
    _455.worldNormal_VS = in.m_455_worldNormal_VS;
    _455.uvCoords = in.m_455_uvCoords;
    _455.teamCol = in.m_455_teamCol;
    _455.shadowVertexPos = in.m_455_shadowVertexPos;
    _455.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_455_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _175.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _455.uvCoords.xy;
    float unitID = floor(_455.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_455.worldNormal_VS);
    float3 worldTangent = fast::normalize(_455.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _833 = (uint(_200.bitOptions) & 2u) != 0u;
    bool _843;
    if (!_833)
    {
        _843 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _843 = _833;
    }
    float4 normalTexVal;
    if (_843)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _200);
    }
    float healthMix = 0.0;
    float4 myPerlin = float4(0.0);
    bool _860 = (uint(_200.bitOptions) & 128u) != 0u;
    bool _869;
    if (!_860)
    {
        _869 = (uint(_200.bitOptions) & 512u) != 0u;
    }
    else
    {
        _869 = _860;
    }
    if (_869)
    {
        float3 seedVec = _455.pieceVertexPosOrig.xyz * 0.60000002384185791015625;
        float param_2 = unitID;
        seedVec.y += ((1024.0 * hash11(param_2)) + dot(seedVec.xz, float2(0.100000001490116119384765625)));
        int param_3 = 10;
        float3 param_4 = fract(seedVec * 0.100000001490116119384765625);
        float param_5 = 0.0;
        myPerlin = (NativeCUSLod(noisetex3dcube, noisetex3dcubeSmplr, param_3, param_4, param_5, _200) * 2.0) - float4(1.0);
        healthMix = ((myPerlin.x * 0.5) + 0.5) * (2.0 - _200.baseVertexDisplacement);
        healthMix = smoothstep(0.0, healthMix, fast::max((1.0 - _455.aoterm_fogFactor_selfIllumMod_healthFraction.w) + _200.baseVertexDisplacement, 0.0)) * 0.800000011920928955078125;
    }
    float3 tbnNormal;
    if ((uint(_200.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        bool _942 = (uint(_200.bitOptions) & 128u) != 0u;
        bool _951;
        if (!_942)
        {
            _951 = (uint(_200.bitOptions) & 512u) != 0u;
        }
        else
        {
            _951 = _942;
        }
        if (_951)
        {
            if (healthMix > 0.0500000007450580596923828125)
            {
                int param_6 = 5;
                float2 param_7 = myUV;
                float param_8 = textureLODBias;
                float3 tbnNormalw = (NativeCUSTexture(normalTexw, normalTexwSmplr, param_6, param_7, param_8, _200).xyz * 2.0) - float3(1.0);
                float3 wrecknormal = tbnNormalw;
                tbnNormal = mix(tbnNormal, tbnNormalw, float3(healthMix));
            }
        }
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_9 = 0;
    float2 param_10 = myUV;
    float param_11 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_9, param_10, param_11, _200);
    int param_12 = 1;
    float2 param_13 = myUV;
    float param_14 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_12, param_13, param_14, _200);
    if ((uint(_200.bitOptions) & 128u) != 0u)
    {
        if (healthMix > 0.0500000007450580596923828125)
        {
            int param_15 = 3;
            float2 param_16 = myUV;
            float4 texColor1w = NativeCUSTexture(texture1w, texture1wSmplr, param_15, param_16, _200);
            int param_17 = 4;
            float2 param_18 = myUV;
            float4 texColor2w = NativeCUSTexture(texture2w, texture2wSmplr, param_17, param_18, _200);
            healthMix *= (1.0 - (0.89999997615814208984375 * texColor2.x));
            texColor1 = mix(texColor1, texColor1w, float4(healthMix));
            float4 _1034 = texColor2;
            float3 _1040 = mix(_1034.xyz, texColor2w.xyz, float3(healthMix));
            texColor2.x = _1040.x;
            texColor2.y = _1040.y;
            texColor2.z = _1040.z;
            texColor2.z += (0.5 * healthMix);
        }
    }
    float4 teamColor = _455.teamCol;
    float3 param_19 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_19, _200);
    if ((uint(_200.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_455.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_200.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_20 = roughness;
    float3 param_21 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_20, param_21), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_175.sunDir.xyz);
    float3 V = fast::normalize(_1168.cameraViewInv[3].xyz - _455.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_200.bitOptions) & 1u) != 0u)
    {
        float param_22 = NdotL;
        gShadow = GetShadowPCFRandom(param_22, shadowsQuality, _200, _455, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _175.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_23 = 9;
    float2 param_24 = float2(NdotV, roughness);
    float param_25 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_23, param_24, param_25, _200).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_26 = F0;
        float3 param_27 = F90;
        float param_28 = VdotH;
        float3 F = FresnelSchlick(param_26, param_27, param_28);
        float param_29 = NdotL;
        float param_30 = NdotV;
        float param_31 = roughness2;
        float Vis = VisibilityOcclusionFast(param_29, param_30, param_31);
        float param_32 = NdotH;
        float param_33 = roughness4;
        float D = MicrofacetDistribution(param_32, param_33);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_175.sunSpecularModel.xyz, _175.sunDiffuseModel.xyz, float3(step(dot(_175.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_175.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_34 = F0;
    float3 param_35 = F90;
    float param_36 = VdotH;
    float param_37 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_34, param_35, param_36, param_37);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_175.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_38 = Rv;
    float param_39 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_38, param_39, _200, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_40 = NdotV;
    float param_41 = _455.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_42 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_40, param_41, param_42);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _200);
    if ((uint(_200.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_175.fogColor.xyz, outColor, float3(_455.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _200.brightnessFactor;
    float buildProgress = _455.shadowVertexPos.w;
    if (buildProgress > (-0.5))
    {
        myPerlin = myPerlin;
        float height = fast::clamp(_455.pieceVertexPosOrig.w / 1.0, 0.0, 1.0);
        float sintime = 0.5 + (0.5 * sin(simFrame * 0.100000001490116119384765625));
        float sintimefast = 0.5 + (0.5 * sin(simFrame * 0.1500000059604644775390625));
        float sinprogress = 0.5 + (0.5 * sin((buildProgress * 0.00999999977648258209228515625) * 3.141499996185302734375));
        float4 progressLevels = float4(buildProgress);
        progressLevels = powr(progressLevels, float4(3.0, 1.5, 0.699999988079071044921875, 0.3499999940395355224609375));
        progressLevels = mix(progressLevels, float4(myPerlin.y + (myPerlin.z * sin(simFrame * 0.042341999709606170654296875))), float4(0.0500000007450580596923828125 * smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress)));
        float4 levelLines = fast::clamp(float4(1.0) - (abs(progressLevels - float4(height)) * 100.0), float4(0.0), float4(1.0));
        float levelFactor = dot(levelLines, float4(1.0));
        float3 fragSize = fwidth(_455.worldVertexPos.xyz);
        float fragSizeFactor = 1.0 / dot(float3(1.0), fragSize);
        float3 buildGrid = (abs(fract((_455.worldVertexPos.xyz / float3(8.0)) - float3(0.5)) - float3(0.5)) * 8.0) / fragSize;
        float line = 1.0 - fast::min(fast::min(buildGrid.x, buildGrid.y), buildGrid.z);
        line = fast::clamp(line * smoothstep(-0.5, 1.0, fragSizeFactor), 0.0, 1.0);
        float gridSize = fast::clamp(((1.0 - buildProgress) * 10.0) + 2.0, 2.0, 12.0);
        float3 grid = step(float3(0.5), fast::clamp(float3(1.0) - (fract(_455.pieceVertexPosOrig.xyz / float3(gridSize)) * 10.0), float3(0.0), float3(1.0)));
        float3 pulseTeamColor = mix(_455.teamCol.xyz, _455.teamCol.xyz * 1.7799999713897705078125, float3(sintimefast));
        if (height > progressLevels.x)
        {
            float unemissive = 1.0 - smoothstep(progressLevels.x, progressLevels.y, height);
            outSpecularColor *= unemissive;
            outColor -= ((albedoColor * emissiveness) * (1.0 - unemissive));
            emissiveness *= unemissive;
        }
        if (height > progressLevels.y)
        {
            outColor = mix(outColor, pulseTeamColor, float3(smoothstep(progressLevels.y, progressLevels.z, height)));
        }
        if (height > progressLevels.z)
        {
            outColor = pulseTeamColor * fast::clamp(NdotL, 0.300000011920928955078125, 1.0);
        }
        if (height > progressLevels.w)
        {
            float heightProgress = smoothstep(progressLevels.w, 1.0, height);
            float flatlight = 0.300000011920928955078125 + (0.699999988079071044921875 * smoothstep(0.0, 1.0, fast::clamp((dot(worldNormal, L) * 0.5) + 0.5, heightProgress * 0.20000000298023223876953125, 1.0)));
            outColor = pulseTeamColor * flatlight;
            N = worldNormal;
            texColor2.w *= fast::max(0.4000000059604644775390625, fast::max(fast::clamp(line, buildProgress, 1.0), 1.0 - heightProgress));
        }
        float last5percent = smoothstep(0.0, 0.0500000007450580596923828125, 1.0 - buildProgress);
        outColor = mix(outColor, pulseTeamColor, float3(line * last5percent));
        outColor += float3(levelFactor);
        outSpecularColor += float3(levelFactor);
        outSpecularColor += (((pulseTeamColor * line) * sintimefast) * 2.0);
    }
    float selectedness = _455.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_455.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1815 = fast::clamp(_455.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1815.x;
        selectionHighlight.y = _1815.y;
        selectionHighlight.z = _1815.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _455.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float cloakTime = _455.worldVertexPos.w;
    if (abs(cloakTime) > 0.5)
    {
        float cloakedness = 0.0;
        if (cloakTime > 0.0)
        {
            cloakedness = fast::clamp((_175.timeInfo.x - cloakTime) / 15.0, 0.0, 1.0);
        }
        if (cloakTime < 0.0)
        {
            cloakedness = 1.0 - fast::clamp((_175.timeInfo.x + cloakTime) / 15.0, 0.0, 1.0);
        }
        float sintime_1 = fract(simFrame * 0.0199999995529651641845703125);
        myPerlin.y = (myPerlin.y * 0.5) + 0.5;
        texColor2.w = 1.0 - fast::clamp(cloakedness * 0.4900000095367431640625, 0.0, 0.4900000095367431640625);
        float perlinline1 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract(simFrame * 0.004999999888241291046142578125))), 0.0, 1.0);
        float perlinline2 = fast::clamp(1.0 - (20.0 * abs(myPerlin.y - fract((simFrame * 0.004999999888241291046142578125) + 0.5))), 0.0, 1.0);
        float cloaknoise = (cloakedness * perlinline1) + (cloakedness * perlinline2);
        outColor += float3(cloaknoise * 0.85000002384185791015625);
        float dotcamera_1 = dot(worldNormal, V);
        float highLightOpacity_1 = fast::clamp(1.0 - dotcamera_1, 0.0, 1.0);
        highLightOpacity_1 *= highLightOpacity_1;
        outColor = mix(outColor, _455.teamCol.xyz * 3.0, float3(highLightOpacity_1 * cloakedness));
        outSpecularColor += float3(fast::clamp(cloaknoise * 0.75, 0.0, 1.0));
    }
    fragData[0] = float4(outColor, texColor2.w);
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_7_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"baseVertexDisplacement",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture1w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2w",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTexw",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"noisetex3dcube",35679,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_8_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_8_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, constant NativeCUSDrawUniforms& _566, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_566.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _297.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _566.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _566.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _566.featureFadeDistSq) / (_566.featureDrawDistSq - _566.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float4 _646 = piecePos;
    float3 _648 = _646.xyz * featureFadeFactor;
    piecePos.x = _648.x;
    piecePos.y = _648.y;
    piecePos.z = _648.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_566.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _688 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _696;
    if (!_688)
    {
        _696 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _696 = _688;
    }
    if (_696)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _722 = instData.x + 0u;
    uint _725 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_722].quat;
    param_8.trSc = _473.transforms[_722].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_725].quat;
    param_9.trSc = _473.transforms[_725].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _743 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _743;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_297.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_102.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _553.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    _553.worldVertexPos = worldPos;
    _553.shadowVertexPos = _73.shadowView * worldPos;
    float4 _800 = _553.shadowVertexPos;
    float2 _802 = _800.xy + float2(0.5);
    _553.shadowVertexPos.x = _802.x;
    _553.shadowVertexPos.y = _802.y;
    _553.shadowVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[0])[0u];
    if ((uint(_566.bitOptions) & 8u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_566.bitOptions) & 16u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_297.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_566.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _73.cameraView * _553.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_102.fogParams.y - fogCoord) * _102.fogParams.w;
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_553.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_566.drawPass) & 4u) == 4u)
    {
        gl_Position = _73.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _566.clipPlane0);
    }
    else
    {
        gl_Position = _73.cameraViewProj * worldPos;
    }
    _553.worldVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _566 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, _566, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_566.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_566.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_566.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_8_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_8_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _181 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _447 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_426_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_teamCol [[user(locn5), flat]];
    float4 m_426_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _187)
{
    return (_187.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _187)
{
    if ((_187.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _187)
{
    uint code = _187.NativeCUSSampleCodes[slot].x;
    if ((_187.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _253;
        if (mode == 0u)
        {
            _253 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _265;
            if (mode == 1u)
            {
                _265 = float4(value.x);
            }
            else
            {
                _265 = float4(0.0, 0.0, 0.0, value.x);
            }
            _253 = _265;
        }
        value = _253;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _320 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _320);
    float4 _325 = NativeCUSChannels(param_3, param_4, _187);
    return _325;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _339 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _339, bias(bias0));
    float4 _345 = NativeCUSChannels(param_3, param_4, _187);
    return _345;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_187.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _369 = NativeCUSUV(param, param_1, _187);
    uv.x = _369.x;
    uv.y = _369.y;
    if ((_187.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _187, thread Data& _426, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _426.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _447[presetIndex].samplingRandomness;
    float samplingDistance = _447[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _492 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _187);
        shadow = _492;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _391 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _391, level(lod));
    float4 _397 = NativeCUSChannels(param_3, param_4, _187);
    return _397;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    int2 _404;
    if (NativeCUSEmpty(param, _187))
    {
        _404 = int2(0);
    }
    else
    {
        _404 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _404;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _361 = NativeCUSChannels(param_1, param_2, _187);
    return _361;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _187)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _187);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _187).xyz;
    return SampleReflectionMapMod(param_3, _187);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _187).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _187, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _187.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _187)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _187);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _949 [[buffer(2)]], constant UniformParamsBuffer& _162 [[buffer(3)]], constant NativeCUSDrawUniforms& _187 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> losMapTex [[texture(8)]], texture2d<float> brdfLUT [[texture(9)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler losMapTexSmplr [[sampler(8)]], sampler brdfLUTSmplr [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _426 = {};
    _426.pieceVertexPosOrig = in.m_426_pieceVertexPosOrig;
    _426.worldVertexPos = in.m_426_worldVertexPos;
    _426.worldTangent_VS = in.m_426_worldTangent_VS;
    _426.worldNormal_VS = in.m_426_worldNormal_VS;
    _426.uvCoords = in.m_426_uvCoords;
    _426.teamCol = in.m_426_teamCol;
    _426.shadowVertexPos = in.m_426_shadowVertexPos;
    _426.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_426_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _162.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _426.uvCoords.xy;
    float unitID = floor(_426.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_426.worldNormal_VS);
    float3 worldTangent = fast::normalize(_426.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _777 = (uint(_187.bitOptions) & 2u) != 0u;
    bool _787;
    if (!_777)
    {
        _787 = (uint(_187.bitOptions) & 512u) != 0u;
    }
    else
    {
        _787 = _777;
    }
    float4 normalTexVal;
    if (_787)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _187);
    }
    float healthMix = 0.0;
    float3 tbnNormal;
    if ((uint(_187.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_2 = 0;
    float2 param_3 = myUV;
    float param_4 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_2, param_3, param_4, _187);
    int param_5 = 1;
    float2 param_6 = myUV;
    float param_7 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_5, param_6, param_7, _187);
    float4 teamColor = _426.teamCol;
    float3 param_8 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_8, _187);
    if ((uint(_187.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_426.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_187.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_9 = roughness;
    float3 param_10 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_9, param_10), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_162.sunDir.xyz);
    float3 V = fast::normalize(_949.cameraViewInv[3].xyz - _426.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_187.bitOptions) & 1u) != 0u)
    {
        float param_11 = NdotL;
        gShadow = GetShadowPCFRandom(param_11, shadowsQuality, _187, _426, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _162.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_12 = 9;
    float2 param_13 = float2(NdotV, roughness);
    float param_14 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_12, param_13, param_14, _187).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_15 = F0;
        float3 param_16 = F90;
        float param_17 = VdotH;
        float3 F = FresnelSchlick(param_15, param_16, param_17);
        float param_18 = NdotL;
        float param_19 = NdotV;
        float param_20 = roughness2;
        float Vis = VisibilityOcclusionFast(param_18, param_19, param_20);
        float param_21 = NdotH;
        float param_22 = roughness4;
        float D = MicrofacetDistribution(param_21, param_22);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_162.sunSpecularModel.xyz, _162.sunDiffuseModel.xyz, float3(step(dot(_162.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_162.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_23 = F0;
    float3 param_24 = F90;
    float param_25 = VdotH;
    float param_26 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_23, param_24, param_25, param_26);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_162.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_27 = Rv;
    float param_28 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_27, param_28, _187, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_29 = NdotV;
    float param_30 = _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_31 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_29, param_30, param_31);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float2 losMapUV = _426.worldVertexPos.xz;
    losMapUV /= _162.mapSize.zw;
    int param_32 = 8;
    float2 param_33 = losMapUV;
    float losValue = NativeCUSTexture(losMapTex, losMapTexSmplr, param_32, param_33, _187).x * 2.0;
    losValue = fast::clamp(losValue, 0.5, 1.0);
    outColor *= losValue;
    outSpecularColor *= losValue;
    emissiveness *= losValue;
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _187);
    if ((uint(_187.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_162.fogColor.xyz, outColor, float3(_426.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _187.brightnessFactor;
    float selectedness = _426.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_426.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1356 = fast::clamp(_426.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1356.x;
        selectionHighlight.y = _1356.y;
        selectionHighlight.z = _1356.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _426.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    fragData[0] = float4(outColor, texColor2.w);
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_8_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"losMapTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_9_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,false},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,false},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,false},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_9_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, thread float3& pos, thread float4& uv, constant NativeCUSDrawUniforms& _608, thread float3& T, thread float3& B, thread float4& gl_Position, thread float (&gl_ClipDistance)[1])
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float4 _592 = piecePos;
    float3 _594 = _592.xyz * featureFadeFactor;
    piecePos.x = _594.x;
    piecePos.y = _594.y;
    piecePos.z = _594.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_608.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _638 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _646;
    if (!_638)
    {
        _646 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _646 = _638;
    }
    if (_646)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _672 = instData.x + 0u;
    uint _675 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_672].quat;
    param_8.trSc = _473.transforms[_672].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_675].quat;
    param_9.trSc = _473.transforms[_675].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _693 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _693;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    float4 lightVertexPos = _73.shadowView * worldPos;
    float3 lightVertexNormal = normal;
    float NdotL = fast::clamp(dot(lightVertexNormal, float3(0.0, 0.0, 1.0)), 0.0, 1.0);
    float bias0 = 4.9999998736893758177757263183594e-05 * tan(acos(NdotL));
    bias0 = fast::clamp(bias0, 0.0, 0.00025000001187436282634735107421875);
    float4 _761 = lightVertexPos;
    float2 _763 = _761.xy + float2(0.5);
    lightVertexPos.x = _763.x;
    lightVertexPos.y = _763.y;
    lightVertexPos.z += bias0;
    gl_Position = _73.shadowProj * lightVertexPos;
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _608 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(_73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, in.pos, in.uv, _608, in.T, in.B, out.gl_Position, out.gl_ClipDistance);
    if ((_608.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_608.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_608.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_9_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,false},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,false},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,false},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,false},
};
inline constexpr char CUS_9_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

constant spvUnsafeArray<float, 8> _40 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });

fragment void NativeCUSFragment(constant UniformParamsBuffer& _21 [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    spvUnsafeArray<ShadowQuality, 4> _91 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });
    
    int shadowsQuality = 0;
    float simFrame = _21.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_9_defaults[]={{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_10_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_10_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, constant NativeCUSDrawUniforms& _566, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_566.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _297.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _566.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _566.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _566.featureFadeDistSq) / (_566.featureDrawDistSq - _566.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float4 _646 = piecePos;
    float3 _648 = _646.xyz * featureFadeFactor;
    piecePos.x = _648.x;
    piecePos.y = _648.y;
    piecePos.z = _648.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_566.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _688 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _696;
    if (!_688)
    {
        _696 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _696 = _688;
    }
    if (_696)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _722 = instData.x + 0u;
    uint _725 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_722].quat;
    param_8.trSc = _473.transforms[_722].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_725].quat;
    param_9.trSc = _473.transforms[_725].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _743 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _743;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_297.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_102.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _553.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    _553.worldVertexPos = worldPos;
    _553.shadowVertexPos = _73.shadowView * worldPos;
    float4 _800 = _553.shadowVertexPos;
    float2 _802 = _800.xy + float2(0.5);
    _553.shadowVertexPos.x = _802.x;
    _553.shadowVertexPos.y = _802.y;
    _553.shadowVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[0])[0u];
    if ((uint(_566.bitOptions) & 8u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_566.bitOptions) & 16u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_297.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_566.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _73.cameraView * _553.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_102.fogParams.y - fogCoord) * _102.fogParams.w;
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_553.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_566.drawPass) & 4u) == 4u)
    {
        gl_Position = _73.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _566.clipPlane0);
    }
    else
    {
        gl_Position = _73.cameraViewProj * worldPos;
    }
    _553.worldVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _566 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, _566, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_566.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_566.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_566.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_10_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,true},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_10_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _153 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _387 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
    float4 fragData_1 [[color(1)]];
    float4 fragData_2 [[color(2)]];
    float4 fragData_3 [[color(3)]];
    float4 fragData_4 [[color(4)]];
};

struct NativeCUSFragment_in
{
    float4 m_366_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_366_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_366_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_teamCol [[user(locn5), flat]];
    float4 m_366_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _159)
{
    return (_159.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _159)
{
    if ((_159.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _159)
{
    uint code = _159.NativeCUSSampleCodes[slot].x;
    if ((_159.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _225;
        if (mode == 0u)
        {
            _225 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _237;
            if (mode == 1u)
            {
                _237 = float4(value.x);
            }
            else
            {
                _237 = float4(0.0, 0.0, 0.0, value.x);
            }
            _225 = _237;
        }
        value = _225;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    if (NativeCUSEmpty(param, _159))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _292 = NativeCUSUV(param_1, param_2, _159);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _292);
    float4 _297 = NativeCUSChannels(param_3, param_4, _159);
    return _297;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    if (NativeCUSEmpty(param, _159))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _311 = NativeCUSUV(param_1, param_2, _159);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _311, bias(bias0));
    float4 _317 = NativeCUSChannels(param_3, param_4, _159);
    return _317;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _159)
{
    if (_159.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_159.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _325 = NativeCUSUV(param, param_1, _159);
    uv.x = _325.x;
    uv.y = _325.y;
    if ((_159.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _159, thread Data& _366, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _366.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _387[presetIndex].samplingRandomness;
    float samplingDistance = _387[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _432 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _159);
        shadow = _432;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    if (NativeCUSEmpty(param, _159))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _347 = NativeCUSUV(param_1, param_2, _159);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _347, level(lod));
    float4 _353 = NativeCUSChannels(param_3, param_4, _159);
    return _353;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _159)
{
    if (_159.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _159.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _159)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _159);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _837 [[buffer(2)]], constant UniformParamsBuffer& _134 [[buffer(3)]], constant NativeCUSDrawUniforms& _159 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> shadowTex [[texture(6)]], texture2d<float> losMapTex [[texture(8)]], texture2d<float> brdfLUT [[texture(9)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler shadowTexSmplr [[sampler(6)]], sampler losMapTexSmplr [[sampler(8)]], sampler brdfLUTSmplr [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 5> fragData = {};
    Data _366 = {};
    _366.pieceVertexPosOrig = in.m_366_pieceVertexPosOrig;
    _366.worldVertexPos = in.m_366_worldVertexPos;
    _366.worldTangent_VS = in.m_366_worldTangent_VS;
    _366.worldNormal_VS = in.m_366_worldNormal_VS;
    _366.uvCoords = in.m_366_uvCoords;
    _366.teamCol = in.m_366_teamCol;
    _366.shadowVertexPos = in.m_366_shadowVertexPos;
    _366.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_366_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _134.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _366.uvCoords.xy;
    float unitID = floor(_366.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_366.worldNormal_VS);
    float3 worldTangent = fast::normalize(_366.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _664 = (uint(_159.bitOptions) & 2u) != 0u;
    bool _674;
    if (!_664)
    {
        _674 = (uint(_159.bitOptions) & 512u) != 0u;
    }
    else
    {
        _674 = _664;
    }
    float4 normalTexVal;
    if (_674)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _159);
    }
    float healthMix = 0.0;
    float3 tbnNormal;
    if ((uint(_159.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_2 = 0;
    float2 param_3 = myUV;
    float param_4 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_2, param_3, param_4, _159);
    int param_5 = 1;
    float2 param_6 = myUV;
    float param_7 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_5, param_6, param_7, _159);
    float4 teamColor = _366.teamCol;
    float3 param_8 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_8, _159);
    if ((uint(_159.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_366.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_159.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_9 = roughness;
    float3 param_10 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_9, param_10), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_134.sunDir.xyz);
    float3 V = fast::normalize(_837.cameraViewInv[3].xyz - _366.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_159.bitOptions) & 1u) != 0u)
    {
        float param_11 = NdotL;
        gShadow = GetShadowPCFRandom(param_11, shadowsQuality, _159, _366, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _134.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_12 = 9;
    float2 param_13 = float2(NdotV, roughness);
    float param_14 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_12, param_13, param_14, _159).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_15 = F0;
        float3 param_16 = F90;
        float param_17 = VdotH;
        float3 F = FresnelSchlick(param_15, param_16, param_17);
        float param_18 = NdotL;
        float param_19 = NdotV;
        float param_20 = roughness2;
        float Vis = VisibilityOcclusionFast(param_18, param_19, param_20);
        float param_21 = NdotH;
        float param_22 = roughness4;
        float D = MicrofacetDistribution(param_21, param_22);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_134.sunSpecularModel.xyz, _134.sunDiffuseModel.xyz, float3(step(dot(_134.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_134.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_23 = F0;
    float3 param_24 = F90;
    float param_25 = VdotH;
    float param_26 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_23, param_24, param_25, param_26);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_134.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _366.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 reflectionColor = float3(0.0);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_27 = NdotV;
    float param_28 = _366.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_29 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_27, param_28, param_29);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float2 losMapUV = _366.worldVertexPos.xz;
    losMapUV /= _134.mapSize.zw;
    int param_30 = 8;
    float2 param_31 = losMapUV;
    float losValue = NativeCUSTexture(losMapTex, losMapTexSmplr, param_30, param_31, _159).x * 2.0;
    losValue = fast::clamp(losValue, 0.5, 1.0);
    outColor *= losValue;
    outSpecularColor *= losValue;
    emissiveness *= losValue;
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _159);
    if ((uint(_159.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_134.fogColor.xyz, outColor, float3(_366.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _159.brightnessFactor;
    float selectedness = _366.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_366.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1243 = fast::clamp(_366.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1243.x;
        selectionHighlight.y = _1243.y;
        selectionHighlight.z = _1243.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _366.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float alphaBin = (texColor2.w < 0.5) ? 0.0 : 1.0;
    outSpecularColor = CustomTM(outSpecularColor, pbrParams, _159);
    fragData[0] = float4((N * 0.5) + float3(0.5), alphaBin);
    fragData[1] = float4(outColor, alphaBin);
    fragData[2] = float4(outSpecularColor, alphaBin);
    fragData[3] = float4(((albedoColor * emissiveness) * 2.0) + (outSpecularColor * 0.300000011920928955078125), alphaBin);
    fragData[4] = float4(float(_159.materialIndex) / 255.0, 0.0, 0.0, alphaBin);
    out.fragData_0 = fragData[0];
    out.fragData_1 = fragData[1];
    out.fragData_2 = fragData[2];
    out.fragData_3 = fragData[3];
    out.fragData_4 = fragData[4];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_10_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"materialIndex",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"losMapTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_11_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_11_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, constant NativeCUSDrawUniforms& _566, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_566.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _297.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _566.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _566.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _566.featureFadeDistSq) / (_566.featureDrawDistSq - _566.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float4 _646 = piecePos;
    float3 _648 = _646.xyz * featureFadeFactor;
    piecePos.x = _648.x;
    piecePos.y = _648.y;
    piecePos.z = _648.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_566.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _688 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _696;
    if (!_688)
    {
        _696 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _696 = _688;
    }
    if (_696)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _722 = instData.x + 0u;
    uint _725 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_722].quat;
    param_8.trSc = _473.transforms[_722].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_725].quat;
    param_9.trSc = _473.transforms[_725].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _743 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _743;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_297.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_102.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _553.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    _553.worldVertexPos = worldPos;
    _553.shadowVertexPos = _73.shadowView * worldPos;
    float4 _800 = _553.shadowVertexPos;
    float2 _802 = _800.xy + float2(0.5);
    _553.shadowVertexPos.x = _802.x;
    _553.shadowVertexPos.y = _802.y;
    _553.shadowVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[0])[0u];
    if ((uint(_566.bitOptions) & 8u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_566.bitOptions) & 16u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_297.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_566.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _73.cameraView * _553.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_102.fogParams.y - fogCoord) * _102.fogParams.w;
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_553.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_566.drawPass) & 4u) == 4u)
    {
        gl_Position = _73.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _566.clipPlane0);
    }
    else
    {
        gl_Position = _73.cameraViewProj * worldPos;
    }
    _553.worldVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _566 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, _566, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_566.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_566.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_566.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_11_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_11_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _181 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _447 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_426_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_teamCol [[user(locn5), flat]];
    float4 m_426_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _187)
{
    return (_187.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _187)
{
    if ((_187.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _187)
{
    uint code = _187.NativeCUSSampleCodes[slot].x;
    if ((_187.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _253;
        if (mode == 0u)
        {
            _253 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _265;
            if (mode == 1u)
            {
                _265 = float4(value.x);
            }
            else
            {
                _265 = float4(0.0, 0.0, 0.0, value.x);
            }
            _253 = _265;
        }
        value = _253;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _320 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _320);
    float4 _325 = NativeCUSChannels(param_3, param_4, _187);
    return _325;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _339 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _339, bias(bias0));
    float4 _345 = NativeCUSChannels(param_3, param_4, _187);
    return _345;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_187.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _369 = NativeCUSUV(param, param_1, _187);
    uv.x = _369.x;
    uv.y = _369.y;
    if ((_187.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _187, thread Data& _426, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _426.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _447[presetIndex].samplingRandomness;
    float samplingDistance = _447[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _492 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _187);
        shadow = _492;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _391 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _391, level(lod));
    float4 _397 = NativeCUSChannels(param_3, param_4, _187);
    return _397;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    int2 _404;
    if (NativeCUSEmpty(param, _187))
    {
        _404 = int2(0);
    }
    else
    {
        _404 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _404;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _361 = NativeCUSChannels(param_1, param_2, _187);
    return _361;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _187)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _187);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _187).xyz;
    return SampleReflectionMapMod(param_3, _187);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _187).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _187, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _187.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _187)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _187);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _949 [[buffer(2)]], constant UniformParamsBuffer& _162 [[buffer(3)]], constant NativeCUSDrawUniforms& _187 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> losMapTex [[texture(8)]], texture2d<float> brdfLUT [[texture(9)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler losMapTexSmplr [[sampler(8)]], sampler brdfLUTSmplr [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _426 = {};
    _426.pieceVertexPosOrig = in.m_426_pieceVertexPosOrig;
    _426.worldVertexPos = in.m_426_worldVertexPos;
    _426.worldTangent_VS = in.m_426_worldTangent_VS;
    _426.worldNormal_VS = in.m_426_worldNormal_VS;
    _426.uvCoords = in.m_426_uvCoords;
    _426.teamCol = in.m_426_teamCol;
    _426.shadowVertexPos = in.m_426_shadowVertexPos;
    _426.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_426_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _162.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _426.uvCoords.xy;
    float unitID = floor(_426.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_426.worldNormal_VS);
    float3 worldTangent = fast::normalize(_426.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _777 = (uint(_187.bitOptions) & 2u) != 0u;
    bool _787;
    if (!_777)
    {
        _787 = (uint(_187.bitOptions) & 512u) != 0u;
    }
    else
    {
        _787 = _777;
    }
    float4 normalTexVal;
    if (_787)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _187);
    }
    float healthMix = 0.0;
    float3 tbnNormal;
    if ((uint(_187.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_2 = 0;
    float2 param_3 = myUV;
    float param_4 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_2, param_3, param_4, _187);
    int param_5 = 1;
    float2 param_6 = myUV;
    float param_7 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_5, param_6, param_7, _187);
    float4 teamColor = _426.teamCol;
    float3 param_8 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_8, _187);
    if ((uint(_187.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_426.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_187.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float param_9 = roughness;
    float3 param_10 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_9, param_10), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_162.sunDir.xyz);
    float3 V = fast::normalize(_949.cameraViewInv[3].xyz - _426.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_187.bitOptions) & 1u) != 0u)
    {
        float param_11 = NdotL;
        gShadow = GetShadowPCFRandom(param_11, shadowsQuality, _187, _426, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _162.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_12 = 9;
    float2 param_13 = float2(NdotV, roughness);
    float param_14 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_12, param_13, param_14, _187).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_15 = F0;
        float3 param_16 = F90;
        float param_17 = VdotH;
        float3 F = FresnelSchlick(param_15, param_16, param_17);
        float param_18 = NdotL;
        float param_19 = NdotV;
        float param_20 = roughness2;
        float Vis = VisibilityOcclusionFast(param_18, param_19, param_20);
        float param_21 = NdotH;
        float param_22 = roughness4;
        float D = MicrofacetDistribution(param_21, param_22);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_162.sunSpecularModel.xyz, _162.sunDiffuseModel.xyz, float3(step(dot(_162.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_162.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_23 = F0;
    float3 param_24 = F90;
    float param_25 = VdotH;
    float param_26 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_23, param_24, param_25, param_26);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_162.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_27 = Rv;
    float param_28 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_27, param_28, _187, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_29 = NdotV;
    float param_30 = _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_31 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_29, param_30, param_31);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float2 losMapUV = _426.worldVertexPos.xz;
    losMapUV /= _162.mapSize.zw;
    int param_32 = 8;
    float2 param_33 = losMapUV;
    float losValue = NativeCUSTexture(losMapTex, losMapTexSmplr, param_32, param_33, _187).x * 2.0;
    losValue = fast::clamp(losValue, 0.5, 1.0);
    outColor *= losValue;
    outSpecularColor *= losValue;
    emissiveness *= losValue;
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _187);
    if ((uint(_187.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_162.fogColor.xyz, outColor, float3(_426.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _187.brightnessFactor;
    float selectedness = _426.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_426.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1356 = fast::clamp(_426.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1356.x;
        selectionHighlight.y = _1356.y;
        selectionHighlight.z = _1356.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _426.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    fragData[0] = float4(outColor, texColor2.w);
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_11_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"losMapTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_12_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_12_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, constant NativeCUSDrawUniforms& _566, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_566.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _297.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _566.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _566.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _566.featureFadeDistSq) / (_566.featureDrawDistSq - _566.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float randomScale = (fract(float(_297.uni[instData.y].composite >> uint(16)) * 0.00999999977648258209228515625) * 0.20000000298023223876953125) + 0.89999997615814208984375;
    float4 _659 = piecePos;
    float3 _661 = _659.xyz * randomScale;
    piecePos.x = _661.x;
    piecePos.y = _661.y;
    piecePos.z = _661.z;
    float4 _669 = piecePos;
    float3 _671 = _669.xyz * featureFadeFactor;
    piecePos.x = _671.x;
    piecePos.y = _671.y;
    piecePos.z = _671.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_566.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _711 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _719;
    if (!_711)
    {
        _719 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _719 = _711;
    }
    if (_719)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _745 = instData.x + 0u;
    uint _748 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_745].quat;
    param_8.trSc = _473.transforms[_745].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_748].quat;
    param_9.trSc = _473.transforms[_748].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _766 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _766;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_297.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_102.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _553.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    _553.worldVertexPos = worldPos;
    _553.shadowVertexPos = _73.shadowView * worldPos;
    float4 _823 = _553.shadowVertexPos;
    float2 _825 = _823.xy + float2(0.5);
    _553.shadowVertexPos.x = _825.x;
    _553.shadowVertexPos.y = _825.y;
    _553.shadowVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[0])[0u];
    if ((uint(_566.bitOptions) & 8u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_566.bitOptions) & 16u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_297.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_566.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _73.cameraView * _553.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_102.fogParams.y - fogCoord) * _102.fogParams.w;
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_553.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_566.drawPass) & 4u) == 4u)
    {
        gl_Position = _73.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _566.clipPlane0);
    }
    else
    {
        gl_Position = _73.cameraViewProj * worldPos;
    }
    _553.worldVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _566 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, _566, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_566.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_566.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_566.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_12_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_12_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _181 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _447 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_426_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_teamCol [[user(locn5), flat]];
    float4 m_426_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _187)
{
    return (_187.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _187)
{
    if ((_187.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _187)
{
    uint code = _187.NativeCUSSampleCodes[slot].x;
    if ((_187.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _253;
        if (mode == 0u)
        {
            _253 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _265;
            if (mode == 1u)
            {
                _265 = float4(value.x);
            }
            else
            {
                _265 = float4(0.0, 0.0, 0.0, value.x);
            }
            _253 = _265;
        }
        value = _253;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _320 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _320);
    float4 _325 = NativeCUSChannels(param_3, param_4, _187);
    return _325;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _339 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _339, bias(bias0));
    float4 _345 = NativeCUSChannels(param_3, param_4, _187);
    return _345;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_187.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _369 = NativeCUSUV(param, param_1, _187);
    uv.x = _369.x;
    uv.y = _369.y;
    if ((_187.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _187, thread Data& _426, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _426.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _447[presetIndex].samplingRandomness;
    float samplingDistance = _447[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _492 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _187);
        shadow = _492;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _391 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _391, level(lod));
    float4 _397 = NativeCUSChannels(param_3, param_4, _187);
    return _397;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    int2 _404;
    if (NativeCUSEmpty(param, _187))
    {
        _404 = int2(0);
    }
    else
    {
        _404 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _404;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _361 = NativeCUSChannels(param_1, param_2, _187);
    return _361;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _187)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _187);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _187).xyz;
    return SampleReflectionMapMod(param_3, _187);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _187).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _187, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _187.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _187)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _187);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1017 [[buffer(2)]], constant UniformParamsBuffer& _162 [[buffer(3)]], constant NativeCUSDrawUniforms& _187 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> losMapTex [[texture(8)]], texture2d<float> brdfLUT [[texture(9)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler losMapTexSmplr [[sampler(8)]], sampler brdfLUTSmplr [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _426 = {};
    _426.pieceVertexPosOrig = in.m_426_pieceVertexPosOrig;
    _426.worldVertexPos = in.m_426_worldVertexPos;
    _426.worldTangent_VS = in.m_426_worldTangent_VS;
    _426.worldNormal_VS = in.m_426_worldNormal_VS;
    _426.uvCoords = in.m_426_uvCoords;
    _426.teamCol = in.m_426_teamCol;
    _426.shadowVertexPos = in.m_426_shadowVertexPos;
    _426.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_426_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _162.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _426.uvCoords.xy;
    float unitID = floor(_426.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_426.worldNormal_VS);
    float3 worldTangent = fast::normalize(_426.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _777 = (uint(_187.bitOptions) & 2u) != 0u;
    bool _787;
    if (!_777)
    {
        _787 = (uint(_187.bitOptions) & 512u) != 0u;
    }
    else
    {
        _787 = _777;
    }
    float4 normalTexVal;
    if (_787)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _187);
    }
    float healthMix = 0.0;
    float3 tbnNormal;
    if ((uint(_187.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_2 = 0;
    float2 param_3 = myUV;
    float param_4 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_2, param_3, param_4, _187);
    int param_5 = 1;
    float2 param_6 = myUV;
    float param_7 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_5, param_6, param_7, _187);
    float funitID = unitID;
    float3 randluma = (float3(0.00999999977648258209228515625, 0.0130000002682209014892578125, 0.0170000009238719940185546875) * funitID) - float3(0.5);
    float saturation = fast::clamp(dot(abs(texColor1.xyz - texColor1.yzx), float3(1.0)), 0.0, 1.0);
    randluma = (fract(randluma) * saturation) + float3(1.0);
    float4 _858 = texColor1;
    float3 _860 = _858.xyz * randluma;
    texColor1.x = _860.x;
    texColor1.y = _860.y;
    texColor1.z = _860.z;
    float4 teamColor = _426.teamCol;
    float3 param_8 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_8, _187);
    if ((uint(_187.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_426.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_187.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float charAmount = fast::clamp(1.0 - _426.aoterm_fogFactor_selfIllumMod_healthFraction.w, 0.0, 1.0);
    charAmount *= step(0.5, simFrame);
    if (charAmount > 0.00200000009499490261077880859375)
    {
        albedoColor = mix(albedoColor, float3(0.017999999225139617919921875, 0.014999999664723873138427734375, 0.0130000002682209014892578125), float3(charAmount));
        roughness = fast::max(roughness, mix(roughness, 0.89999997615814208984375, charAmount));
        metalness = fast::min(metalness, mix(metalness, 0.0, charAmount));
    }
    float param_9 = roughness;
    float3 param_10 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_9, param_10), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_162.sunDir.xyz);
    float3 V = fast::normalize(_1017.cameraViewInv[3].xyz - _426.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_187.bitOptions) & 1u) != 0u)
    {
        float param_11 = NdotL;
        gShadow = GetShadowPCFRandom(param_11, shadowsQuality, _187, _426, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _162.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_12 = 9;
    float2 param_13 = float2(NdotV, roughness);
    float param_14 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_12, param_13, param_14, _187).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_15 = F0;
        float3 param_16 = F90;
        float param_17 = VdotH;
        float3 F = FresnelSchlick(param_15, param_16, param_17);
        float param_18 = NdotL;
        float param_19 = NdotV;
        float param_20 = roughness2;
        float Vis = VisibilityOcclusionFast(param_18, param_19, param_20);
        float param_21 = NdotH;
        float param_22 = roughness4;
        float D = MicrofacetDistribution(param_21, param_22);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_162.sunSpecularModel.xyz, _162.sunDiffuseModel.xyz, float3(step(dot(_162.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_162.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_23 = F0;
    float3 param_24 = F90;
    float param_25 = VdotH;
    float param_26 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_23, param_24, param_25, param_26);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_162.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_27 = Rv;
    float param_28 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_27, param_28, _187, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_29 = NdotV;
    float param_30 = _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_31 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_29, param_30, param_31);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float2 losMapUV = _426.worldVertexPos.xz;
    losMapUV /= _162.mapSize.zw;
    int param_32 = 8;
    float2 param_33 = losMapUV;
    float losValue = NativeCUSTexture(losMapTex, losMapTexSmplr, param_32, param_33, _187).x * 2.0;
    losValue = fast::clamp(losValue, 0.5, 1.0);
    outColor *= losValue;
    outSpecularColor *= losValue;
    emissiveness *= losValue;
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _187);
    if ((uint(_187.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_162.fogColor.xyz, outColor, float3(_426.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _187.brightnessFactor;
    float selectedness = _426.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_426.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1423 = fast::clamp(_426.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1423.x;
        selectionHighlight.y = _1423.y;
        selectionHighlight.z = _1423.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _426.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    fragData[0] = float4(outColor, texColor2.w);
    if (texColor2.w < 0.5)
    {
        discard_fragment();
    }
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_12_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"losMapTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_13_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,false},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,false},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,false},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_13_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, thread float3& pos, thread float4& uv, constant NativeCUSDrawUniforms& _631, thread float3& T, thread float3& B, thread float4& gl_Position, thread float (&gl_ClipDistance)[1])
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float randomScale = (fract(float(_297.uni[instData.y].composite >> uint(16)) * 0.00999999977648258209228515625) * 0.20000000298023223876953125) + 0.89999997615814208984375;
    float4 _605 = piecePos;
    float3 _607 = _605.xyz * randomScale;
    piecePos.x = _607.x;
    piecePos.y = _607.y;
    piecePos.z = _607.z;
    float4 _615 = piecePos;
    float3 _617 = _615.xyz * featureFadeFactor;
    piecePos.x = _617.x;
    piecePos.y = _617.y;
    piecePos.z = _617.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_631.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _661 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _669;
    if (!_661)
    {
        _669 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _669 = _661;
    }
    if (_669)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _695 = instData.x + 0u;
    uint _698 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_695].quat;
    param_8.trSc = _473.transforms[_695].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_698].quat;
    param_9.trSc = _473.transforms[_698].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _716 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _716;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    float4 lightVertexPos = _73.shadowView * worldPos;
    float3 lightVertexNormal = normal;
    float NdotL = fast::clamp(dot(lightVertexNormal, float3(0.0, 0.0, 1.0)), 0.0, 1.0);
    float bias0 = 4.9999998736893758177757263183594e-05 * tan(acos(NdotL));
    bias0 = fast::clamp(bias0, 0.0, 0.00025000001187436282634735107421875);
    float4 _784 = lightVertexPos;
    float2 _786 = _784.xy + float2(0.5);
    lightVertexPos.x = _786.x;
    lightVertexPos.y = _786.y;
    lightVertexPos.z += bias0;
    gl_Position = _73.shadowProj * lightVertexPos;
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _631 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(_73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, in.pos, in.uv, _631, in.T, in.B, out.gl_Position, out.gl_ClipDistance);
    if ((_631.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_631.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_631.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_13_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,false},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,false},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,false},
};
inline constexpr char CUS_13_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

constant spvUnsafeArray<float, 8> _75 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _282 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_in
{
    float4 m_227_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_227_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_227_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_227_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_227_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_227_teamCol [[user(locn5), flat]];
    float4 m_227_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_227_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _81)
{
    return (_81.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _81)
{
    if ((_81.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _81)
{
    uint code = _81.NativeCUSSampleCodes[slot].x;
    if ((_81.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _147;
        if (mode == 0u)
        {
            _147 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _159;
            if (mode == 1u)
            {
                _159 = float4(value.x);
            }
            else
            {
                _159 = float4(0.0, 0.0, 0.0, value.x);
            }
            _147 = _159;
        }
        value = _147;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _81)
{
    int param = slot;
    if (NativeCUSEmpty(param, _81))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _214 = NativeCUSUV(param_1, param_2, _81);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _214, bias(bias0));
    float4 _220 = NativeCUSChannels(param_3, param_4, _81);
    return _220;
}

fragment void NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformParamsBuffer& _56 [[buffer(3)]], constant NativeCUSDrawUniforms& _81 [[buffer(7)]], texture2d<float> texture2 [[texture(1)]], sampler texture2Smplr [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    Data _227 = {};
    _227.pieceVertexPosOrig = in.m_227_pieceVertexPosOrig;
    _227.worldVertexPos = in.m_227_worldVertexPos;
    _227.worldTangent_VS = in.m_227_worldTangent_VS;
    _227.worldNormal_VS = in.m_227_worldNormal_VS;
    _227.uvCoords = in.m_227_uvCoords;
    _227.teamCol = in.m_227_teamCol;
    _227.shadowVertexPos = in.m_227_shadowVertexPos;
    _227.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_227_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _56.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    int param = 1;
    float2 param_1 = _227.uvCoords.xy;
    float param_2 = 0.0;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param, param_1, param_2, _81);
    if (texColor2.w < 0.5)
    {
        discard_fragment();
    }
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_13_defaults[]={{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_14_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_14_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, constant NativeCUSDrawUniforms& _566, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_566.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _297.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _566.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _566.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _566.featureFadeDistSq) / (_566.featureDrawDistSq - _566.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float randomScale = (fract(float(_297.uni[instData.y].composite >> uint(16)) * 0.00999999977648258209228515625) * 0.20000000298023223876953125) + 0.89999997615814208984375;
    float4 _659 = piecePos;
    float3 _661 = _659.xyz * randomScale;
    piecePos.x = _661.x;
    piecePos.y = _661.y;
    piecePos.z = _661.z;
    float4 _669 = piecePos;
    float3 _671 = _669.xyz * featureFadeFactor;
    piecePos.x = _671.x;
    piecePos.y = _671.y;
    piecePos.z = _671.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_566.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _711 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _719;
    if (!_711)
    {
        _719 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _719 = _711;
    }
    if (_719)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _745 = instData.x + 0u;
    uint _748 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_745].quat;
    param_8.trSc = _473.transforms[_745].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_748].quat;
    param_9.trSc = _473.transforms[_748].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _766 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _766;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_297.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_102.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _553.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    _553.worldVertexPos = worldPos;
    _553.shadowVertexPos = _73.shadowView * worldPos;
    float4 _823 = _553.shadowVertexPos;
    float2 _825 = _823.xy + float2(0.5);
    _553.shadowVertexPos.x = _825.x;
    _553.shadowVertexPos.y = _825.y;
    _553.shadowVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[0])[0u];
    if ((uint(_566.bitOptions) & 8u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_566.bitOptions) & 16u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_297.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_566.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _73.cameraView * _553.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_102.fogParams.y - fogCoord) * _102.fogParams.w;
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_553.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_566.drawPass) & 4u) == 4u)
    {
        gl_Position = _73.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _566.clipPlane0);
    }
    else
    {
        gl_Position = _73.cameraViewProj * worldPos;
    }
    _553.worldVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _566 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, _566, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_566.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_566.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_566.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_14_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,true},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_14_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _153 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _387 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
    float4 fragData_1 [[color(1)]];
    float4 fragData_2 [[color(2)]];
    float4 fragData_3 [[color(3)]];
    float4 fragData_4 [[color(4)]];
};

struct NativeCUSFragment_in
{
    float4 m_366_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_366_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_366_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_teamCol [[user(locn5), flat]];
    float4 m_366_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_366_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _159)
{
    return (_159.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _159)
{
    if ((_159.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _159)
{
    uint code = _159.NativeCUSSampleCodes[slot].x;
    if ((_159.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _225;
        if (mode == 0u)
        {
            _225 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _237;
            if (mode == 1u)
            {
                _237 = float4(value.x);
            }
            else
            {
                _237 = float4(0.0, 0.0, 0.0, value.x);
            }
            _225 = _237;
        }
        value = _225;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    if (NativeCUSEmpty(param, _159))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _292 = NativeCUSUV(param_1, param_2, _159);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _292);
    float4 _297 = NativeCUSChannels(param_3, param_4, _159);
    return _297;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    if (NativeCUSEmpty(param, _159))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _311 = NativeCUSUV(param_1, param_2, _159);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _311, bias(bias0));
    float4 _317 = NativeCUSChannels(param_3, param_4, _159);
    return _317;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _159)
{
    if (_159.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_159.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _325 = NativeCUSUV(param, param_1, _159);
    uv.x = _325.x;
    uv.y = _325.y;
    if ((_159.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _159, thread Data& _366, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _366.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _387[presetIndex].samplingRandomness;
    float samplingDistance = _387[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _432 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _159);
        shadow = _432;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _159)
{
    int param = slot;
    if (NativeCUSEmpty(param, _159))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _347 = NativeCUSUV(param_1, param_2, _159);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _347, level(lod));
    float4 _353 = NativeCUSChannels(param_3, param_4, _159);
    return _353;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _159)
{
    if (_159.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _159.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _159)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _159);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _905 [[buffer(2)]], constant UniformParamsBuffer& _134 [[buffer(3)]], constant NativeCUSDrawUniforms& _159 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> shadowTex [[texture(6)]], texture2d<float> losMapTex [[texture(8)]], texture2d<float> brdfLUT [[texture(9)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler shadowTexSmplr [[sampler(6)]], sampler losMapTexSmplr [[sampler(8)]], sampler brdfLUTSmplr [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 5> fragData = {};
    Data _366 = {};
    _366.pieceVertexPosOrig = in.m_366_pieceVertexPosOrig;
    _366.worldVertexPos = in.m_366_worldVertexPos;
    _366.worldTangent_VS = in.m_366_worldTangent_VS;
    _366.worldNormal_VS = in.m_366_worldNormal_VS;
    _366.uvCoords = in.m_366_uvCoords;
    _366.teamCol = in.m_366_teamCol;
    _366.shadowVertexPos = in.m_366_shadowVertexPos;
    _366.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_366_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _134.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _366.uvCoords.xy;
    float unitID = floor(_366.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_366.worldNormal_VS);
    float3 worldTangent = fast::normalize(_366.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _664 = (uint(_159.bitOptions) & 2u) != 0u;
    bool _674;
    if (!_664)
    {
        _674 = (uint(_159.bitOptions) & 512u) != 0u;
    }
    else
    {
        _674 = _664;
    }
    float4 normalTexVal;
    if (_674)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _159);
    }
    float healthMix = 0.0;
    float3 tbnNormal;
    if ((uint(_159.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_2 = 0;
    float2 param_3 = myUV;
    float param_4 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_2, param_3, param_4, _159);
    int param_5 = 1;
    float2 param_6 = myUV;
    float param_7 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_5, param_6, param_7, _159);
    float funitID = unitID;
    float3 randluma = (float3(0.00999999977648258209228515625, 0.0130000002682209014892578125, 0.0170000009238719940185546875) * funitID) - float3(0.5);
    float saturation = fast::clamp(dot(abs(texColor1.xyz - texColor1.yzx), float3(1.0)), 0.0, 1.0);
    randluma = (fract(randluma) * saturation) + float3(1.0);
    float4 _745 = texColor1;
    float3 _747 = _745.xyz * randluma;
    texColor1.x = _747.x;
    texColor1.y = _747.y;
    texColor1.z = _747.z;
    float4 teamColor = _366.teamCol;
    float3 param_8 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_8, _159);
    if ((uint(_159.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_366.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_159.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float charAmount = fast::clamp(1.0 - _366.aoterm_fogFactor_selfIllumMod_healthFraction.w, 0.0, 1.0);
    charAmount *= step(0.5, simFrame);
    if (charAmount > 0.00200000009499490261077880859375)
    {
        albedoColor = mix(albedoColor, float3(0.017999999225139617919921875, 0.014999999664723873138427734375, 0.0130000002682209014892578125), float3(charAmount));
        roughness = fast::max(roughness, mix(roughness, 0.89999997615814208984375, charAmount));
        metalness = fast::min(metalness, mix(metalness, 0.0, charAmount));
    }
    float param_9 = roughness;
    float3 param_10 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_9, param_10), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_134.sunDir.xyz);
    float3 V = fast::normalize(_905.cameraViewInv[3].xyz - _366.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_159.bitOptions) & 1u) != 0u)
    {
        float param_11 = NdotL;
        gShadow = GetShadowPCFRandom(param_11, shadowsQuality, _159, _366, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _134.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_12 = 9;
    float2 param_13 = float2(NdotV, roughness);
    float param_14 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_12, param_13, param_14, _159).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_15 = F0;
        float3 param_16 = F90;
        float param_17 = VdotH;
        float3 F = FresnelSchlick(param_15, param_16, param_17);
        float param_18 = NdotL;
        float param_19 = NdotV;
        float param_20 = roughness2;
        float Vis = VisibilityOcclusionFast(param_18, param_19, param_20);
        float param_21 = NdotH;
        float param_22 = roughness4;
        float D = MicrofacetDistribution(param_21, param_22);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_134.sunSpecularModel.xyz, _134.sunDiffuseModel.xyz, float3(step(dot(_134.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_134.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_23 = F0;
    float3 param_24 = F90;
    float param_25 = VdotH;
    float param_26 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_23, param_24, param_25, param_26);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_134.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _366.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 reflectionColor = float3(0.0);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_27 = NdotV;
    float param_28 = _366.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_29 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_27, param_28, param_29);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float2 losMapUV = _366.worldVertexPos.xz;
    losMapUV /= _134.mapSize.zw;
    int param_30 = 8;
    float2 param_31 = losMapUV;
    float losValue = NativeCUSTexture(losMapTex, losMapTexSmplr, param_30, param_31, _159).x * 2.0;
    losValue = fast::clamp(losValue, 0.5, 1.0);
    outColor *= losValue;
    outSpecularColor *= losValue;
    emissiveness *= losValue;
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _159);
    if ((uint(_159.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_134.fogColor.xyz, outColor, float3(_366.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _159.brightnessFactor;
    float selectedness = _366.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_366.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1310 = fast::clamp(_366.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1310.x;
        selectionHighlight.y = _1310.y;
        selectionHighlight.z = _1310.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _366.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    float alphaBin = (texColor2.w < 0.5) ? 0.0 : 1.0;
    outSpecularColor = CustomTM(outSpecularColor, pbrParams, _159);
    if (texColor2.w < 0.5)
    {
        discard_fragment();
    }
    fragData[0] = float4((N * 0.5) + float3(0.5), alphaBin);
    fragData[1] = float4(outColor, alphaBin);
    fragData[2] = float4(outSpecularColor, alphaBin);
    fragData[4] = float4(float(_159.materialIndex) / 255.0, 0.0, 0.0, alphaBin);
    out.fragData_0 = fragData[0];
    out.fragData_1 = fragData[1];
    out.fragData_2 = fragData[2];
    out.fragData_3 = fragData[3];
    out.fragData_4 = fragData[4];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_14_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"materialIndex",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"losMapTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
inline constexpr NativeCUSGeneratedField CUS_15_vertex_fields[]={
{"NativeCUSWater0",NativeCUSUniformScalar::Float,0,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,16,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,32,3,1,12,false},
{"drawPass",NativeCUSUniformScalar::Int,44,1,1,4,true},
{"featureFadeDistSq",NativeCUSUniformScalar::Float,48,1,1,4,true},
{"featureDrawDistSq",NativeCUSUniformScalar::Float,52,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,56,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,60,1,1,4,true},
{"clipPlane0",NativeCUSUniformScalar::Float,64,4,1,16,true},
{"NativeCUSClipFlags",NativeCUSUniformScalar::UInt,80,1,1,4,true},
};
inline constexpr char CUS_15_vertex_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Transform
{
    float4 quat;
    float4 trSc;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct SUniformsBuffer
{
    uint composite;
    uint unused2;
    uint unused3;
    uint unused4;
    float maxHealth;
    float health;
    float unused5;
    float unused6;
    float4 drawPos;
    float4 speed;
    float4 userDefined[4];
};

struct UniformsBuffer
{
    SUniformsBuffer uni[1];
};

struct Transform_1
{
    float4 quat;
    float4 trSc;
};

struct TransformBuffer
{
    Transform_1 transforms[1];
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct NativeCUSDrawUniforms
{
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int drawPass;
    float featureFadeDistSq;
    float featureDrawDistSq;
    float baseVertexDisplacement;
    int bitOptions;
    float4 clipPlane0;
    uint NativeCUSClipFlags;
};

struct NativeCUSVertex_out
{
    float4 m_553_pieceVertexPosOrig [[user(locn0)]];
    float4 m_553_worldVertexPos [[user(locn1)]];
    float3 m_553_worldTangent_VS [[user(locn2)]];
    float3 m_553_worldNormal_VS [[user(locn3)]];
    float4 m_553_uvCoords [[user(locn4)]];
    float4 m_553_teamCol [[user(locn5)]];
    float4 m_553_shadowVertexPos [[user(locn6)]];
    float4 m_553_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7)]];
    float4 gl_Position [[position]];
    float gl_ClipDistance [[clip_distance]] [1];
};

struct NativeCUSVertex_in
{
    float3 pos [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 T [[attribute(2)]];
    float3 B [[attribute(3)]];
    float4 uv [[attribute(4)]];
    uint3 bonesInfo [[attribute(5)]];
    uint4 instData [[attribute(6)]];
};

static inline __attribute__((always_inline))
float2 GetWind(thread const float& period)
{
    float2 wind = sin(float2(0.0, 1.559999942779541015625) + float2(period * 5.0));
    return wind * 10.0;
}

static inline __attribute__((always_inline))
float3 hash31(thread const float& p)
{
    float3 p3 = fract(float3(p) * float3(0.103100001811981201171875, 0.10300000011920928955078125, 0.097300000488758087158203125));
    p3 += float3(dot(p3, p3.yzx + float3(19.1900005340576171875)));
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

static inline __attribute__((always_inline))
void DoWindVertexMove(thread float4& mVP, thread float& simFrame, const device UniformsBuffer& _297, thread uint4& instData)
{
    float simFrameFloor = floor(simFrame * 0.001333000021986663341522216796875);
    float simFrameFract = fract(simFrame * 0.001333000021986663341522216796875);
    float param = simFrameFloor;
    float2 curWind = GetWind(param);
    float param_1 = simFrameFloor + 1.0;
    float2 nextWind = GetWind(param_1);
    float tweenFactor = smoothstep(0.0, 1.0, simFrameFract);
    float2 wind = mix(curWind, nextWind, float2(tweenFactor));
    float param_2 = float(_297.uni[instData.y].composite >> uint(16));
    float3 modelXYZ = hash31(param_2) * 16.0;
    modelXYZ = fract(modelXYZ);
    modelXYZ = fast::clamp(modelXYZ, float3(0.4000000059604644775390625), float3(1.0));
    float abswind = abs(wind.x) + abs(wind.y);
    float simTime = 0.0199999995529651641845703125 * simFrame;
    float4 cosVec;
    cosVec.w = 0.0;
    cosVec.x = (simTime * modelXYZ.x) + mVP.x;
    cosVec.y = ((simTime * modelXYZ.z) / 3.0) + modelXYZ.x;
    cosVec.z = (simTime * 1.0) + mVP.z;
    float4 tri = abs((fract(cosVec + float4(0.5)) * 2.0) - float4(1.0));
    cosVec = (tri * tri) * (float4(3.0) - (tri * 2.0));
    float limit = fast::clamp(((mVP.x * mVP.z) * mVP.y) / 3000.0, 0.0, 0.20000000298023223876953125);
    float diff = cosVec.x * limit;
    float diff2 = cosVec.y * fast::clamp(mVP.y / 30.0, 0.0500000007450580596923828125, 0.20000000298023223876953125);
    float4 _414 = mVP;
    float3 _417 = _414.xyz + float3((cosVec.z * limit) * fast::clamp(abswind, 1.2000000476837158203125, 1.7000000476837158203125));
    mVP.x = _417.x;
    mVP.y = _417.y;
    mVP.z = _417.z;
    float4 _430 = mVP;
    float2 _432 = _430.xz + (float2(diff) + (wind * diff2));
    mVP.x = _432.x;
    mVP.z = _432.y;
}

static inline __attribute__((always_inline))
uint GetUnpackedValue(thread const uint& packedValue, thread const uint& byteNum)
{
    return (packedValue >> (8u * byteNum)) & 255u;
}

static inline __attribute__((always_inline))
float4 Lerp(thread const float4& qa, thread float4& qb, thread const float& t)
{
    if (dot(qa, qb) < 0.0)
    {
        qb = -qb;
    }
    return fast::normalize(mix(qa, qb, float4(t)));
}

static inline __attribute__((always_inline))
Transform Lerp(thread const Transform& t0, thread const Transform& t1, thread float& a)
{
    a = fast::clamp(a, 0.0, 1.0);
    float4 param = t0.quat;
    float4 param_1 = t1.quat;
    float param_2 = a;
    float4 _206 = Lerp(param, param_1, param_2);
    return Transform{ _206, mix(t0.trSc, t1.trSc, float4(a)) };
}

static inline __attribute__((always_inline))
float3 RotateByQuaternion(thread const float4& q, thread const float3& v)
{
    return ((q.xyz * (2.0 * dot(q.xyz, v))) + (v * ((q.w * q.w) - dot(q.xyz, q.xyz)))) + (cross(q.xyz, v) * (2.0 * q.w));
}

static inline __attribute__((always_inline))
float4 ApplyTransform(thread const Transform& tra, thread const float4& v)
{
    float4 param = tra.quat;
    float3 param_1 = v.xyz * tra.trSc.w;
    return float4(RotateByQuaternion(param, param_1) + (tra.trSc.xyz * v.w), v.w);
}

static inline __attribute__((always_inline))
void GetModelSpaceVertex(thread const float3& displacedPos, thread float4& msPosition, thread float3& msNormal, thread const float3& safeTangent, thread float3& msTangent, thread const float3& safeBitangent, thread float3& msBitangent, constant UniformParamsBuffer& _102, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal)
{
    bool staticModel = false;
    float4 piecePos = float4(displacedPos, 1.0);
    uint param = bonesInfo.y;
    uint param_1 = 0u;
    float4 weights = float4(float(GetUnpackedValue(param, param_1)) / 255.0, 0.0, 0.0, 0.0);
    uint param_2 = bonesInfo.x;
    uint param_3 = 0u;
    uint bID0 = GetUnpackedValue(param_2, param_3);
    Transform tx;
    if (staticModel)
    {
        uint _477 = instData.x + bID0;
        tx.quat = _473.transforms[_477].quat;
        tx.trSc = _473.transforms[_477].trSc;
    }
    else
    {
        uint _492 = (instData.x + (2u * (1u + bID0))) + 0u;
        uint _499 = (instData.x + (2u * (1u + bID0))) + 1u;
        Transform param_4;
        param_4.quat = _473.transforms[_492].quat;
        param_4.trSc = _473.transforms[_492].trSc;
        Transform param_5;
        param_5.quat = _473.transforms[_499].quat;
        param_5.trSc = _473.transforms[_499].trSc;
        float param_6 = _102.timeInfo.w;
        Transform _517 = Lerp(param_4, param_5, param_6);
        tx = _517;
    }
    weights.x *= float(tx.trSc.w > 0.0);
    Transform param_7 = tx;
    float4 param_8 = piecePos;
    msPosition = ApplyTransform(param_7, param_8);
    float4 param_9 = tx.quat;
    float3 param_10 = normal;
    msNormal = RotateByQuaternion(param_9, param_10);
    float4 param_11 = tx.quat;
    float3 param_12 = safeTangent;
    msTangent = RotateByQuaternion(param_11, param_12);
    float4 param_13 = tx.quat;
    float3 param_14 = safeBitangent;
    msBitangent = RotateByQuaternion(param_13, param_14);
}

static inline __attribute__((always_inline))
void NativeCUSOriginalMain(thread float3& cameraPos, constant UniformMatrixBuffer& _73, thread float& simFrame, constant UniformParamsBuffer& _102, const device UniformsBuffer& _297, thread uint4& instData, thread uint3& bonesInfo, const device TransformBuffer& _473, thread float3& normal, thread Data& _553, constant NativeCUSDrawUniforms& _566, thread float4& gl_Position, thread float (&gl_ClipDistance)[1], thread float3& pos, thread float4& uv, thread float3& T, thread float3& B)
{
    _553.uvCoords.z = float(_297.uni[instData.y].composite >> uint(16));
    float featureFadeFactor = 1.0;
    if (_566.featureFadeDistSq > 0.0)
    {
        float3 toCamera = cameraPos - _297.uni[instData.y].drawPos.xyz;
        float distSq = dot(toCamera, toCamera);
        if (distSq > _566.featureDrawDistSq)
        {
            gl_Position = float4(0.0, 0.0, -2.0, 1.0);
            return;
        }
        else
        {
            if (distSq > _566.featureFadeDistSq)
            {
                featureFadeFactor = 1.0 - ((distSq - _566.featureFadeDistSq) / (_566.featureDrawDistSq - _566.featureFadeDistSq));
            }
        }
    }
    float4 piecePos = float4(pos, 1.0);
    _553.uvCoords.x = uv.xy.x;
    _553.uvCoords.y = uv.xy.y;
    _553.aoterm_fogFactor_selfIllumMod_healthFraction.w = fast::clamp(_297.uni[instData.y].health / _297.uni[instData.y].maxHealth, 0.0, 1.0);
    float randomScale = (fract(float(_297.uni[instData.y].composite >> uint(16)) * 0.00999999977648258209228515625) * 0.20000000298023223876953125) + 0.89999997615814208984375;
    float4 _659 = piecePos;
    float3 _661 = _659.xyz * randomScale;
    piecePos.x = _661.x;
    piecePos.y = _661.y;
    piecePos.z = _661.z;
    float4 _669 = piecePos;
    float3 _671 = _669.xyz * featureFadeFactor;
    piecePos.x = _671.x;
    piecePos.y = _671.y;
    piecePos.z = _671.z;
    _553.pieceVertexPosOrig = piecePos;
    float3 modelVertexNormal = normal;
    if ((uint(_566.bitOptions) & 2048u) != 0u)
    {
        float4 param = piecePos;
        DoWindVertexMove(param, simFrame, _297, instData);
        piecePos = param;
    }
    float4 modelPos = float4(pos, 1.0);
    float3 safeT = T;
    float3 safeB = B;
    bool _711 = dot(safeT, safeT) < 0.100000001490116119384765625;
    bool _719;
    if (!_711)
    {
        _719 = dot(safeB, safeB) < 0.100000001490116119384765625;
    }
    else
    {
        _719 = _711;
    }
    if (_719)
    {
        safeT = float3(1.0, 0.0, 0.0);
        safeB = float3(0.0, 0.0, 1.0);
    }
    float3 param_1 = piecePos.xyz;
    float3 param_4 = safeT;
    float3 param_6 = safeB;
    float4 param_2;
    float3 param_3;
    float3 param_5;
    float3 param_7;
    GetModelSpaceVertex(param_1, param_2, param_3, param_4, param_5, param_6, param_7, _102, instData, bonesInfo, _473, normal);
    modelPos = param_2;
    modelVertexNormal = param_3;
    float3 modelSpaceTangent = param_5;
    float3 modelSpaceBitangent = param_7;
    uint _745 = instData.x + 0u;
    uint _748 = instData.x + 1u;
    Transform param_8;
    param_8.quat = _473.transforms[_745].quat;
    param_8.trSc = _473.transforms[_745].trSc;
    Transform param_9;
    param_9.quat = _473.transforms[_748].quat;
    param_9.trSc = _473.transforms[_748].trSc;
    float param_10 = _102.timeInfo.w;
    Transform _766 = Lerp(param_8, param_9, param_10);
    Transform modelToWorldTX = _766;
    Transform param_11 = modelToWorldTX;
    float4 param_12 = modelPos;
    float4 worldPos = ApplyTransform(param_11, param_12);
    float4 param_13 = modelToWorldTX.quat;
    float3 param_14 = modelSpaceTangent;
    _553.worldTangent_VS = RotateByQuaternion(param_13, param_14);
    float4 param_15 = modelToWorldTX.quat;
    float3 param_16 = modelVertexNormal;
    _553.worldNormal_VS = RotateByQuaternion(param_15, param_16);
    _553.pieceVertexPosOrig.w = modelPos.y / fast::max(1.0, ((device float*)&_297.uni[instData.y].userDefined[2])[3u]);
    uint teamIndex = instData.z & 255u;
    _553.teamCol = ((teamIndex == 255u) ? float4(0.0) : _102.teamColor[teamIndex]);
    _553.teamCol.w = ((device float*)&_297.uni[instData.y].userDefined[1])[2u];
    // Original31657: source userDefined[1].w is signed entry/exit draw time.
    float jammedSince = ((device float*)&_297.uni[instData.y].userDefined[1])[3u];
    float jammedFade = clamp((_102.timeInfo.y - abs(jammedSince)) * (1.0 / 0.3), 0.0, 1.0);
    _553.uvCoords.w = (jammedSince > 0.0) ? jammedFade : ((jammedSince < 0.0) ? (1.0 - jammedFade) : 0.0);
    _553.worldVertexPos = worldPos;
    _553.shadowVertexPos = _73.shadowView * worldPos;
    float4 _823 = _553.shadowVertexPos;
    float2 _825 = _823.xy + float2(0.5);
    _553.shadowVertexPos.x = _825.x;
    _553.shadowVertexPos.y = _825.y;
    _553.shadowVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[0])[0u];
    if ((uint(_566.bitOptions) & 8u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = fast::clamp(1.0 * fract(uv.x * 16384.0), 0.100000001490116119384765625, 1.0);
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.x = 1.0;
    }
    if ((uint(_566.bitOptions) & 16u) != 0u)
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = fast::max(-0.20000000298023223876953125, sin(((simFrame * 2.0) / 30.0) + (float(_297.uni[instData.y].composite >> uint(16)) * 0.100000001490116119384765625))) + 0.20000000298023223876953125;
    }
    else
    {
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.z = 1.0;
    }
    if ((uint(_566.bitOptions) & 1024u) != 0u)
    {
        float4 ClipVertex = _73.cameraView * _553.worldVertexPos;
        float fogCoord = length(ClipVertex.xyz);
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = (_102.fogParams.y - fogCoord) * _102.fogParams.w;
        _553.aoterm_fogFactor_selfIllumMod_healthFraction.y = fast::clamp(_553.aoterm_fogFactor_selfIllumMod_healthFraction.y, 0.0, 1.0);
    }
    if ((uint(_566.drawPass) & 4u) == 4u)
    {
        gl_Position = _73.reflectionViewProj * worldPos;
        gl_ClipDistance[0] = dot(worldPos, _566.clipPlane0);
    }
    else
    {
        gl_Position = _73.cameraViewProj * worldPos;
    }
    _553.worldVertexPos.w = ((device float*)&_297.uni[instData.y].userDefined[3])[0u];
}

vertex NativeCUSVertex_out NativeCUSVertex(NativeCUSVertex_in in [[stage_in]], constant UniformMatrixBuffer& _73 [[buffer(2)]], constant UniformParamsBuffer& _102 [[buffer(3)]], const device TransformBuffer& _473 [[buffer(4)]], const device UniformsBuffer& _297 [[buffer(5)]], constant NativeCUSDrawUniforms& _566 [[buffer(6)]])
{
    NativeCUSVertex_out out = {};
    Data _553 = {};
    float3 cameraPos = _73.cameraViewInv[3].xyz;
    float3 cameraDir = float3(_73.cameraView[0].z, _73.cameraView[1].z, _73.cameraView[2].z) * (-1.0);
    float simFrame = _102.timeInfo.x + _102.timeInfo.w;
    out.gl_ClipDistance[0] = 1.0;
    NativeCUSOriginalMain(cameraPos, _73, simFrame, _102, _297, in.instData, in.bonesInfo, _473, in.normal, _553, _566, out.gl_Position, out.gl_ClipDistance, in.pos, in.uv, in.T, in.B);
    if ((_566.NativeCUSClipFlags & 1u) != 0u)
    {
        out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;
    }
    if ((_566.NativeCUSClipFlags & 2u) != 0u)
    {
        out.gl_Position.y = -out.gl_Position.y;
    }
    if ((_566.NativeCUSClipFlags & 4u) == 0u)
    {
        out.gl_ClipDistance[0] = 1.0;
    }
    out.m_553_pieceVertexPosOrig = _553.pieceVertexPosOrig;
    out.m_553_worldVertexPos = _553.worldVertexPos;
    out.m_553_worldTangent_VS = _553.worldTangent_VS;
    out.m_553_worldNormal_VS = _553.worldNormal_VS;
    out.m_553_uvCoords = _553.uvCoords;
    out.m_553_teamCol = _553.teamCol;
    out.m_553_shadowVertexPos = _553.shadowVertexPos;
    out.m_553_aoterm_fogFactor_selfIllumMod_healthFraction = _553.aoterm_fogFactor_selfIllumMod_healthFraction;
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedField CUS_15_fragment_fields[]={
{"NativeCUSImageMasks",NativeCUSUniformScalar::UInt,0,4,1,16,true},
{"NativeCUSSampleCodes",NativeCUSUniformScalar::UInt,16,1,12,16,true},
{"NativeCUSWater0",NativeCUSUniformScalar::Float,208,3,1,12,false},
{"NativeCUSWater1",NativeCUSUniformScalar::Float,224,3,1,12,false},
{"NativeCUSWater2",NativeCUSUniformScalar::Float,240,3,1,12,false},
{"materialIndex",NativeCUSUniformScalar::Int,252,1,1,4,false},
{"gamma",NativeCUSUniformScalar::Float,256,1,1,4,true},
{"baseVertexDisplacement",NativeCUSUniformScalar::Float,260,1,1,4,false},
{"drawPass",NativeCUSUniformScalar::Int,264,1,1,4,false},
{"bitOptions",NativeCUSUniformScalar::Int,268,1,1,4,true},
{"brightnessFactor",NativeCUSUniformScalar::Float,272,1,1,4,true},
};
inline constexpr char CUS_15_fragment_source[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CUSMSL(#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-braces"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

template<typename T, size_t Num>
struct spvUnsafeArray
{
    T elements[Num ? Num : 1];
    
    thread T& operator [] (size_t pos) thread
    {
        return elements[pos];
    }
    constexpr const thread T& operator [] (size_t pos) const thread
    {
        return elements[pos];
    }
    
    device T& operator [] (size_t pos) device
    {
        return elements[pos];
    }
    constexpr const device T& operator [] (size_t pos) const device
    {
        return elements[pos];
    }
    
    constexpr const constant T& operator [] (size_t pos) const constant
    {
        return elements[pos];
    }
    
    threadgroup T& operator [] (size_t pos) threadgroup
    {
        return elements[pos];
    }
    constexpr const threadgroup T& operator [] (size_t pos) const threadgroup
    {
        return elements[pos];
    }
};

template <typename T>
static inline depth2d<T> spvDepthCast(texture2d<T> t)
{
    return reinterpret_cast<thread const depth2d<T> &>(t);
}

template <typename T>
static inline depth2d_array<T> spvDepthCast(texture2d_array<T> t)
{
    return reinterpret_cast<thread const depth2d_array<T> &>(t);
}

template <typename T>
static inline depthcube<T> spvDepthCast(texturecube<T> t)
{
    return reinterpret_cast<thread const depthcube<T> &>(t);
}

template <typename T>
static inline depthcube_array<T> spvDepthCast(texturecube_array<T> t)
{
    return reinterpret_cast<thread const depthcube_array<T> &>(t);
}

struct UniformParamsBuffer
{
    packed_float3 rndVec3;
    uint renderCaps;
    float4 timeInfo;
    float4 viewGeometry;
    float4 mapSize;
    float4 mapHeight;
    float4 fogColor;
    float4 fogParams;
    float4 sunDir;
    float4 sunAmbientModel;
    float4 sunAmbientMap;
    float4 sunDiffuseModel;
    float4 sunDiffuseMap;
    float4 sunSpecularModel;
    float4 sunSpecularMap;
    float4 shadowDensity;
    float4 windInfo;
    float2 mouseScreenPos;
    uint mouseStatus;
    uint mouseUnused;
    float4 mouseWorldPos;
    float4 teamColor[255];
};

struct NativeCUSDrawUniforms
{
    uint4 NativeCUSImageMasks;
    uint4 NativeCUSSampleCodes[12];
    float3 NativeCUSWater0;
    float3 NativeCUSWater1;
    packed_float3 NativeCUSWater2;
    int materialIndex;
    float gamma;
    float baseVertexDisplacement;
    int drawPass;
    int bitOptions;
    float brightnessFactor;
};

struct Data
{
    float4 pieceVertexPosOrig;
    float4 worldVertexPos;
    float3 worldTangent_VS;
    float3 worldNormal_VS;
    float4 uvCoords;
    float4 teamCol;
    float4 shadowVertexPos;
    float4 aoterm_fogFactor_selfIllumMod_healthFraction;
};

struct ShadowQuality
{
    float samplingRandomness;
    float samplingDistance;
    int shadowSamples;
};

struct UniformMatrixBuffer
{
    float4x4 screenView;
    float4x4 screenProj;
    float4x4 screenViewProj;
    float4x4 cameraView;
    float4x4 cameraProj;
    float4x4 cameraViewProj;
    float4x4 cameraBillboardView;
    float4x4 cameraViewInv;
    float4x4 cameraProjInv;
    float4x4 cameraViewProjInv;
    float4x4 shadowView;
    float4x4 shadowProj;
    float4x4 shadowViewProj;
    float4x4 reflectionView;
    float4x4 reflectionProj;
    float4x4 reflectionViewProj;
    float4x4 orthoProj01;
    float4x4 mmDrawView;
    float4x4 mmDrawProj;
    float4x4 mmDrawViewProj;
    float4x4 mmDrawIMMView;
    float4x4 mmDrawIMMProj;
    float4x4 mmDrawIMMViewProj;
    float4x4 mmDrawDimView;
    float4x4 mmDrawDimProj;
    float4x4 mmDrawDimViewProj;
};

constant spvUnsafeArray<float, 8> _181 = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
constant spvUnsafeArray<ShadowQuality, 4> _447 = spvUnsafeArray<ShadowQuality, 4>({ ShadowQuality{ 0.0, 0.0, 1 }, ShadowQuality{ 1.0, 1.0, 3 }, ShadowQuality{ 0.4000000059604644775390625, 2.0, 6 }, ShadowQuality{ 0.4000000059604644775390625, 3.0, 8 } });

struct NativeCUSFragment_out
{
    float4 fragData_0 [[color(0)]];
};

struct NativeCUSFragment_in
{
    float4 m_426_pieceVertexPosOrig [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_worldVertexPos [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldTangent_VS [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
    float3 m_426_worldNormal_VS [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_uvCoords [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_teamCol [[user(locn5), flat]];
    float4 m_426_shadowVertexPos [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
    float4 m_426_aoterm_fogFactor_selfIllumMod_healthFraction [[user(locn7), RECOIL_SAMPLE_PERSPECTIVE]];
};

static inline __attribute__((always_inline))
bool NativeCUSEmpty(thread const int& slot, constant NativeCUSDrawUniforms& _187)
{
    return (_187.NativeCUSImageMasks.x & (1u << uint(slot))) != 0u;
}

static inline __attribute__((always_inline))
float2 NativeCUSUV(thread const int& slot, thread float2& uv, constant NativeCUSDrawUniforms& _187)
{
    if ((_187.NativeCUSImageMasks.y & (1u << uint(slot))) != 0u)
    {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

static inline __attribute__((always_inline))
float NativeCUSChannel(thread const float4& value, thread const uint& channel)
{
    if (channel < 4u)
    {
        return value[int(channel)];
    }
    return float(channel == 5u);
}

static inline __attribute__((always_inline))
float4 NativeCUSChannels(thread const int& slot, thread float4& value, constant NativeCUSDrawUniforms& _187)
{
    uint code = _187.NativeCUSSampleCodes[slot].x;
    if ((_187.NativeCUSImageMasks.z & (1u << uint(slot))) != 0u)
    {
        uint mode = code >> 16u;
        float4 _253;
        if (mode == 0u)
        {
            _253 = float4(value.xxx, 1.0);
        }
        else
        {
            float4 _265;
            if (mode == 1u)
            {
                _265 = float4(value.x);
            }
            else
            {
                _265 = float4(0.0, 0.0, 0.0, value.x);
            }
            _253 = _265;
        }
        value = _253;
    }
    float4 param = value;
    uint param_1 = code & 15u;
    float4 param_2 = value;
    uint param_3 = (code >> 4u) & 15u;
    float4 param_4 = value;
    uint param_5 = (code >> 8u) & 15u;
    float4 param_6 = value;
    uint param_7 = (code >> 12u) & 15u;
    return float4(NativeCUSChannel(param, param_1), NativeCUSChannel(param_2, param_3), NativeCUSChannel(param_4, param_5), NativeCUSChannel(param_6, param_7));
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _320 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _320);
    float4 _325 = NativeCUSChannels(param_3, param_4, _187);
    return _325;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _339 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _339, bias(bias0));
    float4 _345 = NativeCUSChannels(param_3, param_4, _187);
    return _345;
}

static inline __attribute__((always_inline))
float3 SRGBtoLINEAR(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    return powr(c, float3(_187.gamma));
}

static inline __attribute__((always_inline))
float AdjustRoughnessByNormalMap(thread const float& roughness, thread const float3& normal)
{
    float nlen2 = dot(normal, normal);
    if (nlen2 < 1.0)
    {
        float nlen = sqrt(nlen2);
        float kappa = ((3.0 * nlen) - (nlen2 * nlen)) / (1.0 - nlen2);
        return fast::min(1.0, sqrt((roughness * roughness) + (1.0 / kappa)));
    }
    return roughness;
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod)
{
    return int2(image.get_width(lod), image.get_height(lod));
}

static inline __attribute__((always_inline))
float NativeCUSTexture(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread float3& uv, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    float2 param_1 = uv.xy;
    float2 _369 = NativeCUSUV(param, param_1, _187);
    uv.x = _369.x;
    uv.y = _369.y;
    if ((_187.NativeCUSImageMasks.w & (1u << uint(slot))) != 0u) uv.z = clamp(uv.z, 0.0f, 1.0f);
    return spvDepthCast(image).sample_compare(imageSmplr, uv.xy, uv.z);
}

static inline __attribute__((always_inline))
float GetShadowPCFRandom(thread const float& NdotL, thread int& shadowsQuality, constant NativeCUSDrawUniforms& _187, thread Data& _426, texture2d<float> shadowTex, sampler shadowTexSmplr)
{
    float shadow = 0.0;
    float3 shadowCoord = _426.shadowVertexPos.xyz;
    int presetIndex = clamp(shadowsQuality, 0, 3);
    float samplingRandomness = _447[presetIndex].samplingRandomness;
    float samplingDistance = _447[presetIndex].samplingDistance;
    int shadowSamples = 1;
    if (shadowSamples > 1)
    {
    }
    else
    {
        int param = 6;
        int param_1 = 0;
        float shadowTexelSizeInv = 0.5 / float(NativeCUSSize(shadowTex, shadowTexSmplr, param, param_1).x);
        float bias0 = shadowTexelSizeInv * tan(acos(NdotL));
        bias0 = fast::clamp(bias0, 0.0, 5.0 * shadowTexelSizeInv);
        shadowCoord.z -= bias0;
        int param_2 = 6;
        float3 param_3 = shadowCoord;
        float _492 = NativeCUSTexture(shadowTex, shadowTexSmplr, param_2, param_3, _187);
        shadow = _492;
    }
    return shadow;
}

static inline __attribute__((always_inline))
float4 NativeCUSLod(texture2d<float> image, sampler imageSmplr, thread const int& slot, thread const float2& uv, thread const float& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float2 param_2 = uv;
    float2 _391 = NativeCUSUV(param_1, param_2, _187);
    int param_3 = slot;
    float4 param_4 = image.sample(imageSmplr, _391, level(lod));
    float4 _397 = NativeCUSChannels(param_3, param_4, _187);
    return _397;
}

static inline __attribute__((always_inline))
float3 FresnelSchlick(thread const float3& R0, thread const float3& R90, thread const float& VdotH)
{
    return R0 + ((R90 - R0) * powr(fast::clamp(1.0 - VdotH, 0.0, 1.0), 5.0));
}

static inline __attribute__((always_inline))
float VisibilityOcclusionFast(thread const float& NdotL, thread const float& NdotV, thread const float& roughness2)
{
    float GGXV = NdotL * ((NdotV * (1.0 - roughness2)) + roughness2);
    float GGXL = NdotV * ((NdotL * (1.0 - roughness2)) + roughness2);
    float GGX = GGXV + GGXL;
    return mix(0.0, 0.5 / GGX, float(GGX > 0.0));
}

static inline __attribute__((always_inline))
float MicrofacetDistribution(thread const float& NdotH, thread const float& roughness4)
{
    float f = (((NdotH * roughness4) - NdotH) * NdotH) + 1.0;
    return roughness4 / (f * f);
}

static inline __attribute__((always_inline))
float3 FresnelSchlickWithRoughness(thread const float3& R0, thread const float3& R90, thread const float& VdotH, thread const float& roughness)
{
    return R0 + ((fast::max(R90 - float3(roughness), R0) - R0) * powr(1.0 - VdotH, 5.0));
}

static inline __attribute__((always_inline))
int2 NativeCUSSize(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const int& lod, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    int2 _404;
    if (NativeCUSEmpty(param, _187))
    {
        _404 = int2(0);
    }
    else
    {
        _404 = int2(image.get_width(lod), image.get_height(lod));
    }
    return _404;
}

static inline __attribute__((always_inline))
float4 NativeCUSTexture(texturecube<float> image, sampler imageSmplr, thread const int& slot, thread const float3& uv, thread const float& bias0, constant NativeCUSDrawUniforms& _187)
{
    int param = slot;
    if (NativeCUSEmpty(param, _187))
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    int param_1 = slot;
    float4 param_2 = image.sample(imageSmplr, uv, bias(bias0));
    float4 _361 = NativeCUSChannels(param_1, param_2, _187);
    return _361;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMapMod(thread const float3& colorIn, constant NativeCUSDrawUniforms& _187)
{
    float3 param = colorIn;
    float3 color = SRGBtoLINEAR(param, _187);
    color *= (1.0 + (0.4000000059604644775390625 * smoothstep(0.550000011920928955078125, 1.0, dot(float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875), color))));
    return color;
}

static inline __attribute__((always_inline))
float3 SampleReflectionMap(thread const float3& sp, thread const float& lodBias, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    float3 param_1 = sp;
    float param_2 = lodBias;
    float3 param_3 = NativeCUSTexture(reflectTex, reflectTexSmplr, param, param_1, param_2, _187).xyz;
    return SampleReflectionMapMod(param_3, _187);
}

static inline __attribute__((always_inline))
float3 SampleEnvironmentWithRoughness(thread const float3& samplingVec, thread const float& roughness, constant NativeCUSDrawUniforms& _187, texturecube<float> reflectTex, sampler reflectTexSmplr)
{
    int param = 7;
    int param_1 = 0;
    float maxLodLevel = log2(float(NativeCUSSize(reflectTex, reflectTexSmplr, param, param_1, _187).x));
    maxLodLevel -= 4.0;
    float lodBias = maxLodLevel * roughness;
    float3 param_2 = samplingVec;
    float param_3 = lodBias;
    return SampleReflectionMap(param_2, param_3, _187, reflectTex, reflectTexSmplr);
}

static inline __attribute__((always_inline))
float ComputeSpecularAOFilament(thread const float& NoV, thread const float& diffuseAO, thread const float& roughness2)
{
    return fast::clamp((powr(NoV + diffuseAO, exp2(((-16.0) * roughness2) - 1.0)) - 1.0) + diffuseAO, 0.0, 1.0);
}

static inline __attribute__((always_inline))
float3 LINEARtoSRGB(thread const float3& c, constant NativeCUSDrawUniforms& _187)
{
    if (_187.gamma == 1.0)
    {
        return c;
    }
    float invGamma = 1.0 / _187.gamma;
    return powr(c, float3(invGamma));
}

static inline __attribute__((always_inline))
float3 CustomTM(float3 x, thread spvUnsafeArray<float, 8>& pbrParams, constant NativeCUSDrawUniforms& _187)
{
    float3 param = (x * ((x * pbrParams[0]) + float3(pbrParams[1]))) / ((x * ((x * pbrParams[2]) + float3(pbrParams[3]))) + float3(pbrParams[4]));
    return LINEARtoSRGB(param, _187);
}

fragment NativeCUSFragment_out NativeCUSFragment(NativeCUSFragment_in in [[stage_in]], constant UniformMatrixBuffer& _1017 [[buffer(2)]], constant UniformParamsBuffer& _162 [[buffer(3)]], constant NativeCUSDrawUniforms& _187 [[buffer(7)]], texture2d<float> texture1 [[texture(0)]], texture2d<float> texture2 [[texture(1)]], texture2d<float> normalTex [[texture(2)]], texture2d<float> shadowTex [[texture(6)]], texturecube<float> reflectTex [[texture(7)]], texture2d<float> losMapTex [[texture(8)]], texture2d<float> brdfLUT [[texture(9)]], sampler texture1Smplr [[sampler(0)]], sampler texture2Smplr [[sampler(1)]], sampler normalTexSmplr [[sampler(2)]], sampler shadowTexSmplr [[sampler(6)]], sampler reflectTexSmplr [[sampler(7)]], sampler losMapTexSmplr [[sampler(8)]], sampler brdfLUTSmplr [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
    NativeCUSFragment_out out = {};
    spvUnsafeArray<float4, 1> fragData = {};
    Data _426 = {};
    _426.pieceVertexPosOrig = in.m_426_pieceVertexPosOrig;
    _426.worldVertexPos = in.m_426_worldVertexPos;
    _426.worldTangent_VS = in.m_426_worldTangent_VS;
    _426.worldNormal_VS = in.m_426_worldNormal_VS;
    _426.uvCoords = in.m_426_uvCoords;
    _426.teamCol = in.m_426_teamCol;
    _426.shadowVertexPos = in.m_426_shadowVertexPos;
    _426.aoterm_fogFactor_selfIllumMod_healthFraction = in.m_426_aoterm_fogFactor_selfIllumMod_healthFraction;
    int shadowsQuality = 0;
    float simFrame = _162.timeInfo.z * 30.0;
    float textureLODBias = -0.5;
    spvUnsafeArray<float, 8> pbrParams = spvUnsafeArray<float, 8>({ 4.849999904632568359375, 0.85000002384185791015625, 3.5, 0.85000002384185791015625, 1.0, 0.23000000417232513427734375, 1.0, 1.0 });
    float2 myUV = _426.uvCoords.xy;
    float unitID = floor(_426.uvCoords.z + 0.5);
    float3 worldNormal = fast::normalize(_426.worldNormal_VS);
    float3 worldTangent = fast::normalize(_426.worldTangent_VS);
    float3 worldBitangent = fast::normalize(cross(worldNormal, worldTangent));
    float3x3 worldTBN = float3x3(float3(worldTangent), float3(worldBitangent), float3(worldNormal));
    bool _777 = (uint(_187.bitOptions) & 2u) != 0u;
    bool _787;
    if (!_777)
    {
        _787 = (uint(_187.bitOptions) & 512u) != 0u;
    }
    else
    {
        _787 = _777;
    }
    float4 normalTexVal;
    if (_787)
    {
        int param = 2;
        float2 param_1 = myUV;
        normalTexVal = NativeCUSTexture(normalTex, normalTexSmplr, param, param_1, _187);
    }
    float healthMix = 0.0;
    float3 tbnNormal;
    if ((uint(_187.bitOptions) & 2u) != 0u)
    {
        tbnNormal = (normalTexVal.xyz * 2.0) - float3(1.0);
        tbnNormal = fast::normalize(tbnNormal);
    }
    else
    {
        tbnNormal = float3(0.0, 0.0, 1.0);
    }
    int param_2 = 0;
    float2 param_3 = myUV;
    float param_4 = textureLODBias;
    float4 texColor1 = NativeCUSTexture(texture1, texture1Smplr, param_2, param_3, param_4, _187);
    int param_5 = 1;
    float2 param_6 = myUV;
    float param_7 = textureLODBias;
    float4 texColor2 = NativeCUSTexture(texture2, texture2Smplr, param_5, param_6, param_7, _187);
    float funitID = unitID;
    float3 randluma = (float3(0.00999999977648258209228515625, 0.0130000002682209014892578125, 0.0170000009238719940185546875) * funitID) - float3(0.5);
    float saturation = fast::clamp(dot(abs(texColor1.xyz - texColor1.yzx), float3(1.0)), 0.0, 1.0);
    randluma = (fract(randluma) * saturation) + float3(1.0);
    float4 _858 = texColor1;
    float3 _860 = _858.xyz * randluma;
    texColor1.x = _860.x;
    texColor1.y = _860.y;
    texColor1.z = _860.z;
    float4 teamColor = _426.teamCol;
    float3 param_8 = mix(texColor1.xyz, teamColor.xyz, float3(texColor1.w));
    float3 albedoColor = SRGBtoLINEAR(param_8, _187);
    if ((uint(_187.bitOptions) & 512u) != 0u)
    {
        float texHeight = normalTexVal.w;
        float healthyness = fast::clamp((healthMix * 2.0) - 0.5, 0.0, 1.0);
        if (texHeight < healthyness)
        {
            float bloodRedDeepness = fast::clamp((healthyness - texHeight) * 8.0, 0.0, 1.0);
            tbnNormal = mix(tbnNormal, float3(0.0, 0.0, 1.0), float3(bloodRedDeepness));
            texColor2.y = 0.20000000298023223876953125;
            texColor2.z *= 0.20000000298023223876953125;
            albedoColor = mix(float3(0.4000000059604644775390625, 0.0, 0.00999999977648258209228515625), float3(0.119999997317790985107421875, 0.0, 0.0), float3(bloodRedDeepness));
        }
    }
    float3 N = fast::normalize(worldTBN * tbnNormal);
    float emissiveness = texColor2.x;
    emissiveness = fast::clamp(_426.aoterm_fogFactor_selfIllumMod_healthFraction.z * emissiveness, 0.0, 1.0);
    float metalness = texColor2.y;
    metalness = fast::clamp(metalness, 0.0, 1.0);
    float roughness = texColor2.z;
    if ((uint(_187.bitOptions) & 4096u) != 0u)
    {
        emissiveness = 0.20000000298023223876953125;
        roughness = 0.800000011920928955078125;
        metalness = 0.100000001490116119384765625;
    }
    float charAmount = fast::clamp(1.0 - _426.aoterm_fogFactor_selfIllumMod_healthFraction.w, 0.0, 1.0);
    charAmount *= step(0.5, simFrame);
    if (charAmount > 0.00200000009499490261077880859375)
    {
        albedoColor = mix(albedoColor, float3(0.017999999225139617919921875, 0.014999999664723873138427734375, 0.0130000002682209014892578125), float3(charAmount));
        roughness = fast::max(roughness, mix(roughness, 0.89999997615814208984375, charAmount));
        metalness = fast::min(metalness, mix(metalness, 0.0, charAmount));
    }
    float param_9 = roughness;
    float3 param_10 = tbnNormal;
    roughness = mix(roughness, AdjustRoughnessByNormalMap(param_9, param_10), 1.0);
    roughness = fast::clamp(roughness, 0.039999999105930328369140625, 1.0);
    float roughness2 = roughness * roughness;
    float roughness4 = roughness2 * roughness2;
    float3 L = fast::normalize(_162.sunDir.xyz);
    float3 V = fast::normalize(_1017.cameraViewInv[3].xyz - _426.worldVertexPos.xyz);
    float3 H = fast::normalize(L + V);
    float3 Rv = -reflect(V, N);
    float NdotLu = dot(N, L);
    float NdotL = fast::clamp(NdotLu, 0.0, 1.0);
    float NdotH = fast::clamp(dot(H, N), 0.0, 1.0);
    float NdotV = fast::clamp(dot(N, V), 9.9999997473787516355514526367188e-05, 1.0);
    float VdotH = fast::clamp(dot(V, H), 0.0, 1.0);
    float gShadow = 1.0;
    float nShadow = smoothstep(0.0, 0.3499999940395355224609375, NdotLu);
    if ((uint(_187.bitOptions) & 1u) != 0u)
    {
        float param_11 = NdotL;
        gShadow = GetShadowPCFRandom(param_11, shadowsQuality, _187, _426, shadowTex, shadowTexSmplr);
    }
    float shadowMult = mix(1.0, fast::min(nShadow, gShadow), _162.shadowDensity.y);
    float3 F0 = float3(0.039999999105930328369140625);
    F0 = mix(F0, albedoColor, float3(metalness));
    float reflectance = fast::max(F0.x, fast::max(F0.y, F0.z));
    float3 F90 = float3(fast::clamp(reflectance * 50.0, 0.0, 1.0));
    int param_12 = 9;
    float2 param_13 = float2(NdotV, roughness);
    float param_14 = 0.0;
    float2 envBRDF = NativeCUSLod(brdfLUT, brdfLUTSmplr, param_12, param_13, param_14, _187).xy;
    float3 energyCompensation = fast::clamp(float3(1.0) + (F0 * ((1.0 / fast::max(envBRDF.x, 9.9999997473787516355514526367188e-05)) - 1.0)), float3(1.0), float3(2.0));
    float3 dirContrib = float3(0.0);
    float3 outSpecularColor = float3(0.0);
    if (any(float2(NdotL, NdotV) > float2(9.9999997473787516355514526367188e-05)))
    {
        float3 param_15 = F0;
        float3 param_16 = F90;
        float param_17 = VdotH;
        float3 F = FresnelSchlick(param_15, param_16, param_17);
        float param_18 = NdotL;
        float param_19 = NdotV;
        float param_20 = roughness2;
        float Vis = VisibilityOcclusionFast(param_18, param_19, param_20);
        float param_21 = NdotH;
        float param_22 = roughness4;
        float D = MicrofacetDistribution(param_21, param_22);
        outSpecularColor = (F * Vis) * D;
        float3 maxSun = mix(_162.sunSpecularModel.xyz, _162.sunDiffuseModel.xyz, float3(step(dot(_162.sunSpecularModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)), dot(_162.sunDiffuseModel.xyz, float3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875)))));
        maxSun *= pbrParams[6];
        outSpecularColor *= maxSun;
        outSpecularColor *= (NdotL * shadowMult);
        outSpecularColor *= energyCompensation;
        float3 kD = float3(1.0) - F;
        kD *= (1.0 - metalness);
        dirContrib = ((maxSun * (kD * albedoColor)) * NdotL) * shadowMult;
        dirContrib += outSpecularColor;
    }
    Rv = mix(Rv, N, float3(roughness4));
    float3 iblDiffuse = float3(0.0);
    float3 iblSpecular = float3(0.0);
    float3 specular = float3(0.0);
    float3 param_23 = F0;
    float3 param_24 = F90;
    float param_25 = VdotH;
    float param_26 = roughness;
    float3 F_1 = FresnelSchlickWithRoughness(param_23, param_24, param_25, param_26);
    float3 kD_1 = float3(1.0) - F_1;
    kD_1 *= (1.0 - metalness);
    iblDiffuse = mix(_162.sunAmbientModel.xyz, iblDiffuse, float3(pbrParams[5]));
    float3 diffuse = (iblDiffuse * albedoColor) * _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float3 param_27 = Rv;
    float param_28 = roughness;
    float3 reflectionColor = SampleEnvironmentWithRoughness(param_27, param_28, _187, reflectTex, reflectTexSmplr);
    reflectionColor = mix(reflectionColor, iblSpecular, float3(roughness));
    float param_29 = NdotV;
    float param_30 = _426.aoterm_fogFactor_selfIllumMod_healthFraction.x;
    float param_31 = roughness2;
    float aoTermSpec = ComputeSpecularAOFilament(param_29, param_30, param_31);
    specular = reflectionColor * ((F0 * envBRDF.x) + (F90 * envBRDF.y));
    specular *= (energyCompensation * aoTermSpec);
    outSpecularColor += specular;
    float3 ambientContrib = (kD_1 * diffuse) + specular;
    float3 outColor = ambientContrib + dirContrib;
    outColor += (albedoColor * emissiveness);
    float2 losMapUV = _426.worldVertexPos.xz;
    losMapUV /= _162.mapSize.zw;
    int param_32 = 8;
    float2 param_33 = losMapUV;
    float losValue = NativeCUSTexture(losMapTex, losMapTexSmplr, param_32, param_33, _187).x * 2.0;
    losValue = fast::clamp(losValue, 0.5, 1.0);
    outColor *= losValue;
    outSpecularColor *= losValue;
    emissiveness *= losValue;
    float4 debugColor = float4(outColor, 1.0);
    outSpecularColor *= pbrParams[7];
    outColor *= pbrParams[7];
    outColor = CustomTM(outColor, pbrParams, _187);
    if ((uint(_187.bitOptions) & 1024u) != 0u)
    {
        outColor = mix(_162.fogColor.xyz, outColor, float3(_426.aoterm_fogFactor_selfIllumMod_healthFraction.y));
    }
    outColor *= _187.brightnessFactor;
    float selectedness = _426.teamCol.w;
    if (selectedness > 0.25)
    {
        float inselection = float((selectedness == 1.0) || (selectedness == 3.0));
        float mouseovered = float(selectedness > 1.5);
        float allyselected = step(abs(fract(selectedness) - 0.5), 0.25);
        float mouseOverAnimation = fract((_426.worldVertexPos.y * 0.0333333350718021392822265625) + (simFrame * 0.0500000007450580596923828125));
        float4 selectionHighlight = float4(0.0);
        float3 _1423 = fast::clamp(_426.teamCol.xyz, float3(0.64999997615814208984375), float3(1.0));
        selectionHighlight.x = _1423.x;
        selectionHighlight.y = _1423.y;
        selectionHighlight.z = _1423.z;
        float dotcamera = dot(worldNormal, V);
        float highLightOpacity = fast::clamp(1.0 - dotcamera, 0.0, 1.0);
        highLightOpacity *= highLightOpacity;
        float4 mouseOverHighlight = selectionHighlight;
        outColor += ((mouseOverHighlight.xyz * (mouseovered * mouseOverAnimation)) * 0.5);
        outColor += (selectionHighlight.xyz * (inselection * highLightOpacity));
    }
    // Original31657 jammer tint, after selection highlight and before cloak.
    float jammedness = _426.uvCoords.w;
    if (jammedness > 0.001)
    {
        float jammedRim = clamp(1.0 - dot(worldNormal, V), 0.0, 1.0);
        jammedRim *= jammedRim;
        float brightness = max(outColor.r, max(outColor.g, outColor.b));
        float3 jammedColor = mix(float3(1.0, 0.25, 0.18), float3(0.45, 0.02, 0.0), float3(smoothstep(0.35, 0.7, brightness)));
        outColor = mix(outColor, jammedColor, float3(jammedness * min(0.09 + 0.25 * jammedRim, 1.0)));
        float jammedEdge = jammedRim * jammedRim * jammedRim;
        outColor = mix(outColor, jammedColor, float3(jammedness * 0.33 * jammedEdge));
    }
    fragData[0] = float4(outColor, texColor2.w);
    if (texColor2.w < 0.5)
    {
        discard_fragment();
    }
    out.fragData_0 = fragData[0];
    return out;
}

)CUSMSL";
inline constexpr NativeCUSGeneratedDefault CUS_15_defaults[]={{"drawPass",5124,1,true,{1.0f,0.0f,0.0f,0.0f}},
{"featureFadeDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"featureDrawDistSq",5126,1,false,{0.0f,0.0f,0.0f,0.0f}},
{"bitOptions",5124,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"clipPlane0",35666,4,false,{0.0f,0.0f,0.0f,1.0f}},
{"texture1",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"texture2",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"normalTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"shadowTex",35682,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"reflectTex",35680,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"losMapTex",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"brdfLUT",35678,1,true,{0.0f,0.0f,0.0f,0.0f}},
{"gamma",5126,1,false,{1.0f,0.0f,0.0f,0.0f}},
{"brightnessFactor",5126,1,false,{1.5f,0.0f,0.0f,0.0f}}};
// Reviewed official31691 fragment semantics, derived from the byte-pinned
// 31657 translation. Official GLSL changes only JAMMED_FILL .09->.08,
// JAMMED_RIM .25->.22 and JAMMED_EDGE .33->.3. 0.30 is the same float literal.
// Each exact source site is compile-time checked; no runtime source editing.
// Vertex code, shadow code, all bindings/defaults/ABI and every old consumer remain.
template<size_t N>
consteval std::array<char,N> CUS31691JammedFragment(const char (&original)[N],
 size_t fillSite,size_t edgeSite) {
 constexpr std::string_view fill="jammedness * min(0.09 + 0.25 * jammedRim, 1.0)";
 constexpr std::string_view edge="jammedness * 0.33 * jammedEdge";
 if(std::string_view(original,N-1).substr(fillSite,fill.size())!=fill ||
    std::string_view(original,N-1).substr(edgeSite,edge.size())!=edge)
  throw "CUS31691 jammed translation source sites differ";
 std::array<char,N> result{};
 for(size_t i=0;i<N;++i)result[i]=original[i];
 result[fillSite+20]='8'; // 0.09 -> 0.08
 result[fillSite+27]='2'; // 0.25 -> 0.22
 result[edgeSite+16]='0'; // 0.33 -> 0.30
 return result;
}
inline constexpr auto CUS31691_0_fragment_source=CUS31691JammedFragment(CUS_0_fragment_source,35001,35166);
inline constexpr auto CUS31691_2_fragment_source=CUS31691JammedFragment(CUS_2_fragment_source,31310,31475);
inline constexpr auto CUS31691_3_fragment_source=CUS31691JammedFragment(CUS_3_fragment_source,35001,35166);
inline constexpr auto CUS31691_4_fragment_source=CUS31691JammedFragment(CUS_4_fragment_source,35001,35166);
inline constexpr auto CUS31691_6_fragment_source=CUS31691JammedFragment(CUS_6_fragment_source,31310,31475);
inline constexpr auto CUS31691_7_fragment_source=CUS31691JammedFragment(CUS_7_fragment_source,35001,35166);
inline constexpr auto CUS31691_8_fragment_source=CUS31691JammedFragment(CUS_8_fragment_source,28262,28427);
inline constexpr auto CUS31691_10_fragment_source=CUS31691JammedFragment(CUS_10_fragment_source,25760,25925);
inline constexpr auto CUS31691_11_fragment_source=CUS31691JammedFragment(CUS_11_fragment_source,28262,28427);
inline constexpr auto CUS31691_12_fragment_source=CUS31691JammedFragment(CUS_12_fragment_source,29302,29467);
inline constexpr auto CUS31691_14_fragment_source=CUS31691JammedFragment(CUS_14_fragment_source,26798,26963);
inline constexpr auto CUS31691_15_fragment_source=CUS31691JammedFragment(CUS_15_fragment_source,29302,29467);
inline constexpr NativeCUSGeneratedProgram NativeCUSGeneratedPrograms[]={{"unit",1,"89db431382839823bd2bdb256d548503127591d5d76aa098b1d1f3bb23ca1985c2ca3fddd4c9c53050b91d739614dea200017d8c62dd160cb5c0faed29dbe62d","f2ba63e42481b52a8010c278a3cd225a9d9af94fca53c03d55d42974019e89194b14a8c507ab1f6cbfae74d9728516632447368ec7750797c75464915c475b43",
{CUS_0_vertex_source,CUS_0_vertex_fields,84,0},{CUS_0_fragment_source,CUS_0_fragment_fields,276,1791},CUS_0_defaults},
{"unit",16,"aeed23dc459bb6bf7a3273bd9d348af2139c86d0f81786ac0889dcb6a0c41a89b6d5e4df20a4b7ce759eb1943483dd7e81ce762b1973c72d01d744eb8423103f","2f33f7b6ecc61793133daaec19c13f128c410c1a0481e97a1dc911db332911610de054c10c96d74b5e1b1fb6a97ac542105e9842a93edf5bb3e76b8a4ce22513",
{CUS_1_vertex_source,CUS_1_vertex_fields,84,0},{CUS_1_fragment_source,CUS_1_fragment_fields,276,0},CUS_1_defaults},
{"unit",0,"acb86b46f884bf5433b8b0fd391fbdcea376e66e257236bb6869e7b96514f9d8bdfe613c2f84c9c224ce224bafe90c12e3c27a8722af42031930bd9c9e5198d7","a0c221f3568f9fb2836e3069e3afbc97b41da96b1694f43243fa769c85fe436796d7d4e7a12f274aca8f0a3975379166c0fe8f92c106306581b2d164112b59e6",
{CUS_2_vertex_source,CUS_2_vertex_fields,84,0},{CUS_2_fragment_source,CUS_2_fragment_fields,276,1639},CUS_2_defaults},
{"unit",5,"9367cbc33730e7c90f155532ac2c07f759ece8cc172bdaaef0a75850eb65733f0e45b425fc38ccd9541d866d4522b44c4f7f6c6665b92ea61ac2f191d78e47db","e951522706a3d46c6a079d4ff907969504db1be45a5a26f624759251cb9c7dae755b9e613f4197439e59361f487d4d16fb676e1e26a8d6411a3ccfc204ecdd14",
{CUS_3_vertex_source,CUS_3_vertex_fields,84,0},{CUS_3_fragment_source,CUS_3_fragment_fields,276,1791},CUS_3_defaults},
{"unitskinning",1,"0491345aeed16a10efef1df8d69c8d5cd399e14aea873c3360b25b84936e63d2c8fdbb14fbe4c6fdb9140cf09d068b39674c9b8b1545cee115750ab3b04902cd","90977d961ac27de51862736df99c5a21278e527e4c48b2bd60efbaaa2cd130f420fac60881fcae53704feebd26b64edbb4f92171df2a64c0953adf6342d43c10",
{CUS_4_vertex_source,CUS_4_vertex_fields,84,0},{CUS_4_fragment_source,CUS_4_fragment_fields,276,1791},CUS_4_defaults},
{"unitskinning",16,"bfb33857ea755b2620eb000b07e856a1cdc2dc35be053e4f4d4cfd695e523dbd01b8591200e95620256e0beed045a77168e98e649bc516675f87a12b02b3f2a0","0a3aae592597ac8ee3f1206b3674f35c63de08f29ae609f8c7f0361b834384788f278650091ad15d8b5b535d5c7e5829d588d138ced93807f94ebf8b77ec42b4",
{CUS_5_vertex_source,CUS_5_vertex_fields,84,0},{CUS_5_fragment_source,CUS_5_fragment_fields,276,0},CUS_5_defaults},
{"unitskinning",0,"8ad2b46d6b304d40df361f745309213a3ee11f170c295241a763163d5047c9345f203dc587be712cfb520838227b3e8d96650fb2593dbe47917288f96715dce3","709acc41ff2043e45f089c9e2dbce84f5af3e08a7063cf4578edbe10d3223dbc3a162606b68e2758696ba7c3e838f8faf6c0961da399e0b039a4d43e9dc9a673",
{CUS_6_vertex_source,CUS_6_vertex_fields,84,0},{CUS_6_fragment_source,CUS_6_fragment_fields,276,1639},CUS_6_defaults},
{"unitskinning",5,"9b6aa2b472827e7bf8db844ffe9d5e855d20cc94facf7ebe3bab4001c773ff90e535ccd506a50f91a9a66dd6170c2387c5176cbeb1c7454e97ad825e1eb833c4","528b89c3b6aa5054d83f0a5c42d592cfdb7896b3191e0e573bd1d671f52f7753256e180955dc1d25143a1c0bb07fe1a7b2b916abeae5ea154f0fa3a36b7ec13c",
{CUS_7_vertex_source,CUS_7_vertex_fields,84,0},{CUS_7_fragment_source,CUS_7_fragment_fields,276,1791},CUS_7_defaults},
{"feature",1,"ed68cc135c7a536f8d4c0e884e3e6962859c63e87b56dc87a562bd2c0046c38c3ce237fb81d5c7ae4163f15871b9151d2caa0f36d602f2f65e0bd9285ec5ee64","126ff82e50e286bc8c4d18d42bf924fcfe47124335727f191026ebbcc6ec7d8d01b027f1cc2ee4cb6d4c4edd724221912667be1a3cfe3dcbd2ebbd397ed3e2cd",
{CUS_8_vertex_source,CUS_8_vertex_fields,84,0},{CUS_8_fragment_source,CUS_8_fragment_fields,276,967},CUS_8_defaults},
{"feature",16,"0510767521e0bdd9bb5d15bde70370736bb33d8ea6e53915a460aa4b95705dd12e9897e2fcf9e3cc151e9f39ecee644c6c3dbe9811c49812f44e8bba0f9b995c","b7b4b298e72f4daaa7db1a923be1580dd555f6e7f3e97d46963b59d49fd0704013e8e82339197016485e7330dc231858c5e150034342c915e182e8608d82e1e8",
{CUS_9_vertex_source,CUS_9_vertex_fields,84,0},{CUS_9_fragment_source,CUS_9_fragment_fields,276,0},CUS_9_defaults},
{"feature",0,"f3e14e597737943db1e4942db4b397db551d8173d6b663de735ff28a778f17db5edcf4790ca747240229ff1094a6ec274f99cfbcc326edc821a2e20218ded8f0","61f4890c2f06b72e12a3b534ceb2c970116cd4fa83b3d41c305bc9e8fbc2b79d67b4aedac866788fcff786951094022367586a392aadc9978cea3c2510c40b88",
{CUS_10_vertex_source,CUS_10_vertex_fields,84,0},{CUS_10_fragment_source,CUS_10_fragment_fields,276,839},CUS_10_defaults},
{"feature",5,"b035c5140c4dfa1261a6df88490ddbe27ab75704c574beacf4a5d651496c01b8057e769049cc0a968ad64e2617da730b8b93cd10bab1f2b179f089388c15b4b3","3341e5d707e10b9f52b2640a17480db43253ffb4daeba50a07fbecd55b8b1718d3cdbfb4f8cca173c1070b2abfe59c36d0f4be9dc38b886e7a67fc82c46bdcca",
{CUS_11_vertex_source,CUS_11_vertex_fields,84,0},{CUS_11_fragment_source,CUS_11_fragment_fields,276,967},CUS_11_defaults},
{"tree",1,"ad789ac6877ca54c3d7cba74a05ff18384e34c1dcbda7c2df60f9f8071cf399e51bf3ae8c810730f4ee1c4196759f6d451358be8571e003416fa216c92ff06df","da96e8c081c1f68a0feee2344074a780a53ba069daf9feb52fe881e08d102dd61f3970dade384159c6cdf1ffe6d9341ce11c9c09e7ee2b20c5dff48a23c44baf",
{CUS_12_vertex_source,CUS_12_vertex_fields,84,0},{CUS_12_fragment_source,CUS_12_fragment_fields,276,967},CUS_12_defaults},
{"tree",16,"63b15e596432fcd667f456ca3dbd02ba6f8719043edbac377921acd15a15db6bff0351fc45b432c026f94cc6565caa20c9931ce59aa9fecfcdeb428b10e2f405","6c6b1d59118b5ffeff492780d467c8eb137846a1c5859f25e11fb6e8f0b17302bd91c856b0db9b2aa5b5e00561d75ffb7a672d87c73330db0a51f33a513ea0f2",
{CUS_13_vertex_source,CUS_13_vertex_fields,84,0},{CUS_13_fragment_source,CUS_13_fragment_fields,276,2},CUS_13_defaults},
{"tree",0,"f30ceb6ac492e4953f2baa256afd64227e92c729660452d5ad024a020b279ea23252d58e4d4deb69649c5eca92565f9cda3026dc4044b2fd5b35af542a97d598","7ac5b2dac04335b4b0181eac7cdd2b3bfe06320e100c2bc02a75a83e063f2fdf66116e8b992301083589e39307125569d3816cdc2e1cd256e0547e0d147ec08a",
{CUS_14_vertex_source,CUS_14_vertex_fields,84,0},{CUS_14_fragment_source,CUS_14_fragment_fields,276,839},CUS_14_defaults},
{"tree",5,"4e98cf73c989c6ab51270007e08200e8e87e84c5f2102fd206bff369fcec2e305a6c371534ceb1cc18fbf759ee2cd90d538c8682b1f57ae0d7db0430e68f725a","26e5de6a4399cf48c2a1dad9b620d281fcd1b0caf6538a369c02d894c826f98c0fdb9da5a1f10036c10d5d115a12f87fc8ad5def902daa431c355d629fc0d009",
{CUS_15_vertex_source,CUS_15_vertex_fields,84,0},{CUS_15_fragment_source,CUS_15_fragment_fields,276,967},CUS_15_defaults},
 // The native 0/0 shadow variants retain the same source calculations.
 // SUPPORT_CLIP_CONTROL is unused by the original CUS stages; depth layout
 // only declares unchanged depth. These consumers write raster depth and
 // transport the actual renderCaps clip convention through NativeCUSClipFlags.
 {"unit",16,"7f63d4c63ddef5a767261c505c48fe8007d178a669984e71290bf6f86042c0dbf94a880ea3fac8385586335157dcaee866ed57151511562f884473bf4a9f2e6b","4e8418e16c30313b71d4c642efa5e3f519b0192ab85d7c4a7c78e91ed760e2a29d4bf658f1a39bc99cf2fdb59a12f3cfa793249227ad140f164d390b8f3f6f83",
 {CUS_1_vertex_source,CUS_1_vertex_fields,84,0},{CUS_1_fragment_source,CUS_1_fragment_fields,276,0},CUS_1_defaults},
 {"unitskinning",16,"0729254573e68a63616c851ab267fef9af8a6ed1462fe4640d6a6770be79b65ed449cdeee659c360daf5dbff270fbf7520c0fe3ff3a832ee98e1c314411f2d9d","b4f5ac11448d2544783d4971dd2f69f4e1ff89669a82e6d07b3985b201c79208c496e7a070dbda1d865bad57bf5175a4dd75a3e7feb64bd6805f3016abad1f4e",
 {CUS_5_vertex_source,CUS_5_vertex_fields,84,0},{CUS_5_fragment_source,CUS_5_fragment_fields,276,0},CUS_5_defaults},
 {"feature",16,"287e845f47aae4dbc462705a18f61840a46f794f5d49c4ec6abecd1a67f1547ad48875d74f33eb456ff74a8a795f4daee1bc13a30412ce139dcb54e6b3a16e44","d3255355b5cc2b05c1455d232e77adfcbc41574cfc166a24bc3d62b829db74dd19eec70c03fead50ddc8726f0c90ee0923471d80568ffdc027ccbdf1cd8db3c3",
 {CUS_9_vertex_source,CUS_9_vertex_fields,84,0},{CUS_9_fragment_source,CUS_9_fragment_fields,276,0},CUS_9_defaults},
 {"tree",16,"15cbb20bd798e04fd921ec193db29d398ba7ed4ebfda95ad43bc8d77431170f9e24c97b86eebb768ed793c9832772ec10b95b8f52370843a6e88dd93c283e596","197b9d684b2020d8efc3091b2ebe89b28715aa6ac58f73f1b75bc0a503e3a4592a7040d164b91691ef207f23a3fa96751cf2ba04070e0e3a4b2d7780912f9c37",
 {CUS_13_vertex_source,CUS_13_vertex_fields,84,0},{CUS_13_fragment_source,CUS_13_fragment_fields,276,2},CUS_13_defaults},
 // Exact official31691 stage pairs, including the actual native0/0 shadow variants.
 {"unit",1,"89db431382839823bd2bdb256d548503127591d5d76aa098b1d1f3bb23ca1985c2ca3fddd4c9c53050b91d739614dea200017d8c62dd160cb5c0faed29dbe62d","50dd39947d751289c4beb4f740995737a3128cf555aa207d15f4780732a8a46d556b1325bf3fe3eadfd8296e80ae393dbd0ee152110a3bc884a0381eeea75f11",
{CUS_0_vertex_source,CUS_0_vertex_fields,84,0},{CUS31691_0_fragment_source.data(),CUS_0_fragment_fields,276,1791},CUS_0_defaults},
 {"unit",16,"aeed23dc459bb6bf7a3273bd9d348af2139c86d0f81786ac0889dcb6a0c41a89b6d5e4df20a4b7ce759eb1943483dd7e81ce762b1973c72d01d744eb8423103f","06085256a73cf94b6621c00aca924f334fb11b0c973d8212ed203c2750b2082bbc7505b0c0061fee321a57172bb0eb25785b65d27c71d0c5f372bdff76fb6568",
{CUS_1_vertex_source,CUS_1_vertex_fields,84,0},{CUS_1_fragment_source,CUS_1_fragment_fields,276,0},CUS_1_defaults},
 {"unit",0,"acb86b46f884bf5433b8b0fd391fbdcea376e66e257236bb6869e7b96514f9d8bdfe613c2f84c9c224ce224bafe90c12e3c27a8722af42031930bd9c9e5198d7","44819aba82f26277d1c611ab3bd21324635d2da40ec40eed33abfca3fed1b5604d495eb56194862f3445b0a61484428af78daf123a07bbe4e3da85a87b888109",
{CUS_2_vertex_source,CUS_2_vertex_fields,84,0},{CUS31691_2_fragment_source.data(),CUS_2_fragment_fields,276,1639},CUS_2_defaults},
 {"unit",5,"9367cbc33730e7c90f155532ac2c07f759ece8cc172bdaaef0a75850eb65733f0e45b425fc38ccd9541d866d4522b44c4f7f6c6665b92ea61ac2f191d78e47db","5dda80edc5b61a379556a770e3df64f2e48d0bca91bf45ff1f6f0e142aab010961626a3855b38dee5aaa6ec205ca6b642909e989a0c527e50ff868c75297b3cd",
{CUS_3_vertex_source,CUS_3_vertex_fields,84,0},{CUS31691_3_fragment_source.data(),CUS_3_fragment_fields,276,1791},CUS_3_defaults},
 {"unitskinning",1,"0491345aeed16a10efef1df8d69c8d5cd399e14aea873c3360b25b84936e63d2c8fdbb14fbe4c6fdb9140cf09d068b39674c9b8b1545cee115750ab3b04902cd","dd576a989618b1a5dafcb1a7f90860ae0c5e24c79d1675170756bf6b84253a3fa478a7b7d4b7d32d467543a650c52720b6dd94a5f6f57da5fbe5b1f0bfa4685d",
{CUS_4_vertex_source,CUS_4_vertex_fields,84,0},{CUS31691_4_fragment_source.data(),CUS_4_fragment_fields,276,1791},CUS_4_defaults},
 {"unitskinning",16,"bfb33857ea755b2620eb000b07e856a1cdc2dc35be053e4f4d4cfd695e523dbd01b8591200e95620256e0beed045a77168e98e649bc516675f87a12b02b3f2a0","2430f89acdf3b4ec6b86b631051b6102153fd6e92d4def1f12b9bbf4399f3bcbfa8b220a9ca0c097001b76bf8d9de1406116ea768228c3bfa68241615642d6d2",
{CUS_5_vertex_source,CUS_5_vertex_fields,84,0},{CUS_5_fragment_source,CUS_5_fragment_fields,276,0},CUS_5_defaults},
 {"unitskinning",0,"8ad2b46d6b304d40df361f745309213a3ee11f170c295241a763163d5047c9345f203dc587be712cfb520838227b3e8d96650fb2593dbe47917288f96715dce3","1880cb607d6314bf7d31c3942bec2608c086d66b5ef2b4d66c6d0b63650165477dc32ecf4ce691665f30ad053ef6947c45c46a9c12070e0ab992353bd7e354cc",
{CUS_6_vertex_source,CUS_6_vertex_fields,84,0},{CUS31691_6_fragment_source.data(),CUS_6_fragment_fields,276,1639},CUS_6_defaults},
 {"unitskinning",5,"9b6aa2b472827e7bf8db844ffe9d5e855d20cc94facf7ebe3bab4001c773ff90e535ccd506a50f91a9a66dd6170c2387c5176cbeb1c7454e97ad825e1eb833c4","6c275372548aba192e72d63702d8c2f5fa97cce3d22c5e1c0637d232c133382d1c9607980143eafd03398e8a48c71f02910c33927fed45d404636a8eb7718357",
{CUS_7_vertex_source,CUS_7_vertex_fields,84,0},{CUS31691_7_fragment_source.data(),CUS_7_fragment_fields,276,1791},CUS_7_defaults},
 {"feature",1,"ed68cc135c7a536f8d4c0e884e3e6962859c63e87b56dc87a562bd2c0046c38c3ce237fb81d5c7ae4163f15871b9151d2caa0f36d602f2f65e0bd9285ec5ee64","d6d5e969aec4cb055556eb3e373f2e96aed9404ac67da75ce594b27c3a9d68fb52748ea42c96960b6204fbfa87b5f8dbe3329d7f50970a936c3ca05080ac084d",
{CUS_8_vertex_source,CUS_8_vertex_fields,84,0},{CUS31691_8_fragment_source.data(),CUS_8_fragment_fields,276,967},CUS_8_defaults},
 {"feature",16,"0510767521e0bdd9bb5d15bde70370736bb33d8ea6e53915a460aa4b95705dd12e9897e2fcf9e3cc151e9f39ecee644c6c3dbe9811c49812f44e8bba0f9b995c","e721b3b6f67f3fbd2134753297ef7c25cee29950e2dabf03857e6a8176be83ed013afc43e3fff94ac80d710bfde41658cbbd99f1d5faed45e698cabef7ccc9cf",
{CUS_9_vertex_source,CUS_9_vertex_fields,84,0},{CUS_9_fragment_source,CUS_9_fragment_fields,276,0},CUS_9_defaults},
 {"feature",0,"f3e14e597737943db1e4942db4b397db551d8173d6b663de735ff28a778f17db5edcf4790ca747240229ff1094a6ec274f99cfbcc326edc821a2e20218ded8f0","d3fdabb59b1d052af78cf1d13714413a0d70edb06992523cb52d57225f63fd38bb523acc7688600484e9c00f93a6c47d8b9e9300fde25d26a8f33da9e38f2a03",
{CUS_10_vertex_source,CUS_10_vertex_fields,84,0},{CUS31691_10_fragment_source.data(),CUS_10_fragment_fields,276,839},CUS_10_defaults},
 {"feature",5,"b035c5140c4dfa1261a6df88490ddbe27ab75704c574beacf4a5d651496c01b8057e769049cc0a968ad64e2617da730b8b93cd10bab1f2b179f089388c15b4b3","9192b4337dae9e2f74d52ed24e163d2f2fa408f14f19a2999d41651e63709f6911e730cfafadf56f5d062c08c67655dc930ae7b731eb56e9517ecb6315d716c6",
{CUS_11_vertex_source,CUS_11_vertex_fields,84,0},{CUS31691_11_fragment_source.data(),CUS_11_fragment_fields,276,967},CUS_11_defaults},
 {"tree",1,"ad789ac6877ca54c3d7cba74a05ff18384e34c1dcbda7c2df60f9f8071cf399e51bf3ae8c810730f4ee1c4196759f6d451358be8571e003416fa216c92ff06df","534a27984939328d8fb8ea95a96fc868e3112fdec4c9b7b3ba7249b15320382a0c90de368da026aed4818e90790f3fd4e2c4975ed8ce931208a276f1d510f372",
{CUS_12_vertex_source,CUS_12_vertex_fields,84,0},{CUS31691_12_fragment_source.data(),CUS_12_fragment_fields,276,967},CUS_12_defaults},
 {"tree",16,"63b15e596432fcd667f456ca3dbd02ba6f8719043edbac377921acd15a15db6bff0351fc45b432c026f94cc6565caa20c9931ce59aa9fecfcdeb428b10e2f405","64429b8331c5e64030337e2226314e8170811a7a963c7abdcf3ce5ce71b1cab59ce3abfbe6ab1347420bc5fe4154d669619223ccc084af49da7c88330ccacc07",
{CUS_13_vertex_source,CUS_13_vertex_fields,84,0},{CUS_13_fragment_source,CUS_13_fragment_fields,276,2},CUS_13_defaults},
 {"tree",0,"f30ceb6ac492e4953f2baa256afd64227e92c729660452d5ad024a020b279ea23252d58e4d4deb69649c5eca92565f9cda3026dc4044b2fd5b35af542a97d598","cf5e9bdfd5994fc221ce5bee438c74e8c4563b4e08737693001b69db3da5902137e30e14f9b044487b3f586f95431d32f7155a15f90525c7b15549eff6b6b610",
{CUS_14_vertex_source,CUS_14_vertex_fields,84,0},{CUS31691_14_fragment_source.data(),CUS_14_fragment_fields,276,839},CUS_14_defaults},
 {"tree",5,"4e98cf73c989c6ab51270007e08200e8e87e84c5f2102fd206bff369fcec2e305a6c371534ceb1cc18fbf759ee2cd90d538c8682b1f57ae0d7db0430e68f725a","5f953747101455498a60610f6e9e3110c8cad921005dad4ecf958564103a5c70c8868ad9cad5f0d796899d506c0db8e4f1b37e800f876d5833fbef212b8572fd",
{CUS_15_vertex_source,CUS_15_vertex_fields,84,0},{CUS31691_15_fragment_source.data(),CUS_15_fragment_fields,276,967},CUS_15_defaults},
 {"unit",16,"7f63d4c63ddef5a767261c505c48fe8007d178a669984e71290bf6f86042c0dbf94a880ea3fac8385586335157dcaee866ed57151511562f884473bf4a9f2e6b","da99c96503b390452d490ffc2f24defa06d2b3542f30c66e7ff682785de0e1bf06e11dac9a620efc274f7e3531d038f3ff8c1352a8e75ee7a0b37b8dabd47b1f",
 {CUS_1_vertex_source,CUS_1_vertex_fields,84,0},{CUS_1_fragment_source,CUS_1_fragment_fields,276,0},CUS_1_defaults},
 {"unitskinning",16,"0729254573e68a63616c851ab267fef9af8a6ed1462fe4640d6a6770be79b65ed449cdeee659c360daf5dbff270fbf7520c0fe3ff3a832ee98e1c314411f2d9d","bfac2ab92d94c710c82975a3fee32ef5dd3cdfef8e2281f73a4888726cf11cf72652ef95d1742a0e1fac57bd5403516f720d7a2666c708a02001f4171b263601",
 {CUS_5_vertex_source,CUS_5_vertex_fields,84,0},{CUS_5_fragment_source,CUS_5_fragment_fields,276,0},CUS_5_defaults},
 {"feature",16,"287e845f47aae4dbc462705a18f61840a46f794f5d49c4ec6abecd1a67f1547ad48875d74f33eb456ff74a8a795f4daee1bc13a30412ce139dcb54e6b3a16e44","b953585b4f673e30d040640a6936318d842df70989eb5e43db83bc8c9cf6a3280835c13988966986ad8c1ea0feadcdf3d7541b9075596a531a53aacebb506cdb",
 {CUS_9_vertex_source,CUS_9_vertex_fields,84,0},{CUS_9_fragment_source,CUS_9_fragment_fields,276,0},CUS_9_defaults},
 {"tree",16,"15cbb20bd798e04fd921ec193db29d398ba7ed4ebfda95ad43bc8d77431170f9e24c97b86eebb768ed793c9832772ec10b95b8f52370843a6e88dd93c283e596","447777176ae758e39f4d17901f887fa1a6f9febb3a89b01002a9345ae78b449329861002673f6e93fcc5775700cc4d5eefcdec09df2c861565ce470bee870c03",
 {CUS_13_vertex_source,CUS_13_vertex_fields,84,0},{CUS_13_fragment_source,CUS_13_fragment_fields,276,2},CUS_13_defaults},
};
} // namespace rendering::metal4
