#include "pch.h"
#include "XVideoEditorDocument.h"
#include "Resource.h"

#include <mfapi.h>
#include <mferror.h>
#include <propvarutil.h>
#include <codecapi.h>
#include <wmcodecdsp.h>

#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "mfuuid.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(XVideoEditorDocument, CDocument)

BEGIN_MESSAGE_MAP(XVideoEditorDocument, CDocument)
END_MESSAGE_MAP()

XVideoEditorDocument::XVideoEditorDocument() noexcept
    : _SourceReader(nullptr)
    , _OutputMediaType(nullptr)
    , _MarkIn(-1)
    , _MarkOut(-1)
    , _CachedFramePosition(-1)
{
    ZeroMemory(&_VideoInfo, sizeof(_VideoInfo));
}

XVideoEditorDocument::~XVideoEditorDocument()
{
    ReleaseResources();
}

BOOL XVideoEditorDocument::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;

    return TRUE;
}

BOOL XVideoEditorDocument::OnOpenDocument(LPCTSTR pPathName)
{
    ReleaseResources();

    _FilePath = pPathName;

    HRESULT hr = InitializeSourceReader(pPathName);
    if (FAILED(hr))
    {
        CString msg;
        if (hr == 0xC00D5212 || hr == static_cast<HRESULT>(0xC00D5212))
            msg = _T("The video format or codec is not supported.\n\nPlease convert the video to a compatible format (H.264/MP4).");
        else if (hr == 0xC00D36C4 || hr == static_cast<HRESULT>(0xC00D36C4))
            msg = _T("Cannot open the video file.\n\nThe file may be corrupted or in use by another application.");
        else
            msg.Format(_T("Failed to open video file.\n\nError code: 0x%08X"), hr);
        AfxMessageBox(msg, MB_ICONERROR);
        return FALSE;
    }

    hr = ExtractVideoInfo();
    if (FAILED(hr))
    {
        AfxMessageBox(_T("Failed to extract video information."), MB_ICONERROR);
        ReleaseResources();
        return FALSE;
    }

    CString title = PathFindFileName(pPathName);
    SetTitle(title);

    _MarkIn = -1;
    _MarkOut = -1;
    _CachedFramePosition = -1;

    return TRUE;
}

void XVideoEditorDocument::OnCloseDocument()
{
    ReleaseResources();
    CDocument::OnCloseDocument();
}

void XVideoEditorDocument::Serialize(CArchive& ar)
{
    if (ar.IsStoring())
    {
    }
    else
    {
    }
}

HRESULT XVideoEditorDocument::InitializeSourceReader(LPCTSTR pPath)
{
    IMFAttributes* pAttributes = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttributes, 2);
    if (FAILED(hr))
        return hr;

    hr = pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return hr;
    }

    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return hr;
    }

    hr = MFCreateSourceReaderFromURL(pPath, pAttributes, &_SourceReader);
    pAttributes->Release();

    if (FAILED(hr))
        return hr;

    hr = MFCreateMediaType(&_OutputMediaType);
    if (FAILED(hr))
        return hr;

    hr = _OutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr))
        return hr;

    hr = _OutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr))
        return hr;

    hr = _SourceReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, _OutputMediaType);

    return hr;
}

