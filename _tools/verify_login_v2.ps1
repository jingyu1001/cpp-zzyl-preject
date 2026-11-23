# zzyl 桌面端集成验收脚本 v2：按客户区比例点击，避免坐标写死
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class V2 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
}
"@
$cpp = 'C:\Users\ASUS\Desktop\zzyl\cpp-zzyl-preject'
$exe = Join-Path $cpp 'build\Release\zzyl_desktop.exe'
$out = Join-Path $cpp 'artifacts\_verify_lead2'
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out -Force | Out-Null }

function Get-ClientSize([IntPtr]$h) {
  $c = New-Object V2+RECT
  [V2]::GetClientRect($h, [ref]$c) | Out-Null
  $w = [int]$c.Right - [int]$c.Left
  $hh = [int]$c.Bottom - [int]$c.Top
  return [pscustomobject]@{ W = $w; H = $hh }
}

function Click-Rel([IntPtr]$h, [double]$fx, [double]$fy) {
  $cs = Get-ClientSize $h
  $pt = New-Object V2+POINT
  $pt.X = [int]($cs.W * $fx); $pt.Y = [int]($cs.H * $fy)
  [V2]::ClientToScreen($h, [ref]$pt) | Out-Null
  [V2]::SetCursorPos($pt.X, $pt.Y) | Out-Null
  Start-Sleep -Milliseconds 150
  [V2]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 60
  [V2]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 250
}

function Save-Shot([IntPtr]$h, [string]$name) {
  $r = New-Object V2+RECT
  [V2]::GetWindowRect($h, [ref]$r) | Out-Null
  $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap($w, $hh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
  $path = Join-Path $out "$name.png"
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  $cs = Get-ClientSize $h
  Write-Output "SHOT $name window=${w}x${hh} client=$($cs.W)x$($cs.H) -> $path"
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

function Start-App {
  $p = Start-Process -FilePath $exe -WorkingDirectory $cpp -PassThru
  $h = Wait-Window $p
  if ($h -eq [IntPtr]::Zero) { return @($p, [IntPtr]::Zero) }
  [V2]::ShowWindow($h, 9) | Out-Null      # SW_RESTORE
  [V2]::SetForegroundWindow($h) | Out-Null
  Start-Sleep -Milliseconds 2200
  return @($p, $h)
}

# ---------- A：正确登录 ----------
Write-Output '=== A: admin/admin123/1234 登录 ==='
$r = Start-App
$pa = $r[0]; $ha = $r[1]
if ($ha -eq [IntPtr]::Zero) { Write-Output 'NO_WINDOW_A'; exit 2 }
Start-Sleep -Milliseconds 800
Save-Shot $ha 'A0_login'
Click-Rel $ha 0.500 0.347
[System.Windows.Forms.SendKeys]::SendWait('admin')
Click-Rel $ha 0.500 0.436
[System.Windows.Forms.SendKeys]::SendWait('admin123')
Click-Rel $ha 0.3265 0.531
[System.Windows.Forms.SendKeys]::SendWait('1234')
Start-Sleep -Milliseconds 300
Save-Shot $ha 'A1_filled'
Click-Rel $ha 0.500 0.710
Start-Sleep -Milliseconds 2500
Save-Shot $ha 'A2_after_login'
# 侧栏“服务管理”第 5 项
Click-Rel $ha 0.078 0.714
Start-Sleep -Milliseconds 900
Save-Shot $ha 'A3_sidebar_click'
if (-not $pa.HasExited) { $pa.Kill() }
Start-Sleep -Milliseconds 600

# ---------- B：错误口令 ----------
Write-Output '=== B: 错误口令 ==='
$r = Start-App
$pb = $r[0]; $hb = $r[1]
if ($hb -ne [IntPtr]::Zero) {
  Click-Rel $hb 0.500 0.347
  [System.Windows.Forms.SendKeys]::SendWait('admin')
  Click-Rel $hb 0.500 0.436
  [System.Windows.Forms.SendKeys]::SendWait('wrongpass')
  Click-Rel $hb 0.3265 0.531
  [System.Windows.Forms.SendKeys]::SendWait('1234')
  Click-Rel $hb 0.500 0.710
  Start-Sleep -Milliseconds 2000
  Save-Shot $hb 'B1_wrong_password'
}
if (-not $pb.HasExited) { $pb.Kill() }
Start-Sleep -Milliseconds 600

# ---------- C：空值校验 ----------
Write-Output '=== C: 空值提交 ==='
$r = Start-App
$pc = $r[0]; $hc = $r[1]
if ($hc -ne [IntPtr]::Zero) {
  Click-Rel $hc 0.500 0.710
  Start-Sleep -Milliseconds 1000
  Save-Shot $hc 'C1_empty_submit'
}
if (-not $pc.HasExited) { $pc.Kill() }
Write-Output 'VERIFY2_DONE'
