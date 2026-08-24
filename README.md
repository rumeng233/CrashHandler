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

## `AbnormalExitDump`
开启`AbnormalExitDump`后, 当目标进程抛出异常时, 会立即记录下当前线程的TID、异常信息、线程上下文 _(该操作早于目标进程中任何异常处理程序的执行)_  
若目标进程捕获该异常且准备退出时 _(`EXIT_THREAD_DEBUG_EVENT`, 当调用`NtTerminateProcess`时操作系统会先向调试器发送`EXIT_THREAD_DEBUG_EVENT`, 所有线程都退出后才会发送`EXIT_PROCESS_DEBUG_EVENT`, 但由于`EXIT_PROCESS_DEBUG_EVENT`触发时间太晚, 线程已被销毁, 故选择使用`EXIT_THREAD_DEBUG_EVENT`)_  
如果进程退出代码不等于`STATUS_SUCCESS`且进程中有任意一个线程的最后系统调用是`NtTerminateProcess`时, 则生成转储并退出

## 注意事项
如需自包含dbghelp.dll, 可将dbghelp.dll与CrashHandler.exe放在同一目录下, 程序会自动使用当前目录下的dbghelp.dll, 详见[DbgHelp 版本](https://learn.microsoft.com/windows/win32/debug/dbghelp-versions)

`AbnormalExitDump`参数仅在Windows 10及以上版本中测试有效, Windows 10以下的版本不保证其生效  
碍于`AbnormalExitDump`的实现方式, 开启后可能会导致误报、漏报等情况, 且会导致目标进程性能降低(降低程度待测试), 请酌情开启