HRESULT XVideoEditorDocument::ExtractVideoInfo()
{
    if (!_SourceReader)
        return E_POINTER;

    PROPVARIANT var;
    PropVariantInit(&var);

    HRESULT hr = _SourceReader->GetPresentationAttribute(
        MF_SOURCE_READER_MEDIASOURCE,
        MF_PD_DURATION,
        &var);

    if (SUCCEEDED(hr))
    {
        _VideoInfo.Duration = var.uhVal.QuadPart;
        PropVariantClear(&var);
    }

    IMFMediaType* pNativeType = nullptr;
    hr = _SourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType);
    if (SUCCEEDED(hr))
    {
        UINT32 width = 0, height = 0;
        MFGetAttributeSize(pNativeType, MF_MT_FRAME_SIZE, &width, &height);
        _VideoInfo.Width = width;
        _VideoInfo.Height = height;

        UINT32 num = 0, denom = 1;
        MFGetAttributeRatio(pNativeType, MF_MT_FRAME_RATE, &num, &denom);
        _VideoInfo.FrameRate = (denom > 0) ? static_cast<double>(num) / denom : 30.0;

        UINT32 bitrate = 0;
        if (SUCCEEDED(pNativeType->GetUINT32(MF_MT_AVG_BITRATE, &bitrate)))
            _VideoInfo.Bitrate = bitrate;
        else
            _VideoInfo.Bitrate = static_cast<UINT32>(width * height * _VideoInfo.FrameRate * 0.15);

        pNativeType->GetGUID(MF_MT_SUBTYPE, &_VideoInfo.VideoFormat);
        pNativeType->Release();
    }

    _VideoInfo.TotalFrames = static_cast<UINT64>(
        (_VideoInfo.Duration / 10000000.0) * _VideoInfo.FrameRate);

    IMFMediaType* pAudioType = nullptr;
    hr = _SourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &pAudioType);
    _VideoInfo.HasAudio = SUCCEEDED(hr);
    if (pAudioType)
    {
        pAudioType->GetGUID(MF_MT_SUBTYPE, &_VideoInfo.AudioFormat);
        pAudioType->Release();
    }

    return S_OK;
}

void XVideoEditorDocument::ReleaseResources()
{
    _CachedFrameBitmap.DeleteObject();
    _CachedFramePosition = -1;

    if (_OutputMediaType)
    {
        _OutputMediaType->Release();
        _OutputMediaType = nullptr;
    }

    if (_SourceReader)
    {
        _SourceReader->Release();
        _SourceReader = nullptr;
    }

    ZeroMemory(&_VideoInfo, sizeof(_VideoInfo));
}

HRESULT XVideoEditorDocument::FlipSampleVertically(IMFSample* pSample)
{
    if (!pSample)
        return E_POINTER;

    IMFMediaBuffer* pBuffer = nullptr;
    HRESULT hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (FAILED(hr))
        return hr;

    BYTE* pData = nullptr;
    DWORD cbMaxLen = 0, cbCurLen = 0;
    hr = pBuffer->Lock(&pData, &cbMaxLen, &cbCurLen);
    if (FAILED(hr))
    {
        pBuffer->Release();
        return hr;
    }

    UINT32 stride = _VideoInfo.Width * 4;
    UINT32 height = _VideoInfo.Height;

    std::vector<BYTE> tempRow(stride);
    
    for (UINT32 y = 0; y < height / 2; y++)
    {
        BYTE* pTop = pData + y * stride;
        BYTE* pBottom = pData + (height - 1 - y) * stride;
        
        memcpy(tempRow.data(), pTop, stride);
        memcpy(pTop, pBottom, stride);
        memcpy(pBottom, tempRow.data(), stride);
    }

    pBuffer->Unlock();
    pBuffer->Release();
    
    return S_OK;
}

LONGLONG XVideoEditorDocument::FrameToPosition(UINT64 pFrame) const
{
    if (_VideoInfo.FrameRate <= 0)
        return 0;

    return static_cast<LONGLONG>((pFrame / _VideoInfo.FrameRate) * 10000000.0);
}

UINT64 XVideoEditorDocument::PositionToFrame(LONGLONG pPosition) const
{
    if (_VideoInfo.FrameRate <= 0)
        return 0;

    return static_cast<UINT64>((pPosition / 10000000.0) * _VideoInfo.FrameRate);
}

HRESULT XVideoEditorDocument::ReadFrameAtPosition(LONGLONG pPosition, IMFSample** pSample)
{
    if (!_SourceReader || !pSample)
        return E_POINTER;

    *pSample = nullptr;

    PROPVARIANT varPosition;
    PropVariantInit(&varPosition);
    varPosition.vt = VT_I8;
    varPosition.hVal.QuadPart = pPosition;

    HRESULT hr = _SourceReader->SetCurrentPosition(GUID_NULL, varPosition);
    PropVariantClear(&varPosition);

    if (FAILED(hr))
        return hr;

    DWORD dwFlags = 0;
    LONGLONG timestamp = 0;

    hr = _SourceReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        nullptr,
        &dwFlags,
        &timestamp,
        pSample);

    return hr;
}

