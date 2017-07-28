// TreePCPane.cpp : implementation file
//

#include "stdafx.h"
#include "ModelTool2.h"
#include "NtlMTXml.h"
#include "BBoxPane.h"
#include "PropertyListXML.h"

#include "TreePCPane.h"

CTreePCPane* CTreePCPane::m_pInstance = NULL;

// CTreePCPane

IMPLEMENT_DYNCREATE(CTreePCPane, CXTTreeView)

CTreePCPane::CTreePCPane()
{
    m_pInstance = this;

    m_pImageList = NULL;
    m_pCharacter = NULL;
    m_bDrag      = FALSE;

    m_strRootName = L"PC";        
}

CTreePCPane::~CTreePCPane()
{


    if(m_pImageList)
    {
        delete m_pImageList;
        m_pImageList = NULL;
    }
}

BEGIN_MESSAGE_MAP(CTreePCPane, CXTTreeView)
    ON_NOTIFY_REFLECT(NM_RCLICK, &CTreePCPane::OnNMRclick)
    ON_COMMAND(ID_MENU_ADD_FOLDER, &CTreePCPane::OnMenuAddFolder)
    ON_COMMAND(ID_MENU_RENAME_FOLDER, &CTreePCPane::OnMenuRenameFolder)
    ON_COMMAND(ID_MENU_DELETE_FOLDER, &CTreePCPane::OnMenuDeleteFolder)
    ON_COMMAND(ID_MENU_NEW_SCRIPT, &CTreePCPane::OnMenuNewScript)
    ON_COMMAND(ID_MENU_LOAD_SCRIPT, &CTreePCPane::OnMenuLoadScript)
    ON_COMMAND(ID_SCRIPT_SETCLUMP, &CTreePCPane::OnScriptSetclump)
    ON_COMMAND(ID_MENU_SCRIPT_SAVE, &CTreePCPane::OnMenuScriptSave)
    ON_COMMAND(ID_MENU_SCRIPT_SAVEAS, &CTreePCPane::OnMenuScriptSaveas)
    ON_COMMAND(ID_MENU_SCRIPT_RENAME, &CTreePCPane::OnMenuScriptRename)
    ON_COMMAND(ID_MENU_DELETE_SCRIPT, &CTreePCPane::OnMenuDeleteScript)
    ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, &CTreePCPane::OnTvnEndlabeledit)
    ON_NOTIFY_REFLECT(TVN_BEGINLABELEDIT, &CTreePCPane::OnTvnBeginlabeledit)
    ON_WM_DESTROY()
    ON_NOTIFY_REFLECT(TVN_SELCHANGED, &CTreePCPane::OnTvnSelchanged)
    ON_COMMAND(ID_SCRIPT_SAVEALL, &CTreePCPane::OnScriptSaveall)
    ON_WM_SETFOCUS()
    ON_NOTIFY_REFLECT(TVN_BEGINDRAG, &CTreePCPane::OnTvnBegindrag)
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()


// CTreePCPane diagnostics

#ifdef _DEBUG
void CTreePCPane::AssertValid() const
{
	CXTTreeView::AssertValid();
}

#ifndef _WIN32_WCE
void CTreePCPane::Dump(CDumpContext& dc) const
{
	CXTTreeView::Dump(dc);
}
#endif
#endif //_DEBUG


// CTreePCPane message handlers

void CTreePCPane::OnInitialUpdate()
{
    CXTTreeView::OnInitialUpdate();

    ModifyStyle(0, TVS_SHOWSELALWAYS | TVS_EDITLABELS  | TVS_HASBUTTONS  | TVS_HASLINES);

    // �̹��� ����Ʈ ����
    m_pImageList = new CImageList();
    m_pImageList->Create(16, 16, ILC_COLOR32, 4, 4);
    m_pImageList->Add(AfxGetApp()->LoadIcon(IDI_ICON_CLOSE));
    m_pImageList->Add(AfxGetApp()->LoadIcon(IDI_ICON_OPEN));
    m_pImageList->Add(AfxGetApp()->LoadIcon(IDI_ICON_SCRIPT));        
    GetTreeCtrl().SetImageList(m_pImageList, TVSIL_NORMAL);    

    // Ʈ�� ��Ʈ�� �ʱ�ȭ
    TV_INSERTSTRUCT tvStruct;
    tvStruct.hParent      = NULL;
    tvStruct.hInsertAfter = TVI_LAST;
    tvStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvStruct.item.pszText = (LPWSTR)(LPCWSTR)m_strRootName;
    tvStruct.item.iImage  = 0;
    tvStruct.item.iSelectedImage = 1;
    GetTreeCtrl().InsertItem(&tvStruct);
    
    InitClass();
    LoadTreeXML(m_strTreeFileName);
}

