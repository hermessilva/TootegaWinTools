#include "pch.h"
#include "XVideoEditorView.h"
#include "Resource.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// XExportProgressDlg Implementation
BEGIN_MESSAGE_MAP(XExportProgressDlg, CDialogEx)
    ON_BN_CLICKED(IDCANCEL, &XExportProgressDlg::OnCancel)
END_MESSAGE_MAP()

XExportProgressDlg::XExportProgressDlg(CWnd* pParent)
    : CDialogEx(IDD_EXPORT_PROGRESS, pParent)
    , _Cancelled(false)
{
}

XExportProgressDlg::~XExportProgressDlg()
{
}

void XExportProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL XExportProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CRect rc;
    GetClientRect(&rc);

    _StatusLabel.Create(_T("Exporting video..."), WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(20, 20, rc.Width() - 20, 40), this, IDC_EXPORT_STATUS);

    _Progress.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        CRect(20, 50, rc.Width() - 20, 70), this, IDC_EXPORT_PROGRESS);
    _Progress.SetRange(0, 1000);

    _ETALabel.Create(_T("Calculating..."), WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(20, 80, rc.Width() - 20, 100), this, IDC_EXPORT_ETA);

    CButton* pCancel = (CButton*)GetDlgItem(IDCANCEL);
    if (pCancel)
        pCancel->MoveWindow((rc.Width() - 80) / 2, 110, 80, 25);

    return TRUE;
}

void XExportProgressDlg::SetProgress(UINT64 pCurrent, UINT64 pTotal)
{
    if (pTotal > 0)
    {
        UINT64 safeProgress = min(pCurrent, pTotal);
        int pos = static_cast<int>((safeProgress * 1000) / pTotal);
        pos = min(1000, max(0, pos));
        _Progress.SetPos(pos);

        double percent = (safeProgress * 100.0) / pTotal;
        percent = min(100.0, max(0.0, percent));

        CString txt;
        txt.Format(_T("Exporting: %llu / %llu frames (%.1f%%)"),
            safeProgress, pTotal, percent);
        _StatusLabel.SetWindowText(txt);
    }
}

void XExportProgressDlg::SetETA(double pSeconds)
{
    CString txt;
    
    if (pSeconds < 0 || pSeconds > 86400)
    {
        txt = _T("ETA: Calculating...");
    }
    else if (pSeconds < 1)
    {
        txt = _T("ETA: Almost done...");
    }
    else if (pSeconds < 60)
    {
        txt.Format(_T("ETA: %.0f seconds"), pSeconds);
    }
    else if (pSeconds < 3600)
    {
        int mins = static_cast<int>(pSeconds) / 60;
        int secs = static_cast<int>(pSeconds) % 60;
        txt.Format(_T("ETA: %d:%02d"), mins, secs);
    }
    else
    {
        int hours = static_cast<int>(pSeconds) / 3600;
        int mins = (static_cast<int>(pSeconds) / 60) % 60;
        int secs = static_cast<int>(pSeconds) % 60;
        txt.Format(_T("ETA: %d:%02d:%02d"), hours, mins, secs);
    }

    _ETALabel.SetWindowText(txt);
}

void XExportProgressDlg::SetStatus(LPCTSTR pStatus)
{
    _StatusLabel.SetWindowText(pStatus);
}

void XExportProgressDlg::OnCancel()
{
    _Cancelled = true;
}

// XVideoEditorView Implementation
IMPLEMENT_DYNCREATE(XVideoEditorView, CView)

BEGIN_MESSAGE_MAP(XVideoEditorView, CView)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_WM_ERASEBKGND()
    ON_WM_HSCROLL()
    ON_WM_TIMER()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()

    ON_EN_KILLFOCUS(IDC_EDIT_FRAMES, &XVideoEditorView::OnFrameCountChange)
    ON_BN_CLICKED(IDC_BTN_MARK_START, &XVideoEditorView::OnMarkStart)
    ON_BN_CLICKED(IDC_BTN_MARK_END, &XVideoEditorView::OnMarkEnd)
    ON_BN_CLICKED(IDC_BTN_EXPORT, &XVideoEditorView::OnExport)
    ON_BN_CLICKED(IDC_BTN_PLAY, &XVideoEditorView::OnPlayPause)
    ON_BN_CLICKED(IDC_BTN_STOP_VIDEO, &XVideoEditorView::OnStop)
    ON_BN_CLICKED(IDC_BTN_AUDIO, &XVideoEditorView::OnToggleAudio)
    ON_BN_CLICKED(IDC_BTN_SAVE_FRAME, &XVideoEditorView::OnSaveFrame)
    ON_BN_CLICKED(IDC_CHECK_CROP, &XVideoEditorView::OnCropToggle)

    ON_MESSAGE(XThumbnailStrip::WM_THUMBNAILCLICKED, &XVideoEditorView::OnThumbnailClicked)
END_MESSAGE_MAP()

XVideoEditorView::XVideoEditorView() noexcept
    : _CurrentFrame(0)
    , _CurrentPosition(0)
    , _SelectedThumbnail(0)
    , _IsPlaying(false)
    , _AudioEnabled(true)
    , _IsDragging(false)
    , _PendingSeekPosition(-1)
    , _LastSeekTime(0)
    , _CropEnabled(false)
    , _CropDragMode(0)
{
}

