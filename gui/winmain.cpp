#include"common.h"
#include<Windows.h>
#include<CommCtrl.h>
#include<iostream>
#include"resource.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define DEBUGMSG switch(MessageBox(NULL, msg, NULL, MB_ICONERROR | MB_ABORTRETRYIGNORE))\
{case IDABORT:ExitProcess(1);break;case IDRETRY:DebugBreak();break;default:break;}

#define CheckErrorHRESULT(_hr, file, func, code, line)\
{\
    HRESULT __error_check_hr=_hr;\
    TCHAR msg[512], buf[256];\
    wsprintf(msg, TEXT("%s : %d :\n%s :\n%s\n%#x\n"), file, line, func, code, __error_check_hr);\
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, __error_check_hr, GetUserDefaultLangID(), buf, ARRAYSIZE(buf), NULL);\
    lstrcat(msg, buf);\
	DEBUGMSG\
}
//检查HRESULT结果是否为SUCCEEDED
#define HRCHECK(x) CheckErrorHRESULT(x,__FILEW__,__FUNCTIONW__,TEXT(_CRT_STRINGIZE(x)),__LINE__)


TCHAR bufFloat[32] = TEXT("0.0");

BOOL ChooseFile(HWND hWnd)
{
	TCHAR buf[MAX_PATH] = TEXT("");
	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrDefExt = TEXT("mp4");
	ofn.lpstrFilter = TEXT("视频文件\0*.avi;*.asf;*.mp4;*.m4v;*.mkv;*.mov;*.h264;*.hevc;*.m2ts;*.mpeg;*.mpg;*.ogm;*.webm;*.wmv;*.ts\0所有文件\0*\0\0");
	ofn.nFilterIndex = 1;
	ofn.nMaxFile = ARRAYSIZE(buf);
	ofn.lpstrFile = buf;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetOpenFileName(&ofn))
	{
		SetDlgItemText(hWnd, IDC_EDIT_PATH, ofn.lpstrFile);
		return TRUE;
	}
	return FALSE;
}

#define ERRMSG(x) MessageBox(hWnd,x,NULL,MB_ICONERROR)

