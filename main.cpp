#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <conio.h>
#include <algorithm>

#define VERSION "1.2.6"

using namespace std;

string g_SignToolPath = "";
volatile bool g_RequestReset = false;

// --- Обработчик Ctrl+C ---
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        g_RequestReset = true;
        // Мы НЕ шлем Enter здесь, чтобы не засорять ввод. 
        // Просто ставим флаг. Система сама прервет блокирующий ввод в консоли.
        return TRUE; 
    }
    return FALSE;
}

// --- Безопасный ввод с поддержкой отмены ---
bool SafeGetLine(string& out) {
    out = "";
    while (true) {
        if (g_RequestReset) return false;
        
        // Если в буфере что-то появилось
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 3) { // Ctrl+C на уровне символа
                g_RequestReset = true;
                return false;
            }
            if (ch == 13) { // Enter
                cout << endl;
                return true;
            }
            if (ch == 8) { // Backspace
                if (!out.empty()) {
                    out.pop_back();
                    cout << "\b \b";
                }
            } else if (ch >= 32) {
                out += (char)ch;
                cout << (char)ch;
            }
        }
        
        if (g_RequestReset) return false;
        Sleep(10); // Чтобы не грузить процессор
    }
}

// --- Вспомогательные функции ---

string CleanPath(string path) {
    path.erase(remove(path.begin(), path.end(), '\"'), path.end());
    size_t first = path.find_first_not_of(' ');
    if (string::npos == first) return path;
    size_t last = path.find_last_not_of(' ');
    return path.substr(first, (last - first + 1));
}

void setCursorVisible(bool visible) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = visible;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

int GetConsoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) return 80;
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
}

bool FileExists(const string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

string ExecCommand(const string& cmd) {
    string result;
    char buffer[128], tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "tui", 0, tempFile);
    string fullCmd = "cmd /c \" " + cmd + " > \"" + string(tempFile) + "\" 2>&1 \"";
    system(fullCmd.c_str());
    ifstream file(tempFile);
    if (file.is_open()) {
        while (file.getline(buffer, sizeof(buffer))) result += buffer + string("\n");
        file.close();
    }
    DeleteFileA(tempFile);
    return result;
}

string FindSignTool() {
    vector<string> paths = {
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\App Certification Kit\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe"
    };
    for (const auto& path : paths) if (FileExists(path)) return path;
    return "";
}

bool CreateCertificate(const string& name, const string& password, string& pfxPath) {
    char curDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, curDir);
    pfxPath = string(curDir) + "\\" + name + ".pfx";
    string psCommand = "powershell -ExecutionPolicy Bypass -Command \""
        "$Cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=" + name + "' -CertStoreLocation 'Cert:\\CurrentUser\\My'; "
        "$CertPassword = ConvertTo-SecureString -String '" + password + "' -Force -AsPlainText; "
        "Export-PfxCertificate -Cert $Cert -FilePath '" + pfxPath + "' -Password $CertPassword\"";
    cout << "[*] Running PowerShell..." << endl;
    ExecCommand(psCommand);
    return FileExists(pfxPath);
}

bool SignFile(const string& signtool, const string& cert, const string& pass, const string& exe) {
    if (signtool.empty()) return false;
    string cmd = "\"" + signtool + "\" sign /fd SHA256 /f \"" + cert + "\" /p \"" + pass + "\" /tr http://timestamp.digicert.com /td SHA256 \"" + exe + "\"";
    string output = ExecCommand(cmd);
    if (output.find("Successfully signed") != string::npos) return true;
    cout << "\n[!] SignTool Error:\n" << output << endl;
    return false;
}