XVideoEditorView::~XVideoEditorView()
{
}

BOOL XVideoEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CView::PreCreateWindow(cs))
        return FALSE;

    cs.dwExStyle |= WS_EX_CLIENTEDGE;
    cs.style &= ~WS_BORDER;
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);

    return TRUE;
}

BOOL XVideoEditorView::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
    {
        CWnd* pFocus = GetFocus();
        if (pFocus && pFocus->GetSafeHwnd() == _FrameCountEdit.GetSafeHwnd())
        {
            OnFrameCountChange();
            return TRUE;
        }
    }
    return CView::PreTranslateMessage(pMsg);
}

int XVideoEditorView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
        return -1;

    _UIFont.CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    _LabelFont.CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

    CreateControls();

    return 0;
}

void XVideoEditorView::CreateControls()
{
    CRect rc;
    GetClientRect(&rc);

    _FrameCountEdit.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
        CRect(10, 8, 80, 32), this, IDC_EDIT_FRAMES);
    _FrameCountEdit.SetFont(&_UIFont);
    _FrameCountEdit.SetWindowText(_T("500"));

    _MarkStartButton.Create(_T("Mark Start"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(90, 8, 170, 32), this, IDC_BTN_MARK_START);
    _MarkStartButton.SetFont(&_UIFont);

    _MarkEndButton.Create(_T("Mark End"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(180, 8, 260, 32), this, IDC_BTN_MARK_END);
    _MarkEndButton.SetFont(&_UIFont);

    _ExportButton.Create(_T("Export"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(270, 8, 350, 32), this, IDC_BTN_EXPORT);
    _ExportButton.SetFont(&_UIFont);

    _PlayButton.Create(_T("Play"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(370, 8, 430, 32), this, IDC_BTN_PLAY);
    _PlayButton.SetFont(&_UIFont);

    _StopButton.Create(_T("Stop"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(440, 8, 500, 32), this, IDC_BTN_STOP_VIDEO);
    _StopButton.SetFont(&_UIFont);

    _AudioToggle.Create(_T("Audio On"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(520, 8, 600, 32), this, IDC_BTN_AUDIO);
    _AudioToggle.SetFont(&_UIFont);

    _SaveFrameButton.Create(_T("Save Frame"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(610, 8, 700, 32), this, IDC_BTN_SAVE_FRAME);
    _SaveFrameButton.SetFont(&_UIFont);

    _CropCheckbox.Create(_T("Crop"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        CRect(710, 8, 770, 32), this, IDC_CHECK_CROP);
    _CropCheckbox.SetFont(&_UIFont);

    _StartFrameLabel.Create(_T("Start: -"), WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 100, 20), this, IDC_LABEL_START);
    _StartFrameLabel.SetFont(&_LabelFont);

    _EndFrameLabel.Create(_T("End: -"), WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 100, 20), this, IDC_LABEL_END);
    _EndFrameLabel.SetFont(&_LabelFont);

    _DurationLabel.Create(_T("Duration: -"), WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 150, 20), this, IDC_LABEL_DURATION);
    _DurationLabel.SetFont(&_LabelFont);

    _CurrentFrameLabel.Create(_T("Frame: 0"), WS_CHILD | WS_VISIBLE | SS_RIGHT,
        CRect(0, 0, 100, 20), this, IDC_LABEL_CURRENT);
    _CurrentFrameLabel.SetFont(&_LabelFont);

    _ThumbnailStrip.Create(nullptr, _T("ThumbnailStrip"),
        WS_CHILD | WS_VISIBLE, CRect(0, 0, 100, 80), this, IDC_THUMBNAIL_STRIP);

    _FrameSlider.Create(WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        CRect(0, 0, 100, 30), this, IDC_FRAME_SLIDER);
    _FrameSlider.SetRange(0, 1000);
    _FrameSlider.SetPos(0);
}

void XVideoEditorView::LayoutControls()
{
    CRect rc;
    GetClientRect(&rc);

    int thumbY = rc.bottom - THUMBNAIL_HEIGHT - SLIDER_HEIGHT;
    int sliderY = rc.bottom - SLIDER_HEIGHT;

    _ThumbnailStrip.MoveWindow(LABEL_WIDTH, thumbY, rc.Width() - LABEL_WIDTH * 2, THUMBNAIL_HEIGHT);
    _FrameSlider.MoveWindow(LABEL_WIDTH, sliderY, rc.Width() - LABEL_WIDTH * 2, SLIDER_HEIGHT);

    _StartFrameLabel.MoveWindow(5, thumbY, LABEL_WIDTH - 10, 20);
    _EndFrameLabel.MoveWindow(5, thumbY + 25, LABEL_WIDTH - 10, 20);
    _DurationLabel.MoveWindow(5, thumbY + 50, LABEL_WIDTH - 10, 20);

    _CurrentFrameLabel.MoveWindow(rc.Width() - LABEL_WIDTH + 5, thumbY, LABEL_WIDTH - 10, 20);

    _VideoRect = CRect(10, TOOLBAR_HEIGHT + 10, rc.Width() - 10, thumbY - 10);
}

void XVideoEditorView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    LayoutControls();
    Invalidate();
}

void XVideoEditorView::OnDestroy()
{
    if (_IsPlaying)
        KillTimer(IDT_PLAY_TIMER);
    
    CView::OnDestroy();
}

BOOL XVideoEditorView::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

void XVideoEditorView::OnInitialUpdate()
{
    CView::OnInitialUpdate();

    XVideoEditorDocument* pDoc = GetDocument();
    if (pDoc)
    {
        const XVideoInfo& info = pDoc->GetVideoInfo();
        _ThumbnailStrip.SetTotalFrames(info.TotalFrames);

        _SelectedThumbnail = 0;
        _ThumbnailStrip.SetSelectedThumbnail(0);
        UpdateSliderForThumbnail(0);

        SeekToFrame(0);
        GenerateThumbnails();
    }
}

void XVideoEditorView::OnDraw(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);

    CDC memDC;
    memDC.CreateCompatibleDC(pDC);

    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(pDC, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);

    memDC.FillSolidRect(rc, RGB(45, 45, 48));

    CRect toolbar(0, 0, rc.Width(), TOOLBAR_HEIGHT);
    memDC.FillSolidRect(toolbar, RGB(60, 60, 65));

    DrawVideoFrame(&memDC);

    pDC->BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void XVideoEditorView::DrawVideoFrame(CDC* pDC)
{
    if (_VideoRect.IsRectEmpty())
        return;

    pDC->FillSolidRect(_VideoRect, RGB(0, 0, 0));

    CBitmap* pBmp = _CurrentFrameBitmap.m_hObject ? &_CurrentFrameBitmap : nullptr;

    if (pBmp && pBmp->m_hObject)
    {
        BITMAP bm;
        pBmp->GetBitmap(&bm);

        double srcRatio = static_cast<double>(bm.bmWidth) / bm.bmHeight;
        double dstRatio = static_cast<double>(_VideoRect.Width()) / _VideoRect.Height();

        int dstW, dstH;
        if (srcRatio > dstRatio)
        {
            dstW = _VideoRect.Width();
            dstH = static_cast<int>(dstW / srcRatio);
        }
        else
        {
            dstH = _VideoRect.Height();
            dstW = static_cast<int>(dstH * srcRatio);
        }

        int x = _VideoRect.left + (_VideoRect.Width() - dstW) / 2;
        int y = _VideoRect.top + (_VideoRect.Height() - dstH) / 2;

        CDC srcDC;
        srcDC.CreateCompatibleDC(pDC);
        CBitmap* pOldBmp = srcDC.SelectObject(pBmp);

        pDC->SetStretchBltMode(COLORONCOLOR);
        pDC->StretchBlt(x, y, dstW, dstH, &srcDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

        srcDC.SelectObject(pOldBmp);
        
        if (_CropEnabled)
            DrawCropRect(pDC);
    }
}

void XVideoEditorView::DrawCropRect(CDC* pDC)
{
    _CropRectScreen = VideoToScreen(_CropRect);
    
    CPen pen(PS_SOLID, 2, RGB(255, 0, 0));
    CPen* pOldPen = pDC->SelectObject(&pen);
    pDC->SelectStockObject(NULL_BRUSH);
    
    pDC->Rectangle(_CropRectScreen);
    
    int handleSize = 8;
    CBrush handleBrush(RGB(255, 0, 0));
    CBrush* pOldBrush = pDC->SelectObject(&handleBrush);
    
    CRect handles[8];
    handles[0] = CRect(_CropRectScreen.left - handleSize/2, _CropRectScreen.top - handleSize/2, 
                       _CropRectScreen.left + handleSize/2, _CropRectScreen.top + handleSize/2);
    handles[1] = CRect(_CropRectScreen.CenterPoint().x - handleSize/2, _CropRectScreen.top - handleSize/2,
                       _CropRectScreen.CenterPoint().x + handleSize/2, _CropRectScreen.top + handleSize/2);
    handles[2] = CRect(_CropRectScreen.right - handleSize/2, _CropRectScreen.top - handleSize/2,
                       _CropRectScreen.right + handleSize/2, _CropRectScreen.top + handleSize/2);
    handles[3] = CRect(_CropRectScreen.right - handleSize/2, _CropRectScreen.CenterPoint().y - handleSize/2,
                       _CropRectScreen.right + handleSize/2, _CropRectScreen.CenterPoint().y + handleSize/2);
    handles[4] = CRect(_CropRectScreen.right - handleSize/2, _CropRectScreen.bottom - handleSize/2,
                       _CropRectScreen.right + handleSize/2, _CropRectScreen.bottom + handleSize/2);
    handles[5] = CRect(_CropRectScreen.CenterPoint().x - handleSize/2, _CropRectScreen.bottom - handleSize/2,
                       _CropRectScreen.CenterPoint().x + handleSize/2, _CropRectScreen.bottom + handleSize/2);
    handles[6] = CRect(_CropRectScreen.left - handleSize/2, _CropRectScreen.bottom - handleSize/2,
                       _CropRectScreen.left + handleSize/2, _CropRectScreen.bottom + handleSize/2);
    handles[7] = CRect(_CropRectScreen.left - handleSize/2, _CropRectScreen.CenterPoint().y - handleSize/2,
                       _CropRectScreen.left + handleSize/2, _CropRectScreen.CenterPoint().y + handleSize/2);
    
    for (int i = 0; i < 8; i++)
        pDC->Rectangle(handles[i]);
    
    pDC->SelectObject(pOldBrush);
    pDC->SelectObject(pOldPen);
}

int XVideoEditorView::HitTestCropRect(CPoint pPoint) const
{
    if (!_CropEnabled)
        return 0;
    
    int handleSize = 10;
    
    CRect handles[8];
    handles[0] = CRect(_CropRectScreen.left - handleSize, _CropRectScreen.top - handleSize,
                       _CropRectScreen.left + handleSize, _CropRectScreen.top + handleSize);
    handles[1] = CRect(_CropRectScreen.CenterPoint().x - handleSize, _CropRectScreen.top - handleSize,
                       _CropRectScreen.CenterPoint().x + handleSize, _CropRectScreen.top + handleSize);
    handles[2] = CRect(_CropRectScreen.right - handleSize, _CropRectScreen.top - handleSize,
                       _CropRectScreen.right + handleSize, _CropRectScreen.top + handleSize);
    handles[3] = CRect(_CropRectScreen.right - handleSize, _CropRectScreen.CenterPoint().y - handleSize,
                       _CropRectScreen.right + handleSize, _CropRectScreen.CenterPoint().y + handleSize);
    handles[4] = CRect(_CropRectScreen.right - handleSize, _CropRectScreen.bottom - handleSize,
                       _CropRectScreen.right + handleSize, _CropRectScreen.bottom + handleSize);
    handles[5] = CRect(_CropRectScreen.CenterPoint().x - handleSize, _CropRectScreen.bottom - handleSize,
                       _CropRectScreen.CenterPoint().x + handleSize, _CropRectScreen.bottom + handleSize);
    handles[6] = CRect(_CropRectScreen.left - handleSize, _CropRectScreen.bottom - handleSize,
                       _CropRectScreen.left + handleSize, _CropRectScreen.bottom + handleSize);
    handles[7] = CRect(_CropRectScreen.left - handleSize, _CropRectScreen.CenterPoint().y - handleSize,
                       _CropRectScreen.left + handleSize, _CropRectScreen.CenterPoint().y + handleSize);
    
    for (int i = 0; i < 8; i++)
    {
        if (handles[i].PtInRect(pPoint))
            return i + 1;
    }
    
    if (_CropRectScreen.PtInRect(pPoint))
        return 9;
    
    return 0;
}

CRect XVideoEditorView::VideoToScreen(const CRect& pVideoRect) const
{
    XVideoEditorDocument* pDoc = const_cast<XVideoEditorView*>(this)->GetDocument();
    if (!pDoc)
        return CRect();
    
    const XVideoInfo& info = pDoc->GetVideoInfo();
    if (info.Width == 0 || info.Height == 0)
        return CRect();
    
    double srcRatio = static_cast<double>(info.Width) / info.Height;
    double dstRatio = static_cast<double>(_VideoRect.Width()) / _VideoRect.Height();
    
    int dstW, dstH;
    if (srcRatio > dstRatio)
    {
        dstW = _VideoRect.Width();
        dstH = static_cast<int>(dstW / srcRatio);
    }
    else
    {
        dstH = _VideoRect.Height();
        dstW = static_cast<int>(dstH * srcRatio);
    }
    
    int offsetX = _VideoRect.left + (_VideoRect.Width() - dstW) / 2;
    int offsetY = _VideoRect.top + (_VideoRect.Height() - dstH) / 2;
    
    double scaleX = static_cast<double>(dstW) / info.Width;
    double scaleY = static_cast<double>(dstH) / info.Height;
    
    CRect screenRect;
    screenRect.left = offsetX + static_cast<int>(pVideoRect.left * scaleX);
    screenRect.top = offsetY + static_cast<int>(pVideoRect.top * scaleY);
    screenRect.right = offsetX + static_cast<int>(pVideoRect.right * scaleX);
    screenRect.bottom = offsetY + static_cast<int>(pVideoRect.bottom * scaleY);
    
    return screenRect;
}

CRect XVideoEditorView::ScreenToVideo(const CRect& pScreenRect) const
{
    XVideoEditorDocument* pDoc = const_cast<XVideoEditorView*>(this)->GetDocument();
    if (!pDoc)
        return CRect();
    
    const XVideoInfo& info = pDoc->GetVideoInfo();
    if (info.Width == 0 || info.Height == 0)
        return CRect();
    
    double srcRatio = static_cast<double>(info.Width) / info.Height;
    double dstRatio = static_cast<double>(_VideoRect.Width()) / _VideoRect.Height();
    
    int dstW, dstH;
    if (srcRatio > dstRatio)
    {
        dstW = _VideoRect.Width();
        dstH = static_cast<int>(dstW / srcRatio);
    }
    else
    {
        dstH = _VideoRect.Height();
        dstW = static_cast<int>(dstH * srcRatio);
    }
    
    int offsetX = _VideoRect.left + (_VideoRect.Width() - dstW) / 2;
    int offsetY = _VideoRect.top + (_VideoRect.Height() - dstH) / 2;
    
    double scaleX = static_cast<double>(info.Width) / dstW;
    double scaleY = static_cast<double>(info.Height) / dstH;
    
    CRect videoRect;
    videoRect.left = static_cast<int>((pScreenRect.left - offsetX) * scaleX);
    videoRect.top = static_cast<int>((pScreenRect.top - offsetY) * scaleY);
    videoRect.right = static_cast<int>((pScreenRect.right - offsetX) * scaleX);
    videoRect.bottom = static_cast<int>((pScreenRect.bottom - offsetY) * scaleY);
    
    videoRect.left = max(0, min(static_cast<int>(info.Width), videoRect.left));
    videoRect.top = max(0, min(static_cast<int>(info.Height), videoRect.top));
    videoRect.right = max(0, min(static_cast<int>(info.Width), videoRect.right));
    videoRect.bottom = max(0, min(static_cast<int>(info.Height), videoRect.bottom));
    
    return videoRect;
}

void XVideoEditorView::OnFrameCountChange()
{
    CString text;
    _FrameCountEdit.GetWindowText(text);

    int count = _ttoi(text);
    if (count < 1)
        count = 1;
    if (count > 10000)
        count = 10000;

    _ThumbnailStrip.SetFrameCount(count);
    
    int framesPerThumb = count;
    int newThumbIndex = static_cast<int>(_CurrentFrame / framesPerThumb);
    int maxThumbIndex = GetThumbnailCount() - 1;
    if (maxThumbIndex < 0) maxThumbIndex = 0;
    if (newThumbIndex > maxThumbIndex)
        newThumbIndex = maxThumbIndex;

    _SelectedThumbnail = newThumbIndex;
    _ThumbnailStrip.SetSelectedThumbnail(newThumbIndex);
    UpdateSliderForThumbnail(newThumbIndex);
    
    GenerateThumbnails();
}

void XVideoEditorView::OnMarkStart()
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    LONGLONG pos = pDoc->FrameToPosition(_CurrentFrame);
    pDoc->SetMarkIn(pos);
    _ThumbnailStrip.SetMarkIn(static_cast<LONGLONG>(_CurrentFrame));

    CString txt;
    txt.Format(_T("Start: %llu"), _CurrentFrame);
    _StartFrameLabel.SetWindowText(txt);

    UpdateDurationLabel();
    Invalidate();
}

void XVideoEditorView::OnMarkEnd()
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    LONGLONG pos = pDoc->FrameToPosition(_CurrentFrame);
    pDoc->SetMarkOut(pos);
    _ThumbnailStrip.SetMarkOut(static_cast<LONGLONG>(_CurrentFrame));

    CString txt;
    txt.Format(_T("End: %llu"), _CurrentFrame);
    _EndFrameLabel.SetWindowText(txt);

    UpdateDurationLabel();
    Invalidate();
}

void XVideoEditorView::UpdateDurationLabel()
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    LONGLONG markIn = pDoc->GetMarkIn();
    LONGLONG markOut = pDoc->GetMarkOut();

    if (markIn >= 0 && markOut >= 0 && markOut > markIn)
    {
        const XVideoInfo& info = pDoc->GetVideoInfo();
        UINT64 startFrame = pDoc->PositionToFrame(markIn);
        UINT64 endFrame = pDoc->PositionToFrame(markOut);
        UINT64 frameCount = endFrame - startFrame;

        double seconds = 0;
        if (info.FrameRate > 0)
            seconds = frameCount / info.FrameRate;

        int mins = static_cast<int>(seconds) / 60;
        int secs = static_cast<int>(seconds) % 60;
        int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000);

        CString txt;
        txt.Format(_T("Duration: %d:%02d.%03d"), mins, secs, ms);
        _DurationLabel.SetWindowText(txt);
    }
    else
    {
        _DurationLabel.SetWindowText(_T("Duration: -"));
    }
}

