<#
.SYNOPSIS
    Registers Tools\Build\Nightly.ps1 as a daily Windows scheduled task for the current user.

.DESCRIPTION
    The task runs only while you are logged in, because the perf route renders to a window.
    Leave the PC on (or asleep: the task can wake it) and logged in overnight.
    Remove it with:  Unregister-ScheduledTask -TaskName "WebOfTheCity Nightly"

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Tools\Build\RegisterNightlyTask.ps1 -Time 03:00
#>
param(
    [string]$Time = "03:00",
    [string]$TaskName = "WebOfTheCity Nightly"
)

$ErrorActionPreference = "Stop"

$script = (Resolve-Path (Join-Path $PSScriptRoot "Nightly.ps1")).Path
$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$script`" -Pull"
$trigger = New-ScheduledTaskTrigger -Daily -At $Time
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -WakeToRun -ExecutionTimeLimit (New-TimeSpan -Hours 6)

Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Settings $settings `
    -Description "Web of the City: package a Development build, run the perf route, check Section 4.3 budgets." -Force | Out-Null

Write-Host "Registered '$TaskName' to run daily at $Time. Results go to $env:USERPROFILE\WebOfTheCityBuilds."
