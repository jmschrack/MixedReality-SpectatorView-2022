// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#if defined(INCLUDE_ELGATO)
#include "dshowcapture.hpp"
#include "ElgatoFrameProvider.h"
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
extern "C" {
    #include "ffmpeg-decode.h"
}
using namespace DShow;
using namespace std;

const auto ELGATO_NAME = wstring(L"Game Capture");
void LogOut(LogType type, const wchar_t* msg, void* param) {
    std::wstringstream ss;
    ss << "LibDShow:";
    switch (type) {
    case LogType::Error: ss << "ERROR:"; break;
    case LogType::Debug: ss << "DEBUG:"; break;
    case LogType::Info: ss << "INFO:"; break;
    case LogType::Warning: ss << "WARNIING:"; break;
    default: ss << "LOG:";
    }
    ss << msg << "\n";
    OutputDebugString(ss.str().c_str());
    ss.clear();
}

ElgatoFrameProvider::ElgatoFrameProvider(bool useCPU) :
    _useCPU(useCPU), dshowDevice(InitGraph::False)
{
    DShow::SetLogCallback(&LogOut, nullptr);
    ifstream myfile;
    OutputDebugString(L"LibDShow: Attempting to load dummyfile wall.yuv....\n");
    std::wstringstream ss;
    ss << "LibDShow::Dummy file Size:";
    myfile.open("wall.yuv", ios::in  | ios::binary);
    myfile.seekg(0, std::ios::end);
    std::streamsize size = myfile.tellg();
    myfile.seekg(0, std::ios::beg);
    ss << size;
    OutputDebugString(ss.str().c_str());
    ss.clear();
    std::vector<char> buffer(size);
    if (myfile.read(buffer.data(), size)) {
        OutputDebugString(L"LibDShow: Loaded Dummy Frame!\n");
        dummyFrame = (unsigned char*)buffer.data();
        dummySize = size;
    }
    else {
        OutputDebugString(L"LibDShow: Failed to load wall.yuv????????\n");
    }
    //myfile.read((unsigned char*)dummyFrame);
    //myfile.write((char*)data, size);
    myfile.close();
}


ElgatoFrameProvider::~ElgatoFrameProvider()
{
    DestroyGraph();
    dshowDevice.~Device();
    ffmpeg_decode_free(video_decoder);
    if (scaler_ctx) {
        sws_freeContext(scaler_ctx);
    }
    SafeRelease(frameCallback);

}




HRESULT ElgatoFrameProvider::Initialize(ID3D11ShaderResourceView* colorSRV, ID3D11ShaderResourceView* depthSRV, ID3D11ShaderResourceView* bodySRV, ID3D11Texture2D* outputTexture)
{
    //If we failed once lets not keep trying since this hangs the machine
    if (FAILED(errorCode))
    {
        return errorCode;
    }

    if (IsEnabled())
    {
        return S_OK;
    }

    if (frameCallback)
    {
        return E_PENDING;
    }

    _colorSRV = colorSRV;
    if (colorSRV != nullptr)
    {
        colorSRV->GetDevice(&_device);
    }

    frameCallback = new ElgatoSampleCallback(_device);
    frameCallback->AddRef();

    errorCode= InitGraph();
    if (FAILED(errorCode))
    {
        OutputDebugString(L"Failed on InitGraph.\n");
        DestroyGraph();
        return errorCode;
    }

    if (!IsEnabled())
    {
        return E_PENDING;
    }

    return S_OK;
}

void ElgatoFrameProvider::Update(int compositeFrameIndex)
{
    if (!IsEnabled() ||
        _colorSRV == nullptr ||
        _device == nullptr ||
        frameCallback == nullptr)
    {
        return;
    }
    
    frameCallback->UpdateSRV(_colorSRV, false, compositeFrameIndex);//_useCPU
}

LONGLONG ElgatoFrameProvider::GetTimestamp(int frame)
{
    if (frameCallback != nullptr)
    {
        return frameCallback->GetTimestamp();
    }

    return -1;
}

LONGLONG ElgatoFrameProvider::GetDurationHNS()
{
    return (LONGLONG)((1.0f / 30.0f) * QPC_MULTIPLIER);
}

bool ElgatoFrameProvider::IsEnabled()
{
    if (frameCallback == nullptr)
    {
        return false;
    }

    return frameCallback->IsEnabled();
}