void XVideoEditorView::OnExport()
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    LONGLONG markIn = pDoc->GetMarkIn();
    LONGLONG markOut = pDoc->GetMarkOut();

    if (markIn < 0 || markOut < 0 || markOut <= markIn)
    {
        AfxMessageBox(_T("Please set valid start and end markers before exporting."), MB_ICONWARNING);
        return;
    }

    CFileDialog dlg(FALSE, _T("mp4"), nullptr,
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("MP4 Video (*.mp4)|*.mp4||"));

    if (dlg.DoModal() != IDOK)
        return;

    CString outputPath = dlg.GetPathName();

    XExportProgressDlg progressDlg(this);
    progressDlg.Create(IDD_EXPORT_PROGRESS, this);
    progressDlg.ShowWindow(SW_SHOW);
    progressDlg.CenterWindow();

    EnableWindow(FALSE);

    HRESULT hr;
    if (_CropEnabled && !_CropRect.IsRectEmpty())
    {
        hr = pDoc->ExportRangeWithCrop(outputPath, markIn, markOut, _CropRect,
            [&progressDlg](const XExportProgress& pProgress) -> bool
            {
                progressDlg.SetProgress(pProgress.CurrentFrame, pProgress.TotalFrames);
                progressDlg.SetETA(pProgress.EstimatedSecondsRemaining);

                progressDlg.UpdateWindow();
                progressDlg.RedrawWindow();

                MSG msg;
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                    {
                        PostQuitMessage(static_cast<int>(msg.wParam));
                        return false;
                    }
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                return !progressDlg.IsCancelled();
            });
    }
    else
    {
        hr = pDoc->ExportRange(outputPath, markIn, markOut,
            [&progressDlg](const XExportProgress& pProgress) -> bool
            {
                progressDlg.SetProgress(pProgress.CurrentFrame, pProgress.TotalFrames);
                progressDlg.SetETA(pProgress.EstimatedSecondsRemaining);

                progressDlg.UpdateWindow();
                progressDlg.RedrawWindow();

                MSG msg;
                while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                    {
                        PostQuitMessage(static_cast<int>(msg.wParam));
                        return false;
                    }
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                return !progressDlg.IsCancelled();
            });
    }

    EnableWindow(TRUE);
    progressDlg.DestroyWindow();

    if (hr == E_ABORT)
        AfxMessageBox(_T("Export cancelled."), MB_ICONINFORMATION);
    else if (SUCCEEDED(hr))
        AfxMessageBox(_T("Export completed successfully."), MB_ICONINFORMATION);
    else
    {
        CString msg;
        msg.Format(_T("Export failed: 0x%08X"), hr);
        AfxMessageBox(msg, MB_ICONERROR);
    }
}