HRESULT XVideoEditorDocument::GetFrameBitmap(LONGLONG pPosition, CBitmap& pBitmap)
{
    IMFSample* pSample = nullptr;
    HRESULT hr = ReadFrameAtPosition(pPosition, &pSample);
    if (FAILED(hr) || !pSample)
        return hr;

    IMFMediaBuffer* pBuffer = nullptr;
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (FAILED(hr))
    {
        pSample->Release();
        return hr;
    }

    BYTE* pData = nullptr;
    DWORD cbMaxLen = 0, cbCurLen = 0;
    hr = pBuffer->Lock(&pData, &cbMaxLen, &cbCurLen);
    if (FAILED(hr))
    {
        pBuffer->Release();
        pSample->Release();
        return hr;
    }

    pBitmap.DeleteObject();

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = _VideoInfo.Width;
    bmi.bmiHeader.biHeight = -static_cast<LONG>(_VideoInfo.Height);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hdc = ::GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    ::ReleaseDC(nullptr, hdc);

    if (hBmp && pBits)
    {
        UINT32 stride = _VideoInfo.Width * 4;
        memcpy(pBits, pData, stride * _VideoInfo.Height);
        pBitmap.Attach(hBmp);
    }

    pBuffer->Unlock();
    pBuffer->Release();
    pSample->Release();

    return S_OK;
}

HRESULT XVideoEditorDocument::GetFrameBitmapFast(LONGLONG pPosition, CBitmap& pBitmap)
{
    LONGLONG frameDuration = (_VideoInfo.FrameRate > 0) ? 
        static_cast<LONGLONG>(10000000.0 / _VideoInfo.FrameRate) : 333333;
    
    if (_CachedFramePosition >= 0 && 
        llabs(pPosition - _CachedFramePosition) < frameDuration &&
        _CachedFrameBitmap.m_hObject)
    {
        return S_OK;
    }

    HRESULT hr = GetFrameBitmap(pPosition, _CachedFrameBitmap);
    if (SUCCEEDED(hr))
    {
        _CachedFramePosition = pPosition;
        pBitmap.DeleteObject();
        
        if (_CachedFrameBitmap.m_hObject)
        {
            HBITMAP hBmpCopy = (HBITMAP)CopyImage(
                (HBITMAP)_CachedFrameBitmap.GetSafeHandle(),
                IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
            if (hBmpCopy)
                pBitmap.Attach(hBmpCopy);
        }
    }
    
    return hr;
}

HRESULT XVideoEditorDocument::FindNearestKeyframe(LONGLONG pPosition, LONGLONG& pKeyframePosition)
{
    if (!_SourceReader)
        return E_POINTER;

    PROPVARIANT varPosition;
    PropVariantInit(&varPosition);
    varPosition.vt = VT_I8;
    varPosition.hVal.QuadPart = pPosition;

    HRESULT hr = _SourceReader->SetCurrentPosition(GUID_NULL, varPosition);
    PropVariantClear(&varPosition);

    if (FAILED(hr))
        return hr;

    DWORD dwFlags = 0;
    LONGLONG timestamp = 0;
    IMFSample* pSample = nullptr;

    while (TRUE)
    {
        hr = _SourceReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            nullptr,
            &dwFlags,
            &timestamp,
            &pSample);

        if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
            break;

        if (pSample)
        {
            pKeyframePosition = timestamp;
            pSample->Release();
            break;
        }
    }

    return hr;
}

HRESULT XVideoEditorDocument::ConfigureH264Encoder(IMFSinkWriter* pSinkWriter, DWORD* pStreamIndex)
{
    return ConfigureH264EncoderWithSize(pSinkWriter, pStreamIndex, _VideoInfo.Width, _VideoInfo.Height);
}

