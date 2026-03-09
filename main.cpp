#define _WIN32_IE 0x0300        // Add this to fix the 'INITCOMMONCONTROLSEX' error
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

// Alternative for popen/pclose in MinGW
#include <io.h>
#include <fcntl.h>

// Add missing declarations for MinGW
#ifndef _popen
#define _popen _popen
#define _pclose _pclose
#endif

// Add missing structures for OpenFileName
#include <cderr.h>
#include <commdlg.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

// Global variables
HWND hTabControl;
HWND hCreateDialog, hAssignDialog;
HWND hStatusBar;
HINSTANCE hInst;
HWND hMainWindow;

// Controls for "Create" tab
HWND hEditCreateName, hEditCreatePassword, hEditCreateExePath;
HWND hBtnCreateBrowseExe, hBtnCreate;

// Controls for "Assign" tab
HWND hEditAssignCertPath, hEditAssignPassword, hEditAssignExePath;
HWND hBtnAssignBrowseCert, hBtnAssignBrowseExe, hBtnAssign;
HWND hEditAssignSignToolPath;
HWND hBtnAssignBrowseSignTool;

// Status colors
const COLORREF COLOR_SUCCESS = RGB(0, 128, 0);
const COLORREF COLOR_ERROR = RGB(255, 0, 0);
const COLORREF COLOR_INFO = RGB(0, 0, 255);

// Alternative function to execute command and get output
std::string ExecCommand(const std::string& cmd) {
    std::string result;
    char buffer[128];
    
    // Create temporary file for output
    char tempPath[MAX_PATH];
    char tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "cmd", 0, tempFile);
    
    // Execute command with output redirected to file
    std::string fullCmd = cmd + " > \"" + std::string(tempFile) + "\" 2>&1";
    system(fullCmd.c_str());
    
    // Read result from file
    std::ifstream file(tempFile);
    if (file.is_open()) {
        while (file.getline(buffer, sizeof(buffer))) {
            result += buffer;
            result += "\n";
        }
        file.close();
    }
    
    // Delete temporary file
    DeleteFileA(tempFile);
    
    return result;
}

// Function to find signtool.exe
std::string FindSignTool() {
    std::vector<std::string> paths;
    
    paths.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\signtool.exe");
    paths.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22000.0\\x64\\signtool.exe");
    paths.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.20348.0\\x64\\signtool.exe");
    paths.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.19041.0\\x64\\signtool.exe");
    paths.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe");
    paths.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\App Certification Kit\\signtool.exe");
    paths.push_back("C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.1A\\Bin\\signtool.exe");
    
    for (const auto& path : paths) {
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }
    
    // Try to find via 'where' command
    std::string result = ExecCommand("where signtool");
    if (!result.empty() && result.find("signtool.exe") != std::string::npos) {
        // Take first line
        size_t pos = result.find('\n');
        if (pos != std::string::npos) {
            return result.substr(0, pos);
        }
        return result;
    }
    
    return "";
}

// Function to open file selection dialog
std::string BrowseFile(HWND hwnd, const char* filter, const char* title, bool save = false) {
    OPENFILENAMEA ofn = {0};
    char fileName[MAX_PATH] = "";
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_HIDEREADONLY | OFN_EXPLORER;
    
    if (save) {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn)) {
            return std::string(fileName);
        }
    } else {
        ofn.Flags |= OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            return std::string(fileName);
        }
    }
    
    return "";
}

// Function to update status
void SetStatus(const std::string& text, COLORREF color) {
    SetWindowTextA(hStatusBar, text.c_str());
}

// Function to execute PowerShell command
std::string RunPowerShell(const std::string& command) {
    std::string fullCommand = "powershell -ExecutionPolicy Bypass -Command \"" + command + "\"";
    return ExecCommand(fullCommand);
}

