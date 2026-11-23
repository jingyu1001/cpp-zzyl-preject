# 截图/窗口操控辅助脚本（总控验收用）
param(
  [string]$ProcName = 'zzyl',
  [string]$OutDir = 'C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject\_verify',
  [int]$WaitSec = 6
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Helper {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int X, int Y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
}
"@
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$proc = Get-Process -Name $ProcName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) { Write-Output "NO_WINDOW_PROCESS"; exit 2 }
$h = $proc.MainWindowHandle
[Win32Helper]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 800

function Save-WindowShot([string]$name) {
  $r = New-Object Win32Helper+RECT
  [Win32Helper]::GetWindowRect($h, [ref]$r) | Out-Null
  $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
  if ($w -le 0 -or $hh -le 0) { Write-Output "BAD_RECT $name"; return }
  $bmp = New-Object System.Drawing.Bitmap($w, $hh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
  $path = Join-Path $OutDir "$name.png"
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Output "SHOT $name ${w}x${hh} -> $path"
}

Write-Output "WINDOW_HANDLE=$h PID=$($proc.Id)"
Save-WindowShot '01_default'

# 调整到 1600x900 测试自适应
[Win32Helper]::SetWindowPos($h, [IntPtr]::Zero, 40, 40, 1600, 900, 0x0040) | Out-Null
Start-Sleep -Milliseconds 1200
Save-WindowShot '02_resize_1600x900'

# 调整到 1024x640 测试自适应
[Win32Helper]::SetWindowPos($h, [IntPtr]::Zero, 60, 60, 1024, 640, 0x0040) | Out-Null
Start-Sleep -Milliseconds 1200
Save-WindowShot '03_resize_1024x640'

# 还原 1280x720，再按 F11 全屏
[Win32Helper]::SetWindowPos($h, [IntPtr]::Zero, 80, 80, 1280, 720, 0x0040) | Out-Null
Start-Sleep -Milliseconds 1000
[Win32Helper]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
[Win32Helper]::keybd_event(0x7A, 0, 0, [UIntPtr]::Zero)      # VK_F11 down
[Win32Helper]::keybd_event(0x7A, 0, 2, [UIntPtr]::Zero)      # VK_F11 up
Start-Sleep -Milliseconds 1500
Save-WindowShot '04_fullscreen_F11'
Write-Output 'DONE'