void CTreePCPane::InitClass()
{
    USES_CONVERSION;

    CString sWorkFolder = A2W(CModelToolApplication::GetInstance()->GetWorkDir());
    m_sSaveFolderName = sWorkFolder;
    m_sSaveFolderName += MT_PATH_CHARACTER;
    
    m_strTreeFileName = sWorkFolder;
    m_strTreeFileName += L"\\Tool\\ModelTool\\TreePC.xml";

    m_strPropertyListFileName = m_sSaveFolderName;
    m_strPropertyListFileName += L"CharacterPropertyList.xml";
}

void CTreePCPane::SetModel(CMTCharacter* pCharacter)
{
    if(m_pCharacter == pCharacter)
        return;

    if(m_pCharacter)
    {
        m_pCharacter->SetVisible(FALSE);
    }

    m_pCharacter = pCharacter;
}

RwBool CTreePCPane::LoadTreeXML(const WCHAR* szXmlFileName)
{
    if(!szXmlFileName)
        return FALSE;

    CNtlMTXml xmlLoad;
    xmlLoad.LoadTreeXML((WCHAR*)szXmlFileName, &m_itemNode);    

    GetTreeCtrl().DeleteAllItems();
	InsertItemRecursive(&m_itemNode, NULL);

    GetTreeCtrl().Expand(GetTreeCtrl().GetRootItem(), TVE_EXPAND);

    return TRUE;
}

void CTreePCPane::GetAllChildNode(HTREEITEM hItem, TREEITEM_VECTOR& vTreeItem)
{
    while (hItem)
    {
        vTreeItem.push_back(hItem);

        if (GetTreeCtrl().ItemHasChildren(hItem))
        {
            GetAllChildNode(GetTreeCtrl().GetChildItem(hItem), vTreeItem);  //���ȣ��
        } 

        hItem = GetTreeCtrl().GetNextSiblingItem(hItem);
    }   
}

RwBool CTreePCPane::SaveTreeXML(const WCHAR* szXmlFileName)
{
    if(!szXmlFileName)
        return FALSE;

    CString strRootName = GetTreeCtrl().GetItemText(GetTreeCtrl().GetRootItem());

    CNtlMTXml xmlSave;
    xmlSave.CreateXML((LPWSTR)(LPCWSTR)strRootName);

    TREEITEM_VECTOR vTreeItem;
    GetAllChildNode(GetTreeCtrl().GetRootItem(), vTreeItem);
    for(size_t i = 0; i < vTreeItem.size(); ++i)
    {
        HTREEITEM pItem = GetTreeCtrl().GetParentItem(vTreeItem[i]);
        if(!pItem)
            continue;
        int nImage = 0, nSelectedImage = 0;
        GetTreeCtrl().GetItemImage(vTreeItem[i], nImage, nSelectedImage);
        CString sParentName = GetTreeCtrl().GetItemText(pItem);
        CString sItemName = GetTreeCtrl().GetItemText(vTreeItem[i]);
        
        sItemName.Replace(L"*", L"");

        if(sItemName.Find(L"*") == -1)   // '*' ǥ�ð� ���� �͵鸸 �����Ѵ�.
        {
            // XML�� �߰��Ѵ�.
            if(nImage == 0)    // ����
            {
                xmlSave.AddFolderNode((LPWSTR)(LPCWSTR)sParentName, (LPWSTR)(LPCWSTR)sItemName);
            }
            else if(nImage == 2)  // ��ũ��Ʈ
            {
                xmlSave.AddScriptNode((LPWSTR)(LPCWSTR)sParentName, (LPWSTR)(LPCWSTR)sItemName);
            }        
        }
    }
    xmlSave.Save((WCHAR*)szXmlFileName);  

    // ������Ƽ ����Ʈ ����
    CPropertyListXML xmlPropertyList;
    xmlPropertyList.Save(m_strPropertyListFileName, CPropertyListXML::PROPERTY_CHARACTER, &GetTreeCtrl(), vTreeItem);
    
    return TRUE;
}


