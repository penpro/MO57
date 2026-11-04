# DARKTOWER: run wangp_images.py on a job file as a one-shot scheduled task (outlives the SSH session).
# Usage: powershell -File run_job.ps1 -Job C:\Users\Wes\mo57_crops\ab1.json -Out D:\WanGP\outputs\mo57_ab1 -Name MO57_ab1
param([string]$Job, [string]$Out, [string]$Name)
$py = 'D:\WanGP\env_venv\Scripts\python.exe'
$script = 'C:\Users\Wes\mo57_crops\wangp_images.py'
$log = Join-Path 'C:\Users\Wes\mo57_crops' ($Name + '.log')
$cmd = "/c set PYTHONIOENCODING=utf-8 && cd /d D:\WanGP && `"$py`" `"$script`" `"$Job`" `"$Out`" > `"$log`" 2>&1"
$action = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument $cmd -WorkingDirectory 'D:\WanGP'
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 6) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
Unregister-ScheduledTask -TaskName $Name -Confirm:$false -ErrorAction SilentlyContinue
Register-ScheduledTask -TaskName $Name -Action $action -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName $Name
Start-Sleep -Seconds 3
(Get-ScheduledTask -TaskName $Name).State
