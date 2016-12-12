/*
 * Copyright (c) 2010-2016 Stephane Poirier
 *
 * stephane.poirier@oifii.org
 *
 * Stephane Poirier
 * 3532 rue Ste-Famille, #3
 * Montreal, QC, H2X 2L1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// spissvoronoimidi.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
/* //spi 2014, switching to MFC application
#include "windows.h"
*/
#include "scrnsave.h"
#include "resource.h"
#include "math.h"

#include "FreeImage.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

#include "oifiilib.h" //note: oifiilib.lib/.dll is an MFC extension and resource DLL

BYTE global_alpha=200;

#define szAppName	"spissvoronoimidi 1.0"
#define szAuthor	"written by Stephane Poirier, © 2014"
#define szPreview	"spissvoronoimidi"

//string global_imagefolder="."; //the local folder of the .scr
//string global_imagefolder="c:\\temp\\spivideocaptureframetodisk\\";
string global_imagefolder="c:\\temp\\spissmandelbrotmidi\\";
//string global_imagefolder="c:\\temp\\spicapturewebcam\\";
vector<string> global_txtfilenames;
int global_imageid=0;
FIBITMAP* global_dib;
int global_imagewidth=-1; //will be computed within WM_SIZE handler 
int global_imageheight=-1; //will be computed within WM_SIZE handler
int global_slideshowrate_ms=1000;
COW2Doc* global_pOW2Doc=NULL;
COW2View* global_pOW2View=NULL;

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(const std::wstring &wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(const std::string &str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo( size_needed, 0 );
    MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}


LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	static int i = 0;
	static PAINTSTRUCT ps = {NULL};
	static HDC hDC = NULL;
	static UINT uTimer = 0;
	static int xpos, ypos;
	
	switch(message)
	{
	case WM_CREATE:
		{
			//spi, avril 2015, begin
			SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
			SetLayeredWindowAttributes(hWnd, 0, global_alpha, LWA_ALPHA);
			//SetLayeredWindowAttributes(h, 0, 200, LWA_ALPHA);
			//spi, avril 2015, end

			//int nShowCmd = false;
			ShellExecuteA(NULL, "open", "begin.bat", "", NULL, false);
			//0) execute cmd line to get all folder's .bmp filenames
			string quote = "\"";
			string pathfilter;
			string path=global_imagefolder;
			pathfilter = path + "\\*.bmp";
			//pathfilter = path + "\\*.jpg";
			string systemcommand;
			//systemcommand = "DIR " + quote + pathfilter + quote + "/B /O:N > wsic_filenames.txt"; //wsip tag standing for wav set (library) instrumentset (class) populate (function)
			systemcommand = "DIR " + quote + pathfilter + quote + "/B /S /O:N > spiss_filenames.txt"; // /S for adding path into "wsic_filenames.txt"
			system(systemcommand.c_str());

			//2) load in all "spiss_filenames.txt" file
			//vector<string> global_txtfilenames;
			ifstream ifs("spiss_filenames.txt");
			string temp;
			while(getline(ifs,temp))
			{
				//txtfilenames.push_back(path + "\\" + temp);
				global_txtfilenames.push_back(temp);
			}
			/*
			global_imagewidth=GetSystemMetrics(SM_CXSCREEN);
			global_imageheight=GetSystemMetrics(SM_CYSCREEN);
			*/
			RECT rect;
			GetClientRect( hWnd, &rect );
			global_imagewidth = rect.right;         
			global_imageheight = rect.bottom;

			uTimer = SetTimer(hWnd, 1, global_slideshowrate_ms, NULL);
		}
		break;

    case WM_DESTROY:
		if(uTimer) KillTimer(hWnd, uTimer);

		if(global_dib!=NULL) FreeImage_Unload(global_dib);

		if(global_pOW2Doc!=NULL) delete global_pOW2Doc;
		if(global_pOW2View!=NULL) delete global_pOW2View;

		//nShowCmd = false;
		ShellExecuteA(NULL, "open", "end.bat", "", NULL, false);
		PostQuitMessage(0);
		break;

	case WM_TIMER:
		{
			xpos = GetSystemMetrics(SM_CXSCREEN) / 2;
			ypos = GetSystemMetrics(SM_CYSCREEN) / 2;

			//unload previous image
			/*
			if(global_dib!=NULL) FreeImage_Unload(global_dib);
			*/
			//close document
			if(global_pOW2Doc) delete global_pOW2Doc;
			if(global_pOW2View) delete global_pOW2View;
			//if at least one image in the folder
			if(global_txtfilenames.size()>0)
			{
				//reset image id if end of file
				if(global_imageid>=global_txtfilenames.size()) global_imageid=0;
				/*
				//load new image
				global_dib = FreeImage_Load(FIF_BMP, (global_txtfilenames[global_imageid]).c_str(), BMP_DEFAULT);
				//global_dib = FreeImage_Load(FIF_JPEG, (global_txtfilenames[global_imageid]).c_str(), JPEG_DEFAULT);
				*/
				//open ow2doc document
				//global_pOW2DocTemplate->OpenDocumentFile((global_txtfilenames[global_imageid]).c_str());
				
				global_pOW2Doc = new COW2Doc(hWnd); //hwnd is the parent window where drawing will occur
				global_pOW2View = new COW2View(hWnd, (COWDocument*)global_pOW2Doc); 
				global_pOW2View->OnCreate(NULL);
				if(global_pOW2Doc!=NULL && global_pOW2View!=NULL) 
				{
					bool bresult=global_pOW2Doc->OnOpenDocument((global_txtfilenames[global_imageid]).c_str());
					global_pOW2View->OnInitialUpdate();
					InvalidateRect(hWnd, NULL, false);
					global_pOW2Doc->OnAnalysisTextureextraction();
					global_pOW2View->m_pViewDataSettings->bDrawPointset = TRUE;
					global_pOW2View->m_pViewDataSettings->bDrawVoronoiset = TRUE;
					InvalidateRect(hWnd, NULL, false);

				}
				//trigger WM_PAINT
				//InvalidateRect(hWnd, NULL, false);
				global_imageid++;
			}
		}
		break;

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		
		
		if(fChildPreview)
		{
			SetBkColor(hDC, RGB(0, 0, 0));
			SetTextColor(hDC, RGB(255, 255, 0));
			TextOut(hDC, 25, 45, szPreview, strlen(szPreview));
		}
		else
		{
		
			SetBkColor(hDC, RGB(0, 0, 0));
			SetTextColor(hDC, RGB(120, 120, 120));
			TextOut(hDC, 0, ypos * 2 - 40, szAppName, strlen(szAppName));
			TextOut(hDC, 0, ypos * 2 - 25, szAuthor, strlen(szAuthor));
			
			/*
			if(global_dib!=NULL)
			{
				SetStretchBltMode(hDC, COLORONCOLOR);
				StretchDIBits(hDC, 0, 0, global_imagewidth, global_imageheight,
								0, 0, FreeImage_GetWidth(global_dib), FreeImage_GetHeight(global_dib),
								FreeImage_GetBits(global_dib), FreeImage_GetInfo(global_dib), DIB_RGB_COLORS, SRCCOPY);
			}
			*/
			if(global_pOW2Doc!=NULL && global_pOW2View!=NULL)
			{
				CDC* pDC = CDC::FromHandle(hDC);
				global_pOW2View->OnDraw(pDC);
			}

		}
		
		
		EndPaint(hWnd, &ps);
		break;

    default: 
		return DefScreenSaverProc(hWnd, message, wParam, lParam);
	}
	
	return 0;
}

BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
	return TRUE;
}