HRESULT XVideoEditorDocument::ConfigureH264EncoderWithSize(IMFSinkWriter* pSinkWriter, DWORD* pStreamIndex, UINT32 pWidth, UINT32 pHeight)
{
    if (!pSinkWriter || !pStreamIndex)
        return E_POINTER;

    IMFMediaType* pOutputType = nullptr;
    HRESULT hr = MFCreateMediaType(&pOutputType);
    if (FAILED(hr))
        return hr;

    hr = pOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = pOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    UINT32 targetBitrate = _VideoInfo.Bitrate;
    if (targetBitrate == 0)
    {
        double bpp = 0.07;
        targetBitrate = static_cast<UINT32>(pWidth * pHeight * _VideoInfo.FrameRate * bpp);
    }
    else
    {
        double sizeRatio = static_cast<double>(pWidth * pHeight) / (_VideoInfo.Width * _VideoInfo.Height);
        targetBitrate = static_cast<UINT32>(targetBitrate * sizeRatio * 0.8);
    }
    
    targetBitrate = max(500000u, min(targetBitrate, 20000000u));

    hr = pOutputType->SetUINT32(MF_MT_AVG_BITRATE, targetBitrate);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = MFSetAttributeSize(pOutputType, MF_MT_FRAME_SIZE, pWidth, pHeight);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    UINT32 fpsNum = static_cast<UINT32>(_VideoInfo.FrameRate * 1000);
    hr = MFSetAttributeRatio(pOutputType, MF_MT_FRAME_RATE, fpsNum, 1000);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = MFSetAttributeRatio(pOutputType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = pOutputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = pOutputType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = pOutputType->SetUINT32(MF_MT_MPEG2_LEVEL, eAVEncH264VLevel4);
    if (FAILED(hr)) { pOutputType->Release(); return hr; }

    hr = pSinkWriter->AddStream(pOutputType, pStreamIndex);
    pOutputType->Release();

    if (FAILED(hr))
        return hr;

    IMFMediaType* pInputType = nullptr;
    hr = MFCreateMediaType(&pInputType);
    if (FAILED(hr))
        return hr;

    hr = pInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) { pInputType->Release(); return hr; }

    hr = pInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr)) { pInputType->Release(); return hr; }

    hr = MFSetAttributeSize(pInputType, MF_MT_FRAME_SIZE, pWidth, pHeight);
    if (FAILED(hr)) { pInputType->Release(); return hr; }

    hr = MFSetAttributeRatio(pInputType, MF_MT_FRAME_RATE, fpsNum, 1000);
    if (FAILED(hr)) { pInputType->Release(); return hr; }

    hr = MFSetAttributeRatio(pInputType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) { pInputType->Release(); return hr; }

    hr = pInputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) { pInputType->Release(); return hr; }

    hr = pSinkWriter->SetInputMediaType(*pStreamIndex, pInputType, nullptr);
    pInputType->Release();

    return hr;
}

HRESULT XVideoEditorDocument::ConfigureAACEncoder(IMFSinkWriter* pSinkWriter, DWORD* pStreamIndex)
{
    if (!pSinkWriter || !pStreamIndex || !_VideoInfo.HasAudio)
        return E_INVALIDARG;

    IMFMediaType* pAudioOutputType = nullptr;
    HRESULT hr = MFCreateMediaType(&pAudioOutputType);
    if (FAILED(hr))
        return hr;

    hr = pAudioOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) { pAudioOutputType->Release(); return hr; }

    hr = pAudioOutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    if (FAILED(hr)) { pAudioOutputType->Release(); return hr; }

    hr = pAudioOutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (FAILED(hr)) { pAudioOutputType->Release(); return hr; }

    hr = pAudioOutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    if (FAILED(hr)) { pAudioOutputType->Release(); return hr; }

    hr = pAudioOutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    if (FAILED(hr)) { pAudioOutputType->Release(); return hr; }

    hr = pAudioOutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
    if (FAILED(hr)) { pAudioOutputType->Release(); return hr; }

    hr = pSinkWriter->AddStream(pAudioOutputType, pStreamIndex);
    pAudioOutputType->Release();

    if (FAILED(hr))
        return hr;

    IMFMediaType* pAudioInputType = nullptr;
    hr = MFCreateMediaType(&pAudioInputType);
    if (FAILED(hr))
        return hr;

    hr = pAudioInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr)) { pAudioInputType->Release(); return hr; }

    hr = pAudioInputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(hr)) { pAudioInputType->Release(); return hr; }

    hr = pAudioInputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (FAILED(hr)) { pAudioInputType->Release(); return hr; }

    hr = pAudioInputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    if (FAILED(hr)) { pAudioInputType->Release(); return hr; }

    hr = pAudioInputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2);
    if (FAILED(hr)) { pAudioInputType->Release(); return hr; }

    hr = pSinkWriter->SetInputMediaType(*pStreamIndex, pAudioInputType, nullptr);
    pAudioInputType->Release();

    return hr;
}

