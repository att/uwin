/* 
 * Taken from http://www.codeguru.com
 * 		Contributed by : Max Poliashenko (maxim@mindspring.com)
 * 						 (06-Aug-1998)
 */

#include "stdafx.h"
#include "SortClass.h"

CMySortClass::CMySortClass(CListCtrl * _pWnd, const int _iCol)
{
	pWnd = _pWnd;	
	ASSERT(pWnd);	
	int max = pWnd->GetItemCount();	
	DWORD dw;	
	CString txt;	

	// replace Item data with pointer to CSortItem structure
	for (int t = 0; t < max; t++)
	{
		dw = pWnd->GetItemData(t); // save current data to restore it later
		txt = pWnd->GetItemText(t, _iCol); 
		pWnd->SetItemData(t, (DWORD) new CSortItem(dw, txt));
	}
}

CMySortClass::~CMySortClass()
{
	ASSERT(pWnd);	
	int max = pWnd->GetItemCount();
	CSortItem * pItem;	
	
	for (int t = 0; t < max; t++)
	{
		pItem = (CSortItem *) pWnd->GetItemData(t);		
		ASSERT(pItem);
		pWnd->SetItemData(t, pItem->dw);
		delete pItem;
	}
}

void CMySortClass::Sort(BOOL _bAsc, int _dtype)
{
	long lParamSort = _dtype;	

	// if lParamSort positive - ascending sort order, negative - descending
	if (!_bAsc)
		lParamSort *= -1;
	pWnd->SortItems(Compare, lParamSort);
}

int CALLBACK CMySortClass::Compare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CSortItem *item1 = (CSortItem *) lParam1;
	CSortItem *item2 = (CSortItem *) lParam2;	
	ASSERT(item1 && item2);	

	// restore data type and sort order from lParamSort
	// if lParamSort positive - ascending sort order, negative - descending
	short   sOrder = lParamSort < 0 ? -1 : 1; 
	int dType  = (int) (lParamSort * sOrder); // get rid of sign	

	switch (dType)
	{
		case INT_TYPE:
			return (atol(item1->txt) - atol(item2->txt))*sOrder;
			break;

		case STR_TYPE:
			return item1->txt.CompareNoCase(item2->txt)*sOrder;
			break;
	}

	return 0;
}

CMySortClass::CSortItem::CSortItem(const DWORD _dw, const CString & _txt)
{
	dw  = _dw;
	txt = _txt;
}
