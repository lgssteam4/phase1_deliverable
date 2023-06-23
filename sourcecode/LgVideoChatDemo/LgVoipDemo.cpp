// LgVoipDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "Network.h"
#include <windows.h>
#include <dmo.h>
#include <Mmsystem.h>
#include <objbase.h>
#include <mediaobj.h>
#include <uuids.h>
#include <propidl.h>
#include <wmcodecdsp.h>
#include <atlbase.h>
#include <ATLComCli.h>
#include <audioclient.h>
#include <MMDeviceApi.h>
#include <AudioEngineEndPoint.h>
#include <DeviceTopology.h>
#include <propkey.h>
#include <strsafe.h>
#include <conio.h>
#include <iostream>
#include "mediabuf.h"
#include "AecKsBinder.h"
#include "WaveWriter.h"
#include "FixedSizeQueue.h"
#include <avrt.h>
#pragma comment(lib, "avrt.lib")

#define SAFE_ARRAYDELETE(p) {if (p) delete[] (p); (p) = NULL;}
#define SAFE_RELEASE(p) {if (NULL != p) {(p)->Release(); (p) = NULL;}}

#define VBFALSE (VARIANT_BOOL)0
#define VBTRUE  (VARIANT_BOOL)-1

#define STREAM_BUFFER_LENGTH 0.1f  //streaming buffer is 0.1 second long.

#define CHECK_RET(hr, message) if (FAILED(hr)) { puts(message); goto exit;}
#define CHECKHR(x) hr = x; if (FAILED(hr)) {printf("%d: %08X\n", __LINE__, hr); goto exit;}
#define CHECK_ALLOC(pb, message) if (NULL == pb) { puts(message); goto exit;}

HANDLE hEvent_MicThreadEnd;
HANDLE hEvent_RenderThreadEnd;
CRITICAL_SECTION CriticalSection;


class CStaticMediaBuffer : public CBaseMediaBuffer {
public:
    STDMETHODIMP_(ULONG) AddRef() { return 2; }
    STDMETHODIMP_(ULONG) Release() { return 1; }
    void Init(BYTE* pData, ULONG ulSize, ULONG ulData) {
        m_pData = pData;
        m_ulSize = ulSize;
        m_ulData = ulData;
    }
};

typedef struct {
    char Data[160*sizeof(short)];
} voipbuffer;

FixedSizeQueue<voipbuffer> VoipBufferQueue(100);

static DWORD WINAPI CaptureMicThread(LPVOID ivalue);
static DWORD WINAPI RenderToSpkrThread(LPVOID ivalue);
static DWORD WINAPI UdpRecievingWaitingThread(LPVOID ivalue);
static int SetVtI4Property(IPropertyStore* ptrPS, REFPROPERTYKEY key, LONG value);
static int SetBoolProperty(IPropertyStore* ptrPS, REFPROPERTYKEY key, VARIANT_BOOL value);

