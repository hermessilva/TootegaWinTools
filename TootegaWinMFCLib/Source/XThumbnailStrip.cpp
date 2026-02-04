#include "pch.h"
#include "XThumbnailStrip.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(XThumbnailStrip, CWnd)

BEGIN_MESSAGE_MAP(XThumbnailStrip, CWnd)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

XThumbnailStrip::XThumbnailStrip() noexcept
    : _FrameCount(500)
    , _CurrentFrame(0)
    , _TotalFrames(0)
    , _SelectedThumbnail(0)
    , _MarkIn(-1)
    , _MarkOut(-1)
    , _ThumbnailWidth(120)
    , _ThumbnailHeight(68)
    , _Spacing(4)
{
}

XThumbnailStrip::~XThumbnailStrip()
{
    ClearThumbnails();
}

void XThumbnailStrip::SetFrameCount(UINT pFrameCount)
{
    if (_FrameCount == pFrameCount)
        return;

    _FrameCount = max(1, pFrameCount);
    RegenerateThumbnails();
    Invalidate();
}

void XThumbnailStrip::SetCurrentFrame(UINT64 pFrame)
{
    _CurrentFrame = pFrame;
    Invalidate();
}

void XThumbnailStrip::SetTotalFrames(UINT64 pTotal)
{
    _TotalFrames = pTotal;
}

void XThumbnailStrip::SetSelectedThumbnail(int pIndex)
{
    if (_SelectedThumbnail != pIndex)
    {
        _SelectedThumbnail = pIndex;
        Invalidate();
    }
}

int XThumbnailStrip::GetThumbnailCount() const
{
    if (_TotalFrames == 0 || _FrameCount == 0)
        return 0;
    return static_cast<int>((_TotalFrames + _FrameCount - 1) / _FrameCount);
}

void XThumbnailStrip::SetMarkIn(LONGLONG pFrame)
{
    _MarkIn = pFrame;
    Invalidate();
}

void XThumbnailStrip::SetMarkOut(LONGLONG pFrame)
{
    _MarkOut = pFrame;
    Invalidate();
}

void XThumbnailStrip::SetThumbnailBitmap(int pIndex, HBITMAP pBitmap)
{
    if (pIndex < 0 || pIndex >= static_cast<int>(_Thumbnails.size()))
        return;

    if (_Thumbnails[pIndex])
    {
        delete _Thumbnails[pIndex];
        _Thumbnails[pIndex] = nullptr;
    }

    _Thumbnails[pIndex] = new CBitmap();
    _Thumbnails[pIndex]->Attach(pBitmap);
    Invalidate();
}

void XThumbnailStrip::ClearThumbnails()
{
    for (auto& thumb : _Thumbnails)
    {
        if (thumb)
        {
            delete thumb;
            thumb = nullptr;
        }
    }
    _Thumbnails.clear();
}

void XThumbnailStrip::RegenerateThumbnails()
{
    ClearThumbnails();

    CRect rc;
    GetClientRect(&rc);
    if (rc.Width() <= 0)
        return;

    int thumbCount = (rc.Width() + _Spacing) / (_ThumbnailWidth + _Spacing);
    thumbCount = max(1, thumbCount);

    _Thumbnails.resize(thumbCount, nullptr);

    CWnd* pParent = GetParent();
    if (pParent)
        pParent->SendMessage(WM_THUMBNAILCLICKED, (WPARAM)-1, 0);
}

CRect XThumbnailStrip::GetThumbnailRect(int pIndex) const
{
    int x = pIndex * (_ThumbnailWidth + _Spacing);
    CRect rc;
    GetClientRect(&rc);
    int y = (rc.Height() - _ThumbnailHeight) / 2;

    return CRect(x, y, x + _ThumbnailWidth, y + _ThumbnailHeight);
}

int XThumbnailStrip::HitTest(CPoint pPoint) const
{
    for (int i = 0; i < static_cast<int>(_Thumbnails.size()); i++)
    {
        CRect rc = GetThumbnailRect(i);
        if (rc.PtInRect(pPoint))
            return i;
    }
    return -1;
}

void XThumbnailStrip::DrawSelectionRange(CDC* pDC)
{
    if (_MarkIn < 0 || _MarkOut < 0 || _TotalFrames == 0)
        return;

    CRect rc;
    GetClientRect(&rc);

    double startPct = static_cast<double>(_MarkIn) / _TotalFrames;
    double endPct = static_cast<double>(_MarkOut) / _TotalFrames;

    int startX = static_cast<int>(startPct * rc.Width());
    int endX = static_cast<int>(endPct * rc.Width());

    CRect selRect(startX, 0, endX, rc.Height());

    CBrush brush;
    brush.CreateSolidBrush(RGB(0, 120, 215));
    pDC->FillRect(selRect, &brush);
}

