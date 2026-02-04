#pragma once

class XThumbnailStrip : public CWnd
{
    DECLARE_DYNAMIC(XThumbnailStrip)

public:
    XThumbnailStrip() noexcept;
    virtual ~XThumbnailStrip();

    void SetFrameCount(UINT pFrameCount);
    UINT GetFrameCount() const { return _FrameCount; }

    void SetCurrentFrame(UINT64 pFrame);
    UINT64 GetCurrentFrame() const { return _CurrentFrame; }

    void SetTotalFrames(UINT64 pTotal);
    UINT64 GetTotalFrames() const { return _TotalFrames; }

    void SetSelectedThumbnail(int pIndex);
    int GetSelectedThumbnail() const { return _SelectedThumbnail; }

    void SetMarkIn(LONGLONG pFrame);
    void SetMarkOut(LONGLONG pFrame);

    void SetThumbnailBitmap(int pIndex, HBITMAP pBitmap);
    void ClearThumbnails();
    void RegenerateThumbnails();

    int HitTest(CPoint pPoint) const;
    int GetThumbnailCount() const;

    static const UINT WM_THUMBNAILCLICKED = WM_USER + 200;

protected:
    DECLARE_MESSAGE_MAP()

    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

private:
    UINT _FrameCount;
    UINT64 _CurrentFrame;
    UINT64 _TotalFrames;
    int _SelectedThumbnail;
    LONGLONG _MarkIn;
    LONGLONG _MarkOut;

    std::vector<CBitmap*> _Thumbnails;
    int _ThumbnailWidth;
    int _ThumbnailHeight;
    int _Spacing;

    CRect GetThumbnailRect(int pIndex) const;
    void DrawSelectionRange(CDC* pDC);
    void DrawSelectedThumbnailHighlight(CDC* pDC, int pIndex);
};
