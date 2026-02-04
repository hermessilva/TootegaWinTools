#include "pch.h"
#include "XVideoRecorder.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

XVideoRecorder::XVideoRecorder() noexcept
    : _Recording(false)
    , _SourceWindow(nullptr)
    , _FPS(30)
    , _Grayscale(false)
    , _SinkWriter(nullptr)
    , _StreamIndex(0)
{
}

XVideoRecorder::~XVideoRecorder()
{
    Stop();
}

bool XVideoRecorder::Start(const CString& pFilePath, HWND pSourceWindow, const CRect& pCaptureRect, int pFPS, bool pGrayscale)
{
    if (_Recording.load())
    {
        _LastError = _T("Recording already in progress");
        return false;
    }

    if (!pSourceWindow || !IsWindow(pSourceWindow))
    {
        _LastError = _T("Invalid source window");
        return false;
    }

    if (pCaptureRect.IsRectEmpty())
    {
        _LastError = _T("Invalid capture rectangle");
        return false;
    }

    _FilePath = pFilePath;
    _SourceWindow = pSourceWindow;
    _CaptureRect = pCaptureRect;
    _FPS = pFPS > 0 ? pFPS : 30;
    _Grayscale = pGrayscale;

    _Recording.store(true);
    _RecordThread = std::thread(&XVideoRecorder::RecordLoop, this);

    return true;
}

void XVideoRecorder::Stop()
{
    if (_Recording.load())
    {
        _Recording.store(false);
        if (_RecordThread.joinable())
            _RecordThread.join();
    }
}

void XVideoRecorder::RecordLoop()
{
    if (!InitializeSinkWriter())
    {
        _Recording.store(false);
        return;
    }

    HRESULT hr = _SinkWriter->BeginWriting();
    if (FAILED(hr))
    {
        _LastError.Format(_T("Failed to begin writing: 0x%08X"), hr);
        ReleaseSinkWriter();
        _Recording.store(false);
        return;
    }

    LONGLONG frameInterval = 10000000LL / _FPS;
    LONGLONG timestamp = 0;
    DWORD sleepTime = 1000 / _FPS;

    std::vector<BYTE> buffer;
    int width = 0, height = 0;

    while (_Recording.load())
    {
        DWORD startTime = GetTickCount();

        if (CaptureFrame(buffer, width, height))
        {
            if (_Grayscale)
                ConvertToGrayscale(buffer, width, height);

            WriteFrame(buffer, width, height, timestamp);
            timestamp += frameInterval;
        }

        DWORD elapsed = GetTickCount() - startTime;
        if (elapsed < sleepTime)
            Sleep(sleepTime - elapsed);
    }

    _SinkWriter->Finalize();
    ReleaseSinkWriter();
}

bool XVideoRecorder::InitializeSinkWriter()
{
    int width = _CaptureRect.Width();
    int height = _CaptureRect.Height();

    if (width % 2 != 0) width++;
    if (height % 2 != 0) height++;

    IMFMediaType* pMediaTypeOut = nullptr;
    IMFMediaType* pMediaTypeIn = nullptr;

    HRESULT hr = MFCreateSinkWriterFromURL(CT2W(_FilePath), nullptr, nullptr, &_SinkWriter);
    if (FAILED(hr))
    {
        _LastError.Format(_T("Failed to create sink writer: 0x%08X"), hr);
        return false;
    }

    hr = MFCreateMediaType(&pMediaTypeOut);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 4000000);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) goto cleanup;

    hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, width, height);
    if (FAILED(hr)) goto cleanup;

    hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, _FPS, 1);
    if (FAILED(hr)) goto cleanup;

    hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) goto cleanup;

    hr = _SinkWriter->AddStream(pMediaTypeOut, &_StreamIndex);
    if (FAILED(hr))
    {
        _LastError.Format(_T("Failed to add stream: 0x%08X"), hr);
        goto cleanup;
    }

    hr = MFCreateMediaType(&pMediaTypeIn);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr)) goto cleanup;

    hr = pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (FAILED(hr)) goto cleanup;

    hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, width, height);
    if (FAILED(hr)) goto cleanup;

    hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, _FPS, 1);
    if (FAILED(hr)) goto cleanup;

    hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr)) goto cleanup;

    hr = _SinkWriter->SetInputMediaType(_StreamIndex, pMediaTypeIn, nullptr);
    if (FAILED(hr))
    {
        _LastError.Format(_T("Failed to set input media type: 0x%08X"), hr);
        goto cleanup;
    }

cleanup:
    if (pMediaTypeOut) pMediaTypeOut->Release();
    if (pMediaTypeIn) pMediaTypeIn->Release();

    if (FAILED(hr))
    {
        if (_SinkWriter)
        {
            _SinkWriter->Release();
            _SinkWriter = nullptr;
        }
        return false;
    }

    return true;
}

void XVideoRecorder::ReleaseSinkWriter()
{
    if (_SinkWriter)
    {
        _SinkWriter->Release();
        _SinkWriter = nullptr;
    }
}

