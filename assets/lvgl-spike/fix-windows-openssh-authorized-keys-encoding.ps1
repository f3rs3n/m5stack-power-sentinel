# Run in an elevated PowerShell window on the Windows PC.
# Purpose: fix Hermes public-key login for Windows OpenSSH.
#
# Why this exists:
# - Windows PowerShell 5.1 Add-Content can create authorized_keys as UTF-16LE.
#   OpenSSH cannot parse that as an authorized_keys file.
# - For Windows users that are local administrators, the default sshd_config usually
#   ignores C:\Users\<user>\.ssh\authorized_keys and instead uses:
#     C:\ProgramData\ssh\administrators_authorized_keys
#   via the "Match Group administrators" block.
# - This script therefore fixes the administrator key file first and does not try
#   to rewrite the user authorized_keys file, avoiding Access Denied on files with
#   awkward ACLs.

$ErrorActionPreference = "Stop"

$HermesPublicKey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIAblSTZAg1h+BdXXJFrdOChpI2J+OTIAbDbQkaNW7kHn hermes@windows-dev-lvgl"
$HermesLxcIp = "192.168.2.101"

function Assert-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run this script from an elevated PowerShell window: right-click PowerShell -> Run as administrator."
    }
}

Assert-Admin

Write-Host "== Ensuring OpenSSH Server is installed/running =="
$server = Get-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
if ($server.State -ne "Installed") {
    Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
}
Set-Service -Name sshd -StartupType Automatic
Start-Service -Name sshd -ErrorAction SilentlyContinue

Write-Host "== Writing administrator authorized_keys as ASCII =="
$programDataSsh = Join-Path $env:ProgramData "ssh"
$adminKeys = Join-Path $programDataSsh "administrators_authorized_keys"
New-Item -ItemType Directory -Force -Path $programDataSsh | Out-Null

# Keep only non-empty non-duplicate existing lines, then append Hermes key.
# Read errors are non-fatal because the old file may have bad encoding/ACL.
$lines = @()
if (Test-Path $adminKeys) {
    try {
        $existing = Get-Content -Path $adminKeys -ErrorAction SilentlyContinue
        foreach ($line in $existing) {
            if ($line -and $line.Trim() -and $line -notlike "*$HermesPublicKey*") {
                $lines += $line.Trim()
            }
        }
    } catch {
        Write-Host "Could not read existing admin authorized_keys; replacing with Hermes key."
    }
}
$lines += $HermesPublicKey
$text = ($lines -join "`n") + "`n"
[System.IO.File]::WriteAllText($adminKeys, $text, [System.Text.Encoding]::ASCII)

Write-Host "== Fixing administrator authorized_keys ACL =="
# Strict ACL required by Windows OpenSSH. Use SID for Administrators to avoid localization issues.
icacls $adminKeys /inheritance:r | Out-Null
icacls $adminKeys /grant "*S-1-5-32-544:F" /grant "SYSTEM:F" | Out-Null

Write-Host "== Ensuring firewall rule is restricted to Hermes LXC =="
$ruleName = "OpenSSH-Server-In-TCP-Hermes-LXC"
$oldDefaultRule = Get-NetFirewallRule -Name "OpenSSH-Server-In-TCP" -ErrorAction SilentlyContinue
if ($oldDefaultRule) {
    Disable-NetFirewallRule -Name "OpenSSH-Server-In-TCP" | Out-Null
}

# PowerShell/NetSecurity versions differ: Set-NetFirewallAddressFilter may not
# support -AssociatedNetFirewallRule on this Windows install. The most robust
# path is to remove and recreate our dedicated restricted rule every time.
$existingRule = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
if ($existingRule) {
    Remove-NetFirewallRule -Name $ruleName
}
New-NetFirewallRule `
    -Name $ruleName `
    -DisplayName "OpenSSH Server for Hermes LXC only" `
    -Enabled True `
    -Direction Inbound `
    -Protocol TCP `
    -LocalPort 22 `
    -Action Allow `
    -Profile Private,Domain `
    -RemoteAddress $HermesLxcIp | Out-Null

Write-Host "== Ensuring PowerShell is the default SSH shell =="
New-Item -Path "HKLM:\SOFTWARE\OpenSSH" -Force | Out-Null
New-ItemProperty `
    -Path "HKLM:\SOFTWARE\OpenSSH" `
    -Name DefaultShell `
    -Value "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
    -PropertyType String `
    -Force | Out-Null

Write-Host "== Restarting sshd =="
Restart-Service sshd

Write-Host ""
Write-Host "== sshd_config relevant lines =="
$sshdConfig = Join-Path $programDataSsh "sshd_config"
if (Test-Path $sshdConfig) {
    Get-Content $sshdConfig | Select-String -Pattern "PubkeyAuthentication|AuthorizedKeysFile|Match Group administrators" -Context 0,2
} else {
    Write-Host "sshd_config not found at $sshdConfig"
}

Write-Host ""
Write-Host "== admin authorized_keys content =="
Get-Content $adminKeys

Write-Host ""
Write-Host "== status =="
Get-Service sshd
Write-Host ""
Write-Host "Windows user: $env:USERNAME"
Write-Host "Hostname: $env:COMPUTERNAME"
Write-Host "IPv4 addresses:"
Get-NetIPAddress -AddressFamily IPv4 |
    Where-Object { $_.IPAddress -notlike '169.254*' -and $_.IPAddress -ne '127.0.0.1' } |
    Select-Object InterfaceAlias,IPAddress,PrefixLength |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Done. Tell Hermes to retry SSH."