void CTreePCPane::OnNMRclick(NMHDR *pNMHDR, LRESULT *pResult)
{
    CPoint point;
    ::GetCursorPos(&point);

    CPoint tempPoint = point;
    ScreenToClient(&tempPoint);
    HTREEITEM hItem     = GetTreeCtrl().HitTest(tempPoint);
    if(hItem)
    {
        GetTreeCtrl().SelectItem(hItem);

        int nImage = 0;
        int nSelectedImage = 0;
        GetTreeCtrl().GetItemImage(hItem, nImage, nSelectedImage);

        CMenu  menuPopup;         
        CMenu* subMenu = NULL;
        menuPopup.LoadMenu(IDR_MENU_POPUP);

        if(nImage == 0)         // ����
        {
            // �޴��� ����.
            subMenu = menuPopup.GetSubMenu(0);
            subMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);        
        }
        else if(nImage == 2)    // ��ũ��Ʈ 
        {
            subMenu = menuPopup.GetSubMenu(1);            
            subMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);        
        }               
    }    

    *pResult = 0;
}

void CTreePCPane::OnMenuAddFolder()
{
    // �� ������ �߰��Ѵ�.
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();;

    if(!hSelectedItem)
        return;

    TV_INSERTSTRUCT tvStruct;
    tvStruct.hParent      = hSelectedItem;    
    tvStruct.hInsertAfter = TVI_LAST;
    tvStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvStruct.item.pszText = L"New Folder";
    tvStruct.item.iImage  = 0;
    tvStruct.item.iSelectedImage = 1;
    HTREEITEM hAddItem = GetTreeCtrl().InsertItem(&tvStruct);

    GetTreeCtrl().SelectItem(hAddItem);
    GetTreeCtrl().EditLabel(hAddItem);
}

void CTreePCPane::OnMenuRenameFolder()
{
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
    GetTreeCtrl().EditLabel(hSelectedItem);
}

void CTreePCPane::OnMenuDeleteFolder()
{
    if(MessageBox(L"Sure Delete Folder?", NULL, MB_OKCANCEL) == IDOK)
    {
        HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();;
        GetTreeCtrl().DeleteItem(hSelectedItem);
    }   
}

void CTreePCPane::OnMenuNewScript()
{
    USES_CONVERSION;

    // ���ο� ��ũ��Ʈ�� ������ �߰��Ѵ�.   

    // ��ũ��Ʈ �׸� �߰�
    TV_INSERTSTRUCT tvStruct;
    tvStruct.hParent      = GetTreeCtrl().GetSelectedItem();;    
    tvStruct.hInsertAfter = TVI_LAST;
    tvStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvStruct.item.pszText = L"New Script";
    tvStruct.item.iImage  = 2;    
    tvStruct.item.iSelectedImage = 2;
    HTREEITEM hAddItem = GetTreeCtrl().InsertItem(&tvStruct);

    GetTreeCtrl().SelectItem(hAddItem);

    if(!OnSetClump())
    {
        MessageBox(L"Clump Load Fail!");
    }
    else
    {
        // ��ũ��Ʈ ���� DFF �̸����� �ٲ۴�.
        // ������� ���� ��ũ��Ʈ�� �̸� �տ� '*'�� ���δ�.        
        CMTCharacter* pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hAddItem);
        if(pCharacter)
        {
            CString strScriptName = L"*";
            strScriptName  += A2W(pCharacter->GetProperty()->GetBaseMeshFileName().c_str());
            strScriptName = strScriptName.MakeUpper();
            strScriptName .Replace(L".DFF", L".XML");
                        
            GetTreeCtrl().SetItemText(hAddItem, strScriptName);        
        }        
    }
}

