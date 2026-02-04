#pragma once

#include "Resource.h"

class XApplication : public CWinAppEx
{
public:
    XApplication() noexcept;
    virtual ~XApplication();

    virtual BOOL InitInstance() override;
    virtual int ExitInstance() override;

    afx_msg void OnAppAbout();
    afx_msg void OnFileNewCapture();
    afx_msg void OnFileOpenVideo();

    DECLARE_MESSAGE_MAP()

private:
    void InitializeMF();
    void ShutdownMF();

    CMultiDocTemplate* _CaptureTemplate = nullptr;
    CMultiDocTemplate* _VideoEditorTemplate = nullptr;
};

extern XApplication theApp;
