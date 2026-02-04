#include "pch.h"
#include "XChildFrame.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(XChildFrame, CMDIChildWndEx)

BEGIN_MESSAGE_MAP(XChildFrame, CMDIChildWndEx)
END_MESSAGE_MAP()

XChildFrame::XChildFrame() noexcept
{
}

XChildFrame::~XChildFrame()
{
}

BOOL XChildFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CMDIChildWndEx::PreCreateWindow(cs))
        return FALSE;

    return TRUE;
}