void CTreePCPane::OnMenuLoadScript()
{
    USES_CONVERSION;
    // ��ũ��Ʈ ������ �ε� �Ѵ�. (��ũ��Ʈ ������ .\Character\ ������ �־�߸� �Ѵ�.)
    WCHAR sLoadFilter[] = L"Character Script File (*.XML)|*.XML||";
    CString workDir = A2W(CModelToolApplication::GetInstance()->GetWorkDir());
    workDir += L"\\Character\\*.XML";    
    CFileDialog fileDlg(TRUE, NULL, workDir, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, sLoadFilter);
    if(fileDlg.DoModal() == IDOK)
    {
        CString sFilePath = fileDlg.GetPathName();
        CString sLoadFileName = fileDlg.GetFileName();        

        // Ʈ���� �׸��� �߰��Ѵ�.
        // �׸� �߰�
        TV_INSERTSTRUCT sTvStruct;
        sTvStruct.hParent = GetTreeCtrl().GetSelectedItem();
        sTvStruct.hInsertAfter = TVI_SORT;
        sTvStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        sTvStruct.item.iImage = 2;
        sTvStruct.item.iSelectedImage = 2;
        sTvStruct.item.pszText = (LPWSTR)(LPCWSTR)sLoadFileName;
        HTREEITEM hItem = GetTreeCtrl().InsertItem(&sTvStruct);
        GetTreeCtrl().SelectItem(hItem);
    }       
}


void CTreePCPane::OnScriptSetclump()
{
    if(!OnSetClump())
    {
        MessageBox(L"Clump Load Fail!");
    }
}

void CTreePCPane::OnMenuScriptSave()
{
    // ���� ���õ� ��ũ��Ʈ�� �����Ѵ�.
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
    if(!hSelectedItem)
        return;

    CString sScriptName = GetTreeCtrl().GetItemText(hSelectedItem);
    CString strMsg;
    
    // ���� '*'���� ���ο� ��ũ��Ʈ��� '*'�� ���� �����Ѵ�.
    int nIndex = sScriptName.Replace(L"*", L"");

    if(OnSaveScript(hSelectedItem, sScriptName, FALSE))
    {
        if(nIndex)
        {
            GetTreeCtrl().SetItemText(hSelectedItem, sScriptName);
        }
    }    
}

void CTreePCPane::OnMenuScriptSaveas()
{
    // ���ο� �̸����� ��ũ��Ʈ�� �����Ѵ�.
    HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();
    if(hItem)
    {
        OnSaveScript(hItem, L"", TRUE);
    }    
}

void CTreePCPane::OnMenuScriptRename()
{
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
    if(!hSelectedItem)
        return;

    GetTreeCtrl().EditLabel(hSelectedItem);
}

void CTreePCPane::OnTvnBeginlabeledit(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
    // ��ũ��Ʈ �̸��� ����Ǳ����� �����صд�.
    if(pTVDispInfo->item.iImage == 2)
    {
        m_strPrevScriptName = pTVDispInfo->item.pszText;
    }
    
    *pResult = 0;
}

void CTreePCPane::OnTvnEndlabeledit(NMHDR *pNMHDR, LRESULT *pResult)
{
    USES_CONVERSION;

    LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
    
    // �̸��� ����Ǿ�����, ��ũ��Ʈ �̸��� ����Ǿ����� ���ϸ��� �������ش�.
    CString strChangedName = pTVDispInfo->item.pszText;
    if(strChangedName != "")
    {
        HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();

        GetTreeCtrl().SetItemText(hSelectedItem, strChangedName);

        if(pTVDispInfo->item.iImage == 2)
        {
            CMTCharacter* pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hSelectedItem);
            if(pCharacter && pCharacter->GetProperty())
            {
                CString strPropName = strChangedName;
                strPropName.Replace(L".XML", L"");
                pCharacter->GetProperty()->SetName(W2A(strChangedName));

                CString sOrgScriptName = m_sSaveFolderName + m_strPrevScriptName;
                CString sNewScriptName = m_sSaveFolderName + strChangedName;
                CFile::Rename(sOrgScriptName, sNewScriptName);
            }
        }
    }
    
    *pResult = 0;
}

void CTreePCPane::OnMenuDeleteScript()
{
    HTREEITEM hSeledtedItem = GetTreeCtrl().GetSelectedItem();
    if(!hSeledtedItem)
        return;

    CMTCharacter* pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hSeledtedItem);
    if(pCharacter)
    {
        // �޸𸮿��� ���Ŵ� ���߿� �Ѵ�.
        pCharacter->SetVisible(FALSE);        
    }

    GetTreeCtrl().DeleteItem(hSeledtedItem);
}