void XVideoEditorView::OnPlayPause()
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    const XVideoInfo& info = pDoc->GetVideoInfo();
    if (info.TotalFrames == 0)
        return;

    _IsPlaying = !_IsPlaying;
    _PlayButton.SetWindowText(_IsPlaying ? _T("Pause") : _T("Play"));

    if (_IsPlaying)
    {
        UINT timerInterval = 33;
        if (info.FrameRate > 0)
            timerInterval = static_cast<UINT>(1000.0 / info.FrameRate);
        timerInterval = max(16u, min(timerInterval, 100u));
        
        SetTimer(IDT_PLAY_TIMER, timerInterval, nullptr);
    }
    else
    {
        KillTimer(IDT_PLAY_TIMER);
    }
}

void XVideoEditorView::OnStop()
{
    if (_IsPlaying)
    {
        KillTimer(IDT_PLAY_TIMER);
        _IsPlaying = false;
    }
    _PlayButton.SetWindowText(_T("Play"));
    
    UINT64 baseFrame = GetThumbnailStartFrame(_SelectedThumbnail);
    SeekToFrame(baseFrame);
}

void XVideoEditorView::OnToggleAudio()
{
    _AudioEnabled = !_AudioEnabled;
    _AudioToggle.SetWindowText(_AudioEnabled ? _T("Audio On") : _T("Audio Off"));
}

