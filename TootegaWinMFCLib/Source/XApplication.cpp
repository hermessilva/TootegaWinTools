#include "pch.h"
#include "XApplication.h"
#include "XMainFrame.h"
#include "XChildFrame.h"
#include "XCaptureDocument.h"
#include "XCaptureView.h"
#include "XVideoEditorDocument.h"
#include "XVideoEditorView.h"
#include "XVideoEditorFrame.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

XApplication theApp;

BEGIN_MESSAGE_MAP(XApplication, CWinAppEx)
    ON_COMMAND(ID_APP_ABOUT, &XApplication::OnAppAbout)
    ON_COMMAND(ID_FILE_NEW, &XApplication::OnFileNewCapture)
    ON_COMMAND(ID_FILE_OPEN_VIDEO, &XApplication::OnFileOpenVideo)
END_MESSAGE_MAP()

XApplication::XApplication() noexcept
{
    SetAppID(_T("Tootega.VideoTool.1.0"));
}

XApplication::~XApplication()
{
}

BOOL XApplication::InitInstance()
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    CWinAppEx::InitInstance();

    if (!AfxOleInit())
    {
        AfxMessageBox(_T("OLE initialization failed."));
        return FALSE;
    }

    InitializeMF();

    EnableTaskbarInteraction(FALSE);

    SetRegistryKey(_T("Tootega"));
    LoadStdProfileSettings(4);

    InitContextMenuManager();
    InitKeyboardManager();
    InitTooltipManager();

    CMFCToolTipInfo params;
    params.m_bVislManagerTheme = TRUE;
    GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL, RUNTIME_CLASS(CMFCToolTipCtrl), &params);

    _CaptureTemplate = new CMultiDocTemplate(
        IDR_CAPTURETYPE,
        RUNTIME_CLASS(XCaptureDocument),
        RUNTIME_CLASS(XChildFrame),
        RUNTIME_CLASS(XCaptureView));

    if (!_CaptureTemplate)
        return FALSE;

    AddDocTemplate(_CaptureTemplate);

    _VideoEditorTemplate = new CMultiDocTemplate(
        IDR_VIDEOEDITORTYPE,
        RUNTIME_CLASS(XVideoEditorDocument),
        RUNTIME_CLASS(XVideoEditorFrame),
        RUNTIME_CLASS(XVideoEditorView));

    if (!_VideoEditorTemplate)
        return FALSE;

    AddDocTemplate(_VideoEditorTemplate);

    auto pMainFrame = new XMainFrame;
    if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
    {
        delete pMainFrame;
        return FALSE;
    }

    m_pMainWnd = pMainFrame;
    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->UpdateWindow();

    return TRUE;
}

int XApplication::ExitInstance()
{
    ShutdownMF();
    AfxOleTerm(FALSE);
    return CWinAppEx::ExitInstance();
}

void XApplication::InitializeMF()
{
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
        TRACE(_T("Media Foundation initialization failed: 0x%08X\n"), hr);
}

void XApplication::ShutdownMF()
{
    MFShutdown();
}

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}

    enum { IDD = IDD_ABOUTBOX };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override
    {
        CDialogEx::DoDataExchange(pDX);
    }

    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

void XApplication::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

void XApplication::OnFileNewCapture()
{
    if (_CaptureTemplate)
        _CaptureTemplate->OpenDocumentFile(nullptr);
}

void XApplication::OnFileOpenVideo()
{
    CFileDialog dlg(TRUE, nullptr, nullptr,
        OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
        _T("Video Files (*.mp4;*.avi;*.mov;*.wmv;*.mkv)|*.mp4;*.avi;*.mov;*.wmv;*.mkv|All Files (*.*)|*.*||"));

    if (dlg.DoModal() != IDOK)
        return;

    if (_VideoEditorTemplate)
        _VideoEditorTemplate->OpenDocumentFile(dlg.GetPathName());
}