void drawMenu(const vector<string> &menuItems, int selectedIndex) {
    system("cls");
    setCursorVisible(false);
    int width = GetConsoleWidth();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    cout << "Main Menu" << endl;
    cout << "Arrows to navigate, Enter to select, ^C to Cancel anytime" << endl << endl;

    for (int i = 0; i < (int)menuItems.size(); i++) {
        bool isSel = (i == selectedIndex);
        if (isSel) SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        cout << (isSel ? "> " : "  ") << menuItems[i] << endl;
        if (isSel) SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    cout << "\n\n";
    for (int i = 0; i < width; ++i) cout << "-";
    if (!g_SignToolPath.empty()) {
        string sp = g_SignToolPath;
        if (sp.length() > (size_t)width - 25) sp = "..." + sp.substr(sp.length() - (width - 28));
        cout << "SignTool: " << sp << endl;
    }
    cout << "Version: " << VERSION << " | [^C] Abort Mode" << endl;
    for (int i = 0; i < width; ++i) cout << "-";
    cout << endl;
}

void HandleAction(int choice) {
    g_RequestReset = false;
    system("cls");
    setCursorVisible(true);
    string name, pass, exe, pfx;

    if (choice == 0) {
        cout << ">>> CREATE & SIGN (CTRL+C to Abort) <<<\n\n";
        cout << "Enter Cert Name: "; if(!SafeGetLine(name)) return;
        cout << "Enter Password:  "; if(!SafeGetLine(pass)) return;
        cout << "Enter EXE Path:  "; if(!SafeGetLine(exe)) return;
        exe = CleanPath(exe);
        if (CreateCertificate(name, pass, pfx)) {
            if (g_RequestReset) return;
            if (SignFile(g_SignToolPath, pfx, pass, exe)) cout << "\n[+] Success!" << endl;
        }
    } 
    else if (choice == 1) {
        cout << ">>> MANUAL SIGN (CTRL+C to Abort) <<<\n\n";
        string st = g_SignToolPath;
        if (st.empty()) { 
            cout << "SignTool Path: "; if(!SafeGetLine(st)) return; 
            st = CleanPath(st); 
        }
        cout << "PFX Path: "; if(!SafeGetLine(pfx)) return; pfx = CleanPath(pfx);
        cout << "Password: "; if(!SafeGetLine(pass)) return;
        cout << "EXE Path: "; if(!SafeGetLine(exe)) return; exe = CleanPath(exe);
        if (SignFile(st, pfx, pass, exe)) cout << "\n[+] Success!" << endl;
    }
    else if (choice == 2) {
        cout << ">>> GENERATE PFX (CTRL+C to Abort) <<<\n\n";
        cout << "Enter Cert Name: "; if(!SafeGetLine(name)) return;
        cout << "Enter Password:  "; if(!SafeGetLine(pass)) return;
        if (CreateCertificate(name, pass, pfx)) cout << "\n[+] Created: " << pfx << endl;
    }

    if (!g_RequestReset) {
        cout << "\nOperation finished. Press any key...";
        _getch();
    }
}

int main() {
    SetConsoleTitleA("Certificate Tool TUI");
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    g_SignToolPath = FindSignTool();
    vector<string> menuItems = {"Create New Certificate & Sign EXE", "Sign EXE using existing PFX", "Just Generate New Certificate (PFX)", "Exit"};
    int selectedIndex = 0;

    while (true) {
        g_RequestReset = false;
        drawMenu(menuItems, selectedIndex);
        
        // Ожидание ввода с проверкой флага
        while (!_kbhit()) {
            if (g_RequestReset) break;
            Sleep(50);
        }

        if (g_RequestReset) continue;

        int key = _getch();
        if (key == 0 || key == 224) {
            switch (_getch()) {
                case 72: selectedIndex = (selectedIndex - 1 + (int)menuItems.size()) % (int)menuItems.size(); break;
                case 80: selectedIndex = (selectedIndex + 1) % (int)menuItems.size(); break;
            }
        } 
        else if (key == 13) {
            if (selectedIndex == 3) break;
            HandleAction(selectedIndex);
        }
        else if (key == 'q' || key == 'Q') break;
    }
    return 0;
}