RwBool CTreePCPane::OnSetClump()
{
    // Clump ������ �ε��Ѵ�.
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();;
    if(!hSelectedItem)
        return FALSE;

    USES_CONVERSION;

    WCHAR szOpenFilter[] = L"Clump File (*.dff)|*.dff||";
    CFileDialog fileDlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, szOpenFilter);    
    if(fileDlg.DoModal() == IDOK)
    {
        CString sLoadFilePath = L".";
        sLoadFilePath += fileDlg.GetPathName();	
        CString sLoadFileName = fileDlg.GetFileName();

        CMTCharacter* pCharacter = NULL;
        pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hSelectedItem);
        if(!pCharacter)
        {
            // ������ �߰����ش�.
            pCharacter = new CMTCharacter;
            pCharacter->Create();
            GetTreeCtrl().SetItemData(hSelectedItem, (DWORD_PTR)pCharacter);
            m_vCharacter.push_back(pCharacter);
        }

        RwBool retBool = pCharacter->LoadClump(W2A(sLoadFilePath));
        if(retBool == (RwBool)TRUE)
        {
            ////  ������ ����ü�� �����Ѵ�.                        
            //CString sWorkPath = A2W(CModelToolApplication::GetInstance()->GetWorkDir());

            //sWorkPath = sWorkPath.MakeUpper();                
            //sLoadFilePath = sLoadFilePath.MakeUpper();
            //sLoadFileName = sLoadFileName.MakeUpper();

            //sLoadFilePath.Replace(sWorkPath, L"");                
            //pCharacter->GetProperty()->SetBaseMeshFileName(W2A(sLoadFilePath));

            m_pCharacter = pCharacter;
            
            CModelToolApplication::GetInstance()->SetEditChar(pCharacter);            

            return TRUE;
        }        
    }	

    return FALSE;
}

RwBool CTreePCPane::OnSaveScript(HTREEITEM hItem, const WCHAR* szScriptName, BOOL bVisible)
{
    if(!szScriptName || !hItem)
        return FALSE;

    USES_CONVERSION;

    CString strMsg;
    strMsg.Format(L"[%s] ", szScriptName);
    RwBool bReturn = FALSE;

    CMTCharacter* pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hItem);
    if(!pCharacter)
    {
        strMsg += L"Not Exist Character Data";
        MessageBox(strMsg, NULL, MB_ICONERROR);
        return FALSE;
    }

    CString sCurrentScriptFileName = szScriptName;
    if(bVisible == FALSE)
    {
        // �׳� ��ũ��Ʈ �̸����� �ٷ� �����Ѵ�.
        sCurrentScriptFileName.Replace(L".XML", L"");
        pCharacter->GetProperty()->SetName(W2A(sCurrentScriptFileName));        

        CString sSaveFileName = m_sSaveFolderName;        
        sSaveFileName += szScriptName;
        
        bReturn = pCharacter->GetProperty()->SaveScript(W2A(sSaveFileName));        
    }
    else
    {
        // ���̾�α׸� �ٿ��� ���� �����Ѵ�.
        CString sSavePath = m_sSaveFolderName;
        sSavePath += GetTreeCtrl().GetItemText(hItem);
        sSavePath.Replace(L"*", L"");

        WCHAR szSaveFilter[] = L"Character Script File (*.XML)|*.XML||";
        CFileDialog fileDlg(FALSE, L"XML", sSavePath, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szSaveFilter);
        if(fileDlg.DoModal() == IDOK)
        {
            CString sSaveFilePath = fileDlg.GetPathName();
            CString sSaveFileName = fileDlg.GetFileName();

            sSaveFileName.Replace(L".XML", L""); // ������Ƽ�� ���ӿ����� Ȯ���ڸ� �����Ѵ�.
            pCharacter->GetProperty()->SetName(W2A(sSaveFileName));                
            bReturn = pCharacter->GetProperty()->SaveScript(W2A(sSaveFilePath));
            if(bReturn)
            {
                sSaveFileName += L".XML";   // �ٽ� Ȯ���ڸ� �ٿ��ش�.                
                GetTreeCtrl().SetItemText(hItem, sSaveFileName);    
            }            
        }
    }  

    if(!bReturn)
    {
        strMsg += L"Script File Save Fail!";
        MessageBox(strMsg, NULL, MB_ICONERROR);
    }    

    return bReturn;
}


