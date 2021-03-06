#if !defined(AFX_CONNECTTO_H__2AC38243_83DE_11D2_A014_0080C8443AA1__INCLUDED_)
#define AFX_CONNECTTO_H__2AC38243_83DE_11D2_A014_0080C8443AA1__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// connectto.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// connectto dialog

#define DEFPORT "16962"
#define DEFIP "ludus.res.cmu.edu"

class connectto : public CDialog
{
// Construction
public:
	void setdefaults();
	connectto(CWnd* pParent = NULL);   // standard constructor

    int THEPORT;
	char THEIP[128];

// Dialog Data
	//{{AFX_DATA(connectto)
	enum { IDD = ID_CONNECTTO };
	CEdit	ipedit;
	CEdit	portedit;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(connectto)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(connectto)
	afx_msg void OnChangeIpaddr();
	afx_msg void OnChangePort();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONNECTTO_H__2AC38243_83DE_11D2_A014_0080C8443AA1__INCLUDED_)
