#pragma once

#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <functional>

struct XVideoInfo
{
    UINT64      Duration;
    UINT32      Width;
    UINT32      Height;
    double      FrameRate;
    UINT64      TotalFrames;
    UINT32      Bitrate;
    GUID        VideoFormat;
    GUID        AudioFormat;
    bool        HasAudio;
};

struct XExportProgress
{
    UINT64  CurrentFrame;
    UINT64  TotalFrames;
    double  ElapsedSeconds;
    double  EstimatedSecondsRemaining;
    bool    Cancelled;
};

using XExportCallback = std::function<bool(const XExportProgress&)>;

class XVideoEditorDocument : public CDocument
{
    DECLARE_DYNCREATE(XVideoEditorDocument)

public:
    XVideoEditorDocument() noexcept;
    virtual ~XVideoEditorDocument();

    virtual BOOL OnOpenDocument(LPCTSTR pPathName) override;
    virtual void OnCloseDocument() override;

    const XVideoInfo& GetVideoInfo() const { return _VideoInfo; }
    const CString& GetFilePath() const { return _FilePath; }

    IMFSourceReader* GetSourceReader() const { return _SourceReader; }

    HRESULT ReadFrameAtPosition(LONGLONG pPosition, IMFSample** pSample);
    HRESULT GetFrameBitmap(LONGLONG pPosition, CBitmap& pBitmap);
    HRESULT GetFrameBitmapFast(LONGLONG pPosition, CBitmap& pBitmap);
    CBitmap* GetCachedBitmap() { return _CachedFrameBitmap.m_hObject ? &_CachedFrameBitmap : nullptr; }
    
    LONGLONG FrameToPosition(UINT64 pFrame) const;
    UINT64 PositionToFrame(LONGLONG pPosition) const;

    HRESULT FindNearestKeyframe(LONGLONG pPosition, LONGLONG& pKeyframePosition);

    void SetMarkIn(LONGLONG pPosition) { _MarkIn = pPosition; }
    void SetMarkOut(LONGLONG pPosition) { _MarkOut = pPosition; }
    LONGLONG GetMarkIn() const { return _MarkIn; }
    LONGLONG GetMarkOut() const { return _MarkOut; }
    bool HasValidRange() const { return _MarkIn >= 0 && _MarkOut > _MarkIn; }

    HRESULT ExportRange(LPCTSTR pOutputPath, LONGLONG pStart, LONGLONG pEnd, XExportCallback pCallback = nullptr);
    HRESULT ExportRangeWithCrop(LPCTSTR pOutputPath, LONGLONG pStart, LONGLONG pEnd, const CRect& pCropRect, XExportCallback pCallback = nullptr);

protected:
    virtual BOOL OnNewDocument() override;
    virtual void Serialize(CArchive& ar) override;

    DECLARE_MESSAGE_MAP()

private:
    CString             _FilePath;
    XVideoInfo          _VideoInfo;
    IMFSourceReader*    _SourceReader;
    IMFMediaType*       _OutputMediaType;
    LONGLONG            _MarkIn;
    LONGLONG            _MarkOut;

    LONGLONG            _CachedFramePosition;
    CBitmap             _CachedFrameBitmap;

    HRESULT InitializeSourceReader(LPCTSTR pPath);
    HRESULT ExtractVideoInfo();
    HRESULT ConfigureH264Encoder(IMFSinkWriter* pSinkWriter, DWORD* pStreamIndex);
    HRESULT ConfigureH264EncoderWithSize(IMFSinkWriter* pSinkWriter, DWORD* pStreamIndex, UINT32 pWidth, UINT32 pHeight);
    HRESULT ConfigureAACEncoder(IMFSinkWriter* pSinkWriter, DWORD* pStreamIndex);
    HRESULT FlipSampleVertically(IMFSample* pSample);
    void ReleaseResources();
};
