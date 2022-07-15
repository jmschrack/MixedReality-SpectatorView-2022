// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#if defined(INCLUDE_ELGATO)

#include "IFrameProvider.h"
#include "DirectXHelper.h"
#include "IVideoCaptureFilterTypes.h"
#include "IVideoCaptureFilter.h"
#include "ElgatoSampleCallback.h"

#include <dshow.h>
#include <initguid.h>
#include <wmcodecdsp.h>
#include <mmreg.h>
#include <dvdmedia.h>
#include <bdaiface.h>
#include "qedit.h"

#include "dshowcapture.hpp"
extern "C" {
#include "ffmpeg-decode.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#pragma comment(lib, "wmcodecdspuuid")    
#pragma comment(lib, "dxguid")
#pragma comment(lib, "strmbase")
// {39F50F4C-99E1-464a-B6F9-D605B4FB5918}
//DEFINE_GUID(CLSID_ElgatoVideoCaptureFilter,
  //  0x39f50f4c, 0x99e1, 0x464a, 0xb6, 0xf9, 0xd6, 0x5, 0xb4, 0xfb, 0x59, 0x18);

//{17CCA71B-ECD7-11D0-B908-00A0C9223196}
DEFINE_GUID(CLSID_ElgatoVideoCaptureFilter,
    0x17cca71b, 0xecd7, 0x11d0, 0xb9, 0x08, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
  
// {13DD0CCF-A773-4CB7-8C98-8E31E69F0252}
DEFINE_GUID(IID_IElgatoVideoCaptureFilterEnumeration,
    0x13dd0ccf, 0xa773, 0x4cb7, 0x8c, 0x98, 0x8e, 0x31, 0xe6, 0x9f, 0x2, 0x52);
// {39F50F4C-99E1-464a-B6F9-D605B4FB5925}
DEFINE_GUID(IID_IElgatoVideoCaptureFilter6,
    0x39f50f4c, 0x99e1, 0x464a, 0xb6, 0xf9, 0xd6, 0x05, 0xb4, 0xfb, 0x59, 0x25);

using namespace ElgatoGameCapture;

class Decoder {
    struct ffmpeg_decode decode;

public:
    inline Decoder() { memset(&decode, 0, sizeof(decode)); }
    inline ~Decoder() { ffmpeg_decode_free(&decode); }

    inline operator ffmpeg_decode* () { return &decode; }
    inline ffmpeg_decode* operator->() { return &decode; }
};

class ElgatoFrameProvider : public IFrameProvider
{
public:

    ElgatoFrameProvider(bool useCPU = false);
    ~ElgatoFrameProvider();

    virtual HRESULT Initialize(ID3D11ShaderResourceView* colorSRV, ID3D11ShaderResourceView* depthSRV, ID3D11ShaderResourceView* bodySRV, ID3D11Texture2D* outputTexture);
    virtual LONGLONG GetTimestamp(int frame);

    virtual LONGLONG GetDurationHNS();

    ProviderType GetProviderType() { return Elgato; }

    virtual void Update(int compositeFrameIndex);

    virtual bool IsEnabled();

    bool SupportsOutput()
    {
        return false;
    }

    virtual void Dispose();

    bool ProvidesYUV()
    {
        return true;
    }

    int GetCaptureFrameIndex()
    {
        if(frameCallback)
            return frameCallback->GetCaptureFrameIndex();

        return 0;
    }


private:
    ID3D11ShaderResourceView* _colorSRV;
    ID3D11Device* _device;

    DShow::Device dshowDevice;
    DShow::VideoConfig videoConfig;
    Decoder video_decoder;
    SwsContext* scaler_ctx;

    bool _useCPU = false;
    HRESULT errorCode = S_OK;

    HRESULT InitGraph();
    HRESULT DestroyGraph();

    // From https://msdn.microsoft.com/en-us/library/windows/desktop/dd387915(v=vs.85).aspx
    HRESULT ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IBaseFilter *pDest);
    HRESULT ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn);
    HRESULT ConnectFilters(IGraphBuilder *pGraph, IPin *pOut, IBaseFilter *pDest);
    HRESULT FindUnconnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin);
    // From: https://msdn.microsoft.com/en-us/library/windows/desktop/dd375792(v=vs.85).aspx
    HRESULT MatchPin(IPin *pPin, PIN_DIRECTION direction, BOOL bShouldBeConnected, BOOL *pResult);
    HRESULT IsPinConnected(IPin *pPin, BOOL *pResult);
    HRESULT IsPinDirection(IPin *pPin, PIN_DIRECTION dir, BOOL *pResult);
    void OnVideoData(const DShow::VideoConfig& config, unsigned char* data,
        size_t size, long long startTime,
        long long endTime, long rotation);
    void OnEncodedVideoData(enum AVCodecID id, unsigned char* data,
        size_t size, long long ts);
    void OnReactivate();
    HRESULT SetSampleGrabberParameters();

    IFilterGraph2 *pGraph = NULL;
    IMediaControl *pControl = NULL;
    IBaseFilter *pElgatoFilter = NULL;
    IBaseFilter *pGrabberF = NULL;
    ISampleGrabber *pGrabber = NULL;
    IEnumPins *pEnum = NULL;
    IPin *pPin = NULL;
    IBaseFilter *pNullF = NULL;
    ElgatoSampleCallback *frameCallback = NULL;
    IElgatoVideoCaptureFilter6 *filter = NULL;
    std::chrono::system_clock::time_point  startTimepoint;
    bool needSnapshot = true;
    unsigned char* dummyFrame;
    size_t dummySize;
};

#endif