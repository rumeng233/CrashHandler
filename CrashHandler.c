#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <DbgHelp.h>
#include <Shlwapi.h>
#include <ShlObj.h>
#include <winternl.h>
#include <TlHelp32.h>

typedef struct _THREAD_LAST_SYSCALL_INFORMATION {
	PVOID   FirstArgument;
	USHORT  SystemCallNumber;
	ULONG64 WaitTime;
} THREAD_LAST_SYSCALL_INFORMATION, * PTHREAD_LAST_SYSCALL_INFORMATION;


typedef NTSTATUS(WINAPI* pNtQueryInformationThread)(
	HANDLE ThreadHandle,
	DWORD ThreadInformationClass,
	PVOID ThreadInformation,
	ULONG ThreadInformationLength,
	PULONG ReturnLength
);

typedef NTSTATUS(NTAPI* pRtlGetVersion)(POSVERSIONINFOEXW);

// 有时可能出现主线程退出但进程还在的情况(例如crypt32.dll线程残留), 故使用ExitProcess显式退出
static int Exit(_In_ UINT uExitCode) {
	ExitProcess(uExitCode);
	return uExitCode;
}

static void* __cdecl Memset(void* dest, int ch, size_t count)
{
	__stosb((unsigned char*)dest, (unsigned char)ch, count);
	return dest;
}

_Success_(return)
static BOOL StrToBoolW(_In_ PCWSTR pszString, _Out_ BOOL* piRet) {
	if (lstrcmpiW(pszString, L"true") == 0 ||
		lstrcmpiW(pszString, L"1") == 0 ||
		lstrcmpiW(pszString, L"yes") == 0 ||
		lstrcmpiW(pszString, L"on") == 0) {
		*piRet = TRUE;
		return TRUE;
	}
	else if (lstrcmpiW(pszString, L"false") == 0 ||
		lstrcmpiW(pszString, L"0") == 0 ||
		lstrcmpiW(pszString, L"no") == 0 ||
		lstrcmpiW(pszString, L"off") == 0) {
		*piRet = FALSE;
		return TRUE;
	}

	return FALSE;
}

static UINT DumpProcess(
	_In_ PCWSTR lpDumpFile,
	_In_ DWORD dwDumpType,
	_In_ DWORD dwProcessId, 
	_In_ DWORD dwThreadId, 
	_In_ EXCEPTION_RECORD* pExceptionRecord,
	_In_opt_ CONTEXT* pContextRecord
) {
	if (!pExceptionRecord) {
		return STATUS_ACCESS_VIOLATION;
	}

	WCHAR szDir[MAX_PATH];
	(void)lstrcpynW(szDir, lpDumpFile, MAX_PATH);
	if (PathRemoveFileSpecW(szDir)) {
		SHCreateDirectoryExW(NULL, szDir, NULL);
	}

	UINT uExitCode = STATUS_SUCCESS;
	HANDLE hFile = CreateFileW(
		lpDumpFile,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hFile != INVALID_HANDLE_VALUE) {
		CONTEXT ContextRecord;
		if (!pContextRecord) {
			Memset(&ContextRecord, 0, sizeof(ContextRecord));
			ContextRecord.ContextFlags = CONTEXT_ALL;
			HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, dwThreadId);
			if (hThread) {
				GetThreadContext(hThread, &ContextRecord);
				CloseHandle(hThread);
			}
			pContextRecord = &ContextRecord;
		}

		EXCEPTION_POINTERS ExceptionPointers;
		Memset(&ExceptionPointers, 0, sizeof(ExceptionPointers));
		ExceptionPointers.ExceptionRecord = pExceptionRecord;
		ExceptionPointers.ContextRecord = pContextRecord;

		MINIDUMP_EXCEPTION_INFORMATION ExceptionParam;
		Memset(&ExceptionParam, 0, sizeof(ExceptionParam));
		ExceptionParam.ThreadId = dwThreadId;
		ExceptionParam.ExceptionPointers = &ExceptionPointers;
		ExceptionParam.ClientPointers = FALSE;

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE, FALSE, dwProcessId);
		if (hProcess) {
			if (!MiniDumpWriteDump(
				hProcess,
				dwProcessId,
				hFile,
				dwDumpType,
				&ExceptionParam,
				NULL,
				NULL
			)) {
				uExitCode = STATUS_UNSUCCESSFUL;
			}
			CloseHandle(hProcess);
		}
		else {
			uExitCode = STATUS_ACCESS_DENIED;
		}
		CloseHandle(hFile);
	}
	else {
		uExitCode = STATUS_FILE_INVALID;
	}

	return uExitCode;
}

