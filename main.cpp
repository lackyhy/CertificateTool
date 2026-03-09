#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <conio.h>
#include <algorithm>

#define VERSION "1.2.1"

using namespace std;

// --- Глобальные переменные ---
string g_SignToolPath = "";

// --- Вспомогательные функции интерфейса ---

// Очистка пути от лишних кавычек (если пользователь скопировал путь как "C:\path")
string CleanPath(string path) {
    path.erase(remove(path.begin(), path.end(), '\"'), path.end());
    // Убираем пробелы в начале и конце
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

// --- Логика работы с системой ---

string ExecCommand(const string& cmd) {
    string result;
    char buffer[128], tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "tui", 0, tempFile);
    
    // ИСПРАВЛЕНИЕ: Оборачиваем всю команду в дополнительные кавычки для корректной работы путей с пробелами
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
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.19041.0\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\8.1\\bin\\x64\\signtool.exe"
    };
    for (const auto& path : paths) if (FileExists(path)) return path;
    return "";
}

bool CreateCertificate(const string& name, const string& password, string& pfxPath) {
    char curDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, curDir);
    pfxPath = string(curDir) + "\\" + name + ".pfx";
    
    // PowerShell команда создания и экспорта
    string psCommand = "powershell -ExecutionPolicy Bypass -Command \""
        "$Cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=" + name + "' -CertStoreLocation 'Cert:\\CurrentUser\\My'; "
        "$CertPassword = ConvertTo-SecureString -String '" + password + "' -Force -AsPlainText; "
        "Export-PfxCertificate -Cert $Cert -FilePath '" + pfxPath + "' -Password $CertPassword\"";
    
    cout << "[*] Generating certificate via PowerShell..." << endl;
    ExecCommand(psCommand);
    return FileExists(pfxPath);
}

bool SignFile(const string& signtool, const string& cert, const string& pass, const string& exe) {
    if (signtool.empty()) {
        cout << "[!] Error: SignTool path is empty!" << endl;
        return false;
    }
    // Оборачиваем все пути в кавычки для cmd
    string cmd = "\"" + signtool + "\" sign /fd SHA256 /f \"" + cert + "\" /p \"" + pass + "\" /tr http://timestamp.digicert.com /td SHA256 \"" + exe + "\"";
    
    cout << "[*] Running SignTool..." << endl;
    string output = ExecCommand(cmd);
    
    if (output.find("Successfully signed") != string::npos) return true;
    
    cout << "\n[!] SignTool Error Details:\n" << output << endl;
    return false;
}

// --- Отрисовка Меню ---

void drawMenu(const vector<string> &menuItems, int selectedIndex) {
    system("cls");
    setCursorVisible(false);
    int width = GetConsoleWidth();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    cout << "Main Menu" << endl;
    cout << "Use Up/Down arrows to navigate, Enter to select, 'q' to exit" << endl << endl;

    for (int i = 0; i < (int)menuItems.size(); i++) {
        bool isSel = (i == selectedIndex);
        if (isSel) {
            SetConsoleTextAttribute(hConsole, BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }
        cout << (isSel ? "> " : "  ") << menuItems[i] << endl;
        if (isSel) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
    }

    cout << "\n\n";
    for (int i = 0; i < width; ++i) cout << "-";

    if (!g_SignToolPath.empty()) {
        string shortPath = g_SignToolPath;
        if (shortPath.length() > (size_t)width - 25) shortPath = "..." + shortPath.substr(shortPath.length() - (width - 28));
        cout << "SignTool: FOUND ( " << shortPath << " )" << endl;
    } else {
        cout << "SignTool: NOT FOUND" << endl;
    }

    cout << "Version: " << VERSION << " | User: " << getenv("USERNAME") << endl;
    for (int i = 0; i < width; ++i) cout << "-";
    cout << endl;
}

void HandleAction(int choice) {
    system("cls");
    setCursorVisible(true);
    string name, pass, exe, pfx;

    if (choice == 0) { // Create & Sign
        cout << ">>> CREATE NEW CERTIFICATE & SIGN EXE <<<\n\n";
        cout << "Enter Cert Name: "; getline(cin, name);
        cout << "Enter Password:  "; getline(cin, pass);
        cout << "Enter EXE Path:  "; getline(cin, exe);
        exe = CleanPath(exe);
        
        if (CreateCertificate(name, pass, pfx)) {
            if (SignFile(g_SignToolPath, pfx, pass, exe)) cout << "\n[+] DONE: File signed successfully!" << endl;
        } else cout << "\n[!] ERROR: Failed to create certificate (check Admin rights)." << endl;
    } 
    else if (choice == 1) { // Manual Sign
        cout << ">>> SIGN EXE USING EXISTING PFX <<<\n\n";
        string stPath = g_SignToolPath;
        if (stPath.empty()) { 
            cout << "SignTool Path: "; getline(cin, stPath); 
            stPath = CleanPath(stPath);
        }
        cout << "PFX Path: "; getline(cin, pfx); pfx = CleanPath(pfx);
        cout << "Password: "; getline(cin, pass);
        cout << "EXE Path: "; getline(cin, exe); exe = CleanPath(exe);
        
        if (SignFile(stPath, pfx, pass, exe)) cout << "\n[+] DONE: File signed successfully!" << endl;
    } 
    else if (choice == 2) { // Just Generate
        cout << ">>> GENERATE NEW CERTIFICATE (PFX) <<<\n\n";
        cout << "Enter Cert Name: "; getline(cin, name);
        cout << "Enter Password:  "; getline(cin, pass);
        
        if (CreateCertificate(name, pass, pfx)) cout << "\n[+] SUCCESS: Created " << pfx << endl;
        else cout << "\n[!] ERROR: Failed to generate certificate." << endl;
    }

    cout << "\nPress any key to return to menu...";
    _getch();
}

int main() {
    SetConsoleTitleA("Certificate Tool TUI");
    
    // Первичный поиск SignTool
    g_SignToolPath = FindSignTool();

    vector<string> menuItems = {
        "Create New Certificate & Sign EXE",
        "Sign EXE using existing PFX",
        "Just Generate New Certificate (PFX)",
        "Exit"
    };

    int selectedIndex = 0;
    bool running = true;

    while (running) {
        drawMenu(menuItems, selectedIndex);

        int key = _getch();
        if (key == 0 || key == 224) { // Стрелки
            switch (_getch()) {
                case 72: // Up
                    selectedIndex = (selectedIndex - 1 + menuItems.size()) % menuItems.size();
                    break; 
                case 80: // Down
                    selectedIndex = (selectedIndex + 1) % menuItems.size();
                    break; 
            }
        } 
        else if (key == 13) { // Enter
            if (selectedIndex == 3) running = false;
            else HandleAction(selectedIndex);
        }
        else if (key == 'q' || key == 'Q') {
            running = false;
        }
    }

    return 0;
}