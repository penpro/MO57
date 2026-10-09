@echo off
rem ============================================================================================================
rem  Tools\ov.bat -- the ONE stable command for unattended work. This file never changes.
rem
rem  Approve/"always allow" `Tools/ov.bat` once and it keeps working. What it runs is Tools\ov_task.sh, which
rem  Claude rewrites for each job. Every run is snapshotted first -- Saved\Logs\ov_history\<stamp>.sh (what ran)
rem  and <stamp>.log (what it printed) -- so the morning review is `dir Saved\Logs\ov_history`, and editing
rem  ov_task.sh while a long run is still going cannot corrupt that run (it executes its own snapshot).
rem
rem  Standing rules the task script follows: only the overnight plan's work; never touch Wes's saves
rem  (Harper_Wright-01, Test158, v1gate); stop only processes it launched, by recorded pid; no force-kill by
rem  name; local/free only.
rem ============================================================================================================
"C:\Program Files\Git\bin\bash.exe" "%~dp0ov_run.sh" %*
exit /b %ERRORLEVEL%
