#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "xr/XRMacro.h"
#include "base/FXRType.h"

namespace ffalcon {

struct XRDeviceStateCallback {
    virtual void OnConnected(int64_t id) = 0;
    virtual void OnDisconnect(int64_t id) = 0;
};

class XRDeviceBase {
    PREVENT_COPY_AND_ASSIGN(XRDeviceBase);

protected:
    using Callback = XRDeviceStateCallback;

public:
    XRDeviceBase() = default;

    void SetId(int64_t id) { mId = id; }
    [[nodiscard]] inline int64_t Id() const { return mId; }
    [[nodiscard]] inline bool IsConnected() const { return mIsConnected; }
    void SetStateCallback(Callback* callback) { mCallback = callback; }

    virtual uint64_t Capabilities() { return 0; }
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
    virtual void Open() {}
    virtual void Close() {}

protected:
    int64_t mId = -1;
    Callback* mCallback = nullptr;
    bool mIsConnected = false;
};

class XRDeviceComponent : public XRDeviceBase {
public:
    void SetParent(XRDeviceBase* parent) { mParent = parent; }
    [[nodiscard]] XRDeviceBase* GetParent() const { return mParent; }

    void Connect() override {}
    void Disconnect() override {}
    void Open() override { XRDeviceBase::Open(); }
    void Close() override { XRDeviceBase::Close(); }

protected:
    XRDeviceBase* mParent = nullptr;
};

class XRDeviceEntity : public XRDeviceBase, public XRDeviceStateCallback {
public:
    uint64_t Capabilities() override {
        uint64_t cap = 0;
        for (auto& c: mComponents) {
            cap |= c->Capabilities();
        }
        return cap;
    }

protected:
    std::vector<std::shared_ptr<XRDeviceComponent>> mComponents;
};

class DevCapImu {
public:
    using ImuEventCallback =
            std::function<int64_t(std::array<float, 3>& acc, std::array<float, 4>& gyro, std::array<float, 3>& mag)>;

    void SetImuEventCallback(ImuEventCallback&& callback) { mImuEventCallback = callback; }

protected:
    ImuEventCallback mImuEventCallback;
};

class DevCapCamera {
public:
    using PreviewFrameCallback = std::function<void(uint64_t timestamp, void* image)>;
    using SnapshotCallback = std::function<void(int64_t timestamp, void* image)>;
    const static uint64_t kCapMask = kCamera;

    void SetPreviewFrameCallback(PreviewFrameCallback&& callback) { mPreviewFrameCallback = callback; }
    void SetSnapshotCallback(SnapshotCallback&& callback) { mSnapshotCallback = callback; }
    [[nodiscard]] inline float FocalLength() const { return mFocalLengthMM; }
    [[nodiscard]] inline bool FacingFront() const { return mFacingFront; }

    virtual void StartPreview(int32_t width, int32_t height, int32_t frameRate) = 0;
    virtual void StopPreview() = 0;
    virtual void TakePhoto(int32_t width, int32_t height) = 0;

protected:
    PreviewFrameCallback mPreviewFrameCallback;
    SnapshotCallback mSnapshotCallback;

    float mFocalLengthMM = 0;
    bool mFacingFront = false;
};

class DevCapDisplay {
public:
    using VsyncEventCallback = std::function<void(int64_t vsyncTimestampUs)>;

    [[nodiscard]] inline uint32_t GetDisplayWidth() const { return mDisplayWidth; }
    [[nodiscard]] inline uint32_t GetDisplayHeight() const { return mDisplayHeight; }

    void SetVsyncEventCallback(VsyncEventCallback&& callback) { mVsyncEventCallback = callback; }
    virtual int32_t GetRefreshRate() = 0;
    virtual int64_t GetLatestVsync() = 0;

protected:
    uint32_t mDisplayWidth = 0;
    uint32_t mDisplayHeight = 0;

    VsyncEventCallback mVsyncEventCallback;
};

template<class... Caps>
class Component : public Caps..., public XRDeviceComponent {};

} // namespace ffalcon