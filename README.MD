# CertificateTool 🔐

A lightweight Windows desktop application built with **C++** and **WinAPI** for creating self-signed code-signing certificates and signing executable files. 

This tool simplifies the process of using Windows SDK's `signtool.exe` and PowerShell's certificate cmdlets through a convenient graphical interface.

## 🚀 Features

* **Create Certificate**: Generate a self-signed code-signing certificate (`.pfx` and `.cer`) via PowerShell with a custom Subject Name and Password.
* **Sign Executables**: Digitally sign `.exe` files using SHA-256 and RFC 3161 compliant timestamping.
* **Auto-Discovery**: Automatically scans common Windows SDK installation paths to locate `signtool.exe`.
* **Dual Tab Interface**: Separate workflows for creating new credentials and applying existing signatures.
* **Status Monitoring**: Real-time feedback on process success or error logs.

## 🛠 Prerequisites

* **OS**: Windows 7 / 10 / 11
* **PowerShell**: Required for certificate generation.
* **Windows SDK**: Required for `signtool.exe` (only for the signing process).

## 🔨 Build Instructions

To compile the project using **MinGW-w64 (g++)**, run the following command in your terminal:

```powershell
g++ main.cpp -o CertificateTool.exe -std=c++17
```