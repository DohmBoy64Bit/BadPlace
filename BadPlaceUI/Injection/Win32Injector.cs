using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

namespace BadPlaceUI.Injection;

public static class Win32Injector
{
    // ── Win32 API declarations ──────────────────────────────────────────────
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenProcess(uint dwAccess, bool bInherit, int dwProcessId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr VirtualAllocEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize,
        uint flAllocationType, uint flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress,
        byte[] lpBuffer, uint nSize, out int lpNumberOfBytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateRemoteThread(IntPtr hProcess, IntPtr lpThreadAttributes,
        uint dwStackSize, IntPtr lpStartAddress, IntPtr lpParameter, uint dwCreationFlags, IntPtr lpThreadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr hObject);

    // ── Access flags ────────────────────────────────────────────────────────
    private const uint PROCESS_ALL_ACCESS  = 0x1F0FFF;
    private const uint MEM_COMMIT          = 0x1000;
    private const uint MEM_RESERVE         = 0x2000;
    private const uint PAGE_EXECUTE_READWRITE = 0x40;

    // ── Public API ──────────────────────────────────────────────────────────
    /// <summary>
    /// Injects BadPlace.dll into the first running instance of the target process.
    /// Returns (true, null) on success or (false, errorMessage) on failure.
    /// </summary>
    public static (bool Success, string? Error) Inject(string processName, string dllPath)
    {
        // 1. Validate DLL path
        if (!File.Exists(dllPath))
            return (false, $"DLL not found at path: {dllPath}");

        // 2. Find the target process
        Process[] procs = Process.GetProcessesByName(processName);
        if (procs.Length == 0)
            return (false, $"Process '{processName}' is not running.");

        Process target = procs[0];
        string fullDllPath = Path.GetFullPath(dllPath);
        byte[] pathBytes = System.Text.Encoding.Unicode.GetBytes(fullDllPath + "\0");

        IntPtr hProcess = IntPtr.Zero;
        IntPtr pRemotePath = IntPtr.Zero;

        try
        {
            // 3. Open process with full access
            hProcess = OpenProcess(PROCESS_ALL_ACCESS, false, target.Id);
            if (hProcess == IntPtr.Zero)
                return (false, $"Failed to open process. Win32 error: {Marshal.GetLastWin32Error()}");

            // 4. Allocate memory inside the target process for the DLL path string
            pRemotePath = VirtualAllocEx(hProcess, IntPtr.Zero, (uint)pathBytes.Length,
                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

            if (pRemotePath == IntPtr.Zero)
                return (false, $"VirtualAllocEx failed. Win32 error: {Marshal.GetLastWin32Error()}");

            // 5. Write the DLL path into the target process memory
            if (!WriteProcessMemory(hProcess, pRemotePath, pathBytes, (uint)pathBytes.Length, out _))
                return (false, $"WriteProcessMemory failed. Win32 error: {Marshal.GetLastWin32Error()}");

            // 6. Get LoadLibraryW address from kernel32 (shared across all processes natively)
            IntPtr loadLibraryAddr = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryW");
            if (loadLibraryAddr == IntPtr.Zero)
                return (false, "Could not resolve LoadLibraryW address.");

            // 7. Launch the DLL inside the target process via a remote thread
            IntPtr hThread = CreateRemoteThread(hProcess, IntPtr.Zero, 0,
                loadLibraryAddr, pRemotePath, 0, IntPtr.Zero);

            if (hThread == IntPtr.Zero)
                return (false, $"CreateRemoteThread failed. Win32 error: {Marshal.GetLastWin32Error()}");

            CloseHandle(hThread);
            return (true, null);
        }
        finally
        {
            if (hProcess != IntPtr.Zero) CloseHandle(hProcess);
        }
    }
}
