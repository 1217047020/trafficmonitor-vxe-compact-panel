#Requires -Version 7.2
[CmdletBinding()]param()
# Shared installation backup covers both the former bridge and native upgrade.
& (Join-Path $PSScriptRoot 'uninstall_traffic_bridge.ps1')