HRESULT XVideoEditorDocument::ExportRange(LPCTSTR pOutputPath, LONGLONG pStart, LONGLONG pEnd, XExportCallback pCallback)
{
    if (!_SourceReader)
        return E_POINTER;

    ULONGLONG startTime = GetTickCount64();

    UINT64 startFrame = PositionToFrame(pStart);
    UINT64 endFrame = PositionToFrame(pEnd);
    
    if (endFrame <= startFrame)
        return E_INVALIDARG;
    
    UINT64 totalFramesToExport = endFrame - startFrame + 1;

    LONGLONG keyframePos = pStart;
    FindNearestKeyframe(pStart, keyframePos);

    IMFSinkWriter* pSinkWriter = nullptr;
    IMFAttributes* pAttributes = nullptr;

    HRESULT hr = MFCreateAttributes(&pAttributes, 4);
    if (FAILED(hr))
        return hr;

    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return hr;
    }

    hr = pAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return hr;
    }

    hr = pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return hr;
    }

    hr = pAttributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
    if (FAILED(hr))
    {
        pAttributes->Release();
        return hr;
    }

    hr = MFCreateSinkWriterFromURL(pOutputPath, nullptr, pAttributes, &pSinkWriter);
    pAttributes->Release();

    if (FAILED(hr))
        return hr;

    DWORD videoStreamIndex = 0;
    hr = ConfigureH264Encoder(pSinkWriter, &videoStreamIndex);
    if (FAILED(hr))
    {
        pSinkWriter->Release();
        return hr;
    }

    DWORD audioStreamIndex = 0;
    bool hasAudio = false;
    if (_VideoInfo.HasAudio)
    {
        IMFMediaType* pAudioType = nullptr;
        hr = MFCreateMediaType(&pAudioType);
        if (SUCCEEDED(hr))
        {
            pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            _SourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pAudioType);
            pAudioType->Release();
        }

        hr = ConfigureAACEncoder(pSinkWriter, &audioStreamIndex);
        hasAudio = SUCCEEDED(hr);
    }

    hr = pSinkWriter->BeginWriting();
    if (FAILED(hr))
    {
        pSinkWriter->Release();
        return hr;
    }

    PROPVARIANT varPosition;
    PropVariantInit(&varPosition);
    varPosition.vt = VT_I8;
    varPosition.hVal.QuadPart = keyframePos;
    _SourceReader->SetCurrentPosition(GUID_NULL, varPosition);
    PropVariantClear(&varPosition);

    LONGLONG firstTimestamp = -1;
    bool startWriting = false;
    UINT64 framesWritten = 0;
    bool cancelled = false;
    UINT64 lastCallbackFrame = 0;

    while (!cancelled && framesWritten < totalFramesToExport)
    {
        DWORD dwFlags = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;

        hr = _SourceReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            nullptr,
            &dwFlags,
            &timestamp,
            &pSample);

        if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            if (pSample)
                pSample->Release();
            break;
        }

        if (timestamp >= pEnd)
        {
            if (pSample)
                pSample->Release();
            break;
        }

        if (pSample)
        {
            if (timestamp >= pStart)
                startWriting = true;

            if (startWriting && framesWritten < totalFramesToExport)
            {
                if (firstTimestamp < 0)
                    firstTimestamp = timestamp;
                
                LONGLONG sampleDuration = 0;
                pSample->GetSampleDuration(&sampleDuration);
                if (sampleDuration <= 0)
                    sampleDuration = 333333;
                
                FlipSampleVertically(pSample);
                
                pSample->SetSampleTime(timestamp - firstTimestamp);
                pSample->SetSampleDuration(sampleDuration);
                hr = pSinkWriter->WriteSample(videoStreamIndex, pSample);
                framesWritten++;

                if (pCallback && (framesWritten - lastCallbackFrame >= 5 || framesWritten >= totalFramesToExport))
                {
                    lastCallbackFrame = framesWritten;
                    
                    XExportProgress progress;
                    progress.CurrentFrame = framesWritten;
                    progress.TotalFrames = totalFramesToExport;
                    
                    ULONGLONG elapsed = GetTickCount64() - startTime;
                    progress.ElapsedSeconds = elapsed / 1000.0;
                    
                    if (framesWritten > 0 && elapsed > 100)
                    {
                        double framesPerSecond = (framesWritten * 1000.0) / elapsed;
                        if (framesPerSecond > 0.01)
                        {
                            UINT64 remainingFrames = (totalFramesToExport > framesWritten) ? 
                                (totalFramesToExport - framesWritten) : 0;
                            progress.EstimatedSecondsRemaining = remainingFrames / framesPerSecond;
                        }
                        else
                        {
                            progress.EstimatedSecondsRemaining = 0;
                        }
                    }
                    else
                    {
                        progress.EstimatedSecondsRemaining = 0;
                    }
                    progress.Cancelled = false;

                    if (!pCallback(progress))
                    {
                        cancelled = true;
                        pSample->Release();
                        break;
                    }
                }
            }

            pSample->Release();
        }
    }

    if (hasAudio && !cancelled)
    {
        PropVariantInit(&varPosition);
        varPosition.vt = VT_I8;
        varPosition.hVal.QuadPart = pStart;
        _SourceReader->SetCurrentPosition(GUID_NULL, varPosition);
        PropVariantClear(&varPosition);

        while (!cancelled)
        {
            DWORD dwFlags = 0;
            LONGLONG timestamp = 0;
            IMFSample* pSample = nullptr;

            hr = _SourceReader->ReadSample(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                0,
                nullptr,
                &dwFlags,
                &timestamp,
                &pSample);

            if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
            {
                if (pSample)
                    pSample->Release();
                break;
            }

            if (timestamp >= pEnd)
            {
                if (pSample)
                    pSample->Release();
                break;
            }

            if (pSample && timestamp >= pStart)
            {
                LONGLONG adjustedTime = timestamp - pStart;
                pSample->SetSampleTime(adjustedTime);
                pSinkWriter->WriteSample(audioStreamIndex, pSample);
            }

            if (pSample)
                pSample->Release();
        }
    }

    if (cancelled)
    {
        pSinkWriter->Release();
        DeleteFile(pOutputPath);
        return E_ABORT;
    }

    hr = pSinkWriter->Finalize();
    pSinkWriter->Release();

    return hr;
}
HRESULT XVideoEditorDocument::ExportRangeWithCrop(LPCTSTR pOutputPath, LONGLONG pStart, LONGLONG pEnd, const CRect& pCropRect, XExportCallback pCallback)
{
    if (!_SourceReader)
        return E_POINTER;

    UINT32 cropWidth = pCropRect.Width();
    UINT32 cropHeight = pCropRect.Height();
    
    cropWidth = (cropWidth + 1) & ~1;
    cropHeight = (cropHeight + 1) & ~1;
    
    if (cropWidth < 16 || cropHeight < 16)
        return E_INVALIDARG;

    ULONGLONG startTime = GetTickCount64();

    UINT64 startFrame = PositionToFrame(pStart);
    UINT64 endFrame = PositionToFrame(pEnd);
    
    if (endFrame <= startFrame)
        return E_INVALIDARG;
    
    UINT64 totalFramesToExport = endFrame - startFrame + 1;

    LONGLONG keyframePos = pStart;
    FindNearestKeyframe(pStart, keyframePos);

    IMFSinkWriter* pSinkWriter = nullptr;
    IMFAttributes* pAttributes = nullptr;

    HRESULT hr = MFCreateAttributes(&pAttributes, 4);
    if (FAILED(hr))
        return hr;

    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    if (FAILED(hr)) { pAttributes->Release(); return hr; }

    hr = pAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    if (FAILED(hr)) { pAttributes->Release(); return hr; }

    hr = pAttributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
    if (FAILED(hr)) { pAttributes->Release(); return hr; }

    hr = MFCreateSinkWriterFromURL(pOutputPath, nullptr, pAttributes, &pSinkWriter);
    pAttributes->Release();

    if (FAILED(hr))
        return hr;

    DWORD videoStreamIndex = 0;
    hr = ConfigureH264EncoderWithSize(pSinkWriter, &videoStreamIndex, cropWidth, cropHeight);
    if (FAILED(hr))
    {
        pSinkWriter->Release();
        return hr;
    }

    DWORD audioStreamIndex = 0;
    bool hasAudio = false;
    if (_VideoInfo.HasAudio)
    {
        IMFMediaType* pAudioType = nullptr;
        hr = MFCreateMediaType(&pAudioType);
        if (SUCCEEDED(hr))
        {
            pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            _SourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pAudioType);
            pAudioType->Release();
        }

        hr = ConfigureAACEncoder(pSinkWriter, &audioStreamIndex);
        hasAudio = SUCCEEDED(hr);
    }

    hr = pSinkWriter->BeginWriting();
    if (FAILED(hr))
    {
        pSinkWriter->Release();
        return hr;
    }

    PROPVARIANT varPosition;
    PropVariantInit(&varPosition);
    varPosition.vt = VT_I8;
    varPosition.hVal.QuadPart = keyframePos;
    _SourceReader->SetCurrentPosition(GUID_NULL, varPosition);
    PropVariantClear(&varPosition);

    LONGLONG firstTimestamp = -1;
    bool startWriting = false;
    UINT64 framesWritten = 0;
    bool cancelled = false;
    UINT64 lastCallbackFrame = 0;

    while (!cancelled && framesWritten < totalFramesToExport)
    {
        DWORD dwFlags = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;

        hr = _SourceReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            nullptr,
            &dwFlags,
            &timestamp,
            &pSample);

        if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            if (pSample)
                pSample->Release();
            break;
        }

        if (timestamp >= pEnd)
        {
            if (pSample)
                pSample->Release();
            break;
        }

        if (pSample)
        {
            if (timestamp >= pStart)
                startWriting = true;

            if (startWriting && framesWritten < totalFramesToExport)
            {
                if (firstTimestamp < 0)
                    firstTimestamp = timestamp;
                
                LONGLONG sampleDuration = 0;
                pSample->GetSampleDuration(&sampleDuration);
                if (sampleDuration <= 0)
                    sampleDuration = 333333;
                
                IMFMediaBuffer* pBuffer = nullptr;
                hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                if (SUCCEEDED(hr))
                {
                    BYTE* pData = nullptr;
                    DWORD cbMaxLen = 0, cbCurLen = 0;
                    hr = pBuffer->Lock(&pData, &cbMaxLen, &cbCurLen);
                    if (SUCCEEDED(hr))
                    {
                        UINT32 srcStride = _VideoInfo.Width * 4;
                        UINT32 dstStride = cropWidth * 4;
                        
                        IMFMediaBuffer* pCropBuffer = nullptr;
                        hr = MFCreateMemoryBuffer(dstStride * cropHeight, &pCropBuffer);
                        if (SUCCEEDED(hr))
                        {
                            BYTE* pCropData = nullptr;
                            DWORD cbCropMax = 0;
                            hr = pCropBuffer->Lock(&pCropData, &cbCropMax, nullptr);
                            if (SUCCEEDED(hr))
                            {
                                for (UINT32 y = 0; y < cropHeight; y++)
                                {
                                    UINT32 srcY = pCropRect.top + y;
                                    UINT32 dstY = cropHeight - 1 - y;
                                    
                                    if (srcY < _VideoInfo.Height)
                                    {
                                        BYTE* pSrcRow = pData + srcY * srcStride + pCropRect.left * 4;
                                        BYTE* pDstRow = pCropData + dstY * dstStride;
                                        memcpy(pDstRow, pSrcRow, dstStride);
                                    }
                                }
                                
                                pCropBuffer->Unlock();
                                pCropBuffer->SetCurrentLength(dstStride * cropHeight);
                                
                                IMFSample* pCropSample = nullptr;
                                hr = MFCreateSample(&pCropSample);
                                if (SUCCEEDED(hr))
                                {
                                    pCropSample->AddBuffer(pCropBuffer);
                                    pCropSample->SetSampleTime(timestamp - firstTimestamp);
                                    pCropSample->SetSampleDuration(sampleDuration);
                                    hr = pSinkWriter->WriteSample(videoStreamIndex, pCropSample);
                                    pCropSample->Release();
                                }
                            }
                            pCropBuffer->Release();
                        }
                        pBuffer->Unlock();
                    }
                    pBuffer->Release();
                }
                
                framesWritten++;

                if (pCallback && (framesWritten - lastCallbackFrame >= 5 || framesWritten >= totalFramesToExport))
                {
                    lastCallbackFrame = framesWritten;
                    
                    XExportProgress progress;
                    progress.CurrentFrame = framesWritten;
                    progress.TotalFrames = totalFramesToExport;
                    
                    ULONGLONG elapsed = GetTickCount64() - startTime;
                    progress.ElapsedSeconds = elapsed / 1000.0;
                    
                    if (framesWritten > 0 && elapsed > 100)
                    {
                        double framesPerSecond = (framesWritten * 1000.0) / elapsed;
                        if (framesPerSecond > 0.01)
                        {
                            UINT64 remainingFrames = (totalFramesToExport > framesWritten) ? 
                                (totalFramesToExport - framesWritten) : 0;
                            progress.EstimatedSecondsRemaining = remainingFrames / framesPerSecond;
                        }
                        else
                        {
                            progress.EstimatedSecondsRemaining = 0;
                        }
                    }
                    else
                    {
                        progress.EstimatedSecondsRemaining = 0;
                    }
                    progress.Cancelled = false;

                    if (!pCallback(progress))
                    {
                        cancelled = true;
                    }
                }
            }

            pSample->Release();
        }
    }

    if (hasAudio && !cancelled)
    {
        PropVariantInit(&varPosition);
        varPosition.vt = VT_I8;
        varPosition.hVal.QuadPart = pStart;
        _SourceReader->SetCurrentPosition(GUID_NULL, varPosition);
        PropVariantClear(&varPosition);

        while (!cancelled)
        {
            DWORD dwFlags = 0;
            LONGLONG timestamp = 0;
            IMFSample* pSample = nullptr;

            hr = _SourceReader->ReadSample(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                0,
                nullptr,
                &dwFlags,
                &timestamp,
                &pSample);

            if (FAILED(hr) || (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM))
            {
                if (pSample)
                    pSample->Release();
                break;
            }

            if (timestamp >= pEnd)
            {
                if (pSample)
                    pSample->Release();
                break;
            }

            if (pSample && timestamp >= pStart)
            {
                LONGLONG adjustedTime = timestamp - pStart;
                pSample->SetSampleTime(adjustedTime);
                pSinkWriter->WriteSample(audioStreamIndex, pSample);
            }

            if (pSample)
                pSample->Release();
        }
    }

    if (cancelled)
    {
        pSinkWriter->Release();
        DeleteFile(pOutputPath);
        return E_ABORT;
    }

    hr = pSinkWriter->Finalize();
    pSinkWriter->Release();

    return hr;
}