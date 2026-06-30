// renderer_interface.h  渲染器抽象接口
// 支持多后端: OpenGL / D3D11, 通过编译宏或运行时选择
// Renderer 只接收 TextureFrame + Uniforms, 不关心纹理来源
#pragma once
#include <windows.h>
#include <d3d11.h>
#include "texture_source.h"

// 黑洞 Shader 所需的全部 uniform 参数
// ☆ 布局必须与 HLSL cbuffer Params 精确匹配 (16字节对齐规则) ☆
// HLSL 默认打包: float=4字节对齐, float4=16字节对齐, float[]=4字节对齐
// 因此 float4 前需要确保在 16 字节边界, 必要时添加 _pad 字段
struct BlackHoleUniforms {
    // ── float4 对齐组 (每个 16 字节) ──
    float iResolution[4];            // offset 0   (16 bytes)
    float iTime;                     // offset 16  (4 bytes)
    float _pad0[3];                  // offset 20  → 推到 32
    float iDate[4];                  // offset 32  (16 bytes)
    float iMouse[4];                 // offset 48  (16 bytes)
    float iCurrentCursorColor[4];    // offset 64  (16 bytes)
    float iPreviousCursorColor[4];   // offset 80  (16 bytes)
    float iTimeCursorChange;         // offset 96  (4 bytes)
    float _pad1[3];                  // offset 100 → 推到 112 (保持 16 字节对齐)

    // ── GUI 可调参数 (每个 float, 4 字节对齐) ──
    float holeRadius;                // offset 112 (uHoleRadius)
    float diskGain;                  // offset 116 (uDiskGain)
    float diskTemp;                  // offset 120 (uDiskTemp)
    float exposure;                  // offset 124 (uExposure)
    float speed;                     // offset 128 (uSpeed)
    float starGain;                  // offset 132 (uStarGain)
    float diskIncl;                  // offset 136 (uDiskIncl)
    float _pad2;                     // offset 140 (对齐到 144)
    int   playMode;                  // offset 144
    float slotSec;                   // offset 148
    int   presetCount;               // offset 152
    int   _padEnd;                   // offset 156 (total header = 160 bytes)

    // ── 预设数组 (每个 64*4=256 字节, 4字节对齐) ──
    float presetTemp [64];           // offset 160
    float presetIncl [64];           // offset 416
    float presetRoll [64];           // offset 672
    float presetInner[64];           // offset 928
    float presetOuter[64];           // offset 1184
    float presetOpac [64];           // offset 1440
    float presetDopp [64];           // offset 1696
    float presetBeam [64];           // offset 1952
    float presetGain [64];           // offset 2208
    float presetContr[64];           // offset 2464
    float presetWind [64];           // offset 2720
    float presetSpeed[64];           // offset 2976
    float presetExpo [64];           // offset 3232
    float presetStar [64];           // offset 3488
    // total = 3744 bytes
};

// 渲染器抽象接口
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Init(HWND hwnd, int width, int height,
                      ID3D11Device* sharedDev = nullptr,
                      ID3D11DeviceContext* sharedCtx = nullptr) = 0;

    virtual void Render(const TextureFrame& frame,
                        const BlackHoleUniforms& uniforms) = 0;

    virtual void Resize(int width, int height) = 0;

    virtual void Shutdown() = 0;

    virtual bool IsActive() const = 0;
};

enum class RendererType {
    OpenGL,
    D3D11,
};
