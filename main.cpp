#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>

std::string g_SignToolPath = "";

// Функция для установки фиксированного размера окна
void SetConsoleWindow(int width, int height) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = { (short)width, (short)height };
    SMALL_RECT rect = { 0, 0, (short)(width - 1), (short)(height - 1) };

    SetConsoleScreenBufferSize(hOut, coord);            // Размер буфера
    SetConsoleWindowInfo(hOut, TRUE, &rect);            // Размер окна
}

bool FileExists(const std::string& path) {
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string ExecCommand(const std::string& cmd) {
    std::string result;
    char buffer[128], tempPath[MAX_PATH], tempFile[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    GetTempFileNameA(tempPath, "tui", 0, tempFile);
    
    std::string fullCmd = "cmd /c " + cmd + " > \"" + std::string(tempFile) + "\" 2>&1";
    system(fullCmd.c_str());
    
    std::ifstream file(tempFile);
    if (file.is_open()) {
        while (file.getline(buffer, sizeof(buffer))) result += buffer + std::string("\n");
        file.close();
    }
    DeleteFileA(tempFile);
    return result;
}

std::string FindSignTool() {
    std::vector<std::string> paths = {
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.19041.0\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe",
        "C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.1A\\Bin\\signtool.exe",
				"C:\\Program Files (x86)\\Windows Kits\\10\\App Certification Kit\\signtool.exe"
    };
    for (const auto& path : paths) if (FileExists(path)) return path;
    return "";
}

bool CreateCertificate(const std::string& name, const std::string& password, std::string& pfxPath) {
    char curDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, curDir);
    pfxPath = std::string(curDir) + "\\" + name + ".pfx";
    
    std::string psCommand = "powershell -ExecutionPolicy Bypass -Command \""
        "$Cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=" + name + "' -CertStoreLocation 'Cert:\\CurrentUser\\My'; "
        "$CertPassword = ConvertTo-SecureString -String '" + password + "' -Force -AsPlainText; "
        "Export-PfxCertificate -Cert $Cert -FilePath '" + pfxPath + "' -Password $CertPassword\"";
    
    std::cout << "[*] Running PowerShell...\n";
    ExecCommand(psCommand);
    return FileExists(pfxPath);
}

bool SignFile(const std::string& signtool, const std::string& cert, const std::string& pass, const std::string& exe) {
    std::string cmd = "\"" + signtool + "\" sign /fd SHA256 /f \"" + cert + "\" /p " + pass + " /tr http://timestamp.digicert.com /td SHA256 \"" + exe + "\"";
    std::cout << "[*] Signing file...\n";
    std::string output = ExecCommand(cmd);
    if (output.find("Successfully signed") != std::string::npos) return true;
    std::cout << "\n[!] Sign Error:\n" << output << "\n";
    return false;
}

void PrintHeader() {
    system("cls");
    // Линия ровно на 80 символов
    std::cout << "================================================================================\n";
    std::cout << "                         CERTIFICATE TOOL                         \n";
    std::cout << "================================================================================\n";
    
    if (g_SignToolPath.empty()) {
        std::cout << " STATUS: [!] SignTool NOT FOUND\n";
    } else {
        std::cout << " STATUS: [+] SignTool FOUND";
        std::cout << "     PATH:   " << (g_SignToolPath.length() > 70 ? "..." + g_SignToolPath.substr(g_SignToolPath.length()-67) : g_SignToolPath) << "\n";
    }
    std::cout << "================================================================================\n\n";
}

int main() {
    // 1. Устанавливаем размер окна 80x30
    SetConsoleWindow(80, 30);
    SetConsoleTitleA("Certificate Tool TUI");
    
    g_SignToolPath = FindSignTool();
    
    while (true) {
        PrintHeader();
        std::cout << "  [1] Create New Certificate & Sign EXE\n";
        std::cout << "  [2] Sign EXE using existing PFX\n";
        std::cout << "  [3] Just Generate New Certificate (PFX)\n";
        std::cout << "  [4] Exit\n\n";
        std::cout << "  Select option > ";
        
        std::string input;
        std::getline(std::cin, input);
        if (input.empty()) continue;
        int choice = std::atoi(input.c_str());

        if (choice == 4) break;

        std::string name, pass, exe, pfx;

        std::cout << "\n--------------------------------------------------------------------------------\n";
        if (choice == 1) {
            std::cout << " > Certificate Name: "; std::getline(std::cin, name);
            std::cout << " > PFX Password:     "; std::getline(std::cin, pass);
            std::cout << " > EXE Path:         "; std::getline(std::cin, exe);
            if (CreateCertificate(name, pass, pfx)) {
                if (!g_SignToolPath.empty()) {
                    if (SignFile(g_SignToolPath, pfx, pass, exe)) std::cout << "\n[+] SUCCESS: File signed!\n";
                } else std::cout << "\n[!] SignTool not found.\n";
            }
        } 
        else if (choice == 2) {
            std::string st = g_SignToolPath;
            if (st.empty()) { std::cout << " > SignTool Path: "; std::getline(std::cin, st); }
            std::cout << " > PFX Path:      "; std::getline(std::cin, pfx);
            std::cout << " > Password:      "; std::getline(std::cin, pass);
            std::cout << " > EXE Path:      "; std::getline(std::cin, exe);
            if (SignFile(st, pfx, pass, exe)) std::cout << "\n[+] SUCCESS: File signed!\n";
        }
        else if (choice == 3) {
            std::cout << " > Certificate Name: "; std::getline(std::cin, name);
            std::cout << " > PFX Password:     "; std::getline(std::cin, pass);
            if (CreateCertificate(name, pass, pfx)) std::cout << "\n[+] SUCCESS: Generated " << name << ".pfx\n";
        }
        
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << "Press Enter to return to menu...";
        std::cin.get();
    }
    return 0;
}