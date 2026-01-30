# MOFramework Project Memory

## Development Environment
- IDE: Rider for C++
- Engine: Unreal Engine 5.7
- Engine Install Path: `D:\UnrealEngine\UE_5.7`
- Engine User Data: `C:\Users\penum\AppData\Local\UnrealEngine\5.7`
- Build Tool Logs: `C:\Users\penum\AppData\Local\UnrealBuildTool`

## Workflow Rules
- **After every successful compile**: Run `git add -A && git commit -m "checkpoint" && git push` to enable rollback if needed
- User will confirm compile success before git operations

## Research Guidelines
- Check Unreal Engine best practices and official documentation for all new code
- Skip web research if scaffolding is already set up and we're making small changes to existing patterns

## Code Conventions
- Always call `RemoveAll(this)` or `RemoveDynamic` before binding delegates in `NativeConstruct()` to prevent duplicate bindings
- UI widgets use CommonUI (`UCommonActivatableWidget`, `UCommonButtonBase`)
- Use Warning log level for important flow events, Log for routine events

## Architecture
- Plugin location: `Plugins/MOFramework/`
- Delegate chain for menus: Panel -> InGameMenu -> UIManager -> Subsystem
- Target names are case-sensitive: `MO57Editor`, `MO57` (not `mo57`)

## CLI Commands

### Build Commands (PowerShell)
```powershell
# Build Editor (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57Editor Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# Build Game (Development)
powershell.exe -Command "& 'D:\UnrealEngine\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe' MO57 Win64 Development '-Project=D:\ueprojects\mo57\mo57.uproject'"

# View build logs
powershell.exe -Command "Get-Content 'C:\Users\penum\AppData\Local\UnrealBuildTool\Log.txt' -Tail 50"
```

### File Operations (PowerShell)
```powershell
# List directory contents
powershell.exe -Command "Get-ChildItem 'D:\ueprojects\mo57' -Directory"

# Find files recursively
powershell.exe -Command "Get-ChildItem 'D:\ueprojects\mo57' -Recurse -Filter '*.cpp'"
```

### Git Operations
```bash
git status
git add -A && git commit -m "message" && git push
git log --oneline -10
```
