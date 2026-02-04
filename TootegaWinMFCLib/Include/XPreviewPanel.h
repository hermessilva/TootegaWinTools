#pragma once

class XPreviewPanel : public CWnd
{
    DECLARE_DYNAMIC(XPreviewPanel)

public:
    XPreviewPanel() noexcept;
    virtual ~XPreviewPanel();

    BOOL Create(DWORD pStyle, const RECT& pRect, CWnd* pParentWnd, UINT pID);

    void SetSourceWindow(HWND pHwnd);
    HWND GetSourceWindow() const { return _SourceWindow; }

    void RefreshCapture();

    CRect GetSelectionRect() const;
    void SetSelectionRect(const CRect& pRect);

    CRect GetScaledSelectionToSource() const;

    void SetTrackerVisible(bool pVisible);
    bool IsTrackerVisible() const { return _TrackerVisible; }

protected:
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT pFlags, CPoint pPoint);
    afx_msg void OnLButtonUp(UINT pFlags, CPoint pPoint);
    afx_msg void OnMouseMove(UINT pFlags, CPoint pPoint);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT pHitTest, UINT pMessage);
    afx_msg void OnSize(UINT pType, int pCX, int pCY);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    HWND            _SourceWindow;
    CBitmap         _Capture;
    CSize           _CaptureSize;
    CRectTracker    _Tracker;
    bool            _Tracking;
    bool            _TrackerVisible;
    CRect           _ImageRect;

    void CaptureWindow();
    void UpdateImageRect();
    CRect ScaleRectToImage(const CRect& pSourceRect) const;
    CRect ScaleRectToSource(const CRect& pImageRect) const;
    void InitializeTracker();
};
