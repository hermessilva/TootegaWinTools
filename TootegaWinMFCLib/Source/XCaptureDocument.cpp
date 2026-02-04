#include "pch.h"
#include "XCaptureDocument.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(XCaptureDocument, CDocument)

BEGIN_MESSAGE_MAP(XCaptureDocument, CDocument)
END_MESSAGE_MAP()

XCaptureDocument::XCaptureDocument() noexcept
    : _TargetWindow(nullptr)
    , _FPS(30)
    , _Grayscale(false)
{
}

XCaptureDocument::~XCaptureDocument()
{
}

BOOL XCaptureDocument::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
        return FALSE;

    SetTitle(_T("Window Capture"));

    _TargetWindow = nullptr;
    _CaptureRect.SetRectEmpty();
    _FPS = 30;
    _Grayscale = false;
    _OutputFilePath.Empty();

    return TRUE;
}

void XCaptureDocument::Serialize(CArchive& ar)
{
    if (ar.IsStoring())
    {
    }
    else
    {
    }
}
