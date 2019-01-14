#define UNICODE
#define _WIN32_WINNT 0x0400

#pragma comment( lib, "win.lib" )
#pragma comment( lib, "kernel32.lib" )
#pragma comment( lib, "gdi32.lib" )
#pragma comment ( lib,"advapi32.lib" )

/*
 * avoid name clashed
 */

#define _INC_STRING
#define _INC_STDLIB
#define Rectangle W32Rectangle
#define accept W32accept
#define listen W32listen

#include <windows.h>
#include <lm.h>

#undef Rectangle
#undef accept
#undef listen

/*
 * undo microsoft's macros for Unicode functions in win library
 */
#undef CallNamedPipe
#undef ChooseFont
#undef CreateDirectory
#undef CreateEvent
#undef CreateFile
#undef CreateFileMapping
#undef CreateFontIndirect
#undef CreateMutex
#undef CreateNamedPipe
#undef CreateProcess
#undef CreateSemaphore
#undef CreateWindow
#undef CreateWindowEx
#undef DefWindowProc
#undef DeleteFile
#undef DispatchMessage
#undef DrawText
#undef EnumFontFamilies
#undef ExtTextOut
#undef FindExecutable
#undef FindFirstFile
#undef FindNextFile
#undef FindWindow
#undef FormatMessage
#undef FreeEnvironmentStrings
#undef GetCharWidth
#undef GetCommandLine
#undef GetComputerName
#undef GetCurrentDirectory
#undef GetDriveType
#undef _GetEnvironmentStrings
#undef GetEnvironmentVariable
#undef GetFileAttributes
#undef GetFullPathName
#undef GetMessage
#undef GetModuleFileName
#undef GetModuleHandle
#undef GetTextFace
#undef GetVersionEx
#undef GetVolumeInformation
#undef GetWindowLong
#undef LoadCursor
#undef LoadIcon
#undef LoadLibrary
#undef MessageBox
#undef MessageBoxEx
#undef MoveFile
#undef OutputDebugString
#undef PostMessage
#undef PostThreadMessage
#undef ReadConsole
#undef RegCreateKey
#undef RegDeleteKey
#undef RegOpenKey
#undef RegQueryValue
#undef RegSetValueEx
#undef RegisterClass
#undef RegisterClassEx
#undef RemoveDirectory
#undef SendMessage
#undef SetClassLong
#undef SetCurrentDirectory
#undef SetEnvironmentVariable
#undef SetFileAttributes
#undef SetWindowLong
#undef TextOut
#undef WNetEnumResource
#undef WNetGetUniversalName
#undef WNetOpenEnum
#undef WSASocket
#undef WriteConsole

BOOL WINAPI CallNamedPipe(LPCWSTR lpNamedPipeName, LPVOID lpInBuffer, DWORD nInBufferSize,
	LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesRead, DWORD nTimeOut);
BOOL WINAPI ChooseFont(LPCHOOSEFONT lpcf);
BOOL WINAPI CreateDirectory(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
HANDLE WINAPI CreateEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
	BOOL bInitialState, LPCWSTR lpName);
HANDLE WINAPI CreateFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE WINAPI CreateFileMapping(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
		DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow,
		LPCWSTR lpName);
HFONT  WINAPI CreateFontIndirect(LOGFONT *lf);
HANDLE WINAPI CreateMutex(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bInitialOwner,
	LPCWSTR lpName);
HANDLE WINAPI CreateNamedPipe(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
	DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL WINAPI CreateProcess(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
	LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags,
	LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
	LPPROCESS_INFORMATION lpProcessInformation);
HANDLE WINAPI CreateSemaphore(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, LONG lInitialCount,
	LONG lMaximumCount, LPCWSTR lpName);
#define CreateWindow(lpClassName, lpWindowName, dwStyle, x, y,\
		nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)\
		CreateWindowEx(0L, lpClassName, lpWindowName, dwStyle, x, y,\
		nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)