void ElgatoFrameProvider::Dispose()
{
    DestroyGraph();
    
}

void ElgatoFrameProvider::OnVideoData(const VideoConfig& config, unsigned char* data,
    size_t size, long long startTime,
    long long endTime, long rotation) {
    
    /*if (needSnapshot&&std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startTimepoint).count()>=30) {

        OutputDebugString(L"LibDShow: Saving snapshot...\n");
        ofstream myfile;
        myfile.open("snapshot.h264", ios::out | ios::trunc | ios::binary);
        
        myfile.write((char*)data, size);
        myfile.close();
        OutputDebugString(L"LibDShow: Snapshot saved!\n");
        needSnapshot = false;
    }*/
   
    if (videoConfig.format == VideoFormat::H264) {
        OnEncodedVideoData(AV_CODEC_ID_H264, data, size, startTime);
        return;
    }
    if (videoConfig.format == VideoFormat::MJPEG) {
        OnEncodedVideoData(AV_CODEC_ID_MJPEG, data, size, startTime);
        return;
    }

    frameCallback->BufferCB(startTime, data, size);
}

void ElgatoFrameProvider::OnEncodedVideoData(enum AVCodecID id, unsigned char* data,
    size_t size, long long ts) {
    //if (needSnapshot && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startTimepoint).count() >= 30) {

    //    OutputDebugString(L"LibDShow: Saving H264 snapshot...\n");
    //    ofstream myfile;
    //    myfile.open("snapshot.h264", ios::out | ios::trunc | ios::binary);

    //    myfile.write((char*)data, size);
    //    myfile.close();
    //    OutputDebugString(L"LibDShow: Snapshot saved!\n");
    //    //needSnapshot = false;
    //}
    /* If format or hw decode changes, recreate the decoder */
    if (ffmpeg_decode_valid(video_decoder) &&
        ((video_decoder->codec->id != id) )) {//||(video_decoder->hw != hw_decode)
        ffmpeg_decode_free(video_decoder);
    }

    if (!ffmpeg_decode_valid(video_decoder)) {
        if (ffmpeg_decode_init(video_decoder, id, false) < 0) {//hw_decode
            OutputDebugString(L"LibDShow:ERROR:Could not initialize video decoder");
            return;
        }
    }
    bool got_output;
    AVFrame frame;
    
    int ret = ffmpeg_decode_video(video_decoder, data, size, &ts,(video_range_type)0);
    /*if (needSnapshot && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startTimepoint).count() >= 30) {

        OutputDebugString(L"LibDShow: Saving YUV snapshot...\n");
        ofstream myfile;
        myfile.open("snapshot.yuv", ios::out | ios::trunc | ios::binary);

        myfile.write((char*)video_decoder->frame->data[0], size);
        myfile.close();
        OutputDebugString(L"LibDShow: Snapshot saved!\n");
        needSnapshot = false;
    }*/
    switch (ret) {
    case 0:
        OutputDebugString(L"LibDShow::ERROR: Decode failed!!\n");
        break;
    case 1:
        //buffering, so do nothing.
        break;
    case 2:
        //frameCallback->BufferCB(ts, video_decoder->frame->data[0], abs(video_decoder->frame->linesize[0]) * 1080);
        //OutputDebugString(L"LibDShow:: Decode SUCCEEDED!!\n");
        
        //std::wstringstream ss;
       
        /*ss << "LibDShow::frame output:";
        ss << ret << ":(";
        ss << video_decoder->frame->width << "x" << video_decoder->frame->height << ":)";
        ss << ":Format (" << video_decoder->frame->format << "):";
        ss << "Linesize[" << video_decoder->frame->linesize[0] << "," << video_decoder->frame->linesize[1] << "," << video_decoder->frame->linesize[2] << "," << video_decoder->frame->linesize[3] << "]:";
        ss << "scaler supports UYVY = " << sws_isSupportedOutput(AVPixelFormat::AV_PIX_FMT_UYVY422);
        OutputDebugString(ss.str().c_str());*/

        if (!scaler_ctx) {
            auto source_pix_fmt = video_decoder->frame->format;

            if (sws_isSupportedOutput(AVPixelFormat::AV_PIX_FMT_BGRA) == 0) {
                OutputDebugString(L"LibDShow::ERROR: UYVY is unspported output!!\n");
            }

            scaler_ctx = sws_getContext(video_decoder->frame->width, video_decoder->frame->height, (AVPixelFormat)source_pix_fmt, FRAME_WIDTH, FRAME_HEIGHT, AVPixelFormat::AV_PIX_FMT_UYVY422, SWS_BILINEAR, NULL,NULL, NULL);
        }
        if (!scaler_ctx) {
            OutputDebugString(L"LibDShow:ERROR: COuldn't initialize sw scaler!");
            return;
        }
       
        sws_scale(scaler_ctx, video_decoder->frame->data, video_decoder->frame->linesize, 0, video_decoder->frame->height, video_decoder->dst_data, video_decoder->dst_linesize);
        //ss.clear();
        //ss(str());
       // ss << "LibDShow:sws_scale Intput: ("<< video_decoder->frame->width << "x" << video_decoder->frame->height << ")" << "output: Linesize [" << video_decoder->dst_linesize[0] << "," << video_decoder->dst_linesize[1] << "," << video_decoder->dst_linesize[2] << "," << video_decoder->dst_linesize[3] << "]";
        //OutputDebugString(ss.str().c_str());
        //if (needSnapshot && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startTimepoint).count() >= 15) {
        //    //OutputDebugString(L"LibDShow: Saving Y buffer as BGRA");
        //    //ofstream yfile;
        //    //yfile.open("snapshot-gray.BGR", ios::out | ios::trunc | ios::binary);
        //    //uint8_t* ptr = video_decoder->frame->data[0];
        //    //for (int x = 0; x < video_decoder->frame->linesize[0]* video_decoder->frame->height; ++x) {
        //    //    yfile.write((const char*)(ptr + x), 1);
        //    //    yfile.write((const char*)(ptr + x), 1);
        //    //    yfile.write((const char*)(ptr + x), 1);
        //    //    //yfile.write((const char*)(ptr + x), 1);
        //    //}
        //    //yfile.close();
        //    //OutputDebugString(L"LibDShow: Saved Y buffer snapshot!\n");

        //    OutputDebugString(L"LibDShow: Saving BGRA snapshot...\n");
        //    ofstream myfile;
        //    myfile.open("snapshot.BGRA", ios::out | ios::trunc | ios::binary);

        //    myfile.write((char*)video_decoder->dst_data[0],1920*1080*4);// (int)FRAME_HEIGHT * (int)video_decoder->dst_linesize[0]
        //    myfile.close();
        //    OutputDebugString(L"LibDShow: Snapshot saved!\n");
        //    needSnapshot = false;
        //}
        frameCallback->BufferCB(ts, video_decoder->dst_data[0],video_decoder->dst_linesize[0]*1080);
    }
    
    
}