BOOL PreviewVideo(HWND hWnd)
{
	char path[MAX_PATH] = "";
	GetDlgItemTextA(hWnd, IDC_EDIT_PATH, path, ARRAYSIZE(path));
	if (strlen(path) == 0)
	{
		ERRMSG(TEXT("请选择文件。"));
		return FALSE;
	}
	STARTUPINFO st = { 0 };
	st.cb = sizeof(st);
	st.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	st.wShowWindow = SW_HIDE;
	HANDLE readPipe = NULL, writePipe = NULL;
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.bInheritHandle = TRUE;
	sa.nLength = sizeof(sa);
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
		return FALSE;
	st.hStdOutput = writePipe;
	st.hStdError = writePipe;
	st.hStdInput = readPipe;
	PROCESS_INFORMATION pi = { 0 };
	TCHAR cmd[1024] = TEXT("");
	wsprintf(cmd, TEXT("RKF \"%S\""), path);
	if (!CreateProcess(TEXT("RKF.EXE"), cmd, NULL, NULL, TRUE, 0, NULL, NULL, &st, &pi))
	{
		HRCHECK(GetLastError());
		return FALSE;
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(writePipe);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	char logs[4096] = "";
	DWORD readLogs = 0;
	while (ReadFile(readPipe, logs, ARRAYSIZE(logs), &readLogs, NULL))
	{
		std::string regionStr = logs;
		size_t cp = regionStr.find("Command:");
		if (cp != std::string::npos)
		{
			CloseHandle(readPipe);
			cp = cp + 13;
			if (regionStr[cp] == '"')
				cp += 2;
			regionStr = regionStr.substr(cp + 1 + strlen(path));
			int l, t, w, h;
			sscanf_s(regionStr.c_str(), "%d %d %d %d", &l, &t, &w, &h);
			SetDlgItemInt(hWnd, IDC_EDIT_LEFT, l, FALSE);
			SetDlgItemInt(hWnd, IDC_EDIT_TOP, t, FALSE);
			SetDlgItemInt(hWnd, IDC_EDIT_WIDTH, w, FALSE);
			SetDlgItemInt(hWnd, IDC_EDIT_HEIGHT, h, FALSE);
			return TRUE;
		}
	}
	CloseHandle(readPipe);
	if (strchr(path, '?'))
		ERRMSG(TEXT("无法读取文件，文件路径中可能有当前系统编码中不支持的字符。"));
	else if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
		ERRMSG(TEXT("无法读取文件，请检查文件路径是否正确。"));
	else
		ERRMSG(TEXT("无法读取文件，请检查文件是否被锁定或者视频编码格式是否可以被读取。"));
	return FALSE;
}

HMODULE hKernel = NULL;
ULONGLONG(WINAPI*pGetTickCount64)() = NULL;

void LoadDModule()
{
	hKernel = LoadLibrary(TEXT("kernel32.dll"));
	if (hKernel)
		pGetTickCount64 = (ULONGLONG(WINAPI*)())GetProcAddress(hKernel, "GetTickCount64");
}

ULONGLONG DGetTickCount()
{
	if (pGetTickCount64)
		return pGetTickCount64();
	return GetTickCount();
}

BOOL RKF(HWND hWnd)
{
	char path[MAX_PATH] = "";
	GetDlgItemTextA(hWnd, IDC_EDIT_PATH, path, ARRAYSIZE(path));
	if (strlen(path) == 0)
	{
		ERRMSG(TEXT("请选择文件。"));
		return FALSE;
	}
	STARTUPINFO st = { 0 };
	st.cb = sizeof(st);
	st.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	st.wShowWindow = SW_HIDE;
	HANDLE readPipe = NULL, writePipe = NULL;
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.bInheritHandle = TRUE;
	sa.nLength = sizeof(sa);
	if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
		return FALSE;
	st.hStdOutput = writePipe;
	st.hStdError = writePipe;
	st.hStdInput = readPipe;
	PROCESS_INFORMATION pi = { 0 };

	int l, t, w, h;
	l=GetDlgItemInt(hWnd, IDC_EDIT_LEFT, NULL, FALSE);
	t=GetDlgItemInt(hWnd, IDC_EDIT_TOP, NULL, FALSE);
	w=GetDlgItemInt(hWnd, IDC_EDIT_WIDTH, NULL, FALSE);
	h=GetDlgItemInt(hWnd, IDC_EDIT_HEIGHT, NULL, FALSE);
	TCHAR simStr[32] = TEXT("");
	GetDlgItemText(hWnd, IDC_EDIT_SIMILARITY, simStr, ARRAYSIZE(simStr) - 1);

	TCHAR cmd[1024] = TEXT("");
	wsprintf(cmd, TEXT("RKF \"%S\" %d %d %d %d %s"), path, l, t, w, h, simStr);
	ULONGLONG tickStart = DGetTickCount();
	if (!CreateProcess(TEXT("RKF.EXE"), cmd, NULL, NULL, TRUE, 0, NULL, NULL, &st, &pi))
	{
		HRCHECK(GetLastError());
		return FALSE;
	}
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(writePipe);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	char logs[4096] = "";
	DWORD readLogs = 0;
	while (ReadFile(readPipe, logs, ARRAYSIZE(logs), &readLogs, NULL))
	{
		std::string regionStr = logs;
		size_t cp = regionStr.find("File outputed to:");
		if (cp != std::string::npos)
		{
			CloseHandle(readPipe);
			regionStr = regionStr.substr(cp + 18);
			cp = regionStr.find("\n");
			if (regionStr[cp - 1] == '\r')
				cp--;
			char timefmt[16] = "";
			ULONGLONG timeDelta = DGetTickCount() - tickStart;
			sprintf_s(timefmt, "%02I64u:%02I64u:%02I64u.%03I64u",
				timeDelta / 3600000, (timeDelta / 60000) % 60, (timeDelta / 1000) % 60, timeDelta % 1000);
			regionStr = "文件已输出至：\n" + regionStr.substr(0, cp) + "\n运行时长：" + timefmt;
			MessageBoxA(hWnd, regionStr.c_str(), "RKF", MB_ICONINFORMATION);
			return TRUE;
		}
	}
	CloseHandle(readPipe);
	ERRMSG(TEXT("导出失败。"));
	return FALSE;
}

INT_PTR CALLBACK DlgCallback(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_BROWSE:
			ChooseFile(hWnd);
			break;
		case IDOK:
			RKF(hWnd);
			break;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;
		case IDC_EDIT_SIMILARITY:
			if (HIWORD(wParam) == EN_KILLFOCUS)
			{
				GetDlgItemText(hWnd, IDC_EDIT_SIMILARITY, bufFloat, ARRAYSIZE(bufFloat));
				SendMessage(GetDlgItem(hWnd, IDC_SLIDER_SIMILARITY), TBM_SETPOS, TRUE, (UINT)(_wtof(bufFloat) * 100));
			}
			break;
		case IDC_BUTTON_REGION:
			PreviewVideo(hWnd);
			break;
		}
		break;
	case WM_INITDIALOG:
		SendMessage(GetDlgItem(hWnd, IDC_SPIN_LEFT), UDM_SETRANGE32, 0, 65535);
		SendMessage(GetDlgItem(hWnd, IDC_SPIN_TOP), UDM_SETRANGE32, 0, 65535);
		SendMessage(GetDlgItem(hWnd, IDC_SPIN_WIDTH), UDM_SETRANGE32, 0, 65535);
		SendMessage(GetDlgItem(hWnd, IDC_SPIN_HEIGHT), UDM_SETRANGE32, 0, 65535);
		SendMessage(GetDlgItem(hWnd, IDC_SLIDER_SIMILARITY), TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
		SendMessage(GetDlgItem(hWnd, IDC_SLIDER_SIMILARITY), TBM_SETPOS, TRUE, 80);
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code)
		{
		case NM_CUSTOMDRAW:
			switch (((LPNMHDR)lParam)->idFrom)
			{
			case IDC_SLIDER_SIMILARITY:
				swprintf_s(bufFloat, L"%g", ((UINT)SendMessage(GetDlgItem(hWnd, IDC_SLIDER_SIMILARITY), TBM_GETPOS, 0, 0)) / 100.0f);
				SetDlgItemText(hWnd, IDC_EDIT_SIMILARITY, bufFloat);
				break;
			}
			break;
		}
		break;
	}
	return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR param, int iShow)
{
	InitCommonControls();
	LoadDModule();
	return (int)DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgCallback);
}