void XVideoEditorView::OnSaveFrame()
{
    if (!_CurrentFrameBitmap.m_hObject)
    {
        AfxMessageBox(_T("No frame to save."), MB_ICONWARNING);
        return;
    }

    CFileDialog dlg(FALSE, _T("png"), nullptr,
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("PNG Image (*.png)|*.png|BMP Image (*.bmp)|*.bmp|JPEG Image (*.jpg)|*.jpg||"));

    if (dlg.DoModal() != IDOK)
        return;

    CString outputPath = dlg.GetPathName();
    CString ext = dlg.GetFileExt().MakeLower();

    BITMAP bm;
    _CurrentFrameBitmap.GetBitmap(&bm);

    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromHBITMAP(
        (HBITMAP)_CurrentFrameBitmap.GetSafeHandle(), nullptr);

    if (!pBitmap)
    {
        AfxMessageBox(_T("Failed to create bitmap for saving."), MB_ICONERROR);
        return;
    }

    CLSID encoderClsid;
    HRESULT hr = E_FAIL;

    if (ext == _T("png"))
    {
        CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &encoderClsid);
        hr = (pBitmap->Save(outputPath, &encoderClsid, nullptr) == Gdiplus::Ok) ? S_OK : E_FAIL;
    }
    else if (ext == _T("jpg") || ext == _T("jpeg"))
    {
        CLSIDFromString(L"{557CF401-1A04-11D3-9A73-0000F81EF32E}", &encoderClsid);
        hr = (pBitmap->Save(outputPath, &encoderClsid, nullptr) == Gdiplus::Ok) ? S_OK : E_FAIL;
    }
    else
    {
        CLSIDFromString(L"{557CF400-1A04-11D3-9A73-0000F81EF32E}", &encoderClsid);
        hr = (pBitmap->Save(outputPath, &encoderClsid, nullptr) == Gdiplus::Ok) ? S_OK : E_FAIL;
    }

    delete pBitmap;

    if (SUCCEEDED(hr))
        AfxMessageBox(_T("Frame saved successfully."), MB_ICONINFORMATION);
    else
        AfxMessageBox(_T("Failed to save frame."), MB_ICONERROR);
}