void ElgatoFrameProvider::OnReactivate() {
    if (!dshowDevice.ResetGraph())
        return;
    if (!dshowDevice.SetVideoConfig(&videoConfig))
        return;
    if (!dshowDevice.ConnectFilters())
        return;
    if (dshowDevice.Start() != Result::Success)
        return;
}

HRESULT ElgatoFrameProvider::InitGraph() {
    HRESULT hr = S_OK;
    vector<VideoDevice> devices;
    startTimepoint = std::chrono::system_clock::now();
    OutputDebugString(L"LibDShow: Beginning InitGraph\n");
    if (Device::EnumVideoDevices(devices)) {
        int index = -1;
        for (int i = 0; i < devices.size(); i++) {
            if (devices[i].name.find(ELGATO_NAME) != wstring::npos) {
                index = i;
                break;
            }
        }
        if (index > -1) {
            //wcout << "Found " << devices[index].name << "\n";
            OutputDebugString(L"LibDShow:Found device. Searching for settings...\n");
            int cindex = -1;
            for (int i = 0; i < devices[index].caps.size(); i++) {
                auto info = &devices[index].caps[i];
                if (info->maxCX == 1920 && info->maxCY == 1080 && info->format == VideoFormat::H264) {
                    cindex = i;
                    break;
                }
            }
            if (cindex < 0) {
                OutputDebugString(L"LibDShow:Couldn't find format!\n");
                return E_FAIL;
            }

            auto caps = &devices[index].caps[cindex];
            //VideoConfig videoConfig;
            
            videoConfig.name = devices[index].name;
            videoConfig.path = devices[index].path;
            videoConfig.useDefaultConfig = false;// resType == ResType_Preferred;
            videoConfig.cx = caps->maxCX; //devices[index].caps[cindex].maxCX;//cx;
            videoConfig.cy_abs = caps->maxCY; //devices[index].caps[cindex].maxCY;//abs(cy);
            videoConfig.cy_flip = false;// cy < 0;
            videoConfig.frameInterval = caps->maxInterval;//interval;
            videoConfig.internalFormat = caps->format;//VideoFormat::Any;//devices[index].caps[cindex].format;//(VideoFormat)301;// YUY2;
            //setup callback
            videoConfig.callback = std::bind(&ElgatoFrameProvider::OnVideoData, this,
                placeholders::_1, placeholders::_2,
                placeholders::_3, placeholders::_4,
                placeholders::_5, placeholders::_6);
            videoConfig.reactivateCallback =
                std::bind(&ElgatoFrameProvider::OnReactivate, this);
            videoConfig.format = VideoFormat::H264;//devices[index].caps[cindex].format;
            devices.clear();
            //apply settings
            if (!dshowDevice.ResetGraph()) {
                OutputDebugString(L"LibDShow:Failed to initialize \n");
                return E_FAIL;
            }
            if (!dshowDevice.SetVideoConfig(&videoConfig)) {
                OutputDebugString(L"LibDShow: Failed to set VideoConfig\n");
                return E_FAIL;
            }
            
            //call start
            if (!dshowDevice.ConnectFilters()) {
                OutputDebugString(L"LibDShow: Failed to connect filters\n");
                return E_FAIL;
            }

            int result = (int)dshowDevice.Start();
            if (result == 1) {
                OutputDebugString(L"LibDShow: Failed to start graph\n");
                return E_FAIL;
            }
            if (result == 2) {
                OutputDebugString(L"LibDShow: Device is in use? \n");
                return E_FAIL;
            }
            return 0;
        }
        else {
            OutputDebugString(L"LibDShow: Could not find device named Game Capture\n");
            return E_FAIL;
        }
    }
    else {
        OutputDebugString(L"LibDShow: Failed enumerating VideoDevices.\n");
        return E_FAIL;
    }
}

