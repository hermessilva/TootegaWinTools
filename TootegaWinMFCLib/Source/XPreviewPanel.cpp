#include "pch.h"
#include "XPreviewPanel.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(XPreviewPanel, CWnd)

BEGIN_MESSAGE_MAP(XPreviewPanel, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

XPreviewPanel::XPreviewPanel() noexcept
    : _SourceWindow(nullptr)
    , _Tracking(false)
    , _TrackerVisible(true)
{
    _Tracker.m_nStyle = CRectTracker::resizeInside | CRectTracker::dottedLine;
    _Tracker.m_rect.SetRectEmpty();
}

XPreviewPanel::~XPreviewPanel()
{
}

BOOL XPreviewPanel::Create(DWORD pStyle, const RECT& pRect, CWnd* pParentWnd, UINT pID)
{
    LPCTSTR className = AfxRegisterWndClass(
        CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
        nullptr);

    return CWnd::Create(className, _T("Preview"), pStyle, pRect, pParentWnd, pID);
}

void XPreviewPanel::SetSourceWindow(HWND pHwnd)
{
    _SourceWindow = pHwnd;
    if (_SourceWindow)
    {
        CaptureWindow();
        InitializeTracker();
    }
    else
    {
        _Capture.DeleteObject();
        _CaptureSize = CSize(0, 0);
        _Tracker.m_rect.SetRectEmpty();
    }
    Invalidate();
}

void XPreviewPanel::RefreshCapture()
{
    if (_SourceWindow && IsWindow(_SourceWindow))
    {
        CaptureWindow();
        // Use InvalidateRect with FALSE to avoid erasing background (reduces flicker)
        InvalidateRect(nullptr, FALSE);
    }
}

void XPreviewPanel::CaptureWindow()
{
    if (!_SourceWindow || !IsWindow(_SourceWindow))
        return;

    RECT windowRect;
    if (!::GetWindowRect(_SourceWindow, &windowRect))
        return;

    int width = windowRect.right - windowRect.left;
    int height = windowRect.bottom - windowRect.top;

    if (width <= 0 || height <= 0)
        return;

    // Reuse existing bitmap if dimensions match
    bool needNewBitmap = (_CaptureSize.cx != width || _CaptureSize.cy != height);
    
    HDC hdcScreen = ::GetDC(nullptr);
    if (!hdcScreen)
        return;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    HBITMAP hBitmap;
    if (needNewBitmap)
    {
        _Capture.DeleteObject();
        hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
        _Capture.Attach(hBitmap);
        _CaptureSize = CSize(width, height);
        UpdateImageRect();
    }
    else
    {
        hBitmap = static_cast<HBITMAP>(_Capture.GetSafeHandle());
    }

    HBITMAP hOldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, hBitmap));

    // Try PrintWindow first for better capture of layered/DWM windows
    BOOL result = ::PrintWindow(_SourceWindow, hdcMem, PW_RENDERFULLCONTENT);
    if (!result)
    {
        // Fallback to BitBlt from screen coordinates
        ::BitBlt(hdcMem, 0, 0, width, height, hdcScreen, windowRect.left, windowRect.top, SRCCOPY);
    }

    ::SelectObject(hdcMem, hOldBitmap);
    ::DeleteDC(hdcMem);
    ::ReleaseDC(nullptr, hdcScreen);
}

void XPreviewPanel::UpdateImageRect()
{
    if (_CaptureSize.cx <= 0 || _CaptureSize.cy <= 0)
    {
        _ImageRect.SetRectEmpty();
        return;
    }

    CRect clientRect;
    GetClientRect(&clientRect);

    double aspectRatio = static_cast<double>(_CaptureSize.cx) / static_cast<double>(_CaptureSize.cy);
    double clientAspect = static_cast<double>(clientRect.Width()) / static_cast<double>(clientRect.Height());

    int imgWidth, imgHeight;
    if (aspectRatio > clientAspect)
    {
        imgWidth = clientRect.Width();
        imgHeight = static_cast<int>(imgWidth / aspectRatio);
    }
    else
    {
        imgHeight = clientRect.Height();
        imgWidth = static_cast<int>(imgHeight * aspectRatio);
    }

    int x = (clientRect.Width() - imgWidth) / 2;
    int y = (clientRect.Height() - imgHeight) / 2;

    _ImageRect = CRect(x, y, x + imgWidth, y + imgHeight);
}

