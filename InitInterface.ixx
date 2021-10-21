export module InitInterface;

export struct InitInterface {
    virtual void OnInit() = 0;
    virtual void InitializeScene() = 0;
    virtual void CreateRaytracingInterfaces() = 0;
};