# CrashHandler

[![License: LGPL v3](https://img.shields.io/badge/License-LGPL%20v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
[![GitHub Release](https://img.shields.io/github/v/release/rumeng233/CrashHandler)](https://github.com/rumeng233/CrashHandler/releases)

一个轻量级Windows崩溃捕获工具，通过**附加调试器**在目标进程发生未处理异常或异常退出(需开启`AbnormalExitDump`)时自动生成转储

## 参数格式
```cmd
CrashHandler.exe <ProcessId> <DumpFilePath> [KillOnExit] [AbnormalExitDump] [DumpType]
```

|参数              |类型    |必填|默认值   |说明                                                                                                                     |
|------------------|-------|----|---------|------------------------------------------------------------------------------------------------------------------------|
|`ProcessId`       |`DWORD`|是  |无       |要附加的目标进程PID                                                                                                       |
|`DumpFilePath`    |`WCHAR`|是  |无       |转储文件的保存路径(目录不存在时自动创建)                                                                                    |
|`KillOnExit`      |`BOOL` |否  |`TRUE`   |调试器退出时是否终止目标进程(`true/1/yes/on`或`false/0/no/off`)                                                            |
|`AbnormalExitDump`|`BOOL` |否  |`FALSE`  |启用异常退出转储功能(格式同上)                                                                                             |
|`DumpType`        |`DWORD`|否  |见下方说明|[MINIDUMP_TYPE](https://learn.microsoft.com/windows/win32/api/minidumpapiset/ne-minidumpapiset-minidump_type)枚举标志组合|

**DumpType默认值** : 
```
MiniDumpNormal |
MiniDumpWithIndirectlyReferencedMemory |
MiniDumpWithHandleData |
MiniDumpWithUnloadedModules |
MiniDumpWithThreadInfo |
MiniDumpWithProcessThreadData |
MiniDumpWithFullMemoryInfo |
MiniDumpWithTokenInformation |
MiniDumpWithDataSegs |
MiniDumpIgnoreInaccessibleMemory
```

## 使用示例
```cmd
附加到PID 1234, 自身退出时终止目标进程
CrashHandler.exe 1234 C:\dumps\crash.dmp

启用异常退出转储, 并在自身退出后不终止目标进程
CrashHandler.exe 1234 C:\dumps\crash.dmp false true

自定义DumpType(仅包含MiniDumpNormal)
CrashHandler.exe 1234 C:\dumps\crash.dmp true false 0x00000000
```

## 注意事项
如需自包含dbghelp.dll, 可将dbghelp.dll与CrashHandler.exe放在同一目录下, 程序会自动使用当前目录下的dbghelp.dll, 详见[DbgHelp 版本](https://learn.microsoft.com/windows/win32/debug/dbghelp-versions)

`AbnormalExitDump`参数仅在Windows 10及以上版本中测试有效, Windows 10以下的版本不保证其生效  
碍于`AbnormalExitDump`的实现方式, 开启后可能会导致误报、漏报等情况, 请酌情开启
