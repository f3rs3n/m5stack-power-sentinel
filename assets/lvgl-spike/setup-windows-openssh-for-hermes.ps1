# Run in an elevated PowerShell window on the Windows PC.
# Purpose: enable OpenSSH Server for Hermes LXC access, restricted to Hermes LXC IP.

param(
    [string]$HermesLxcIp = "192.168.2.101",
    [string]$HermesPublicKey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIAblSTZAg1h+BdXXJFrdOChpI2J+OTIAbDbQkaNW7kHn hermes@windows-dev-lvgl"
)

$ErrorActionPreference = "Stop"

function Assert-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run this script from an elevated PowerShell window: right-click PowerShell -> Run as administrator."
    }
}

Assert-Admin

Write-Host "== Installing/enabling OpenSSH Server =="
$server = Get-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
if ($server.State -ne "Installed") {
    Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0
} else {
    Write-Host "OpenSSH Server already installed."
}

Set-Service -Name sshd -StartupType Automatic
Start-Service -Name sshd

Write-Host "== Restricting Windows firewall SSH inbound rule to Hermes LXC $HermesLxcIp =="
$ruleName = "OpenSSH-Server-In-TCP-Hermes-LXC"
$oldDefaultRule = Get-NetFirewallRule -Name "OpenSSH-Server-In-TCP" -ErrorAction SilentlyContinue
if ($oldDefaultRule) {
    Disable-NetFirewallRule -Name "OpenSSH-Server-In-TCP" | Out-Null
}

$existing = Get-NetFirewallRule -Name $ruleName -ErrorAction SilentlyContinue
if ($existing) {
    Set-NetFirewallRule -Name $ruleName -Enabled True -Direction Inbound -Action Allow -Profile Private,Domain
    Set-NetFirewallAddressFilter -AssociatedNetFirewallRule $existing -RemoteAddress $HermesLxcIp
    Set-NetFirewallPortFilter -AssociatedNetFirewallRule $existing -Protocol TCP -LocalPort 22
} else {
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
}

Write-Host "== Installing Hermes public key =="
# For administrator accounts, Windows OpenSSH's default sshd_config uses:
# Match Group administrators
#   AuthorizedKeysFile __PROGRAMDATA__/ssh/administrators_authorized_keys
# Put the key there and also in the current user's authorized_keys for non-admin/future use.
$programDataSsh = "$env:ProgramData\ssh"
New-Item -ItemType Directory -Force -Path $programDataSsh | Out-Null
$adminKeys = Join-Path $programDataSsh "administrators_authorized_keys"
if (-not (Test-Path $adminKeys)) { New-Item -ItemType File -Path $adminKeys | Out-Null }
$adminContent = Get-Content $adminKeys -Raw -ErrorAction SilentlyContinue
if ($adminContent -notlike "*$HermesPublicKey*") {
    Add-Content -Path $adminKeys -Value $HermesPublicKey
}
# Strict ACL required by Windows OpenSSH.
icacls $adminKeys /inheritance:r | Out-Null
icacls $adminKeys /grant "Administrators:F" /grant "SYSTEM:F" | Out-Null

$userSsh = Join-Path $env:USERPROFILE ".ssh"
New-Item -ItemType Directory -Force -Path $userSsh | Out-Null
$userKeys = Join-Path $userSsh "authorized_keys"
if (-not (Test-Path $userKeys)) { New-Item -ItemType File -Path $userKeys | Out-Null }
$userContent = Get-Content $userKeys -Raw -ErrorAction SilentlyContinue
if ($userContent -notlike "*$HermesPublicKey*") {
    Add-Content -Path $userKeys -Value $HermesPublicKey
}

Write-Host "== Setting PowerShell as default OpenSSH shell =="
New-ItemProperty `
    -Path "HKLM:\SOFTWARE\OpenSSH" `
    -Name DefaultShell `
    -Value "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
    -PropertyType String `
    -Force | Out-Null

Restart-Service sshd

Write-Host "== Status =="
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
Write-Host "From Hermes LXC, test with:"
Write-Host "ssh -i /home/martino/.ssh/windows_dev_ed25519 -o IdentitiesOnly=yes $env:USERNAME@<WINDOWS_IP> powershell -NoProfile -Command \"`$PSVersionTable.PSVersion; hostname; whoami\""
