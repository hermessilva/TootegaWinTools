#include "pch.h"
#include "XCaptureView.h"
#include "XCaptureDocument.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(XCaptureView, CView)

BEGIN_MESSAGE_MAP(XCaptureView, CView)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_CBN_SELCHANGE(IDC_COMBO_WINDOWS, &XCaptureView::OnComboWindowsSelChange)
    ON_BN_CLICKED(IDC_BTN_REFRESH, &XCaptureView::OnBtnRefreshClicked)
    ON_BN_CLICKED(IDC_BTN_START, &XCaptureView::OnBtnStartClicked)
    ON_BN_CLICKED(IDC_BTN_STOP, &XCaptureView::OnBtnStopClicked)
END_MESSAGE_MAP()

XCaptureView::XCaptureView() noexcept
    : _RecordStartTime(0)
    , _FrameCount(0)
    , _AvgFPS(0.0)
{
}

XCaptureView::~XCaptureView()
{
}

XCaptureDocument* XCaptureView::GetDocument() const
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(XCaptureDocument)));
    return reinterpret_cast<XCaptureDocument*>(m_pDocument);
}

int XCaptureView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
        return -1;

    _UIFont.CreatePointFont(90, _T("Segoe UI"));

    CreateControls();

    return 0;
}

void XCaptureView::CreateControls()
{
    CRect rect(0, 0, 100, 24);

    _ComboWindows.Create(
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
        CRect(10, 10, 400, 300),
        this,
        IDC_COMBO_WINDOWS);
    _ComboWindows.SetFont(&_UIFont);

    _BtnRefresh.Create(
        _T("Refresh"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(410, 10, 490, 34),
        this,
        IDC_BTN_REFRESH);
    _BtnRefresh.SetFont(&_UIFont);

    _LabelFPS.Create(
        _T("FPS:"),
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        CRect(500, 13, 530, 31),
        this);
    _LabelFPS.SetFont(&_UIFont);

    _EditFPS.Create(
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER | ES_CENTER,
        CRect(535, 10, 585, 34),
        this,
        IDC_EDIT_FPS);
    _EditFPS.SetFont(&_UIFont);
    _EditFPS.SetWindowText(_T("30"));
    _EditFPS.SetLimitText(3);

    _CheckGrayscale.Create(
        _T("Grayscale"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        CRect(595, 10, 690, 34),
        this,
        IDC_CHECK_GRAYSCALE);
    _CheckGrayscale.SetFont(&_UIFont);

    _BtnStart.Create(
        _T("Start"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(700, 10, 770, 34),
        this,
        IDC_BTN_START);
    _BtnStart.SetFont(&_UIFont);

    _BtnStop.Create(
        _T("Stop"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(780, 10, 850, 34),
        this,
        IDC_BTN_STOP);
    _BtnStop.SetFont(&_UIFont);
    _BtnStop.EnableWindow(FALSE);

    // Status labels for recording info
    _LabelFileName.Create(
        _T(""),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(10, 44, 400, 62),
        this);
    _LabelFileName.SetFont(&_UIFont);

    _LabelRecordTime.Create(
        _T(""),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(410, 44, 550, 62),
        this);
    _LabelRecordTime.SetFont(&_UIFont);

    _LabelAvgFPS.Create(
        _T(""),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(560, 44, 700, 62),
        this);
    _LabelAvgFPS.SetFont(&_UIFont);

    _PreviewPanel.Create(
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        CRect(10, 50, 400, 300),
        this,
        IDC_PREVIEW_PANEL);
}

void XCaptureView::OnInitialUpdate()
{
    CView::OnInitialUpdate();
    RefreshWindowList();
    UpdateButtonStates();
}

void XCaptureView::LayoutControls()
{
    CRect clientRect;
    GetClientRect(&clientRect);

    int topBarHeight = 44;
    int statusBarHeight = 20;
    int margin = 10;
    int btnWidth = 80;
    int editWidth = 50;
    int comboWidth = 350;

    int x = margin;

    _ComboWindows.MoveWindow(x, margin, comboWidth, 300);
    x += comboWidth + margin;

    _BtnRefresh.MoveWindow(x, margin, btnWidth, 24);
    x += btnWidth + margin * 2;

    _LabelFPS.MoveWindow(x, margin + 3, 30, 20);
    x += 35;

    _EditFPS.MoveWindow(x, margin, editWidth, 24);
    x += editWidth + margin;

    _CheckGrayscale.MoveWindow(x, margin, 90, 24);
    x += 95;

    _BtnStart.MoveWindow(x, margin, btnWidth, 24);
    x += btnWidth + margin;

    _BtnStop.MoveWindow(x, margin, btnWidth, 24);

    // Status bar labels
    int statusY = topBarHeight;
    _LabelFileName.MoveWindow(margin, statusY, 400, statusBarHeight);
    _LabelRecordTime.MoveWindow(420, statusY, 140, statusBarHeight);
    _LabelAvgFPS.MoveWindow(570, statusY, 140, statusBarHeight);

    int panelTop = topBarHeight + statusBarHeight + 4;
    int panelHeight = clientRect.Height() - panelTop - margin;
    int panelWidth = clientRect.Width() - margin * 2;

    _PreviewPanel.MoveWindow(margin, panelTop, panelWidth, panelHeight);
}

void XCaptureView::OnSize(UINT pType, int pCX, int pCY)
{
    CView::OnSize(pType, pCX, pCY);

    if (_ComboWindows.GetSafeHwnd())
        LayoutControls();
}

void XCaptureView::OnDestroy()
{
    KillTimer(TIMER_REFRESH);
    KillTimer(TIMER_RECORDING_STATUS);
    
    if (_VideoRecorder.IsRecording())
        _VideoRecorder.Stop();

    CView::OnDestroy();
}

void XCaptureView::OnTimer(UINT_PTR pIDEvent)
{
    if (pIDEvent == TIMER_REFRESH)
        _PreviewPanel.RefreshCapture();
    else if (pIDEvent == TIMER_RECORDING_STATUS)
        UpdateRecordingStatus();

    CView::OnTimer(pIDEvent);
}

void XCaptureView::RefreshWindowList()
{
    _WindowEnumerator.Refresh();

    _ComboWindows.ResetContent();

    for (const auto& wnd : _WindowEnumerator.GetWindows())
    {
        int idx = _ComboWindows.AddString(wnd.Title);
        _ComboWindows.SetItemData(idx, reinterpret_cast<DWORD_PTR>(wnd.Handle));
    }

    if (_ComboWindows.GetCount() > 0)
        _ComboWindows.SetCurSel(0);

    OnComboWindowsSelChange();
}

void XCaptureView::OnComboWindowsSelChange()
{
    int sel = _ComboWindows.GetCurSel();
    if (sel != CB_ERR)
    {
        HWND hwnd = reinterpret_cast<HWND>(_ComboWindows.GetItemData(sel));
        _PreviewPanel.SetSourceWindow(hwnd);
        
        auto pDoc = GetDocument();
        if (pDoc)
            pDoc->SetTargetWindow(hwnd);

        // Real-time preview at ~30 FPS
        SetTimer(TIMER_REFRESH, 33, nullptr);
    }
    else
    {
        KillTimer(TIMER_REFRESH);
        _PreviewPanel.SetSourceWindow(nullptr);
    }

    UpdateButtonStates();
}

void XCaptureView::OnBtnRefreshClicked()
{
    RefreshWindowList();
}

void XCaptureView::OnBtnStartClicked()
{
    int sel = _ComboWindows.GetCurSel();
    if (sel == CB_ERR)
    {
        AfxMessageBox(_T("Please select a window first."), MB_ICONWARNING);
        return;
    }

    CString fpsText;
    _EditFPS.GetWindowText(fpsText);
    int fps = _ttoi(fpsText);
    if (fps <= 0 || fps > 120)
    {
        AfxMessageBox(_T("FPS must be between 1 and 120."), MB_ICONWARNING);
        return;
    }

    CFileDialog dlg(
        FALSE,
        _T("mp4"),
        _T("capture.mp4"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("MP4 Video Files (*.mp4)|*.mp4|All Files (*.*)|*.*||"),
        this);

    if (dlg.DoModal() != IDOK)
        return;

    CString filePath = dlg.GetPathName();
    HWND hwnd = reinterpret_cast<HWND>(_ComboWindows.GetItemData(sel));
    CRect captureRect = _PreviewPanel.GetScaledSelectionToSource();
    bool grayscale = (_CheckGrayscale.GetCheck() == BST_CHECKED);

    auto pDoc = GetDocument();
    if (pDoc)
    {
        pDoc->SetOutputFilePath(filePath);
        pDoc->SetFPS(fps);
        pDoc->SetGrayscale(grayscale);
        pDoc->SetCaptureRect(captureRect);
    }

    if (!_VideoRecorder.Start(filePath, hwnd, captureRect, fps, grayscale))
    {
        CString msg;
        msg.Format(_T("Failed to start recording: %s"), _VideoRecorder.GetLastError().GetString());
        AfxMessageBox(msg, MB_ICONERROR);
        return;
    }

    // Store recording info and start status timer
    _CurrentFileName = dlg.GetFileName();
    _RecordStartTime = GetTickCount();
    _FrameCount = 0;
    _AvgFPS = 0.0;
    
    // Hide the selection rectangle during recording
    _PreviewPanel.SetTrackerVisible(false);
    
    // Start status update timer (100ms interval for smooth updates)
    SetTimer(TIMER_RECORDING_STATUS, 100, nullptr);
    
    UpdateRecordingStatus();
    UpdateButtonStates();
}

void XCaptureView::OnBtnStopClicked()
{
    _VideoRecorder.Stop();
    
    // Stop status timer
    KillTimer(TIMER_RECORDING_STATUS);
    
    // Show the selection rectangle again
    _PreviewPanel.SetTrackerVisible(true);
    
    // Clear status labels
    _LabelFileName.SetWindowText(_T(""));
    _LabelRecordTime.SetWindowText(_T(""));
    _LabelAvgFPS.SetWindowText(_T(""));
    
    UpdateButtonStates();

    AfxMessageBox(_T("Recording stopped and saved."), MB_ICONINFORMATION);
}

void XCaptureView::UpdateButtonStates()
{
    bool recording = _VideoRecorder.IsRecording();
    bool hasSelection = (_ComboWindows.GetCurSel() != CB_ERR);

    _BtnStart.EnableWindow(!recording && hasSelection);
    _BtnStop.EnableWindow(recording);
    _ComboWindows.EnableWindow(!recording);
    _BtnRefresh.EnableWindow(!recording);
    _EditFPS.EnableWindow(!recording);
    _CheckGrayscale.EnableWindow(!recording);
}

void XCaptureView::UpdateRecordingStatus()
{
    if (!_VideoRecorder.IsRecording())
        return;

    // Update file name
    CString fileText;
    fileText.Format(_T("File: %s"), _CurrentFileName.GetString());
    _LabelFileName.SetWindowText(fileText);

    // Calculate and update elapsed time
    DWORD elapsed = GetTickCount() - _RecordStartTime;
    int totalSeconds = elapsed / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    
    CString timeText;
    timeText.Format(_T("Time: %02d:%02d:%02d"), hours, minutes, seconds);
    _LabelRecordTime.SetWindowText(timeText);

    // Calculate average FPS (estimate based on configured FPS and elapsed time)
    CString fpsText;
    _EditFPS.GetWindowText(fpsText);
    int configuredFPS = _ttoi(fpsText);
    
    // Estimated frame count based on time elapsed
    _FrameCount = static_cast<int>((elapsed / 1000.0) * configuredFPS);
    if (elapsed > 0)
        _AvgFPS = (_FrameCount * 1000.0) / elapsed;
    
    CString avgFpsText;
    avgFpsText.Format(_T("Avg FPS: %.1f"), _AvgFPS);
    _LabelAvgFPS.SetWindowText(avgFpsText);
}

BOOL XCaptureView::PreCreateWindow(CREATESTRUCT& cs)
{
    return CView::PreCreateWindow(cs);
}

void XCaptureView::OnDraw(CDC* pDC)
{
    CRect clientRect;
    GetClientRect(&clientRect);
    pDC->FillSolidRect(&clientRect, GetSysColor(COLOR_3DFACE));
}
