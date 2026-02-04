#pragma once

#include <atomic>
#include <thread>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

class XVideoRecorder
{
public:
    XVideoRecorder() noexcept;
    ~XVideoRecorder();

    bool Start(const CString& pFilePath, HWND pSourceWindow, const CRect& pCaptureRect, int pFPS, bool pGrayscale);
    void Stop();
    bool IsRecording() const { return _Recording.load(); }

    CString GetLastError() const { return _LastError; }

private:
    std::atomic<bool>       _Recording;
    std::thread             _RecordThread;
    CString                 _FilePath;
    HWND                    _SourceWindow;
    CRect                   _CaptureRect;
    int                     _FPS;
    bool                    _Grayscale;
    CString                 _LastError;

    IMFSinkWriter*          _SinkWriter;
    DWORD                   _StreamIndex;

    void RecordLoop();
    bool InitializeSinkWriter();
    void ReleaseSinkWriter();
    bool CaptureFrame(std::vector<BYTE>& pBuffer, int& pWidth, int& pHeight);
    bool WriteFrame(const std::vector<BYTE>& pBuffer, int pWidth, int pHeight, LONGLONG pTimestamp);
    void ConvertToGrayscale(std::vector<BYTE>& pBuffer, int pWidth, int pHeight);
};
