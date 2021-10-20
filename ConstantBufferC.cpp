#include "ConstantBufferC.h"

template<class T>
inline ConstantBufferC<T>::ConstantBufferC() : m_alignedInstanceSize(0), m_numInstances(0), m_mappedConstantData(nullptr) {}

template<class T>
inline void ConstantBufferC<T>::Create(ID3D12Device* device, UINT numInstances, LPCWSTR resourceName)
{
    m_numInstances = numInstances;
    m_alignedInstanceSize = Align(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    UINT bufferSize = numInstances * m_alignedInstanceSize;
    Allocate(device, bufferSize, resourceName);
    m_mappedConstantData = MapCpuWriteOnly();
}

template<class T>
inline void ConstantBufferC<T>::CopyStagingToGpu(UINT instanceIndex)
{
    memcpy(m_mappedConstantData + instanceIndex * m_alignedInstanceSize, &staging, sizeof(T));
}