// Function to create certificate
bool CreateCertificate(const std::string& name, const std::string& password, 
                       const std::string& scriptDir, std::string& pfxPath, std::string& cerPath) {
    
    pfxPath = scriptDir + "MyCertificate.pfx";
    cerPath = scriptDir + "MyCertificate.cer";
    
    // PowerShell command to create certificate
    std::string psScript = 
        "$Cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject \"" + name + 
        "\" -KeyUsage DigitalSignature -CertStoreLocation \"Cert:\\CurrentUser\\My\"; " +
        "$CertPassword = ConvertTo-SecureString -String \"" + password + 
        "\" -Force -AsPlainText; " +
        "Export-PfxCertificate -Cert $Cert -FilePath \"" + pfxPath + 
        "\" -Password $CertPassword; " +
        "Export-Certificate -Cert $Cert -FilePath \"" + cerPath + "\" -Type CERT";
    
    std::string result = RunPowerShell(psScript);
    
    // Check if files were created
    if (GetFileAttributesA(pfxPath.c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesA(cerPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    
    return false;
}

// Function to sign file
bool SignFile(const std::string& signToolPath, const std::string& certPath, 
              const std::string& password, const std::string& exePath, std::string& errorMsg) {
    
    std::string command = "\"" + signToolPath + "\" sign /fd SHA256 /f \"" + 
                          certPath + "\" /p " + password + 
                          " /tr http://timestamp.digicert.com /td SHA256 \"" + 
                          exePath + "\"";
    
    std::string result = ExecCommand(command);
    
    if (result.find("Successfully signed") != std::string::npos) {
        return true;
    }
    
    errorMsg = result;
    return false;
}

// Procedure for "Create" tab
INT_PTR CALLBACK CreateDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Create controls
            int y = 10;
            int labelWidth = 100;
            int editWidth = 350;
            int buttonWidth = 30;
            
            // Name
            CreateWindowA("STATIC", "Name/Company (CN=...):", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditCreateName = CreateWindowA("EDIT", "CN=MyCompany", 
                             WS_VISIBLE | WS_CHILD | WS_BORDER,
                             120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            y += 30;
            
            // Password
            CreateWindowA("STATIC", "Password:", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditCreatePassword = CreateWindowA("EDIT", "", 
                                  WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
                                  120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            y += 30;
            
            // EXE file
            CreateWindowA("STATIC", "EXE file:", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditCreateExePath = CreateWindowA("EDIT", "", 
                                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                                 120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            
            hBtnCreateBrowseExe = CreateWindowA("BUTTON", "...", 
                                   WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   480, y, buttonWidth, 20, hwnd, (HMENU)101, hInst, NULL);
            y += 40;
            
            // Create button
            hBtnCreate = CreateWindowA("BUTTON", "CREATE CERTIFICATE AND SIGN", 
                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         150, y, 250, 30, hwnd, (HMENU)102, hInst, NULL);
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            switch (id) {
                case 101: { // Browse EXE
                    std::string path = BrowseFile(GetParent(hwnd), 
                        "Executable Files\0*.exe\0All Files\0*.*\0",
                        "Select EXE file to sign");
                    if (!path.empty()) {
                        SetWindowTextA(hEditCreateExePath, path.c_str());
                    }
                    break;
                }
                
                case 102: { // Create and Sign
                    char name[256], password[256], exePath[256];
                    
                    GetWindowTextA(hEditCreateName, name, 256);
                    GetWindowTextA(hEditCreatePassword, password, 256);
                    GetWindowTextA(hEditCreateExePath, exePath, 256);
                    
                    if (strlen(name) == 0 || strlen(password) == 0 || strlen(exePath) == 0) {
                        SetStatus("Error: Fill all fields!", COLOR_ERROR);
                        MessageBeep(MB_ICONERROR);
                        break;
                    }
                    
                    if (GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
                        SetStatus("Error: EXE file does not exist!", COLOR_ERROR);
                        break;
                    }
                    
                    SetStatus("Creating certificate...", COLOR_INFO);
                    
                    // Get program folder
                    char scriptDir[MAX_PATH];
                    GetCurrentDirectoryA(MAX_PATH, scriptDir);
                    std::string dir(scriptDir);
                    dir += "\\";
                    
                    std::string pfxPath, cerPath;
                    
                    // Create certificate
                    if (!CreateCertificate(name, password, dir, pfxPath, cerPath)) {
                        SetStatus("Error creating certificate!", COLOR_ERROR);
                        break;
                    }
                    
                    SetStatus("Certificate created. Searching for SignTool...", COLOR_INFO);
                    
                    // Find signtool
                    std::string signToolPath = FindSignTool();
                    if (signToolPath.empty()) {
                        SetStatus("Error: SignTool not found!", COLOR_ERROR);
                        break;
                    }
                    
                    SetStatus("Signing file...", COLOR_INFO);
                    
                    // Sign file
                    std::string errorMsg;
                    if (SignFile(signToolPath, pfxPath, password, exePath, errorMsg)) {
                        std::string status = "Success! File signed. Certificates: " + pfxPath;
                        SetStatus(status, COLOR_SUCCESS);
                        MessageBoxA(hwnd, "File successfully signed!", "Success", MB_OK | MB_ICONINFORMATION);
                    } else {
                        SetStatus("Signing error: " + errorMsg, COLOR_ERROR);
                        MessageBoxA(hwnd, ("Signing error:\n" + errorMsg).c_str(), "Error", MB_OK | MB_ICONERROR);
                    }
                    
                    break;
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

// Procedure for "Assign" tab
INT_PTR CALLBACK AssignDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            int y = 10;
            int labelWidth = 100;
            int editWidth = 350;
            int buttonWidth = 30;
            
            // SignTool Path
            CreateWindowA("STATIC", "SignTool.exe:", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditAssignSignToolPath = CreateWindowA("EDIT", "", 
                                       WS_VISIBLE | WS_CHILD | WS_BORDER | ES_READONLY,
                                       120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            
            hBtnAssignBrowseSignTool = CreateWindowA("BUTTON", "...", 
                                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                         480, y, buttonWidth, 20, hwnd, (HMENU)201, hInst, NULL);
            y += 30;
            
            // Cert Path
            CreateWindowA("STATIC", "PFX certificate:", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditAssignCertPath = CreateWindowA("EDIT", "", 
                                  WS_VISIBLE | WS_CHILD | WS_BORDER,
                                  120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            
            hBtnAssignBrowseCert = CreateWindowA("BUTTON", "...", 
                                    WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                    480, y, buttonWidth, 20, hwnd, (HMENU)202, hInst, NULL);
            y += 30;
            
            // Password
            CreateWindowA("STATIC", "Password:", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditAssignPassword = CreateWindowA("EDIT", "", 
                                  WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
                                  120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            y += 30;
            
            // EXE Path
            CreateWindowA("STATIC", "EXE file:", WS_VISIBLE | WS_CHILD,
                         10, y, labelWidth, 20, hwnd, NULL, hInst, NULL);
            
            hEditAssignExePath = CreateWindowA("EDIT", "", 
                                 WS_VISIBLE | WS_CHILD | WS_BORDER,
                                 120, y, editWidth, 20, hwnd, NULL, hInst, NULL);
            
            hBtnAssignBrowseExe = CreateWindowA("BUTTON", "...", 
                                   WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   480, y, buttonWidth, 20, hwnd, (HMENU)203, hInst, NULL);
            y += 40;
            
            // Sign Button
            hBtnAssign = CreateWindowA("BUTTON", "SIGN FILE", 
                         WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         150, y, 250, 30, hwnd, (HMENU)204, hInst, NULL);
            
            // Auto-find signtool
            std::string signToolPath = FindSignTool();
            if (!signToolPath.empty()) {
                SetWindowTextA(hEditAssignSignToolPath, signToolPath.c_str());
            }
            
            return TRUE;
        }
        
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            switch (id) {
                case 201: { // Browse SignTool
                    std::string path = BrowseFile(GetParent(hwnd), 
                        "Executable Files\0*.exe\0All Files\0*.*\0",
                        "Select signtool.exe");
                    if (!path.empty()) {
                        SetWindowTextA(hEditAssignSignToolPath, path.c_str());
                    }
                    break;
                }
                
                case 202: { // Browse Cert
                    std::string path = BrowseFile(GetParent(hwnd), 
                        "PFX Files\0*.pfx\0All Files\0*.*\0",
                        "Select PFX certificate");
                    if (!path.empty()) {
                        SetWindowTextA(hEditAssignCertPath, path.c_str());
                    }
                    break;
                }
                
                case 203: { // Browse EXE
                    std::string path = BrowseFile(GetParent(hwnd), 
                        "Executable Files\0*.exe\0All Files\0*.*\0",
                        "Select EXE file to sign");
                    if (!path.empty()) {
                        SetWindowTextA(hEditAssignExePath, path.c_str());
                    }
                    break;
                }
                
                case 204: { // Sign
                    char signToolPath[256], certPath[256], password[256], exePath[256];
                    
                    GetWindowTextA(hEditAssignSignToolPath, signToolPath, 256);
                    GetWindowTextA(hEditAssignCertPath, certPath, 256);
                    GetWindowTextA(hEditAssignPassword, password, 256);
                    GetWindowTextA(hEditAssignExePath, exePath, 256);
                    
                    if (strlen(signToolPath) == 0 || strlen(certPath) == 0 || 
                        strlen(password) == 0 || strlen(exePath) == 0) {
                        SetStatus("Error: Fill all fields!", COLOR_ERROR);
                        MessageBeep(MB_ICONERROR);
                        break;
                    }
                    
                    if (GetFileAttributesA(signToolPath) == INVALID_FILE_ATTRIBUTES ||
                        GetFileAttributesA(certPath) == INVALID_FILE_ATTRIBUTES ||
                        GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
                        SetStatus("Error: One of the files does not exist!", COLOR_ERROR);
                        break;
                    }
                    
                    SetStatus("Signing file...", COLOR_INFO);
                    
                    std::string errorMsg;
                    if (SignFile(signToolPath, certPath, password, exePath, errorMsg)) {
                        SetStatus("Success! File successfully signed!", COLOR_SUCCESS);
                        MessageBoxA(hwnd, "File successfully signed!", "Success", MB_OK | MB_ICONINFORMATION);
                    } else {
                        SetStatus("Signing error: " + errorMsg, COLOR_ERROR);
                        MessageBoxA(hwnd, ("Signing error:\n" + errorMsg).c_str(), "Error", MB_OK | MB_ICONERROR);
                    }
                    
                    break;
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

// Main window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hMainWindow = hwnd;

            // 1. Initialize Common Controls (Required for Tabs)
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_TAB_CLASSES;
            InitCommonControlsEx(&icex);

            // 2. Create the Tab Control
            hTabControl = CreateWindowA(WC_TABCONTROLA, "", 
                                        WS_VISIBLE | WS_CHILD | TCS_FIXEDWIDTH,
                                        10, 10, 580, 320, hwnd, (HMENU)1000, hInst, NULL);

            // Set font for the Tab Control (Optional but looks better)
            SendMessage(hTabControl, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

            // 3. Add Tabs
            TCITEMA tci = {0};
            tci.mask = TCIF_TEXT;
            
            char tab1[] = "Create Certificate";
            tci.pszText = tab1;
            TabCtrl_InsertItem(hTabControl, 0, &tci);
            
            char tab2[] = "Assign Signature";
            tci.pszText = tab2;
            TabCtrl_InsertItem(hTabControl, 1, &tci);

            // 4. Calculate the display area for the tab content
            RECT rc;
            GetClientRect(hTabControl, &rc);
            TabCtrl_AdjustRect(hTabControl, FALSE, &rc);

            // 5. Create "Container" Windows for each tab
            // We use a simple static class since we aren't using a resource (.rc) file
            hCreateDialog = CreateWindowA("STATIC", "", 
                                          WS_CHILD | WS_VISIBLE, 
                                          rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 
                                          hTabControl, NULL, hInst, NULL);

            hAssignDialog = CreateWindowA("STATIC", "", 
                                          WS_CHILD, // Hidden by default (no WS_VISIBLE)
                                          rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 
                                          hTabControl, NULL, hInst, NULL);

            // 6. Manually trigger the initialization of controls for each tab
            // Since we aren't using real Dialog Templates, we call the Procs manually
            CreateDlgProc(hCreateDialog, WM_INITDIALOG, 0, 0);
            AssignDlgProc(hAssignDialog, WM_INITDIALOG, 0, 0);

            // 7. Create the Status Bar
            hStatusBar = CreateWindowA("STATIC", "Ready to work", 
                                       WS_VISIBLE | WS_CHILD | SS_CENTER | WS_BORDER,
                                       10, 340, 580, 25, hwnd, NULL, hInst, NULL);
            
            SendMessage(hStatusBar, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

            return 0;
        }
        
        case WM_NOTIFY: {
            if (((LPNMHDR)lParam)->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(hTabControl);
                ShowWindow(hCreateDialog, sel == 0 ? SW_SHOW : SW_HIDE);
                ShowWindow(hAssignDialog, sel == 1 ? SW_SHOW : SW_HIDE);
            }
            break;
        }
        
        case WM_SIZE: {
            // Update sizes when window is resized
            RECT rc;
            GetClientRect(hwnd, &rc);
            SetWindowPos(hTabControl, NULL, 10, 10, 
                         rc.right - 20, rc.bottom - 70, SWP_NOZORDER);
            SetWindowPos(hStatusBar, NULL, 10, rc.bottom - 35, 
                         rc.right - 20, 25, SWP_NOZORDER);
            break;
        }
        
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;
    
    // Register window class
    WNDCLASSEXA wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "CertificateTool";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassExA(&wcex)) {
        return 1;
    }
    
    // Create main window
    HWND hwnd = CreateWindowExA(0, "CertificateTool", 
                                "Certificate Tool - Create and Assign Signatures",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT, CW_USEDEFAULT, 620, 420,
                                NULL, NULL, hInstance, NULL);
    
    if (!hwnd) {
        return 1;
    }
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}