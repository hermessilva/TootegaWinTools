#pragma once

class XMainFrame : public CMDIFrameWndEx
{
    DECLARE_DYNAMIC(XMainFrame)

public:
    XMainFrame() noexcept;
    virtual ~XMainFrame();

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
    CMFCMenuBar       _MenuBar;
    CMFCStatusBar     _StatusBar;

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnWindowManager();
    afx_msg void OnResetLayout();

    DECLARE_MESSAGE_MAP()
};