void XVideoEditorView::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == IDT_PLAY_TIMER && _IsPlaying)
    {
        XVideoEditorDocument* pDoc = GetDocument();
        if (!pDoc)
        {
            OnStop();
            return;
        }

        const XVideoInfo& info = pDoc->GetVideoInfo();
        if (info.TotalFrames == 0 || info.Duration == 0)
        {
            OnStop();
            return;
        }

        UINT64 nextFrame = _CurrentFrame + 1;
        if (nextFrame >= info.TotalFrames)
        {
            OnStop();
            return;
        }

        SeekToFrame(nextFrame);
    }

    CView::OnTimer(nIDEvent);
}

void XVideoEditorView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    if (pScrollBar && pScrollBar->GetDlgCtrlID() == IDC_FRAME_SLIDER)
    {
        XVideoEditorDocument* pDoc = GetDocument();
        if (!pDoc)
        {
            CView::OnHScroll(nSBCode, nPos, pScrollBar);
            return;
        }

        const XVideoInfo& info = pDoc->GetVideoInfo();
        if (info.Duration == 0)
        {
            CView::OnHScroll(nSBCode, nPos, pScrollBar);
            return;
        }

        int sliderPos = _FrameSlider.GetPos();
        
        UINT64 baseFrame = GetThumbnailStartFrame(_SelectedThumbnail);
        UINT64 targetFrame = baseFrame + static_cast<UINT64>(sliderPos);
        
        if (targetFrame >= info.TotalFrames && info.TotalFrames > 0)
            targetFrame = info.TotalFrames - 1;

        LONGLONG targetPos = pDoc->FrameToPosition(targetFrame);

        _IsDragging = (nSBCode == TB_THUMBTRACK);
        
        SeekToPosition(targetPos);
    }

    CView::OnHScroll(nSBCode, nPos, pScrollBar);
}