void XPreviewPanel::InitializeTracker()
{
    if (_ImageRect.IsRectEmpty())
        return;

    // Start with full window selection
    _Tracker.m_rect = _ImageRect;
}

CRect XPreviewPanel::GetSelectionRect() const
{
    return _Tracker.m_rect;
}

void XPreviewPanel::SetSelectionRect(const CRect& pRect)
{
    _Tracker.m_rect = pRect;
    Invalidate();
}

CRect XPreviewPanel::ScaleRectToImage(const CRect& pSourceRect) const
{
    if (_CaptureSize.cx <= 0 || _CaptureSize.cy <= 0 || _ImageRect.IsRectEmpty())
        return CRect(0, 0, 0, 0);

    double scaleX = static_cast<double>(_ImageRect.Width()) / static_cast<double>(_CaptureSize.cx);
    double scaleY = static_cast<double>(_ImageRect.Height()) / static_cast<double>(_CaptureSize.cy);

    return CRect(
        _ImageRect.left + static_cast<int>(pSourceRect.left * scaleX),
        _ImageRect.top + static_cast<int>(pSourceRect.top * scaleY),
        _ImageRect.left + static_cast<int>(pSourceRect.right * scaleX),
        _ImageRect.top + static_cast<int>(pSourceRect.bottom * scaleY));
}

CRect XPreviewPanel::ScaleRectToSource(const CRect& pImageRect) const
{
    if (_CaptureSize.cx <= 0 || _CaptureSize.cy <= 0 || _ImageRect.IsRectEmpty())
        return CRect(0, 0, 0, 0);

    double scaleX = static_cast<double>(_CaptureSize.cx) / static_cast<double>(_ImageRect.Width());
    double scaleY = static_cast<double>(_CaptureSize.cy) / static_cast<double>(_ImageRect.Height());

    return CRect(
        static_cast<int>((pImageRect.left - _ImageRect.left) * scaleX),
        static_cast<int>((pImageRect.top - _ImageRect.top) * scaleY),
        static_cast<int>((pImageRect.right - _ImageRect.left) * scaleX),
        static_cast<int>((pImageRect.bottom - _ImageRect.top) * scaleY));
}

CRect XPreviewPanel::GetScaledSelectionToSource() const
{
    return ScaleRectToSource(_Tracker.m_rect);
}

void XPreviewPanel::SetTrackerVisible(bool pVisible)
{
    _TrackerVisible = pVisible;
    Invalidate();
}