void CTreePCPane::OnDestroy()
{   
    // â�� ������ ���� XML�� ������ �����Ѵ�.
    SaveTreeXML(m_strTreeFileName);

    for(size_t i = 0; i < m_vCharacter.size(); ++i)
    {
        CMTCharacter* pCharacter = m_vCharacter[i];
        if(pCharacter)
        {
            pCharacter->Destroy();
            delete pCharacter;
            pCharacter = NULL;
        }
    }
    m_vCharacter.clear();


    CXTTreeView::OnDestroy();
    
}

void CTreePCPane::OnTvnSelchanged(NMHDR *pNMHDR, LRESULT *pResult)
{
    USES_CONVERSION;

    LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

    HTREEITEM hItem =  pNMTreeView->itemOld.hItem;   
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
    int nImage = 0, nSelectedImage = 0;
    GetTreeCtrl().GetItemImage(hSelectedItem, nImage, nSelectedImage);

    if(hItem)
    {
        // ������ ĳ���Ͱ� ������ ȭ�鿡�� �����Ѵ�.
        CMTCharacter* pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hItem);
        if(pCharacter)
        {
            pCharacter->SetVisible(FALSE);            
        }

        GetTreeCtrl().SetItemState(hItem, NULL, TVIS_BOLD);

    }

    if(nImage == 0)    // ������ ����������.
    {
        CModelToolApplication::GetInstance()->SetEditChar(NULL);
    }
    else if(nImage == 2)   // ��ũ��Ʈ�� ����������.
    {
        GetTreeCtrl().SetItemState(hSelectedItem, TVIS_BOLD, TVIS_BOLD);

        // ��ũ��Ʈ ��尡 ���õǾ����� ���ϳ����� ǥ���Ѵ�.
        CString sScriptFileName = GetTreeCtrl().GetItemText(hSelectedItem);
        if(sScriptFileName != L"" && sScriptFileName != L"New Script")
        {

            CMTCharacter* pCharacter = (CMTCharacter*)GetTreeCtrl().GetItemData(hSelectedItem);
            if(pCharacter)
            {
                pCharacter->SetVisible(TRUE);                
            }
            else
            {
                // ĳ���Ͱ� ���� �������� �ʾ������� ĳ���͸� �����Ѵ�.
                pCharacter = new CMTCharacter();
                pCharacter->Create();
                
                std::string strXmlFileName = W2A(m_sSaveFolderName);
                strXmlFileName += W2A(sScriptFileName);
                    
                if(pCharacter->GetProperty()->LoadScript(strXmlFileName))
                {
                    std::string strClumpFileName = CModelToolApplication::GetInstance()->GetWorkDir();
                    strClumpFileName += pCharacter->GetProperty()->GetBaseMeshFilePath().c_str();
                    strClumpFileName += pCharacter->GetProperty()->GetBaseMeshFileName().c_str();

                    if(pCharacter->LoadClump((RwChar*)strClumpFileName.c_str()))
                    {
                        GetTreeCtrl().SetItemData(hSelectedItem, (DWORD_PTR)pCharacter);
                        m_vCharacter.push_back(pCharacter);
                    }
                    else
                    {
                        CString msg;
                        msg.Format(L"[%S] Clump File Load Fail!", pCharacter->GetProperty()->GetBaseMeshFileName().c_str());
                        MessageBox(msg, NULL, MB_ICONERROR);

                        pCharacter->Destroy();
                        delete pCharacter;
                        pCharacter = NULL;
                    }
                }
                else
                {
                    CString msg;
                    msg.Format(L"[%s] Script File Load Fail!", sScriptFileName);
                    MessageBox(msg, NULL, MB_ICONERROR);

                    pCharacter->Destroy();
                    delete pCharacter;
                    pCharacter = NULL;
                }
            }

            m_pCharacter = pCharacter;
            CModelToolApplication::GetInstance()->SetEditChar(pCharacter);
        }
    }
    *pResult = 0;
}