LRESULT XVideoEditorView::OnThumbnailClicked(WPARAM wParam, LPARAM lParam)
{
    int thumbIndex = static_cast<int>(wParam);
    UINT64 frame = static_cast<UINT64>(lParam);

    if (thumbIndex == -1)
    {
        GenerateThumbnails();
    }
    else
    {
        _SelectedThumbnail = thumbIndex;
        _ThumbnailStrip.SetSelectedThumbnail(thumbIndex);
        UpdateSliderForThumbnail(thumbIndex);
        SeekToFrame(frame);
    }

    return 0;
}

void XVideoEditorView::SeekToFrame(UINT64 pFrame)
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    LONGLONG pos = pDoc->FrameToPosition(pFrame);
    SeekToPosition(pos);
}

void XVideoEditorView::SeekToPosition(LONGLONG pPosition)
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    const XVideoInfo& info = pDoc->GetVideoInfo();
    if (info.Duration == 0)
        return;

    if (pPosition < 0)
        pPosition = 0;
    if (pPosition > static_cast<LONGLONG>(info.Duration))
        pPosition = static_cast<LONGLONG>(info.Duration);

    _CurrentPosition = pPosition;
    _CurrentFrame = pDoc->PositionToFrame(pPosition);
    _ThumbnailStrip.SetCurrentFrame(_CurrentFrame);

    int framesPerThumb = GetFramesPerThumbnail();
    int newThumbIndex = static_cast<int>(_CurrentFrame / framesPerThumb);
    int maxThumbIndex = GetThumbnailCount() - 1;
    if (maxThumbIndex < 0) maxThumbIndex = 0;
    if (newThumbIndex > maxThumbIndex)
        newThumbIndex = maxThumbIndex;

    bool thumbnailChanged = (newThumbIndex != _SelectedThumbnail);
    if (thumbnailChanged)
    {
        _SelectedThumbnail = newThumbIndex;
        _ThumbnailStrip.SetSelectedThumbnail(newThumbIndex);
        UpdateSliderForThumbnail(newThumbIndex);
    }

    pDoc->GetFrameBitmapFast(pPosition, _CurrentFrameBitmap);

    if (!_IsDragging)
    {
        UINT64 baseFrame = GetThumbnailStartFrame(_SelectedThumbnail);
        int sliderPos = 0;
        if (_CurrentFrame >= baseFrame)
            sliderPos = static_cast<int>(_CurrentFrame - baseFrame);
        _FrameSlider.SetPos(sliderPos);
    }

    CString txt;
    txt.Format(_T("Frame: %llu"), _CurrentFrame);
    _CurrentFrameLabel.SetWindowText(txt);

    RedrawWindow(_VideoRect, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

void XVideoEditorView::UpdateSliderForThumbnail(int pThumbIndex)
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    int framesPerThumb = GetFramesPerThumbnail();
    const XVideoInfo& info = pDoc->GetVideoInfo();

    UINT64 baseFrame = GetThumbnailStartFrame(pThumbIndex);
    UINT64 endFrame = baseFrame + framesPerThumb - 1;

    if (endFrame >= info.TotalFrames && info.TotalFrames > 0)
        endFrame = info.TotalFrames - 1;

    int rangeMax = static_cast<int>(endFrame - baseFrame);
    _FrameSlider.SetRange(0, rangeMax);

    int currentOffset = 0;
    if (_CurrentFrame >= baseFrame && _CurrentFrame <= endFrame)
        currentOffset = static_cast<int>(_CurrentFrame - baseFrame);

    _FrameSlider.SetPos(currentOffset);
}

int XVideoEditorView::GetFramesPerThumbnail() const
{
    CString text;
    _FrameCountEdit.GetWindowText(text);
    int framesPerThumb = _ttoi(text);
    if (framesPerThumb < 1)
        framesPerThumb = 500;
    return framesPerThumb;
}

int XVideoEditorView::GetThumbnailCount() const
{
    XVideoEditorDocument* pDoc = const_cast<XVideoEditorView*>(this)->GetDocument();
    if (!pDoc)
        return 0;

    const XVideoInfo& info = pDoc->GetVideoInfo();
    int framesPerThumb = GetFramesPerThumbnail();
    
    int count = static_cast<int>((info.TotalFrames + framesPerThumb - 1) / framesPerThumb);
    return max(1, count);
}