//HRESULT oldInitGraph()
//{
//    HRESULT hr = S_OK;
//
//    // Create the filter graph.
//    hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IFilterGraph2, (void **)&pGraph);
//    _ASSERT(SUCCEEDED(hr));
//
//    hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
//    _ASSERT(SUCCEEDED(hr));
//
//    // Add "Elgato Game Capture HD" filter which was registered when installing the elgato capture software.
//    hr = CoCreateInstance(CLSID_ElgatoVideoCaptureFilter, NULL, CLSCTX_INPROC, IID_IBaseFilter, (void **)&pElgatoFilter);
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed creating elgato cap filter.\n");
//        return hr;
//    }
//
//    hr = pElgatoFilter->QueryInterface(IID_IElgatoVideoCaptureFilter6, (void**)&filter);
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed creating elgato filter 6.\n");
//        return hr;
//    }
//
//    // Set default resolution.
//    VIDEO_CAPTURE_FILTER_SETTINGS_EX settings;
//    hr = filter->GetSettingsEx(&settings);
//    _ASSERT(SUCCEEDED(hr));
//
//    if (FRAME_HEIGHT == 1080)
//    {
//        settings.Settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_1080;
//    }
//    else if (FRAME_HEIGHT == 720)
//    {
//        settings.Settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_720;
//    }
//    else if (FRAME_HEIGHT == 480)
//    {
//        settings.Settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_480;
//    }
//    else if (FRAME_HEIGHT == 360)
//    {
//        settings.Settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_360;
//    }
//    else if (FRAME_HEIGHT == 240)
//    {
//        settings.Settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_240;
//    }
//    else
//    {
//        // Resolution does not fit filter - default to 1080 and see what happens.
//        settings.Settings.profile = VIDEO_CAPTURE_FILTER_VID_ENC_PROFILE_1080;
//    }
//    filter->SetSettingsEx(&settings);
//    SafeRelease(filter);
//
//    hr = pGraph->AddFilter(pElgatoFilter, L"Elgato Game Capture HD");
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed adding elgato filter.\n");
//        return hr;
//    }
//
//    // Create the Sample Grabber filter.
//    hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
//        IID_PPV_ARGS(&pGrabberF));
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed creating sample grabber.\n");
//        return hr;
//    }
//
//    hr = pGraph->AddFilter(pGrabberF, L"Sample Grabber");
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed adding grabber.\n");
//        return hr;
//    }
//
//    hr = pGrabberF->QueryInterface(IID_PPV_ARGS(&pGrabber));
//    _ASSERT(SUCCEEDED(hr));
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed creating grabber 2.\n");
//        return hr;
//    }
//
//    // Set parameters to default resolution and UYVY frame format.
//    SetSampleGrabberParameters();
//
//    hr = pElgatoFilter->EnumPins(&pEnum);
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed enumerating pins.\n");
//        return hr;
//    }
//
//    while (S_OK == pEnum->Next(1, &pPin, NULL))
//    {
//        hr = ConnectFilters(pGraph, pPin, pGrabberF);
//        SafeRelease(pPin);
//        if (SUCCEEDED(hr))
//        {
//            break;
//        }
//    }
//
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed connecting filters.\n");
//        return hr;
//    }
//
//    // Connect sample grabber to a null renderer.
//    hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
//        IID_PPV_ARGS(&pNullF));
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed creating null renderer.\n");
//        return hr;
//    }
//
//    hr = pGraph->AddFilter(pNullF, L"Null Filter");
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed adding null renderer.\n");
//        return hr;
//    }
//
//    // Call frame buffer callback on the sample grabber.
//    hr = pGrabber->SetCallback(frameCallback, 1);
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed creating grabber.\n");
//        return hr;
//    }
//
//    hr = ConnectFilters(pGraph, pGrabberF, pNullF);
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed connecting grabber to null renderer.\n");
//        return hr;
//    }
//
//    // Start playback.
//    hr = pControl->Run();
//    if (FAILED(hr))
//    {
//        OutputDebugString(L"Failed starting.\n");
//        return hr;
//    }
//
//    return hr;
//}

