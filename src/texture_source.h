// texture_source.h  纹理输入抽象层
// 将纹理来源与渲染器解耦 — Renderer 不需要知道纹理来自 WGC/DXGI/视频/图片/摄像头
// 支持 D3D11 GPU 路径 (ID3D11Texture2D) 和 CPU 回读路径 (OpenGL 兼容)
#pragma once
#include <d3d11.h>

// 纹理帧 — 支持 GPU 和 CPU 两条路径
struct TextureFrame {
    // D3D11 路径 (GPU-only, 零拷贝)
    ID3D11Texture2D* d3dTex = nullptr;

    // CPU 回读路径 (OpenGL / 软件处理)
    void* cpuData  = nullptr;
    int   rowPitch = 0;

    int width  = 0;
    int height = 0;
    bool valid = false;
};

// 纹理源类型 (面向未来扩展)
enum class TextureSourceType {
    Desktop_WGC,    // Windows Graphics Capture
    Desktop_DXGI,   // DXGI Desktop Duplication
    Video,          // 视频文件
    Image,          // 静态图片
    Camera,         // 摄像头
    Custom,         // 自定义
};

// 纹理源抽象接口
class ITextureSource {
public:
    virtual ~ITextureSource() = default;

    // 初始化捕获源
    virtual bool Init() = 0;

    // 获取当前帧 (调用者负责 ReleaseFrame)
    virtual TextureFrame AcquireFrame() = 0;

    // 释放帧资源
    virtual void ReleaseFrame(TextureFrame& frame) = 0;

    // 纹理源类型
    virtual TextureSourceType GetType() const = 0;

    // 关闭并释放资源
    virtual void Shutdown() = 0;

    // 共享的 D3D11 设备 — Renderer 可复用此设备, 避免跨设备纹理共享
    ID3D11Device*        d3dDev = nullptr;
    ID3D11DeviceContext* d3dCtx = nullptr;
};
