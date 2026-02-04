#include "pch.h"
#include "XMainFrame.h"
#include "XApplication.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(XMainFrame, CMDIFrameWndEx)

BEGIN_MESSAGE_MAP(XMainFrame, CMDIFrameWndEx)
    ON_WM_CREATE()
    ON_COMMAND(ID_WINDOW_MANAGER, &XMainFrame::OnWindowManager)
    ON_COMMAND(ID_WINDOW_RESET_LAYOUT, &XMainFrame::OnResetLayout)
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_SEPARATOR,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

XMainFrame::XMainFrame() noexcept
{
}

XMainFrame::~XMainFrame()
{
}

int XMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CMDIFrameWndEx::OnCreate(lpCreateStruct) == -1)
        return -1;

    if (!_MenuBar.Create(this))
        return -1;

    _MenuBar.SetPaneStyle((_MenuBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY) & ~CBRS_GRIPPER & ~CBRS_SIZE_DYNAMIC);
    _MenuBar.EnableDocking(0);
    _MenuBar.SetDockingMode(DT_UNDEFINED);
    CMFCPopupMenu::SetForceMenuFocus(FALSE);

    if (!_StatusBar.Create(this))
        return -1;

    _StatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT));

    CMDITabInfo mdiTabParams;
    mdiTabParams.m_style = CMFCTabCtrl::STYLE_3D_VS2005;
    mdiTabParams.m_bActiveTabCloseButton = TRUE;
    mdiTabParams.m_bTabIcons = FALSE;
    mdiTabParams.m_bAutoColor = FALSE;
    mdiTabParams.m_bDocumentMenu = TRUE;
    mdiTabParams.m_bEnableTabSwap = TRUE;
    mdiTabParams.m_bFlatFrame = TRUE;
    mdiTabParams.m_bTabCloseButton = FALSE;
    mdiTabParams.m_nTabBorderSize = 0;
    mdiTabParams.m_tabLocation = CMFCTabCtrl::LOCATION_TOP;
    EnableMDITabbedGroups(TRUE, mdiTabParams);

    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
    CMFCVisualManagerWindows::m_b3DTabsXPTheme = TRUE;

    RecalcLayout();

    EnableWindowsDialog(ID_WINDOW_MANAGER, _T("&Windows..."), TRUE);

    return 0;
}

BOOL XMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CMDIFrameWndEx::PreCreateWindow(cs))
        return FALSE;

    cs.style = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE;
    cs.cx = 1280;
    cs.cy = 800;

    return TRUE;
}

void XMainFrame::OnWindowManager()
{
    ShowWindowsDialog();
}

void XMainFrame::OnResetLayout()
{
    CWinAppEx* pApp = DYNAMIC_DOWNCAST(CWinAppEx, AfxGetApp());
    if (pApp)
        pApp->CleanState();

    ShowWindow(SW_RESTORE);
    CRect rc(0, 0, 1280, 800);
    AdjustWindowRect(&rc, GetStyle(), TRUE);
    CenterWindow();
    RecalcLayout();
}
