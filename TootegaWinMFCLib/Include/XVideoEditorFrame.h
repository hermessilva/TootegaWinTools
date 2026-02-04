#pragma once

class XVideoEditorFrame : public CMDIChildWndEx
{
    DECLARE_DYNCREATE(XVideoEditorFrame)

protected:
    XVideoEditorFrame() noexcept;
    virtual ~XVideoEditorFrame();

public:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
    DECLARE_MESSAGE_MAP()
};
