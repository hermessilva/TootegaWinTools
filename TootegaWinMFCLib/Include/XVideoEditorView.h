#pragma once

#include "XVideoEditorDocument.h"
#include "XThumbnailStrip.h"
#include "Resource.h"

class XExportProgressDlg : public CDialogEx
{
public:
    XExportProgressDlg(CWnd* pParent = nullptr);
    virtual ~XExportProgressDlg();

    void SetProgress(UINT64 pCurrent, UINT64 pTotal);
    void SetETA(double pSeconds);
    void SetStatus(LPCTSTR pStatus);
    bool IsCancelled() const { return _Cancelled; }

    enum { IDD = IDD_EXPORT_PROGRESS };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    afx_msg void OnCancel();
    DECLARE_MESSAGE_MAP()

private:
    CProgressCtrl _Progress;
    CStatic _StatusLabel;
    CStatic _ETALabel;
    bool _Cancelled;
};

class XVideoEditorView : public CView
{
    DECLARE_DYNCREATE(XVideoEditorView)

protected:
    XVideoEditorView() noexcept;
    virtual ~XVideoEditorView();

public:
    XVideoEditorDocument* GetDocument() const { return reinterpret_cast<XVideoEditorDocument*>(m_pDocument); }

    virtual void OnDraw(CDC* pDC) override;
    virtual void OnInitialUpdate() override;
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
    DECLARE_MESSAGE_MAP()

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    afx_msg void OnFrameCountChange();
    afx_msg void OnMarkStart();
    afx_msg void OnMarkEnd();
    afx_msg void OnExport();
    afx_msg void OnPlayPause();
    afx_msg void OnStop();
    afx_msg void OnToggleAudio();
    afx_msg void OnSaveFrame();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnCropToggle();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

    afx_msg LRESULT OnThumbnailClicked(WPARAM wParam, LPARAM lParam);

private:
    CEdit _FrameCountEdit;
    CButton _MarkStartButton;
    CButton _MarkEndButton;
    CButton _ExportButton;
    CButton _PlayButton;
    CButton _StopButton;
    CButton _AudioToggle;
    CButton _SaveFrameButton;
    CButton _CropCheckbox;

    CSliderCtrl _FrameSlider;
    XThumbnailStrip _ThumbnailStrip;

    CStatic _StartFrameLabel;
    CStatic _EndFrameLabel;
    CStatic _DurationLabel;
    CStatic _CurrentFrameLabel;

    CBitmap _CurrentFrameBitmap;
    CRect _VideoRect;

    UINT64 _CurrentFrame;
    LONGLONG _CurrentPosition;
    int _SelectedThumbnail;
    bool _IsPlaying;
    bool _AudioEnabled;
    bool _IsDragging;
    LONGLONG _PendingSeekPosition;
    ULONGLONG _LastSeekTime;

    bool _CropEnabled;
    CRect _CropRect;
    CRect _CropRectScreen;
    int _CropDragMode;
    CPoint _CropDragStart;
    CRect _CropDragStartRect;

    CFont _UIFont;
    CFont _LabelFont;

    void CreateControls();
    void LayoutControls();
    void UpdateFrameDisplay();
    void SeekToFrame(UINT64 pFrame);
    void SeekToPosition(LONGLONG pPosition);
    void GenerateThumbnails();
    void UpdateDurationLabel();
    void DrawVideoFrame(CDC* pDC);
    void UpdateSliderForThumbnail(int pThumbIndex);
    int GetFramesPerThumbnail() const;
    int GetThumbnailCount() const;
    UINT64 GetThumbnailStartFrame(int pThumbIndex) const;
    
    void DrawCropRect(CDC* pDC);
    int HitTestCropRect(CPoint pPoint) const;
    CRect VideoToScreen(const CRect& pVideoRect) const;
    CRect ScreenToVideo(const CRect& pScreenRect) const;

    static const int TOOLBAR_HEIGHT = 40;
    static const int THUMBNAIL_HEIGHT = 80;
    static const int SLIDER_HEIGHT = 30;
    static const int LABEL_WIDTH = 100;
};