HRESULT ElgatoFrameProvider::SetSampleGrabberParameters()
{
    AM_MEDIA_TYPE amt;
    ZeroMemory(&amt, sizeof(AM_MEDIA_TYPE));
    amt.majortype = MEDIATYPE_Video;
    amt.subtype = MEDIASUBTYPE_UYVY;
    amt.formattype = FORMAT_VideoInfo;
    amt.bFixedSizeSamples = TRUE;
    amt.lSampleSize = FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP_YUV;
    amt.bTemporalCompression = FALSE;
    
    VIDEOINFOHEADER vih;
    ZeroMemory(&vih, sizeof(VIDEOINFOHEADER));
    vih.rcTarget.right = FRAME_WIDTH;
    vih.rcTarget.bottom = FRAME_HEIGHT;
    vih.AvgTimePerFrame = (REFERENCE_TIME)((1.0f / 30.0f) * QPC_MULTIPLIER);
    vih.bmiHeader.biWidth = FRAME_WIDTH;
    vih.bmiHeader.biHeight = FRAME_HEIGHT;
    vih.bmiHeader.biSizeImage = FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP_YUV;
        
    amt.pbFormat = (BYTE*)&vih;
    
    return pGrabber->SetMediaType(&amt);
}

HRESULT ElgatoFrameProvider::DestroyGraph()
{
    HRESULT hr = S_FALSE;

    if (pControl != nullptr)
    {
        hr = pControl->Stop();
        _ASSERT(SUCCEEDED(hr));
    }

    if (pGrabberF != nullptr)
    {
        hr = pGrabberF->Stop();
        _ASSERT(SUCCEEDED(hr));
    }

    if (pElgatoFilter != nullptr)
    {
        hr = pElgatoFilter->Stop();
        _ASSERT(SUCCEEDED(hr));
    }

    if (pGraph != nullptr)
    {
        hr = pGraph->RemoveFilter(pGrabberF);
        _ASSERT(SUCCEEDED(hr));
    }

    if (pGraph != nullptr)
    {
        hr = pGraph->RemoveFilter(pElgatoFilter);
        _ASSERT(SUCCEEDED(hr));
    }
    dshowDevice.ResetGraph();
    dshowDevice.ShutdownGraph();

    SafeRelease(pPin);
    SafeRelease(pEnum);
    SafeRelease(pNullF);
    SafeRelease(pGrabber);
    SafeRelease(pGrabberF);
    SafeRelease(pControl);
    SafeRelease(pElgatoFilter);
    SafeRelease(filter);
    SafeRelease(pGraph);
    SafeRelease(frameCallback);

    return hr;
}