UINT64 XVideoEditorView::GetThumbnailStartFrame(int pThumbIndex) const
{
    int framesPerThumb = GetFramesPerThumbnail();
    return static_cast<UINT64>(pThumbIndex) * framesPerThumb;
}

void XVideoEditorView::GenerateThumbnails()
{
    XVideoEditorDocument* pDoc = GetDocument();
    if (!pDoc)
        return;

    const XVideoInfo& info = pDoc->GetVideoInfo();
    if (info.TotalFrames == 0)
        return;

    int framesPerThumb = GetFramesPerThumbnail();
    int thumbCount = GetThumbnailCount();

    _ThumbnailStrip.SetFrameCount(framesPerThumb);

    CRect rc;
    _ThumbnailStrip.GetClientRect(&rc);
    if (rc.Width() <= 0)
        return;

    int visibleThumbs = (rc.Width() + 4) / 124;
    visibleThumbs = min(visibleThumbs, thumbCount);
    visibleThumbs = max(1, visibleThumbs);

    for (int i = 0; i < visibleThumbs; i++)
    {
        UINT64 frame = GetThumbnailStartFrame(i);
        if (frame >= info.TotalFrames)
            frame = info.TotalFrames - 1;

        LONGLONG pos = pDoc->FrameToPosition(frame);

        CBitmap bmp;
        HRESULT hr = pDoc->GetFrameBitmap(pos, bmp);
        if (SUCCEEDED(hr) && bmp.m_hObject)
        {
            _ThumbnailStrip.SetThumbnailBitmap(i, static_cast<HBITMAP>(bmp.Detach()));
        }
    }
}
void XVideoEditorView::OnCropToggle()
{
    _CropEnabled = (_CropCheckbox.GetCheck() == BST_CHECKED);
    
    if (_CropEnabled && _CropRect.IsRectEmpty())
    {
        XVideoEditorDocument* pDoc = GetDocument();
        if (pDoc)
        {
            const XVideoInfo& info = pDoc->GetVideoInfo();
            int margin = min(info.Width, info.Height) / 4;
            _CropRect = CRect(margin, margin, info.Width - margin, info.Height - margin);
        }
    }
    
    Invalidate();
}

void XVideoEditorView::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (_CropEnabled)
    {
        _CropDragMode = HitTestCropRect(point);
        if (_CropDragMode > 0)
        {
            SetCapture();
            _CropDragStart = point;
            _CropDragStartRect = _CropRectScreen;
        }
    }
    
    CView::OnLButtonDown(nFlags, point);
}

void XVideoEditorView::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (_CropDragMode > 0)
    {
        ReleaseCapture();
        _CropDragMode = 0;
        _CropRect = ScreenToVideo(_CropRectScreen);
    }
    
    CView::OnLButtonUp(nFlags, point);
}

void XVideoEditorView::OnMouseMove(UINT nFlags, CPoint point)
{
    if (_CropDragMode > 0 && (nFlags & MK_LBUTTON))
    {
        int dx = point.x - _CropDragStart.x;
        int dy = point.y - _CropDragStart.y;
        
        CRect newRect = _CropDragStartRect;
        
        switch (_CropDragMode)
        {
        case 1: // Top-left
            newRect.left += dx;
            newRect.top += dy;
            break;
        case 2: // Top-center
            newRect.top += dy;
            break;
        case 3: // Top-right
            newRect.right += dx;
            newRect.top += dy;
            break;
        case 4: // Right-center
            newRect.right += dx;
            break;
        case 5: // Bottom-right
            newRect.right += dx;
            newRect.bottom += dy;
            break;
        case 6: // Bottom-center
            newRect.bottom += dy;
            break;
        case 7: // Bottom-left
            newRect.left += dx;
            newRect.bottom += dy;
            break;
        case 8: // Left-center
            newRect.left += dx;
            break;
        case 9: // Move entire rect
            newRect.OffsetRect(dx, dy);
            _CropDragStart = point;
            _CropDragStartRect = newRect;
            break;
        }
        
        if (newRect.Width() >= 20 && newRect.Height() >= 20)
        {
            _CropRectScreen = newRect;
            _CropRect = ScreenToVideo(_CropRectScreen);
            Invalidate();
        }
    }
    
    CView::OnMouseMove(nFlags, point);
}

BOOL XVideoEditorView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (_CropEnabled && nHitTest == HTCLIENT)
    {
        CPoint point;
        GetCursorPos(&point);
        ScreenToClient(&point);
        
        int hit = HitTestCropRect(point);
        LPCTSTR cursor = IDC_ARROW;
        
        switch (hit)
        {
        case 1: case 5: cursor = IDC_SIZENWSE; break;
        case 2: case 6: cursor = IDC_SIZENS; break;
        case 3: case 7: cursor = IDC_SIZENESW; break;
        case 4: case 8: cursor = IDC_SIZEWE; break;
        case 9: cursor = IDC_SIZEALL; break;
        }
        
        if (hit > 0)
        {
            SetCursor(LoadCursor(nullptr, cursor));
            return TRUE;
        }
    }
    
    return CView::OnSetCursor(pWnd, nHitTest, message);
}