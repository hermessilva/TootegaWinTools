#pragma once

class XCaptureDocument : public CDocument
{
    DECLARE_DYNCREATE(XCaptureDocument)

public:
    XCaptureDocument() noexcept;
    virtual ~XCaptureDocument();

    virtual BOOL OnNewDocument() override;
    virtual void Serialize(CArchive& ar) override;

    CString GetOutputFilePath() const { return _OutputFilePath; }
    void SetOutputFilePath(const CString& pPath) { _OutputFilePath = pPath; }

    HWND GetTargetWindow() const { return _TargetWindow; }
    void SetTargetWindow(HWND pHwnd) { _TargetWindow = pHwnd; }

    CRect GetCaptureRect() const { return _CaptureRect; }
    void SetCaptureRect(const CRect& pRect) { _CaptureRect = pRect; }

    int GetFPS() const { return _FPS; }
    void SetFPS(int pFPS) { _FPS = pFPS; }

    bool IsGrayscale() const { return _Grayscale; }
    void SetGrayscale(bool pGrayscale) { _Grayscale = pGrayscale; }

protected:
    DECLARE_MESSAGE_MAP()

private:
    CString _OutputFilePath;
    HWND    _TargetWindow;
    CRect   _CaptureRect;
    int     _FPS;
    bool    _Grayscale;
};