void XThumbnailStrip::OnPaint()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(&rc);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);

    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    memDC.FillSolidRect(rc, RGB(30, 30, 30));

    DrawSelectionRange(&memDC);

    for (int i = 0; i < static_cast<int>(_Thumbnails.size()); i++)
    {
        CRect thumbRc = GetThumbnailRect(i);

        if (i == _SelectedThumbnail)
        {
            CRect hlRect = thumbRc;
            hlRect.InflateRect(3, 3);
            memDC.FillSolidRect(hlRect, RGB(0, 120, 215));
        }

        if (_Thumbnails[i])
        {
            CDC thumbDC;
            thumbDC.CreateCompatibleDC(&memDC);
            CBitmap* pOldThumb = thumbDC.SelectObject(_Thumbnails[i]);

            BITMAP bm;
            _Thumbnails[i]->GetBitmap(&bm);

            memDC.SetStretchBltMode(HALFTONE);
            memDC.StretchBlt(
                thumbRc.left, thumbRc.top,
                thumbRc.Width(), thumbRc.Height(),
                &thumbDC,
                0, 0, bm.bmWidth, bm.bmHeight,
                SRCCOPY);

            thumbDC.SelectObject(pOldThumb);
        }
        else
        {
            memDC.FillSolidRect(thumbRc, RGB(60, 60, 60));
            memDC.SetTextColor(RGB(128, 128, 128));
            memDC.SetBkMode(TRANSPARENT);
            memDC.DrawText(_T("..."), thumbRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        COLORREF borderColor = (i == _SelectedThumbnail) ? RGB(255, 255, 255) : RGB(80, 80, 80);
        int borderWidth = (i == _SelectedThumbnail) ? 2 : 1;

        CPen pen(PS_SOLID, borderWidth, borderColor);
        CPen* pOldPen = memDC.SelectObject(&pen);
        memDC.SelectStockObject(NULL_BRUSH);
        memDC.Rectangle(thumbRc);
        memDC.SelectObject(pOldPen);

        memDC.SetTextColor(RGB(200, 200, 200));
        memDC.SetBkMode(TRANSPARENT);
        CString label;
        UINT64 startFrame = static_cast<UINT64>(i) * _FrameCount;
        label.Format(_T("%llu"), startFrame);
        CRect labelRc(thumbRc.left, thumbRc.bottom - 14, thumbRc.right, thumbRc.bottom);
        memDC.DrawText(label, labelRc, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    }

    if (_TotalFrames > 0 && _FrameCount > 0 && _SelectedThumbnail >= 0 
        && _SelectedThumbnail < static_cast<int>(_Thumbnails.size()))
    {
        UINT64 thumbStartFrame = static_cast<UINT64>(_SelectedThumbnail) * _FrameCount;
        UINT64 thumbEndFrame = thumbStartFrame + _FrameCount;
        
        if (thumbEndFrame > _TotalFrames)
            thumbEndFrame = _TotalFrames;

        if (_CurrentFrame >= thumbStartFrame && _CurrentFrame < thumbEndFrame)
        {
            CRect thumbRc = GetThumbnailRect(_SelectedThumbnail);
            
            UINT64 thumbFrameRange = thumbEndFrame - thumbStartFrame;
            if (thumbFrameRange > 0)
            {
                double pct = static_cast<double>(_CurrentFrame - thumbStartFrame) / thumbFrameRange;
                int x = thumbRc.left + static_cast<int>(pct * thumbRc.Width());

                CPen markerPen(PS_SOLID, 2, RGB(255, 255, 0));
                CPen* pOldPen = memDC.SelectObject(&markerPen);
                memDC.MoveTo(x, thumbRc.top);
                memDC.LineTo(x, thumbRc.bottom);
                memDC.SelectObject(pOldPen);
            }
        }
    }

    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void XThumbnailStrip::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    RegenerateThumbnails();
}

void XThumbnailStrip::OnLButtonDown(UINT nFlags, CPoint point)
{
    int idx = HitTest(point);

    if (idx >= 0)
    {
        UINT64 frame = static_cast<UINT64>(idx) * _FrameCount;
        if (frame >= _TotalFrames && _TotalFrames > 0)
            frame = _TotalFrames - 1;

        _CurrentFrame = frame;
        Invalidate();

        CWnd* pParent = GetParent();
        if (pParent)
            pParent->SendMessage(WM_THUMBNAILCLICKED, (WPARAM)idx, (LPARAM)frame);
    }
    else
    {
        CRect rc;
        GetClientRect(&rc);
        if (rc.Width() > 0 && _TotalFrames > 0)
        {
            double pct = static_cast<double>(point.x) / rc.Width();
            UINT64 frame = static_cast<UINT64>(pct * _TotalFrames);
            if (frame >= _TotalFrames)
                frame = _TotalFrames - 1;

            int thumbIdx = static_cast<int>(frame / _FrameCount);

            _CurrentFrame = frame;
            Invalidate();

            CWnd* pParent = GetParent();
            if (pParent)
                pParent->SendMessage(WM_THUMBNAILCLICKED, (WPARAM)thumbIdx, (LPARAM)frame);
        }
    }

    CWnd::OnLButtonDown(nFlags, point);
}

BOOL XThumbnailStrip::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}
