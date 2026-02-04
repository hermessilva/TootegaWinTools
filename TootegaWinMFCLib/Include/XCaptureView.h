#pragma once

#include "XWindowEnumerator.h"
#include "XPreviewPanel.h"
#include "XVideoRecorder.h"

class XCaptureDocument;

class XCaptureView : public CView
{
    DECLARE_DYNCREATE(XCaptureView)

public:
    XCaptureView() noexcept;
    virtual ~XCaptureView();

    XCaptureDocument* GetDocument() const;

    virtual void OnDraw(CDC* pDC) override;
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    virtual void OnInitialUpdate() override;

protected:
    CComboBox       _ComboWindows;
    CButton         _BtnRefresh;
    CEdit           _EditFPS;
    CButton         _CheckGrayscale;
    CButton         _BtnStart;
    CButton         _BtnStop;
    CStatic         _LabelFPS;
    CStatic         _LabelFileName;
    CStatic         _LabelRecordTime;
    CStatic         _LabelAvgFPS;
    XPreviewPanel   _PreviewPanel;

    XWindowEnumerator _WindowEnumerator;
    XVideoRecorder    _VideoRecorder;

    CFont           _UIFont;
    CString         _CurrentFileName;
    DWORD           _RecordStartTime;
    int             _FrameCount;
    double          _AvgFPS;

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT pType, int pCX, int pCY);
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR pIDEvent);

    afx_msg void OnComboWindowsSelChange();
    afx_msg void OnBtnRefreshClicked();
    afx_msg void OnBtnStartClicked();
    afx_msg void OnBtnStopClicked();

    DECLARE_MESSAGE_MAP()

private:
    void CreateControls();
    void LayoutControls();
    void RefreshWindowList();
    void UpdateButtonStates();
    void UpdateRecordingStatus();
    
    static const UINT_PTR TIMER_REFRESH = 1;
    static const UINT_PTR TIMER_RECORDING_STATUS = 2;
};