HWND WINAPI  CreateWindowEx(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
	DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent,
	HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
LRESULT WINAPI DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI DeleteFile(LPCWSTR lpFileName);
LONG WINAPI  DispatchMessage(CONST MSG *lpMsg);
int WINAPI DrawText(HDC hDC, LPWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat);

typedef struct Callback
{
	FONTENUMPROC	fnc;
	LPARAM		a;
} Callback;

int  WINAPI EnumFontFamilies(HDC hdc, LPCWSTR lpszFamily, FONTENUMPROC lpEnumFontFamProc, LPARAM lParam);
BOOL WINAPI ExtTextOut(HDC hdc, int x, int y, UINT opt, RECT *lprc, LPCWSTR s, int n, INT *lpDx);
HINSTANCE APIENTRY FindExecutable(LPCWSTR lpFile, LPCWSTR lpDirectory, LPWSTR lpResult);
HANDLE WINAPI FindFirstFile(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
BOOL WINAPI FindNextFile(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);
HWND WINAPI FindWindow(LPCWSTR lpClassName, LPCWSTR lpWindowName);
DWORD WINAPI FormatMessage(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
	DWORD dwLanguageId, LPWSTR lpBuffer, DWORD nSize, va_list *Arguments);
BOOL WINAPI FreeEnvironmentStrings(LPWSTR s);
BOOL  WINAPI GetCharWidth(HDC hdc, UINT c0, UINT c1, INT *w);
LPWSTR WINAPI GetCommandLine(VOID);
BOOL WINAPI GetComputerName(LPWSTR lpBuffer, LPDWORD nSize);
DWORD WINAPI GetCurrentDirectory(DWORD nBufferLength, LPWSTR lpBuffer);
UINT WINAPI GetDriveType(LPCWSTR lpRootPathName);
LPWSTR WINAPI _GetEnvironmentStrings(VOID);
DWORD WINAPI GetEnvironmentVariable(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);
DWORD WINAPI GetFileAttributes(LPCWSTR lpFileName);
DWORD WINAPI GetFullPathName(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer,
	LPWSTR *lpFilePart);
BOOL WINAPI GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax); 
DWORD WINAPI GetModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize); 
HMODULE WINAPI GetModuleHandle(LPCWSTR lpModuleName);
int  WINAPI GetTextFace(HDC hdc, int nCount, LPTSTR lpFaceName);
BOOL  WINAPI GetVersionEx(OSVERSIONINFO *lpVersionInformation);
BOOL WINAPI GetVolumeInformation(LPCTSTR lpRootPathName, LPTSTR lpVolumeNameBuffer,
	DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
	LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
	LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);
LONG WINAPI GetWindowLong(HWND hWnd, int nIndex);
HCURSOR WINAPI LoadCursor(HINSTANCE hInstance, LPCWSTR lpCursorName);
HICON WINAPI LoadIcon(HINSTANCE hInstance, LPCWSTR lpIconName);
HMODULE WINAPI LoadLibrary(LPCWSTR lpLibFileName);
int WINAPI MessageBox(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
int WINAPI MessageBoxEx(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, WORD wLanguageId);
BOOL WINAPI MoveFile(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName);
BOOL WINAPI PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI PostThreadMessage(DWORD idThread, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI ReadConsole(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
	LPDWORD lpNumberOfCharsRead, LPVOID lpReserved);
LONG APIENTRY RegCreateKey(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult);
LONG APIENTRY RegDeleteKey(HKEY hKey, LPCWSTR lpSubKey);
LONG APIENTRY RegOpenKey(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult);
LONG APIENTRY RegQueryValue(HKEY hKey, LPCWSTR lpSubKey, LPWSTR lpValue, PLONG lpcbValue);
LONG APIENTRY RegSetValueEx(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, 
		DWORD dwType, BYTE* lpData, DWORD cbData);
ATOM WINAPI RegisterClass(CONST WNDCLASSW *lpWndClass);
ATOM WINAPI RegisterClassEx(CONST WNDCLASSEXW *lpWndClass);
BOOL WINAPI RemoveDirectory(LPCWSTR lpPathName);
LRESULT WINAPI SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI SetClassLong(HWND hWnd, int nIndex, LONG dwNewLong);
BOOL WINAPI SetCurrentDirectory(LPCWSTR lpPathName);
BOOL WINAPI SetEnvironmentVariable(LPCWSTR lpName, LPCWSTR lpValue);
BOOL WINAPI SetFileAttributes(LPCWSTR lpFileName, DWORD dwFileAttributes);
LONG WINAPI SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong);
BOOL WINAPI TextOut(HDC hdc, int x, int y, LPCWSTR s, int n);
DWORD WINAPI WNetEnumResource(HANDLE  hEnum, LPDWORD lpcCount, LPVOID lpBuffer,
		LPDWORD lpBufferSize);
DWORD WINAPI WNetGetUniversalName(LPCWSTR lpLocalPath, DWORD dwInfoLevel, LPVOID lpBuffer,
		LPDWORD lpBufferSize);
DWORD WINAPI WNetOpenEnum(DWORD dwScope, DWORD dwType, DWORD dwUsage,
			LPNETRESOURCEW lpNetResource, LPHANDLE lphEnum);
SOCKET PASCAL WSASocket(int af, int type, int protocol, LPWSAPROTOCOL_INFO lpProtocolInfo, GROUP g, DWORD dwFlags);
BOOL WINAPI WriteConsole(HANDLE hConsoleOutput, CONST VOID *lpBuffer,
			DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved);
VOID WINAPI OutputDebugString(LPCWSTR lpOutputString);

extern	char	*win_error(char*, int);
extern	int	win_hasunicode(void);

extern	int win_useunicode;
