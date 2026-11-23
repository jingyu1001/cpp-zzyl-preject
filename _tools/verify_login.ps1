# zzyl 桌面端集成验收脚本：真实登录交互 + 错误提示 + 截图 + 全屏
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class V {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@
$cpp = 'C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject'
$exe = Join-Path $cpp 'build\Release\zzyl_desktop.exe'
$out = Join-Path $cpp 'artifacts\_verify_lead'
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out -Force | Out-Null }

function Click-At([IntPtr]$h, [int]$cx, [int]$cy) {
  $pt = New-Object V+POINT
  $pt.X = $cx; $pt.Y = $cy
  [V]::ClientToScreen($h, [ref]$pt) | Out-Null
  [V]::SetCursorPos($pt.X, $pt.Y) | Out-Null
  Start-Sleep -Milliseconds 120
  [V]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  [V]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 180
}

function Save-Shot([IntPtr]$h, [string]$name) {
  $r = New-Object V+RECT
  [V]::GetWindowRect($h, [ref]$r) | Out-Null
  $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap($w, $hh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
  $path = Join-Path $out "$name.png"
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Write-Output "SHOT $name ${w}x${hh} -> $path"
}

function Wait-Window([System.Diagnostics.Process]$proc, [int]$timeoutSec = 20) {
  $deadline = (Get-Date).AddSeconds($timeoutSec)
  while ((Get-Date) -lt $deadline) {
    $proc.Refresh()
    if ($proc.HasExited) { return [IntPtr]::Zero }
    if ($proc.MainWindowHandle -ne 0) { return $proc.MainWindowHandle }
    Start-Sleep -Milliseconds 250
  }
  return [IntPtr]::Zero
}

# ---------- 场景 A：正确登录 ----------
Write-Output '=== 场景A：admin/admin123 登录 ==='
$p = Start-Process -FilePath $exe -WorkingDirectory $cpp -PassThru
$h = Wait-Window $p
if ($h -eq [IntPtr]::Zero) { Write-Output 'NO_WINDOW'; exit 2 }
[V]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1200
Save-Shot $h 'A0_login_before'

Click-At $h 640 249   # 账号
[System.Windows.Forms.SendKeys]::SendWait('admin')
Start-Sleep -Milliseconds 150
Click-At $h 640 317   # 密码
[System.Windows.Forms.SendKeys]::SendWait('admin123')
Start-Sleep -Milliseconds 150
Click-At $h 555 385   # 验证码
[System.Windows.Forms.SendKeys]::SendWait('1234')
Start-Sleep -Milliseconds 150
Save-Shot $h 'A1_login_filled'
Click-At $h 640 511   # 登录按钮
Start-Sleep -Milliseconds 2200
Save-Shot $h 'A2_after_login'

# 菜单交互：点“服务管理”（侧栏第 5 项附近）看是否弹出规划中提示
Click-At $h 100 514
Start-Sleep -Milliseconds 700
Save-Shot $h 'A3_menu_click'

# 窗口缩放自适应
[V]::SetWindowPos($h, [IntPtr]::Zero, 30, 30, 1600, 900, 0x0040) | Out-Null
Start-Sleep -Milliseconds 1200
Save-Shot $h 'A4_resize_1600x900'
[V]::SetWindowPos($h, [IntPtr]::Zero, 30, 30, 1024, 640, 0x0040) | Out-Null
Start-Sleep -Milliseconds 1200
Save-Shot $h 'A5_resize_1024x640'

# F11 全屏
[V]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300
[V]::keybd_event(0x7A, 0, 0, [UIntPtr]::Zero)
[V]::keybd_event(0x7A, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 1500
Save-Shot $h 'A6_fullscreen'
[V]::keybd_event(0x7A, 0, 0, [UIntPtr]::Zero)
[V]::keybd_event(0x7A, 0, 2, [UIntPtr]::Zero)
Start-Sleep -Milliseconds 1200
if (-not $p.HasExited) { $p.Kill() }
Start-Sleep -Milliseconds 500

# ---------- 场景 B：错误口令 ----------
Write-Output '=== 场景B：错误口令 ==='
$p2 = Start-Process -FilePath $exe -WorkingDirectory $cpp -PassThru
$h2 = Wait-Window $p2
if ($h2 -ne [IntPtr]::Zero) {
  [V]::SetForegroundWindow($h2) | Out-Null
  Start-Sleep -Milliseconds 1200
  Click-At $h2 640 249
  [System.Windows.Forms.SendKeys]::SendWait('admin')
  Click-At $h2 640 317
  [System.Windows.Forms.SendKeys]::SendWait('wrongpass')
  Click-At $h2 555 385
  [System.Windows.Forms.SendKeys]::SendWait('1234')
  Start-Sleep -Milliseconds 150
  Click-At $h2 640 511
  Start-Sleep -Milliseconds 1500
  Save-Shot $h2 'B1_wrong_password'
}
if (-not $p2.HasExited) { $p2.Kill() }

# ---------- 场景 C：空值校验 ----------
Write-Output '=== 场景C：空值提交 ==='
$p3 = Start-Process -FilePath $exe -WorkingDirectory $cpp -PassThru
$h3 = Wait-Window $p3
if ($h3 -ne [IntPtr]::Zero) {
  [V]::SetForegroundWindow($h3) | Out-Null
  Start-Sleep -Milliseconds 1200
  Click-At $h3 640 511
  Start-Sleep -Milliseconds 900
  Save-Shot $h3 'C1_empty_submit'
}
if (-not $p3.HasExited) { $p3.Kill() }
Write-Output 'VERIFY_DONE'