int __cdecl _tmain(int argc, const TCHAR** argv)
{
    DWORD ThreadCaptureMicID, ThreadRenderToSpkrID, ThreadRecvUdpID;
    HANDLE hThreadCaptureMic, hThreadRenderToSpkr, hThreadRecvUdp;

    if (!InitializeCriticalSectionAndSpinCount(&CriticalSection,
        0x00000400))
    {
        std::cout << "InitializeCriticalSectionAndSpinCount Failure" << std::endl;
        return -1;
    }
    SetUpUdpVoipNetwork();
    hEvent_MicThreadEnd = CreateEvent(NULL, FALSE, FALSE, NULL);
    hEvent_RenderThreadEnd = CreateEvent(NULL, FALSE, FALSE, NULL);

    hThreadRecvUdp = CreateThread(NULL, 0, UdpRecievingWaitingThread, NULL, 0, &ThreadRecvUdpID);
    hThreadRenderToSpkr = CreateThread(NULL, 0, RenderToSpkrThread, NULL, 0, &ThreadRenderToSpkrID);
    hThreadCaptureMic = CreateThread(NULL, 0, CaptureMicThread, NULL, 0, &ThreadCaptureMicID);
    std::cout << "Running" << std::endl;
    while(1) {
        Sleep(100);
        if (_kbhit())
        {
            std::cout << "Hit" << std::endl;
            int ch = _getch();
            if (ch == 's' || ch == 'S') {
                std::cout << "Exit" << std::endl;
                SetEvent(hEvent_MicThreadEnd);
                SetEvent(hEvent_RenderThreadEnd);
                SignalUdpThreadEnd();
                break;
            }
        }
   }
    WaitForSingleObject(hThreadRenderToSpkr, INFINITE);
    WaitForSingleObject(hThreadCaptureMic, INFINITE);
    WaitForSingleObject(hThreadRecvUdp, INFINITE);
    CloseHandle(hEvent_MicThreadEnd);
    CloseHandle(hEvent_RenderThreadEnd);
    CloseHandle(hThreadRecvUdp);
    CloseHandle(hThreadRenderToSpkr);
    CloseHandle(hThreadCaptureMic);
    ShutdownUdpVoipNetwork();
    std::cout << "Done" << std::endl;
}
static DWORD WINAPI RenderToSpkrThread(LPVOID ivalue)
{
    IMMDevice* pDevice = nullptr;
    IMMDeviceEnumerator* pDeviceEnumerator = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioRenderClient* pAudioRenderClient = nullptr;
    HANDLE hEvent = nullptr;
    HRESULT hr = S_OK;
    bool VoipStarted = false;
    int index;
    DWORD  dwFlags;
    DWORD avrtTaskIndex;
    try
    {   
     AvSetMmThreadCharacteristicsA("Voip Audio", &avrtTaskIndex);
     hr = CoInitializeEx(NULL, COINIT_SPEED_OVER_MEMORY);
     if (FAILED(hr)) throw std::runtime_error("CoInitialize error");

     hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pDeviceEnumerator);
    if (FAILED(hr))
    {
        printf("hr=0x%x\n,", hr);
        throw std::runtime_error("CoCreateInstance error");
        
    }

    hr = pDeviceEnumerator->GetDefaultAudioEndpoint(
        eRender,
        eConsole,
        &pDevice);
    if (FAILED(hr)) throw std::runtime_error("IMMDeviceEnumerator.GetDefaultAudioEndpoint error");
    std::cout << "IMMDeviceEnumerator.GetDefaultAudioEndpoint()->OK" << std::endl;

    hr = pDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        (void**)&pAudioClient);
    if (FAILED(hr)) throw std::runtime_error("IMMDevice.Activate error");
    std::cout << "IMMDevice.Activate()->OK" << std::endl;


    REFERENCE_TIME MinimumDevicePeriod = 10000000ULL/2;

   // hr = pAudioClient->GetDevicePeriod(nullptr, &MinimumDevicePeriod);
   // if (FAILED(hr)) throw std::runtime_error("IAudioClient.GetDevicePeriod error");

    std::cout << "minimum device period=" << MinimumDevicePeriod * 100 << "[nano seconds]" << std::endl;

    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 1;
    wave_format.nSamplesPerSec = 16000;
    wave_format.wBitsPerSample = sizeof(short) * 8;
    wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    wave_format.cbSize = 0;
 
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        MinimumDevicePeriod,
        0,
        &wave_format,
        nullptr);
    if (FAILED(hr))
    {
        printf("hr=0x%x\n,", hr);
        throw std::runtime_error("IAudioClient.Initialize error");
    }
    std::cout << "IAudioClient.Initialize()->OK" << std::endl;

    // event
    hEvent = CreateEvent(nullptr, false, false, nullptr);
    if (FAILED(hr)) throw std::runtime_error("CreateEvent error");

    hr = pAudioClient->SetEventHandle(hEvent);
    if (FAILED(hr)) throw std::runtime_error("IAudioClient.SetEventHandle error");

    UINT32 NumBufferFrames = 0;
    hr = pAudioClient->GetBufferSize(&NumBufferFrames);
    if (FAILED(hr)) throw std::runtime_error("IAudioClient.GetBufferSize error");
    std::cout << "buffer frame size=" << NumBufferFrames << "[frames]" << std::endl;

    hr = pAudioClient->GetService(
        __uuidof(IAudioRenderClient),
        (void**)&pAudioRenderClient);
    if (FAILED(hr)) throw std::runtime_error("IAudioClient.GetService error");

    BYTE* pData = nullptr;
    __int64 read_count = NumBufferFrames;
    hr = pAudioRenderClient->GetBuffer(read_count, &pData);
    if (FAILED(hr)) throw std::runtime_error("IAudioRenderClient.GetBuffer error");

    hr = pAudioRenderClient->ReleaseBuffer((UINT32)read_count, AUDCLNT_BUFFERFLAGS_SILENT);
    if (FAILED(hr)) throw std::runtime_error("IAudioRenderClient.ReleaseBuffer error");

    hr = pAudioClient->Start();
    if (FAILED(hr)) throw std::runtime_error("IAudioClient.Start error");
    std::cout << "IAudioClient.Start()->OK" << std::endl;

    bool playing = true;
    HANDLE ghEvents[2];
    DWORD dwEvent;
    ghEvents[0] = hEvent;
    ghEvents[1] = hEvent_RenderThreadEnd;
    while (playing)
    {
        dwEvent = WaitForMultipleObjects(
            2,           // number of objects in array
            ghEvents,     // array of objects
            FALSE,       // wait for any object
            INFINITE);  // INFINITE) wait

        if (dwEvent == WAIT_OBJECT_0 + 1) playing = false;
        else
        {
            UINT32 NumPaddingFrames = 0;
            hr = pAudioClient->GetCurrentPadding(&NumPaddingFrames);
            if (FAILED(hr)) throw std::runtime_error("IAudioClient.GetCurrentPadding error");

            UINT32 numAvailableFrames = NumBufferFrames - NumPaddingFrames;

            if (numAvailableFrames < 159) continue;

            //std::cout << "numAvailableFrames=" << numAvailableFrames << std::endl;
            
            EnterCriticalSection(&CriticalSection);
            if ((!VoipStarted) && (VoipBufferQueue.num_queued() > 2))
            {
                VoipStarted = true;
                std::cout << "VoipStarted" << std::endl;
            }
            read_count = 160;
            hr = pAudioRenderClient->GetBuffer(read_count, &pData);
            if (FAILED(hr)) throw std::runtime_error("IAudioRenderClient.GetBuffer error");
            index = -1;
            dwFlags = AUDCLNT_BUFFERFLAGS_SILENT;
            if (VoipStarted)
            {   
                index = VoipBufferQueue.pop();
                if (index >= 0)
                {
                    memcpy(pData, VoipBufferQueue.start_p_[index].Data, sizeof(short) * read_count);
                    dwFlags = 0;
                }
            }
            LeaveCriticalSection(&CriticalSection);
            
            hr = pAudioRenderClient->ReleaseBuffer((UINT32)read_count, dwFlags);
            if (FAILED(hr)) throw std::runtime_error("IAudioRenderClient.ReleaseBuffer error");
            //std::cout << "ReleaseBuffer=" << read_count << std::endl;
            
        }
    }
    printf("playing exiting\n");
    do
    {
        // wait for buffer to be empty
        WaitForSingleObject(hEvent, INFINITE);

        UINT32 NumPaddingFrames = 0;
        hr = pAudioClient->GetCurrentPadding(&NumPaddingFrames);
        if (FAILED(hr)) throw std::runtime_error("IAudioClient.GetCurrentPadding error");

        if (NumPaddingFrames == 0)
        {
            std::cout << "current buffer padding=0[frames]" << std::endl;
            break;
        }
    } while (true);

    hr = pAudioClient->Stop();
    if (FAILED(hr)) throw std::runtime_error("IAudioClient.Stop error");
    std::cout << "IAudioClient.Stop()->OK" << std::endl;

}
catch (std::exception& ex)
{
    std::cout << "error:" << ex.what() << std::endl;
}

 if (hEvent) CloseHandle(hEvent);
 if (pDeviceEnumerator) pDeviceEnumerator->Release();
 if (pDevice) pDevice->Release();
 if (pAudioClient) pAudioClient->Release();
 if (pAudioRenderClient) pAudioRenderClient->Release();
 CoUninitialize();
 return hr;
}
static DWORD WINAPI CaptureMicThread(LPVOID ivalue)
{
    HRESULT hr = S_OK;
    std::ofstream* FileMic = nullptr;
    IMediaObject* pDMO = NULL;
    IPropertyStore* pPS = NULL;

    CStaticMediaBuffer micInputBuffer;
    DMO_OUTPUT_DATA_BUFFER MicInputBufferStruct = { 0 };
    MicInputBufferStruct.pBuffer = &micInputBuffer;
    int BytesPerFrame;
    int NumberOfFramestoSend;

    ULONG cbProduced = 0;
    DWORD dwStatus;

    // Parameters to config DMO
    int  iMicDevIdx = -1;               // microphone device index
    int  iSpkDevIdx = -1;               // speaker device index

    // Select capture device
    UINT uCapDevCount = 0;
    UINT uRenDevCount = 0;
    char  pcScanBuf[256] = { 0 };


    FILE* pfMicOutPCM;  // dump output signal using PCM format

    DWORD cMicInputBufLen = 0;
    BYTE* pbMicInputBuffer = NULL;


    AUDIO_DEVICE_INFO* pCaptureDeviceInfo = NULL, * pRenderDeviceInfo = NULL;
    DMO_MEDIA_TYPE mt = { 0 };
    // Set DMO output format
    hr = MoInitMediaType(&mt, sizeof(WAVEFORMATEX));
    WAVEFORMATEX* ptrWav = reinterpret_cast<WAVEFORMATEX*>(mt.pbFormat);
    CHECK_RET(hr, "MoInitMediaType failed");

    HANDLE currThread;
    HANDLE currProcess;
    BOOL iRet;

    hr = CoInitialize(nullptr);
    if (FAILED(hr)) throw std::runtime_error("CoInitialize error");

    currProcess = GetCurrentProcess();
    currThread = GetCurrentThread();

    iRet = SetPriorityClass(currProcess, HIGH_PRIORITY_CLASS);
    if (0 == iRet)
    {
        // call getLastError.
        puts("failed to set process priority\n");
        goto exit;
    }

    // DMO initialization
    CHECKHR(CoCreateInstance(CLSID_CWMAudioAEC, NULL, CLSCTX_INPROC_SERVER, IID_IMediaObject, (void**)&pDMO));
    CHECKHR(pDMO->QueryInterface(IID_IPropertyStore, (void**)&pPS));

    hr = GetCaptureDeviceNum(uCapDevCount);
    CHECK_RET(hr, "GetCaptureDeviceNum failed");

    pCaptureDeviceInfo = new AUDIO_DEVICE_INFO[uCapDevCount];
    hr = EnumCaptureDevice(uCapDevCount, pCaptureDeviceInfo);
    CHECK_RET(hr, "EnumCaptureDevice failed");

    printf("\nSystem has totally %d capture devices\n", uCapDevCount);
    for (int i = 0; i < (int)uCapDevCount; i++)
    {
        _tprintf(_T("Device %d is %s"), i, pCaptureDeviceInfo[i].szDeviceName);
        if (pCaptureDeviceInfo[i].bIsMicArrayDevice)
            _tprintf(_T(" -- Mic Array Device \n"));
        else
            _tprintf(_T("\n"));
    }

    if (iMicDevIdx < -1 || iMicDevIdx >= (int)uCapDevCount)
    {
        do {
            printf("Select device ");
            scanf_s("%255s", pcScanBuf, 255);
            iMicDevIdx = atoi(pcScanBuf);
            if (iMicDevIdx < -1 || iMicDevIdx >= (int)uCapDevCount)
                printf("Invalid Capture Device ID \n");
            else
                break;
        } while (1);
    }
    if (iMicDevIdx == -1)
        _tprintf(_T("\n Default device will be used for capturing \n"));
    else
        _tprintf(_T("\n %s is selected for capturing\n"), pCaptureDeviceInfo[iMicDevIdx].szDeviceName);
    SAFE_ARRAYDELETE(pCaptureDeviceInfo);

    hr = GetRenderDeviceNum(uRenDevCount);
    CHECK_RET(hr, "GetRenderDeviceNum failed");

    pRenderDeviceInfo = new AUDIO_DEVICE_INFO[uRenDevCount];
    hr = EnumRenderDevice(uRenDevCount, pRenderDeviceInfo);
    CHECK_RET(hr, "EnumRenderDevice failed");

    printf("\nSystem has totally %d render devices\n", uRenDevCount);
    for (int i = 0; i < (int)uRenDevCount; i++)
    {
        _tprintf(_T("Device %d is %s \n"), i, pRenderDeviceInfo[i].szDeviceName);
    }

    if (iSpkDevIdx < -1 || iSpkDevIdx >= (int)uRenDevCount)
    {
        do {
            printf("Select device ");
            scanf_s("%255s", pcScanBuf, 255);
            iSpkDevIdx = atoi(pcScanBuf);
            if (iSpkDevIdx < -1 || iSpkDevIdx >= (int)uRenDevCount)
                printf("Invalid Render Device ID \n");
            else
                break;
        } while (1);
    }
    if (iSpkDevIdx == -1)
        _tprintf(_T("\n Default device will be used for rendering \n"));
    else
        _tprintf(_T("\n %s is selected for rendering \n"), pRenderDeviceInfo[iSpkDevIdx].szDeviceName);


    SAFE_ARRAYDELETE(pRenderDeviceInfo);

    // --- PREPARE OUTPUT --- //
    if (NULL != _tfopen_s(&pfMicOutPCM, L"mic.raw", _T("wb")))
    {
        puts("cannot open file for output.\n");
        goto exit;
    }


    // Set the AEC system mode.
    // SINGLE_CHANNEL_AEC - AEC processing only.
    if (SetVtI4Property(pPS,
        MFPKEY_WMAAECMA_SYSTEM_MODE,
        SINGLE_CHANNEL_AEC))
    {
        return -1;
    }

    // Set the AEC source mode.
    // VARIANT_TRUE - Source mode (we poll the AEC for captured data).
    if (SetBoolProperty(pPS,
        MFPKEY_WMAAECMA_DMO_SOURCE_MODE,
        VARIANT_TRUE) == -1)
    {
        return -1;
    }

    // Enable the feature mode.
    // This lets us override all the default processing settings below.
    if (SetBoolProperty(pPS,
        MFPKEY_WMAAECMA_FEATURE_MODE,
        VARIANT_TRUE) == -1)
    {
        return -1;
    }

    // Disable analog AGC (default enabled).
    if (SetBoolProperty(pPS,
        MFPKEY_WMAAECMA_MIC_GAIN_BOUNDER,
        VARIANT_FALSE) == -1)
    {
        return -1;
    }

    // Disable noise suppression (default enabled).
    // 0 - Disabled, 1 - Enabled
    if (SetVtI4Property(pPS,
        MFPKEY_WMAAECMA_FEATR_NS,
        0) == -1)
    {
        return -1;
    }

    // Relevant parameters to leave at default settings:
    // MFPKEY_WMAAECMA_FEATR_AGC - Digital AGC (disabled).
    // MFPKEY_WMAAECMA_FEATR_CENTER_CLIP - AEC center clipping (enabled).
    // MFPKEY_WMAAECMA_FEATR_ECHO_LENGTH - Filter length (256 ms).
    // MFPKEY_WMAAECMA_FEATR_FRAME_SIZE - Frame size (0).
    //   0 is automatic; defaults to 160 samples (or 10 ms frames at the
    //   selected 16 kHz) as long as mic array processing is disabled.
    // MFPKEY_WMAAECMA_FEATR_NOISE_FILL - Comfort noise (enabled).
    // MFPKEY_WMAAECMA_FEATR_VAD - VAD (disabled).

     // Tell DMO which capture and render device to use 
     // This is optional. If not specified, default devices will be used
    if (iMicDevIdx >= 0 || iSpkDevIdx >= 0)
    {
        PROPVARIANT pvDeviceId;
        PropVariantInit(&pvDeviceId);
        pvDeviceId.vt = VT_I4;
        pvDeviceId.lVal = (unsigned long)(iSpkDevIdx << 16) + (unsigned long)(0x0000ffff & iMicDevIdx);
        CHECKHR(pPS->SetValue(MFPKEY_WMAAECMA_DEVICE_INDEXES, pvDeviceId));
        CHECKHR(pPS->GetValue(MFPKEY_WMAAECMA_DEVICE_INDEXES, &pvDeviceId));
        PropVariantClear(&pvDeviceId);
    }

    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_PCM;
    mt.lSampleSize = 0;
    mt.bFixedSizeSamples = TRUE;
    mt.bTemporalCompression = FALSE;
    mt.formattype = FORMAT_WaveFormatEx;
    // Supported formats
    // nChannels: 1 (in AEC-only mode)
    // nSamplesPerSec: 8000, 11025, 16000, 22050
    // wBitsPerSample: 16
    ptrWav->wFormatTag = WAVE_FORMAT_PCM;
    ptrWav->nChannels = 1;
    // 16000 is the highest we can support with our resampler.
    ptrWav->nSamplesPerSec = 16000;
    ptrWav->wBitsPerSample = sizeof(short) * 8;
    ptrWav->nBlockAlign = ptrWav->nChannels * ptrWav->wBitsPerSample / 8;
    ptrWav->nAvgBytesPerSec = ptrWav->nSamplesPerSec * ptrWav->nBlockAlign;
    ptrWav->cbSize = 0;



    FileMic = OutputWaveOpen("mic.wav", ptrWav->nChannels, ptrWav->nSamplesPerSec, ptrWav->wBitsPerSample);


    hr = pDMO->SetOutputType(0, &mt, 0);
    CHECK_RET(hr, "SetOutputType failed");


    // Allocate streaming resources. This step is optional. If it is not called here, it
    // will be called when first time ProcessInput() is called. However, if you want to 
    // get the actual frame size being used, it should be called explicitly here.
    hr = pDMO->AllocateStreamingResources();
    CHECK_RET(hr, "AllocateStreamingResources failed");

    // Get actually frame size being used in the DMO. (optional, do as you need)
    int iFrameSize;
    PROPVARIANT pvFrameSize;
    PropVariantInit(&pvFrameSize);
    CHECKHR(pPS->GetValue(MFPKEY_WMAAECMA_FEATR_FRAME_SIZE, &pvFrameSize));
    iFrameSize = pvFrameSize.lVal;
    PropVariantClear(&pvFrameSize);

    BytesPerFrame = iFrameSize * ptrWav->nBlockAlign;
    std::cout << "Frame Size " << iFrameSize << std::endl;

    // allocate output buffer
    cMicInputBufLen = ptrWav->nSamplesPerSec * ptrWav->nBlockAlign;
    pbMicInputBuffer = new BYTE[cMicInputBufLen];

    CHECK_ALLOC(pbMicInputBuffer, "out of memory.\n");
    MoFreeMediaType(&mt);
    // main loop to get mic output from the DMO
    std::cout << "AEC is running ... Press \"s\" to stop" << std::endl;
    while (1)
    {
        
        if (WaitForSingleObject(hEvent_MicThreadEnd, 10) == WAIT_OBJECT_0) break;

        do {
            micInputBuffer.Init((byte*)pbMicInputBuffer, cMicInputBufLen, 0);
            MicInputBufferStruct.dwStatus = 0;
            hr = pDMO->ProcessOutput(0, 1, &MicInputBufferStruct, &dwStatus);

            CHECK_RET(hr, "ProcessOutput failed");

            if (hr == S_FALSE) {
                cbProduced = 0;
            }
            else 
            {
                hr = micInputBuffer.GetBufferAndLength(NULL, &cbProduced);
                CHECK_RET(hr, "GetBufferAndLength failed");

                NumberOfFramestoSend = cbProduced / BytesPerFrame;
                for (int i = 0; i < NumberOfFramestoSend; i++)
                {
                    // dump output data into a file with PCM format.
                    if (fwrite((pbMicInputBuffer + (i * BytesPerFrame)), 1, BytesPerFrame, pfMicOutPCM) != BytesPerFrame)
                    {
                        std::cout << "write error" << std::endl;
                        goto exit;
                    }
                    OutputWaveWrite(FileMic, (const char*)(pbMicInputBuffer + (i * BytesPerFrame)), BytesPerFrame);

                    SendUdpVoipData((const char*)(pbMicInputBuffer+(i* BytesPerFrame)), BytesPerFrame);
                }
            }


        } while (MicInputBufferStruct.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE);


       
    }

exit:

    SAFE_ARRAYDELETE(pbMicInputBuffer);
    SAFE_ARRAYDELETE(pCaptureDeviceInfo);
    SAFE_ARRAYDELETE(pRenderDeviceInfo);

    SAFE_RELEASE(pDMO);
    SAFE_RELEASE(pPS);
    OutputWaveClose(FileMic);
    delete FileMic;
    CoUninitialize();
    return hr;
}