void CTreePCPane::OnDataChanged()
{
    // �����Ͱ� ����Ǹ� ��ũ��Ʈ���տ� '*'�� �ٿ��༭ ǥ���Ѵ�.
    HTREEITEM hSelectedItem = GetTreeCtrl().GetSelectedItem();
    if(hSelectedItem)
    {
        CString strItemName = GetTreeCtrl().GetItemText(hSelectedItem);
        if(strItemName.Find(L"*") < 0)
        {
            strItemName.Insert(0, L"*");
            GetTreeCtrl().SetItemText(hSelectedItem, strItemName);
        }        
    }
}
void CTreePCPane::OnScriptSaveall()
{
    // ��� ��ũ��Ʈ�� �����Ѵ�.
    TREEITEM_VECTOR vTreeItem;
    GetAllChildNode(GetTreeCtrl().GetRootItem(), vTreeItem);
    for(UINT i = 0; i < vTreeItem.size(); ++i)
    {
        HTREEITEM hItem = vTreeItem[i];
        if(hItem && GetTreeCtrl().GetItemData(hItem))
        {
            CString strScriptName = GetTreeCtrl().GetItemText(hItem);
            int nIndex = strScriptName.Replace(L"*", L"");
            if(OnSaveScript(hItem, strScriptName, FALSE))
            {
                if(nIndex)
                {
                    GetTreeCtrl().SetItemText(hItem, strScriptName);
                }
            }
        }
    }
}

void CTreePCPane::OnShowSaveForChanges()
{
    TREEITEM_VECTOR vTreeItem;
    TREEITEM_VECTOR vChangeItem;
    GetAllChildNode(GetTreeCtrl().GetRootItem(), vTreeItem);

    // *ǥ�ð� �ִ� �����۵��� ����Ʈ�� ��´�.
    for(UINT i = 0; i < vTreeItem.size(); ++i)
    {
        HTREEITEM hItem = vTreeItem[i];
        if(GetTreeCtrl().GetItemData(hItem))
        {
            CString strScriptName = GetTreeCtrl().GetItemText(hItem);
            if(strScriptName.Find(L"*") >= 0)
            {
                strScriptName.Replace(L"*", L"");
                GetTreeCtrl().SetItemText(hItem, strScriptName);
                vChangeItem.push_back(hItem);
            }
        }
    }


    if(vChangeItem.size())
    {
        CString strTitle, strScriptList, strMsg;
        strTitle.Format(L"< %s > Save Changes to following Items?\n\n", m_strRootName);
        for(UINT i = 0; i < vChangeItem.size(); ++i)
        {
            HTREEITEM hItem = vChangeItem[i];
            strScriptList += GetTreeCtrl().GetItemText(hItem);
            strScriptList += L"\n";
        }

        strScriptList += L"\n";
        strMsg = strTitle + strScriptList;
        if(MessageBox(strMsg, m_strRootName, MB_YESNO) == IDYES)
        {
            for(UINT i = 0; i < vChangeItem.size(); ++i)
            {
                CString strScriptName = GetTreeCtrl().GetItemText(vChangeItem[i]);
                OnSaveScript(vChangeItem[i], strScriptName, FALSE);
            }
        }
    }    
}


void CTreePCPane::OnSetFocus(CWnd* pOldWnd)
{
    CXTTreeView::OnSetFocus(pOldWnd);

    CModelToolApplication::GetInstance()->SetAppMode(MT_MODE_PC);

    //HTREEITEM hItem = GetTreeCtrl().GetSelectedItem();
    //if(hItem)
    //{
    //    GetTreeCtrl().SelectItem(GetTreeCtrl().GetRootItem());
    //    GetTreeCtrl().SelectItem(hItem);
    //}    
}

