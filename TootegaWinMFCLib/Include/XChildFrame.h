#pragma once

class XChildFrame : public CMDIChildWndEx
{
    DECLARE_DYNCREATE(XChildFrame)

public:
    XChildFrame() noexcept;
    virtual ~XChildFrame();

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;

protected:
    DECLARE_MESSAGE_MAP()
};
