# Real-window helper for packaged / -game builds: screenshot a process's window and send REAL mouse / keyboard input to it (the simulated clicks of `ue.py ui click` skip Slate hit-testing,
# so a bare native button "works" there while real mouse clicks pass through it).
#
#   powershell -NoProfile -File Tools\win_input.ps1 -ProcessId <pid> -Action shot  -Out C:\path\a.png
#   powershell -NoProfile -File Tools\win_input.ps1 -ProcessId <pid> -Action click -X 960 -Y 540          (client-area pixels; -Button right for a right click)
#   powershell -NoProfile -File Tools\win_input.ps1 -ProcessId <pid> -Action move  -X 960 -Y 540
#   powershell -NoProfile -File Tools\win_input.ps1 -ProcessId <pid> -Action key   -Keys "{ESC}"          (System.Windows.Forms.SendKeys syntax)
#   powershell -NoProfile -File Tools\win_input.ps1 -ProcessId <pid> -Action info                          (client rectangle on screen)
#
# SAFETY: shots use PrintWindow (works while another window covers the game and never moves the cursor). click / move / key REFUSE to run unless the target window really is the foreground window
# (Windows may refuse SetForegroundWindow, and real input would then hit whatever application is in front: another game, the user's browser ...). Coordinates are client-area pixels (what a 1920x1080 -ResX/-ResY window shows), converted with the live client rectangle, so a moved window is fine.
param(
    [Parameter(Mandatory = $true)][int]$ProcessId,
    [Parameter(Mandatory = $true)][ValidateSet("shot", "click", "move", "key", "info")][string]$Action,
    [string]$Out = "",
    [int]$X = 0,
    [int]$Y = 0,
    [string]$Keys = "",
    [ValidateSet("left", "right")][string]$Button = "left"
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W32 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    public static IntPtr FindTopWindow(uint pid) {
        IntPtr found = IntPtr.Zero; long best = 0;
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p == pid && IsWindowVisible(h)) {
                RECT r; GetClientRect(h, out r);
                long area = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
                if (area > best) { best = area; found = h; }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@
[void][W32]::SetProcessDPIAware()

$h = [W32]::FindTopWindow([uint32]$ProcessId)
if ($h -eq [IntPtr]::Zero) { Write-Output "ERROR: no visible window for pid $ProcessId"; exit 2 }
if ($Action -ne "shot" -and $Action -ne "info") {
    [void][W32]::ShowWindow($h, 9)
    # Windows only lets the process that last received input take the foreground: a synthetic Alt tap (nothing in UE binds a bare Alt) makes this process eligible.
    [W32]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [W32]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
    [void][W32]::SetForegroundWindow($h)
    Start-Sleep -Milliseconds 300
    if ([W32]::GetForegroundWindow() -ne $h) {
        Write-Output "REFUSED: the game window is not the foreground window (another application is in front); no input sent"
        exit 3
    }
}

$rect = New-Object W32+RECT
[void][W32]::GetClientRect($h, [ref]$rect)
$origin = New-Object W32+POINT
[void][W32]::ClientToScreen($h, [ref]$origin)
$w = $rect.Right - $rect.Left
$ht = $rect.Bottom - $rect.Top

switch ($Action) {
    "info" { Write-Output ("client {0}x{1} at screen ({2},{3})" -f $w, $ht, $origin.X, $origin.Y) }
    "shot" {
        if (-not $Out) { Write-Output "ERROR: -Out required"; exit 2 }
        $bmp = New-Object System.Drawing.Bitmap $w, $ht
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $hdc = $g.GetHdc()
        $ok = [W32]::PrintWindow($h, $hdc, 3)   # PW_CLIENTONLY | PW_RENDERFULLCONTENT: the window's own pixels, wherever it is on screen
        $g.ReleaseHdc($hdc)
        $g.Dispose()
        if (-not $ok) { Write-Output "WARNING: PrintWindow failed; the image may be blank" }
        $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Output ("shot {0}x{1} -> {2}" -f $w, $ht, $Out)
    }
    "move" {
        [void][W32]::SetCursorPos($origin.X + $X, $origin.Y + $Y)
        Write-Output ("cursor -> client ({0},{1})" -f $X, $Y)
    }
    "click" {
        [void][W32]::SetCursorPos($origin.X + $X, $origin.Y + $Y)
        Start-Sleep -Milliseconds 120
        if ($Button -eq "left") { $down = 0x0002; $up = 0x0004 } else { $down = 0x0008; $up = 0x0010 }
        [W32]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 70
        [W32]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
        Write-Output ("{0} click at client ({1},{2})" -f $Button, $X, $Y)
    }
    "key" {
        [System.Windows.Forms.SendKeys]::SendWait($Keys)
        Write-Output ("keys sent: {0}" -f $Keys)
    }
}