void XPreviewPanel::OnPaint()
{
    CPaintDC dc(this);

    CRect clientRect;
    GetClientRect(&clientRect);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap memBitmap;
    memBitmap.CreateCompatibleBitmap(&dc, clientRect.Width(), clientRect.Height());
    CBitmap* pOldBitmap = memDC.SelectObject(&memBitmap);

    memDC.FillSolidRect(&clientRect, RGB(40, 40, 40));

    if (_Capture.GetSafeHandle() && !_ImageRect.IsRectEmpty())
    {
        CDC captureDC;
        captureDC.CreateCompatibleDC(&memDC);
        CBitmap* pOldCapture = captureDC.SelectObject(&_Capture);

        memDC.SetStretchBltMode(HALFTONE);
        memDC.StretchBlt(
            _ImageRect.left, _ImageRect.top,
            _ImageRect.Width(), _ImageRect.Height(),
            &captureDC,
            0, 0,
            _CaptureSize.cx, _CaptureSize.cy,
            SRCCOPY);

        captureDC.SelectObject(pOldCapture);

        // Draw custom red thick selection rectangle only if visible
        if (_TrackerVisible)
        {
            CPen redPen(PS_SOLID, 3, RGB(255, 0, 0));
            CPen* pOldPen = memDC.SelectObject(&redPen);
            CBrush* pOldBrush = static_cast<CBrush*>(memDC.SelectStockObject(NULL_BRUSH));
            memDC.Rectangle(_Tracker.m_rect);
            
            // Draw resize handles
            int handleSize = 8;
            CBrush redBrush(RGB(255, 0, 0));
            memDC.SelectObject(&redBrush);
            
            CRect r = _Tracker.m_rect;
            // Corners
            memDC.Rectangle(r.left - handleSize/2, r.top - handleSize/2, r.left + handleSize/2, r.top + handleSize/2);
            memDC.Rectangle(r.right - handleSize/2, r.top - handleSize/2, r.right + handleSize/2, r.top + handleSize/2);
            memDC.Rectangle(r.left - handleSize/2, r.bottom - handleSize/2, r.left + handleSize/2, r.bottom + handleSize/2);
            memDC.Rectangle(r.right - handleSize/2, r.bottom - handleSize/2, r.right + handleSize/2, r.bottom + handleSize/2);
            // Midpoints
            int midX = (r.left + r.right) / 2;
            int midY = (r.top + r.bottom) / 2;
            memDC.Rectangle(midX - handleSize/2, r.top - handleSize/2, midX + handleSize/2, r.top + handleSize/2);
            memDC.Rectangle(midX - handleSize/2, r.bottom - handleSize/2, midX + handleSize/2, r.bottom + handleSize/2);
            memDC.Rectangle(r.left - handleSize/2, midY - handleSize/2, r.left + handleSize/2, midY + handleSize/2);
            memDC.Rectangle(r.right - handleSize/2, midY - handleSize/2, r.right + handleSize/2, midY + handleSize/2);
            
            memDC.SelectObject(pOldBrush);
            memDC.SelectObject(pOldPen);
        }
    }
    else
    {
        CString text = _T("Select a window to preview");
        memDC.SetTextColor(RGB(180, 180, 180));
        memDC.SetBkMode(TRANSPARENT);
        CFont font;
        font.CreatePointFont(120, _T("Segoe UI"));
        CFont* pOldFont = memDC.SelectObject(&font);
        memDC.DrawText(text, &clientRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        memDC.SelectObject(pOldFont);
    }

    dc.BitBlt(0, 0, clientRect.Width(), clientRect.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBitmap);
}

void XPreviewPanel::OnLButtonDown(UINT pFlags, CPoint pPoint)
{
    if (_Capture.GetSafeHandle())
    {
        if (_Tracker.HitTest(pPoint) >= 0)
        {
            _Tracker.Track(this, pPoint, TRUE);
            
            if (_Tracker.m_rect.left < _ImageRect.left)
                _Tracker.m_rect.left = _ImageRect.left;
            if (_Tracker.m_rect.top < _ImageRect.top)
                _Tracker.m_rect.top = _ImageRect.top;
            if (_Tracker.m_rect.right > _ImageRect.right)
                _Tracker.m_rect.right = _ImageRect.right;
            if (_Tracker.m_rect.bottom > _ImageRect.bottom)
                _Tracker.m_rect.bottom = _ImageRect.bottom;

            Invalidate();
        }
    }

    CWnd::OnLButtonDown(pFlags, pPoint);
}

void XPreviewPanel::OnLButtonUp(UINT pFlags, CPoint pPoint)
{
    _Tracking = false;
    CWnd::OnLButtonUp(pFlags, pPoint);
}

void XPreviewPanel::OnMouseMove(UINT pFlags, CPoint pPoint)
{
    CWnd::OnMouseMove(pFlags, pPoint);
}

BOOL XPreviewPanel::OnSetCursor(CWnd* pWnd, UINT pHitTest, UINT pMessage)
{
    if (pWnd == this && _Capture.GetSafeHandle())
    {
        if (_Tracker.SetCursor(this, pHitTest))
            return TRUE;
    }
    return CWnd::OnSetCursor(pWnd, pHitTest, pMessage);
}

void XPreviewPanel::OnSize(UINT pType, int pCX, int pCY)
{
    CWnd::OnSize(pType, pCX, pCY);

    CRect oldImageRect = _ImageRect;
    UpdateImageRect();

    if (!oldImageRect.IsRectEmpty() && !_ImageRect.IsRectEmpty() && !_Tracker.m_rect.IsRectEmpty())
    {
        double scaleX = static_cast<double>(_ImageRect.Width()) / static_cast<double>(oldImageRect.Width());
        double scaleY = static_cast<double>(_ImageRect.Height()) / static_cast<double>(oldImageRect.Height());

        int left = _ImageRect.left + static_cast<int>((_Tracker.m_rect.left - oldImageRect.left) * scaleX);
        int top = _ImageRect.top + static_cast<int>((_Tracker.m_rect.top - oldImageRect.top) * scaleY);
        int right = _ImageRect.left + static_cast<int>((_Tracker.m_rect.right - oldImageRect.left) * scaleX);
        int bottom = _ImageRect.top + static_cast<int>((_Tracker.m_rect.bottom - oldImageRect.top) * scaleY);

        _Tracker.m_rect = CRect(left, top, right, bottom);
    }

    Invalidate();
}

BOOL XPreviewPanel::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}
