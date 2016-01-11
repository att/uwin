/* 
 * Taken from http://www.codeguru.com
 * 		Contributed by : Max Poliashenko (maxim@mindspring.com)
 * 						 (06-Aug-1998)
 */

#ifndef _SORT_CLASS_H
#define _SORT_CLASS_H

class CMySortClass
{
	public:
		CMySortClass(CListCtrl * _pWnd, const int _iCol);
		virtual ~CMySortClass();
		void Sort(BOOL bAsc, int _dtype);	

	protected:	
		CListCtrl *pWnd;	

		static int CALLBACK Compare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);		
		struct CSortItem	
		{
			CSortItem(const DWORD _dw, const CString &_txt);
			DWORD dw;
	 		CString txt;	
		};
};

#endif // _SORT_CLASS_H