#pragma region DirectShow Filter Logic
HRESULT ElgatoFrameProvider::ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IBaseFilter *pDest)
{
    IPin *pOut = NULL;

    // Find an output pin on the first filter.
    HRESULT hr = FindUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (SUCCEEDED(hr))
    {
        hr = ConnectFilters(pGraph, pOut, pDest);
        pOut->Release();
    }
    return hr;
}

HRESULT ElgatoFrameProvider::ConnectFilters(IGraphBuilder *pGraph, IPin *pOut, IBaseFilter *pDest)
{
    IPin *pIn = NULL;

    // Find an input pin on the downstream filter.
    HRESULT hr = FindUnconnectedPin(pDest, PINDIR_INPUT, &pIn);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Connect(pOut, pIn);
        pIn->Release();
    }
    return hr;
}

HRESULT ElgatoFrameProvider::ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn)
{
    IPin *pOut = NULL;

    // Find an output pin on the upstream filter.
    HRESULT hr = FindUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Connect(pOut, pIn);
        pOut->Release();
    }
    return hr;
}

HRESULT ElgatoFrameProvider::MatchPin(IPin *pPin, PIN_DIRECTION direction, BOOL bShouldBeConnected, BOOL *pResult)
{
    _ASSERT(pResult != NULL);

    BOOL bMatch = FALSE;
    BOOL bIsConnected = FALSE;

    HRESULT hr = IsPinConnected(pPin, &bIsConnected);
    if (SUCCEEDED(hr))
    {
        if (bIsConnected == bShouldBeConnected)
        {
            hr = IsPinDirection(pPin, direction, &bMatch);
        }
    }

    if (SUCCEEDED(hr))
    {
        *pResult = bMatch;
    }
    return hr;
}

HRESULT ElgatoFrameProvider::FindUnconnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
{
    IEnumPins *pEnum = NULL;
    IPin *pPin = NULL;
    BOOL bFound = FALSE;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
    {
        goto done;
    }

    while (S_OK == pEnum->Next(1, &pPin, NULL))
    {
        hr = MatchPin(pPin, PinDir, FALSE, &bFound);
        if (FAILED(hr))
        {
            goto done;
        }
        if (bFound)
        {
            *ppPin = pPin;
            (*ppPin)->AddRef();
            break;
        }
        if (pPin != nullptr)
        {
            pPin->Release();
            pPin = nullptr;
        }
    }

    if (!bFound)
    {
        hr = VFW_E_NOT_FOUND;
    }

done:
    if (pPin != nullptr)
    {
        pPin->Release();
        pPin = nullptr;
    }

    if (pEnum != nullptr)
    {
        pEnum->Release();
        pEnum = nullptr;
    }
    return hr;
}

HRESULT ElgatoFrameProvider::IsPinConnected(IPin *pPin, BOOL *pResult)
{
    IPin *pTmp = NULL;
    HRESULT hr = pPin->ConnectedTo(&pTmp);
    if (SUCCEEDED(hr))
    {
        *pResult = TRUE;
    }
    else if (hr == VFW_E_NOT_CONNECTED)
    {
        // The pin is not connected. This is not an error for our purposes.
        *pResult = FALSE;
        hr = S_OK;
    }

    if (pTmp != nullptr)
    {
        pTmp->Release();
        pTmp = nullptr;
    }
    return hr;
}

HRESULT ElgatoFrameProvider::IsPinDirection(IPin *pPin, PIN_DIRECTION dir, BOOL *pResult)
{
    PIN_DIRECTION pinDir;
    HRESULT hr = pPin->QueryDirection(&pinDir);
    if (SUCCEEDED(hr))
    {
        *pResult = (pinDir == dir);
    }
    return hr;
}
#pragma endregion

#endif
