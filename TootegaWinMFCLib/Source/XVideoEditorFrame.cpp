#include "pch.h"
#include "XVideoEditorFrame.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(XVideoEditorFrame, CMDIChildWndEx)

BEGIN_MESSAGE_MAP(XVideoEditorFrame, CMDIChildWndEx)
END_MESSAGE_MAP()

XVideoEditorFrame::XVideoEditorFrame() noexcept
{
}

XVideoEditorFrame::~XVideoEditorFrame()
{
}

BOOL XVideoEditorFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CMDIChildWndEx::PreCreateWindow(cs))
        return FALSE;

    cs.style |= WS_MAXIMIZE;

    return TRUE;
}
