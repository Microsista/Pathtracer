export module InitInterface;

export struct InitInterface {
    virtual void InitializeScene() = 0;
    virtual void UpdateCameraMatrices() = 0;
};