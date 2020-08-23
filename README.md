# Windows Kernel Development

This repo has my solutions for exercises specified inside the **"Windows Kernel Development"** book written by Pavel Yosifovich (which is more than recommended :)).

## Full Kernel Debugging Environment

### Host Machine

1. Install VS19
2. Install [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk/)
3. Install [Windows Driver Kit](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
4. Install WinDbg Preview
5. Download [Sysinternals](https://docs.microsoft.com/en-us/sysinternals/)
6. Download [Spectre Mitigation requirements](https://docs.microsoft.com/en-us/cpp/build/reference/qspectre?view=vs-2019)

7. [Download](https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/) and create VM (I'm using Hyper-V)

### Guest Machine

1. Run the following commands inside the VM:

```bat
bcdedit /debug on
bcdedit /dbgsettings serial debugport:1 baudrate:115200
```

2. Restart
3. Create a COM Port
4. Check if COM Port created using:

```ps1
Get-VMComPort <VM_NAME>
```

5. Enable test signing (for loading unsigned drivers)

```bat
bcdedit /set testsigning on
```

6. Restart the machine
7. Enable kernel logging:
   7.1. Open registry key _HKLM\SYSTEM\CurrentControlSet\Control\Session Manager_
   7.2. Create new Key - _Debug Print Filter_
   7.3. Create new _DWORD_ value _DEFAULT = 0x8_
8. Restart the machine
9. Open _DbgView.exe_ and set _Capture Kernel_ using _Ctrl+K_.