bool XVideoRecorder::CaptureFrame(std::vector<BYTE>& pBuffer, int& pWidth, int& pHeight)
{
    if (!_SourceWindow || !IsWindow(_SourceWindow))
        return false;

    RECT windowRect;
    if (!GetWindowRect(_SourceWindow, &windowRect))
        return false;

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    pWidth = _CaptureRect.Width();
    pHeight = _CaptureRect.Height();

    if (pWidth % 2 != 0) pWidth++;
    if (pHeight % 2 != 0) pHeight++;

    HDC hdcWindow = GetDC(_SourceWindow);
    if (!hdcWindow)
        return false;

    // First, capture the entire window
    HDC hdcFullWindow = CreateCompatibleDC(hdcWindow);
    HBITMAP hFullBitmap = CreateCompatibleBitmap(hdcWindow, windowWidth, windowHeight);
    HBITMAP hOldFullBitmap = static_cast<HBITMAP>(SelectObject(hdcFullWindow, hFullBitmap));

    BOOL result = PrintWindow(_SourceWindow, hdcFullWindow, PW_RENDERFULLCONTENT);
    if (!result)
    {
        HDC hdcScreen = GetDC(nullptr);
        BitBlt(hdcFullWindow, 0, 0, windowWidth, windowHeight, hdcScreen, windowRect.left, windowRect.top, SRCCOPY);
        ReleaseDC(nullptr, hdcScreen);
    }

    // Now create the cropped DIB for output
    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = pWidth;
    bi.bmiHeader.biHeight = -pHeight;  // Top-down DIB
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBitmap)
    {
        SelectObject(hdcFullWindow, hOldFullBitmap);
        DeleteObject(hFullBitmap);
        DeleteDC(hdcFullWindow);
        DeleteDC(hdcMem);
        ReleaseDC(_SourceWindow, hdcWindow);
        return false;
    }

    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, hBitmap));

    // Copy only the capture rectangle from the full window capture
    BitBlt(hdcMem, 0, 0, pWidth, pHeight, hdcFullWindow, _CaptureRect.left, _CaptureRect.top, SRCCOPY);

    size_t bufferSize = static_cast<size_t>(pWidth) * pHeight * 4;
    pBuffer.resize(bufferSize);
    memcpy(pBuffer.data(), pBits, bufferSize);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    SelectObject(hdcFullWindow, hOldFullBitmap);
    DeleteObject(hFullBitmap);
    DeleteDC(hdcFullWindow);
    ReleaseDC(_SourceWindow, hdcWindow);

    return true;
}

bool XVideoRecorder::WriteFrame(const std::vector<BYTE>& pBuffer, int pWidth, int pHeight, LONGLONG pTimestamp)
{
    if (!_SinkWriter)
        return false;

    DWORD cbWidth = pWidth * 4;
    DWORD cbBuffer = cbWidth * pHeight;

    IMFSample* pSample = nullptr;
    IMFMediaBuffer* pMediaBuffer = nullptr;

    HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pMediaBuffer);
    if (FAILED(hr))
        return false;

    BYTE* pData = nullptr;
    hr = pMediaBuffer->Lock(&pData, nullptr, nullptr);
    if (SUCCEEDED(hr))
    {
        // Media Foundation expects bottom-up image, but we capture top-down
        // Flip the image vertically
        for (int y = 0; y < pHeight; ++y)
        {
            const BYTE* srcRow = pBuffer.data() + (pHeight - 1 - y) * cbWidth;
            BYTE* dstRow = pData + y * cbWidth;
            memcpy(dstRow, srcRow, cbWidth);
        }
        pMediaBuffer->Unlock();
    }

    hr = pMediaBuffer->SetCurrentLength(cbBuffer);
    if (FAILED(hr))
    {
        pMediaBuffer->Release();
        return false;
    }

    hr = MFCreateSample(&pSample);
    if (FAILED(hr))
    {
        pMediaBuffer->Release();
        return false;
    }

    hr = pSample->AddBuffer(pMediaBuffer);
    if (FAILED(hr))
    {
        pSample->Release();
        pMediaBuffer->Release();
        return false;
    }

    hr = pSample->SetSampleTime(pTimestamp);
    if (FAILED(hr))
    {
        pSample->Release();
        pMediaBuffer->Release();
        return false;
    }

    LONGLONG duration = 10000000LL / _FPS;
    hr = pSample->SetSampleDuration(duration);
    if (FAILED(hr))
    {
        pSample->Release();
        pMediaBuffer->Release();
        return false;
    }

    hr = _SinkWriter->WriteSample(_StreamIndex, pSample);

    pSample->Release();
    pMediaBuffer->Release();

    return SUCCEEDED(hr);
}

void XVideoRecorder::ConvertToGrayscale(std::vector<BYTE>& pBuffer, int pWidth, int pHeight)
{
    for (int y = 0; y < pHeight; ++y)
    {
        for (int x = 0; x < pWidth; ++x)
        {
            size_t idx = (static_cast<size_t>(y) * pWidth + x) * 4;
            BYTE b = pBuffer[idx];
            BYTE g = pBuffer[idx + 1];
            BYTE r = pBuffer[idx + 2];

            BYTE gray = static_cast<BYTE>(0.299 * r + 0.587 * g + 0.114 * b);

            pBuffer[idx] = gray;
            pBuffer[idx + 1] = gray;
            pBuffer[idx + 2] = gray;
        }
    }
}
