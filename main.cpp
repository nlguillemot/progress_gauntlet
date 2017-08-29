#include "winutil.h"
#include <d3d12.h>
#include <dxgi1_5.h>
#include "d3dx12.h"

#include "gauntlet.cs.hlsl.h"

#include "config.h"

#include <random>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

int main()
{
#ifdef _DEBUG
    ComPtr<ID3D12Debug> pDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
    }
#endif

    ComPtr<IDXGIFactory5> pDXGIFactory;
    CHECKHR(CreateDXGIFactory1(IID_PPV_ARGS(&pDXGIFactory)));

    ComPtr<IDXGIAdapter> pDXGIAdapter;
    CHECKHR(pDXGIFactory->EnumAdapters(0, &pDXGIAdapter));
    ComPtr<IDXGIAdapter2> pDXGIAdapter2;
    CHECKHR(pDXGIAdapter.As<IDXGIAdapter2>(&pDXGIAdapter2));

    DXGI_ADAPTER_DESC2 adapterDesc2;
    CHECKHR(pDXGIAdapter2->GetDesc2(&adapterDesc2));

    wprintf(L"Adapter: %s\n", adapterDesc2.Description);

    ComPtr<ID3D12Device> pDevice;
    CHECKHR(D3D12CreateDevice(pDXGIAdapter.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&pDevice)));

    ComPtr<ID3D12CommandQueue> pComputeCommandQueue;

    D3D12_COMMAND_QUEUE_DESC computeCommandQueueDesc = {};
    computeCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    CHECKHR(pDevice->CreateCommandQueue(&computeCommandQueueDesc, IID_PPV_ARGS(&pComputeCommandQueue)));

    ComPtr<ID3D12Fence> pFence;
    CHECKHR(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));

    std::shared_ptr<std::remove_pointer_t<HANDLE>> hFenceEvent(
        CreateEventW(NULL, FALSE, FALSE, NULL),
        CloseHandle);
    CHECKHR(hFenceEvent != NULL);
    CHECKHR(pFence->SetEventOnCompletion(1, hFenceEvent.get()));

    ComPtr<ID3D12RootSignature> pRS;
    CHECKHR(pDevice->CreateRootSignature(0, g_gauntlet_cs, sizeof(g_gauntlet_cs), IID_PPV_ARGS(&pRS)));

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = pRS.Get();
    psoDesc.CS.pShaderBytecode = g_gauntlet_cs;
    psoDesc.CS.BytecodeLength = sizeof(g_gauntlet_cs);

    ComPtr<ID3D12PipelineState> pPSO;
    CHECKHR(pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pPSO)));

    ComPtr<ID3D12CommandAllocator> pComputeCmdAlloc;
    CHECKHR(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&pComputeCmdAlloc)));

    ComPtr<ID3D12GraphicsCommandList> pCmdList;
    CHECKHR(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, pComputeCmdAlloc.Get(), NULL, IID_PPV_ARGS(&pCmdList)));

    ComPtr<ID3D12Resource> pIDBuffer;
    CHECKHR(pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(NUM_THREADS * sizeof(UINT)), D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL, IID_PPV_ARGS(&pIDBuffer)));

    ComPtr<ID3D12Resource> pCurrIndexBuffer;
    CHECKHR(pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        NULL, IID_PPV_ARGS(&pCurrIndexBuffer)));

    ComPtr<ID3D12Resource> pReadbackFinalIndexBuffer;
    CHECKHR(pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)), D3D12_RESOURCE_STATE_COPY_DEST,
        NULL, IID_PPV_ARGS(&pReadbackFinalIndexBuffer)));

    struct GPUDescriptors
    {
        enum Enum
        {
            CurrIndexBuffer,
            Count
        };
    };

    UINT kCBV_SRV_UAV_Increment = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeap_CBV_SRV_UAV_SV_Desc = {};
    descriptorHeap_CBV_SRV_UAV_SV_Desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeap_CBV_SRV_UAV_SV_Desc.NumDescriptors = GPUDescriptors::Count;
    descriptorHeap_CBV_SRV_UAV_SV_Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> pDescriptorHeap_CBV_SRV_UAV_SV;
    CHECKHR(pDevice->CreateDescriptorHeap(&descriptorHeap_CBV_SRV_UAV_SV_Desc, IID_PPV_ARGS(&pDescriptorHeap_CBV_SRV_UAV_SV)));
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_CBV_SRV_UAV_SV = pDescriptorHeap_CBV_SRV_UAV_SV->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_CBV_SRV_UAV_SV = pDescriptorHeap_CBV_SRV_UAV_SV->GetGPUDescriptorHandleForHeapStart();

    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeap_CBV_SRV_UAV_Desc = {};
    descriptorHeap_CBV_SRV_UAV_Desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeap_CBV_SRV_UAV_Desc.NumDescriptors = 1;

    ComPtr<ID3D12DescriptorHeap> pDescriptorHeap_CBV_SRV_UAV;
    CHECKHR(pDevice->CreateDescriptorHeap(&descriptorHeap_CBV_SRV_UAV_Desc, IID_PPV_ARGS(&pDescriptorHeap_CBV_SRV_UAV)));
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_CBV_SRV_UAV = pDescriptorHeap_CBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuCurrIndexBufferUAVHandle_SV = { cpu_CBV_SRV_UAV_SV.ptr + GPUDescriptors::CurrIndexBuffer * kCBV_SRV_UAV_Increment };
    D3D12_GPU_DESCRIPTOR_HANDLE gpuCurrIndexBufferUAVHandle_SV = { gpu_CBV_SRV_UAV_SV.ptr + GPUDescriptors::CurrIndexBuffer * kCBV_SRV_UAV_Increment };
    D3D12_UNORDERED_ACCESS_VIEW_DESC currIndexBufferUAVDesc = {};
    currIndexBufferUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    currIndexBufferUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    currIndexBufferUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    currIndexBufferUAVDesc.Buffer.NumElements = 1;
    pDevice->CreateUnorderedAccessView(pCurrIndexBuffer.Get(), NULL, &currIndexBufferUAVDesc, cpuCurrIndexBufferUAVHandle_SV);
    
    {
        // Shuffle thread IDs
        std::random_device rd;
        std::mt19937 g(rd());
        std::vector<uint32_t> threadIDs(NUM_THREADS);
        std::iota(begin(threadIDs), end(threadIDs), 0);
        std::shuffle(begin(threadIDs), end(threadIDs), g);

        // Upload shuffled thread IDs
        void* pMappedThreadIDs;
        CHECKHR(pIDBuffer->Map(0, &CD3DX12_RANGE(0, NUM_THREADS * sizeof(UINT)), &pMappedThreadIDs));
        memcpy(pMappedThreadIDs, threadIDs.data(), threadIDs.size() * sizeof(UINT));
        pIDBuffer->Unmap(0, &CD3DX12_RANGE(0, NUM_THREADS * sizeof(UINT)));

        // Clear the counter to zero
        pDevice->CreateUnorderedAccessView(pCurrIndexBuffer.Get(), NULL, &currIndexBufferUAVDesc, cpu_CBV_SRV_UAV);
        const UINT kClearZero[4] = { 0,0,0,0 };
        pCmdList->ClearUnorderedAccessViewUint(gpuCurrIndexBufferUAVHandle_SV, cpu_CBV_SRV_UAV, pCurrIndexBuffer.Get(), kClearZero, 0, NULL);

        // Kick off the gauntlet
        pCmdList->SetComputeRootSignature(pRS.Get());
        pCmdList->SetPipelineState(pPSO.Get());
        pCmdList->SetComputeRootShaderResourceView(0, pIDBuffer->GetGPUVirtualAddress());
        pCmdList->SetComputeRootUnorderedAccessView(1, pCurrIndexBuffer->GetGPUVirtualAddress());
        pCmdList->Dispatch(1, 1, 1);
        pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pCurrIndexBuffer.Get()));
        
        pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pCurrIndexBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));
        pCmdList->CopyResource(pReadbackFinalIndexBuffer.Get(), pCurrIndexBuffer.Get());

        // Close and submit
        pCmdList->Close();
        ID3D12CommandList* pLists[] = { pCmdList.Get() };
        pComputeCommandQueue->ExecuteCommandLists(1, pLists);
        pComputeCommandQueue->Signal(pFence.Get(), 1);
    }

    if (WaitForSingleObject(hFenceEvent.get(), 10000) == WAIT_OBJECT_0)
    {
        // Make sure the iterator actually got to the end
        void* pMappedFinalIndex;
        CHECKHR(pReadbackFinalIndexBuffer->Map(0, &CD3DX12_RANGE(0, sizeof(UINT)), &pMappedFinalIndex));
        const UINT kExpected = NUM_THREADS;
        assert(memcmp(pMappedFinalIndex, &kExpected, sizeof(UINT)) == 0);
        pReadbackFinalIndexBuffer->Unmap(0, &CD3DX12_RANGE(0, sizeof(UINT)));

        printf("Passed the gauntlet!\n");
    }
    else
    {
        printf("Failed the gauntlet.\n");
        exit(-1);
    }
}