static DWORD WINAPI UdpRecievingWaitingThread(LPVOID ivalue)
{
    struct sockaddr_in RemoteAddrIn;
    static char Buffer[1024 * 5];
    int slen = sizeof(RemoteAddrIn);
    int BytesIn;
    SetUpUdpVoipReceiveEventForThread();
    std::cout << "Udp Recv started" << '\n';
    while (1) {
        if (WaitForVoipData() ==1)
        {
            if ((BytesIn=recvfrom(Socket, Buffer, sizeof(Buffer), 0, (struct sockaddr*)&RemoteAddrIn, &slen)) == SOCKET_ERROR)
            {
                std::cout << "recvfrom() failed with error code :" << WSAGetLastError() << '\n';
             
            }
            else
            {
                int index;
                EnterCriticalSection(&CriticalSection);
                index = VoipBufferQueue.push();
                if (index!=-1)
                   memcpy(VoipBufferQueue.start_p_[index].Data, Buffer, BytesIn);
                else 
                    std::cout << "VoipBufferQueue Full" << '\n';
                LeaveCriticalSection(&CriticalSection);
            }
        }
        else 
        {
            std::cout << "Voip Exiting" << '\n';
            break;
        }
    }
    return 0;
}

int SetBoolProperty(IPropertyStore* ptrPS, REFPROPERTYKEY key, VARIANT_BOOL value)
{
    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_BOOL;
    pv.boolVal = value;
    HRESULT hr = ptrPS->SetValue(key, pv);
    PropVariantClear(&pv);
    if (FAILED(hr)) {
        //_TraceCOMError(hr);

        return -1;
    }
    return 0;
}
int SetVtI4Property(IPropertyStore* ptrPS, REFPROPERTYKEY key, LONG value)
{
    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_I4;
    pv.lVal = value;
    HRESULT hr = ptrPS->SetValue(key, pv);
    PropVariantClear(&pv);
    if (FAILED(hr)) {
        //_TraceCOMError(hr);
        return -1;
    }
    return 0;
}
