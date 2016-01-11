
#include	<windows.h>
#include	<stdio.h>

int WINAPI WinMain(HINSTANCE me, HINSTANCE prev, LPSTR cmdline, int show)
{
	int width,height,planes;
	HDC hdc =  GetDC ((HWND) NULL);
	if(hdc!=NULL)
	{
		width = GetDeviceCaps (hdc, HORZRES);
		height = GetDeviceCaps (hdc, VERTRES);
		planes = GetDeviceCaps (hdc, COLORRES);
		printf("%dx%dx%d\n",width,height,planes);
	}
	return(0);
}