int WINAPI DebuggerEntryPoint(
	_In_ HINSTANCE hInstance, 
	_In_opt_ HINSTANCE hPrevInstance, 
	_In_ LPWSTR lpCmdLine, 
	_In_ int nCmdShow
) {
	DWORD dwProcessId = 0L;
	WCHAR lpDumpFile[MAX_PATH];
	BOOL bKillOnExit = TRUE;
	BOOL bAbnormalExitDump = FALSE;
	DWORD dwDumpType = MiniDumpNormal |
		MiniDumpWithIndirectlyReferencedMemory |
		MiniDumpWithHandleData |
		MiniDumpWithUnloadedModules |
		MiniDumpWithThreadInfo |
		MiniDumpWithProcessThreadData |
		MiniDumpWithFullMemoryInfo |
		MiniDumpWithTokenInformation |
		MiniDumpWithDataSegs |
		MiniDumpIgnoreInaccessibleMemory;

	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv && argc > 2) {
		if (!StrToIntExW(argv[1], STIF_DEFAULT, (int*)&dwProcessId)) {
			LocalFree(argv);
			return Exit(STATUS_INVALID_PARAMETER_1);
		}
		(void)lstrcpynW(lpDumpFile, argv[2], MAX_PATH);
		if (argc > 3) {
			BOOL bTempKillOnExit = TRUE;
			if (StrToBoolW(argv[3], &bTempKillOnExit)) {
				bKillOnExit = bTempKillOnExit;
			}
		}
		if (argc > 4) {
			BOOL bTempAbnormalExitDump = FALSE;
			if (StrToBoolW(argv[4], &bTempAbnormalExitDump)) {
				bAbnormalExitDump = bTempAbnormalExitDump;
			}
		}
		if (argc > 5) {
			DWORD dwTempDumpType = 0L;
			if (StrToIntExW(argv[5], STIF_DEFAULT, (int*)&dwTempDumpType)) {
				dwDumpType = dwTempDumpType;
			}
		}
		LocalFree(argv);
	} else {
		LocalFree(argv);
		return Exit(STATUS_INVALID_PARAMETER_MIX);
	}

	// 消除游标反馈, 因为子系统为WINDOWS但却未创建任何窗口
	PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
	MSG msg;
	PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE);

	if (!DebugActiveProcess(dwProcessId)) {
		return Exit(STATUS_DEBUG_ATTACH_FAILED);
	}
	DebugSetProcessKillOnExit(bKillOnExit);

	UINT uExitCode = STATUS_SUCCESS;
	DWORD dwLastExceptionThreadId;
	EXCEPTION_RECORD LastExceptionRecord;
	CONTEXT LastContextRecord;
	BOOL bHasLastExceptionRecord = FALSE;
	pNtQueryInformationThread NtQueryInformationThread = NULL;
	USHORT uNtTerminateProcessSyscall = 0;
	BOOL bWindows8 = FALSE;

	if (bAbnormalExitDump) {
		HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
		if (hNtdll) {
			NtQueryInformationThread = (pNtQueryInformationThread)GetProcAddress(hNtdll, "NtQueryInformationThread");
			if (NtQueryInformationThread) {
				// 查找NtTerminateProcess的系统调用号
				// 只有NtQueryInformationThread存在时才查找, 不然没有意义
				FARPROC pFunc = GetProcAddress(hNtdll, "NtTerminateProcess");
				if (!pFunc) {
					pFunc = GetProcAddress(hNtdll, "ZwTerminateProcess"); // NtTerminateProcess的别名
				}
				if (pFunc) {
					BYTE stub[32];
					Memset(&stub, 0, sizeof(stub));
					BYTE* src = (BYTE*)pFunc;
					for (int i = 0; i < sizeof(stub); i++) {
						stub[i] = src[i];
					}

					#ifdef _WIN64
					for (int i = 0; i < sizeof(stub) - 5; i++) {
						if (
							stub[i] == 0x4C &&
							stub[i + 1] == 0x8B &&
							stub[i + 2] == 0xD1 &&
							stub[i + 3] == 0xB8
							) {
							uNtTerminateProcessSyscall = *(USHORT*)(stub + i + 4);
							break;
						}
					}
					#else
					for (int i = 0; i < sizeof(stub) - 4; i++) {
						if (stub[i] == 0xB8) {
							uNtTerminateProcessSyscall = *(USHORT*)(stub + i + 1);
							break;
						}
					}
					#endif
				}
			}

			pRtlGetVersion RtlGetVersion = (pRtlGetVersion)GetProcAddress(hNtdll, "RtlGetVersion");
			if (RtlGetVersion) {
				OSVERSIONINFOEXW VersionInfo;
				Memset(&VersionInfo, 0, sizeof(VersionInfo));
				VersionInfo.dwOSVersionInfoSize = sizeof(VersionInfo);
				if (NT_SUCCESS(RtlGetVersion(&VersionInfo))) {
					bWindows8 = VersionInfo.dwMajorVersion > 6 || (VersionInfo.dwMajorVersion == 6 && VersionInfo.dwMinorVersion >= 2);
				}
			}
		}
	}

	DEBUG_EVENT DebugEvent;
	while (WaitForDebugEvent(&DebugEvent, INFINITE)) {
		switch (DebugEvent.dwDebugEventCode) {
			case EXCEPTION_DEBUG_EVENT:
				if (DebugEvent.u.Exception.dwFirstChance) {
					if (bAbnormalExitDump) {
						dwLastExceptionThreadId = DebugEvent.dwThreadId;

						Memset(&LastExceptionRecord, 0, sizeof(LastExceptionRecord));
						LastExceptionRecord = DebugEvent.u.Exception.ExceptionRecord;

						Memset(&LastContextRecord, 0, sizeof(LastContextRecord));
						LastContextRecord.ContextFlags = CONTEXT_ALL;
						HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, DebugEvent.dwThreadId);
						if (hThread) {
							GetThreadContext(hThread, &LastContextRecord);
							CloseHandle(hThread);
						}

						bHasLastExceptionRecord = TRUE;
					}
				} else {
					uExitCode = DumpProcess(
						lpDumpFile,
						dwDumpType,
						dwProcessId,
						DebugEvent.dwThreadId,
						&DebugEvent.u.Exception.ExceptionRecord,
						NULL
					);
					// 使目标进程的UnhandledExceptionFilter在调试器断开后也有机会执行
					ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
					DebugActiveProcessStop(dwProcessId);
					return Exit(uExitCode);
				}
				ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
				break;

			case CREATE_PROCESS_DEBUG_EVENT:
				if (DebugEvent.u.CreateProcessInfo.hFile) {
					CloseHandle(DebugEvent.u.CreateProcessInfo.hFile);
				}
				ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
				break;

			case LOAD_DLL_DEBUG_EVENT:
				if (DebugEvent.u.LoadDll.hFile) {
					CloseHandle(DebugEvent.u.LoadDll.hFile);
				}
				ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
				break;

			case EXIT_PROCESS_DEBUG_EVENT:
				ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
				DebugActiveProcessStop(dwProcessId);
				return Exit(uExitCode);

			case EXIT_THREAD_DEBUG_EVENT:
				if (bAbnormalExitDump) {
					DWORD dwProcessExitCode = STATUS_SUCCESS;
					// 不使用PROCESS_QUERY_LIMITED_INFORMATION防止老系统不支持
					HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);
					if (hProcess) {
						// 获取进程而不是线程的退出代码
						GetExitCodeProcess(hProcess, &dwProcessExitCode);
						CloseHandle(hProcess);
					}
					if (dwProcessExitCode != STATUS_SUCCESS && dwProcessExitCode != STILL_ACTIVE) {
						if (bHasLastExceptionRecord) {

							BOOL bCalledNtTerminateProcess = FALSE;
							if (NtQueryInformationThread && uNtTerminateProcessSyscall) {
								HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, dwProcessId);
								if (hSnap != INVALID_HANDLE_VALUE) {
									THREADENTRY32 ThreadEntry;
									Memset(&ThreadEntry, 0, sizeof(ThreadEntry));
									ThreadEntry.dwSize = sizeof(ThreadEntry);

									BOOL bRet = Thread32First(hSnap, &ThreadEntry);
									while (bRet) {
										if (ThreadEntry.th32OwnerProcessID == dwProcessId) {
											HANDLE hThread = OpenThread(THREAD_GET_CONTEXT, FALSE, ThreadEntry.th32ThreadID);
											if (hThread) {
												THREAD_LAST_SYSCALL_INFORMATION LastSyscall;
												Memset(&LastSyscall, 0, sizeof(LastSyscall));
												NTSTATUS status = NtQueryInformationThread(
													hThread,
													21, // ThreadLastSystemCall
													&LastSyscall,
													bWindows8 ? sizeof(LastSyscall) : FIELD_OFFSET(THREAD_LAST_SYSCALL_INFORMATION, WaitTime),
													NULL
												);
												if (NT_SUCCESS(status)) {
													// 验证异常线程的最后一次系统调用是否是NtTerminateProcess
													// 如果是则认为是被自身结束(如TerminateProcess、ExitProcess、Environment.Exit等)
													// 否则则认为是被外部结束的, 需要判断退出代码
													bCalledNtTerminateProcess = LastSyscall.SystemCallNumber == uNtTerminateProcessSyscall;
												}
												CloseHandle(hThread);

												if (bCalledNtTerminateProcess) {
													break;
												}
											}
										}
										bRet = Thread32Next(hSnap, &ThreadEntry);
									}

									CloseHandle(hSnap);
								}
							}

							if (
								bCalledNtTerminateProcess ||
								(DebugEvent.u.ExitThread.dwExitCode == LastExceptionRecord.ExceptionCode &&
								DebugEvent.dwThreadId == dwLastExceptionThreadId)
							) {
								uExitCode = DumpProcess(
									lpDumpFile,
									dwDumpType,
									dwProcessId,
									dwLastExceptionThreadId,
									&LastExceptionRecord,
									&LastContextRecord
								);
								ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
								DebugActiveProcessStop(dwProcessId);
								return Exit(uExitCode);
							}
						}
					}
				}
				ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
				break;

			default:
				ContinueDebugEvent(dwProcessId, DebugEvent.dwThreadId, DBG_CONTINUE);
				break;
		}
	}

	DebugActiveProcessStop(dwProcessId);
	return Exit(STATUS_DEBUGGER_INACTIVE);
}