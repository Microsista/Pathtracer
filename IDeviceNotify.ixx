export module IDeviceNotify;

export struct IDeviceNotify {
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;
};