void CTreePCPane::InsertItemRecursive( SItemNode* pItemNode, HTREEITEM hParentItem ) 
{
	// �ڽ��� �߰��Ѵ�.
	TV_INSERTSTRUCT sTvStruct;
	sTvStruct.hParent = hParentItem;
	sTvStruct.hInsertAfter = TVI_SORT;
	sTvStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	sTvStruct.item.pszText = (LPWSTR)(LPCWSTR)pItemNode->strNodeName;

	if(pItemNode->eNodeType == NODE_SCIRPT)
	{
		// ��ũ��Ʈ��� �ڽ��� �߰��ϰ� ������.
		sTvStruct.item.iImage = 2;
		sTvStruct.item.iSelectedImage = 2;
		GetTreeCtrl().InsertItem(&sTvStruct);
	}
	else	// ������� �ڽĵ��� ��ͷ� ����.
	{
		sTvStruct.item.iImage = 0;
		sTvStruct.item.iSelectedImage = 1;
		HTREEITEM hItem = GetTreeCtrl().InsertItem(&sTvStruct);

		for each(SItemNode* pChildNode in pItemNode->vecChildList)
		{
			if(pChildNode)
			{
				InsertItemRecursive(pChildNode, hItem);
			}
		}
	}
}
void CTreePCPane::OnTvnBegindrag(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

    if(pNMTreeView->itemNew.hItem)
    {
        SetClassLong(this->m_hWnd, GCL_HCURSOR, (long)AfxGetApp()->LoadCursor(IDC_CURSOR1));
        m_bDrag = TRUE;
        SetCapture();
        m_hDragItem = pNMTreeView->itemNew.hItem;
    }
    
    *pResult = 0;
}

void CTreePCPane::OnMouseMove(UINT nFlags, CPoint point)
{
    if(m_bDrag)
    {
        if(point.y <= 0 && m_hCurItem)
        {
            //m_hCurItem = GetTreeCtrl().GetPrevItem(m_hCurItem);            
            
        }        
        else
        {
            m_hCurItem = GetTreeCtrl().HitTest(point);            
        }   

        if(m_hCurItem)
        {
            GetTreeCtrl().SelectDropTarget(m_hCurItem);  
            GetTreeCtrl().EnsureVisible(m_hCurItem);
        }
    }

    CXTTreeView::OnMouseMove(nFlags, point);
}

void CTreePCPane::OnLButtonUp(UINT nFlags, CPoint point)
{
    if(m_bDrag && m_hCurItem)
    {
        ReleaseCapture();

        //Ŀ���� ������� ����
        SetClassLong(this->m_hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_ARROW));
        m_bDrag = FALSE;

        int nImageIndex = 0, nSelectImageIndex = 0;
        GetTreeCtrl().GetItemImage(m_hCurItem, nImageIndex, nSelectImageIndex);
        if(nImageIndex == 0) // ���� �ؿ��� �̵��� �����ϴ�
        {
            OnMoveItem(m_hDragItem, m_hCurItem);                
        }

        GetTreeCtrl().SelectDropTarget(NULL);
    }

    CXTTreeView::OnLButtonUp(nFlags, point);
}

void CTreePCPane::OnMoveItem( HTREEITEM hItem, HTREEITEM hParentItem, BOOL bRecrusive /*= FALSE*/ ) 
{
    while(hItem)
    {
        if(hItem == hParentItem)
        {
            //hItem = GetTreeCtrl().GetNextSelectedItem(hItem);
            continue;
        }

        int nImageIndex = 0, nSelectedImage = 0;
        GetTreeCtrl().GetItemImage(hItem, nImageIndex, nSelectedImage);
        CString strItemName = GetTreeCtrl().GetItemText(hItem);

        TV_INSERTSTRUCT sTvStruct;
        sTvStruct.hParent = hParentItem;
        sTvStruct.hInsertAfter = TVI_SORT;
        sTvStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        sTvStruct.item.iImage = nImageIndex;
        sTvStruct.item.iSelectedImage = nSelectedImage;
        sTvStruct.item.pszText = (LPWSTR)(LPCWSTR)strItemName;    
        HTREEITEM hNewItem = GetTreeCtrl().InsertItem(&sTvStruct);        

        HTREEITEM hDeleteItem = hItem;

        if(GetTreeCtrl().ItemHasChildren(hItem))
        {
            OnMoveItem(GetTreeCtrl().GetChildItem(hItem), hNewItem, TRUE);
        }

        if(bRecrusive)
        {
            hItem = GetTreeCtrl().GetNextSiblingItem(hItem);
        }
        else
        {
            //hItem = GetTreeCtrl().GetNextSelectedItem(hItem);
            hItem = NULL;
        }

        GetTreeCtrl().SetItemState(hDeleteItem, 0, TVIS_FOCUSED);
        GetTreeCtrl().DeleteItem(hDeleteItem);
        GetTreeCtrl().SelectItem(hNewItem);
    }

}