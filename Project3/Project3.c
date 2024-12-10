#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CommCtrl.h>
#include <shlobj.h>

#define MAX_PATH_LENGTH 1024
#define SIOCTL_TYPE 40000
#define IOCTL_HELLO CTL_CODE(SIOCTL_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA)

HINSTANCE hInst;
HWND hMainWnd;
HWND hEditPath;
HWND hComboExtension;
HWND hLogBox;

typedef struct {
    HANDLE deviceHandle;
    CRITICAL_SECTION cs;
    BOOL hasNewPath;
    WCHAR path[MAX_PATH_LENGTH];
} SHARED_DATA;

SHARED_DATA* sharedData;

void AddLogMessage(const WCHAR* message) {
    int index = SendMessageW(hLogBox, LB_ADDSTRING, 0, (LPARAM)message);
    SendMessageW(hLogBox, LB_SETTOPINDEX, index, 0);
}

DWORD WINAPI SendPathToDriver(LPVOID lpParam) {
    SHARED_DATA* data = (SHARED_DATA*)lpParam;
    DWORD bytesReturned = 0;
    WCHAR buffer[MAX_PATH_LENGTH];

    while (1) {
        EnterCriticalSection(&data->cs);
        
        if (data->hasNewPath) {
            ZeroMemory(buffer, sizeof(buffer));
            wcsncpy_s(buffer, MAX_PATH_LENGTH, data->path, wcslen(data->path));
            
            WCHAR debugMsg[512];
            swprintf_s(debugMsg, 512, L"Sending IOCTL code: 0x%X", IOCTL_HELLO);
            AddLogMessage(debugMsg);
            
            if (!DeviceIoControl(
                data->deviceHandle,
                IOCTL_HELLO,
                buffer,
                (DWORD)(wcslen(buffer) + 1) * sizeof(WCHAR),
                buffer,
                sizeof(buffer),
                &bytesReturned,
                NULL)) {
                WCHAR errMsg[256];
                swprintf_s(errMsg, 256, L"Failed to send path to driver. Error: %d", GetLastError());
                AddLogMessage(errMsg);
            }
            else {
                WCHAR successMsg[512];
                swprintf_s(successMsg, 512, L"Successfully sent path: %s", data->path);
                AddLogMessage(successMsg);
            }
            
            data->hasNewPath = FALSE;
        }
        
        LeaveCriticalSection(&data->cs);
        Sleep(100);
    }

    return 0;
}

void ProcessReceivedPath(const WCHAR* directory, const WCHAR* extension) {
    EnterCriticalSection(&sharedData->cs);
    
    // 디렉토리 경로 끝에 확장자 패턴 추가
    WCHAR fullPath[MAX_PATH_LENGTH];
    ZeroMemory(fullPath, sizeof(fullPath));
    wcscpy_s(fullPath, MAX_PATH_LENGTH, directory);
    
    // 디렉토리 경로 끝에 백슬래시가 없으면 추가
    size_t len = wcslen(fullPath);
    if (len > 0 && fullPath[len - 1] != L'\\') {
        wcscat_s(fullPath, MAX_PATH_LENGTH, L"\\");
    }
    
    // 확장자 패턴 추가
    wcscat_s(fullPath, MAX_PATH_LENGTH, extension);
    
    wcscpy_s(sharedData->path, MAX_PATH_LENGTH, fullPath);
    sharedData->hasNewPath = TRUE;
    
    LeaveCriticalSection(&sharedData->cs);
}

void ShowFolderDialog(HWND hwnd) {
    BROWSEINFOW bi = { 0 };
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"Select a folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            SetWindowTextW(hEditPath, path);
        }
        CoTaskMemFree(pidl);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // 경로 입력 필드
            CreateWindowW(L"STATIC", L"Directory Path:", 
                WS_CHILD | WS_VISIBLE,
                10, 10, 100, 20,
                hwnd, NULL, hInst, NULL);

            hEditPath = CreateWindowW(L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 30, 400, 25,
                hwnd, NULL, hInst, NULL);

            // Browse 버튼
            CreateWindowW(L"BUTTON", L"Browse",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                420, 30, 80, 25,
                hwnd, (HMENU)1, hInst, NULL);

            // 확장자 선택 콤보 박스
            CreateWindowW(L"STATIC", L"File Extension:", 
                WS_CHILD | WS_VISIBLE,
                10, 65, 100, 20,
                hwnd, NULL, hInst, NULL);

            hComboExtension = CreateWindowW(L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
                10, 85, 150, 200,
                hwnd, NULL, hInst, NULL);

            // 확장자 옵션 추가
            SendMessageW(hComboExtension, CB_ADDSTRING, 0, (LPARAM)L"*.txt");
            SendMessageW(hComboExtension, CB_ADDSTRING, 0, (LPARAM)L"*.doc");
            SendMessageW(hComboExtension, CB_ADDSTRING, 0, (LPARAM)L"*.pdf");
            SendMessageW(hComboExtension, CB_SETCURSEL, 0, 0);

            // Send 버튼
            CreateWindowW(L"BUTTON", L"Send",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, 85, 80, 25,
                hwnd, (HMENU)2, hInst, NULL);

            // 로그 리스트 박스
            hLogBox = CreateWindowW(L"LISTBOX", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                10, 120, 560, 230,
                hwnd, NULL, hInst, NULL);

            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1: // Browse 버튼
                    ShowFolderDialog(hwnd);
                    break;

                case 2: { // Send 버튼
                    WCHAR path[MAX_PATH_LENGTH];
                    WCHAR extension[32];
                    
                    GetWindowTextW(hEditPath, path, MAX_PATH_LENGTH);
                    GetWindowTextW(hComboExtension, extension, 32);
                    
                    if (wcslen(path) == 0) {
                        MessageBoxW(hwnd, L"Please select a directory", L"Error", MB_ICONERROR);
                        break;
                    }
                    
                    ProcessReceivedPath(path, extension);
                    break;
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // COM 초기화 (폴더 선택 대화상자에 필요)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    hInst = hInstance;

    sharedData = (SHARED_DATA*)malloc(sizeof(SHARED_DATA));
    if (!sharedData) {
        MessageBoxW(NULL, L"Failed to allocate shared data", L"Error", MB_ICONERROR);
        return 1;
    }

    InitializeCriticalSection(&sharedData->cs);
    sharedData->hasNewPath = FALSE;

    sharedData->deviceHandle = CreateFileW(
        L"\\\\.\\MyDevice",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (sharedData->deviceHandle == INVALID_HANDLE_VALUE) {
        WCHAR errMsg[256];
        swprintf_s(errMsg, 256, L"Failed to open device. Error: %d\nPlease make sure the driver is loaded and running.", GetLastError());
        MessageBoxW(NULL, errMsg, L"Error", MB_ICONERROR);
        DeleteCriticalSection(&sharedData->cs);
        free(sharedData);
        return 1;
    }

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PathSenderClass";
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed", L"Error", MB_ICONERROR);
        return 1;
    }

    hMainWnd = CreateWindowW(
        L"PathSenderClass", L"Path Sender",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400,
        NULL, NULL, hInstance, NULL);

    if (!hMainWnd) {
        MessageBoxW(NULL, L"Window Creation Failed", L"Error", MB_ICONERROR);
        return 1;
    }

    HANDLE hThread = CreateThread(NULL, 0, SendPathToDriver, sharedData, 0, NULL);
    if (!hThread) {
        MessageBoxW(NULL, L"Failed to create sender thread", L"Error", MB_ICONERROR);
        CloseHandle(sharedData->deviceHandle);
        DeleteCriticalSection(&sharedData->cs);
        free(sharedData);
        return 1;
    }

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hThread);
    CloseHandle(sharedData->deviceHandle);
    DeleteCriticalSection(&sharedData->cs);
    free(sharedData);
    CoUninitialize();

    return (int)msg.wParam;
}