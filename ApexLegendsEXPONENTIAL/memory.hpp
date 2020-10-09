#pragma once

#include <Windows.h>
#include <tlhelp32.h>

const wchar_t* symbols = L"\\\\.\\{ED2761FC-91F4-4E1E-A441-19117D9FAC59}";

HANDLE g_device = INVALID_HANDLE_VALUE;

struct KB_READ_PROCESS_MEMORY_IN
{
	UINT64 ProcessId;
	PVOID BaseAddress;
	PVOID Buffer;
	ULONG Size;
};

struct KB_WRITE_PROCESS_MEMORY_IN
{
	UINT64 ProcessId;
	PVOID BaseAddress;
	PVOID Buffer;
	ULONG Size;
	BOOLEAN PerformCopyOnWrite;
};

//Pattern ‰»ÎΩ·ππ
struct KB_FIND_SIGNATURE_IN
{
	UINT64 ProcessId;
	PVOID Memory;
	ULONG Size;
	LPCSTR Signature;
	LPCSTR Mask;
};

struct KB_FIND_SIGNATURE_OUT
{
	PVOID Address;
};

BOOL SendIOCTL(
	IN HANDLE hDevice,
	IN DWORD Ioctl,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	IN PVOID OutputBuffer,
	IN ULONG OutputBufferSize,
	OPTIONAL OUT PDWORD BytesReturned = NULL,
	OPTIONAL IN DWORD Method = 3)
{
	DWORD RawIoctl = CTL_CODE(0x8000, Ioctl, Method, FILE_ANY_ACCESS);
	DWORD Returned = 0;
	BOOL Status = DeviceIoControl(hDevice, RawIoctl, InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize, &Returned, NULL);
	if (BytesReturned) *BytesReturned = Returned;
	return Status;
}

BOOL KbSendRequest(
	int Index,
	IN PVOID Input = NULL,
	ULONG InputSize = 0,
	OUT PVOID Output = NULL,
	ULONG OutputSize = 0)
{
	if (g_device) return SendIOCTL(g_device, 0x800 + Index, Input, InputSize, Output, OutputSize);
	MessageBoxA(0, 0, 0, 0);
	return 0;
}

BOOL KbWriteProcessMemory(
	ULONG ProcessId,
	OUT PVOID BaseAddress,
	IN PVOID Buffer,
	ULONG Size,
	BOOLEAN PerformCopyOnWrite = FALSE)
{
	if (!ProcessId || !BaseAddress || !Buffer || !Size) return FALSE;
	KB_WRITE_PROCESS_MEMORY_IN Input = {};
	Input.ProcessId = ProcessId;
	Input.BaseAddress = BaseAddress;
	Input.Buffer = reinterpret_cast<PVOID>(Buffer);
	Input.Size = Size;
	Input.PerformCopyOnWrite = PerformCopyOnWrite;
	return KbSendRequest(65, &Input, sizeof(Input));
}

BOOL KbReadProcessMemory(
	ULONG ProcessId,
	IN PVOID BaseAddress,
	OUT PVOID Buffer,
	ULONG Size)
{
	if (!ProcessId || !BaseAddress || !Buffer || !Size) return FALSE;
	KB_READ_PROCESS_MEMORY_IN Input = {};
	Input.ProcessId = ProcessId;
	Input.BaseAddress = BaseAddress;
	Input.Buffer = reinterpret_cast<PVOID>(Buffer);
	Input.Size = Size;
	return KbSendRequest(64, &Input, sizeof(Input));
}

BOOL WINAPI KbFindSignature(
	OPTIONAL ULONG ProcessId,
	PVOID Memory,
	ULONG Size,
	LPCSTR Signature,
	LPCSTR Mask,
	OUT PVOID* FoundAddress)
{
	if (!FoundAddress) return FALSE;
	KB_FIND_SIGNATURE_IN Input = {};
	KB_FIND_SIGNATURE_OUT Output = {};
	Input.ProcessId = ProcessId;
	Input.Memory = Memory;
	Input.Size = Size;
	Input.Signature = reinterpret_cast<LPCSTR>(Signature);
	Input.Mask = reinterpret_cast<LPCSTR>(Mask);
	BOOL Status = KbSendRequest(94, &Input, sizeof(Input), &Output, sizeof(Output));
	*FoundAddress = Output.Address;
	return Status;
}

template<class T>
T read(DWORD32 process_id, DWORD64 addr)
{
	T result{};
	int size = sizeof(T);

	KbReadProcessMemory((ULONG)process_id, (PVOID)addr, (PVOID)&result, (ULONG)size);
	return result;
}

template<class T>
void write(DWORD32 process_id, DWORD64 addr, T buf)
{
	int size = sizeof(T);
	KbWriteProcessMemory((ULONG)process_id, (PVOID)addr, (PVOID)&buf, (ULONG)size, FALSE);
}

std::vector<DWORD64> find_pattern(DWORD32 prcess_id, LPCSTR sig, LPCSTR mask, INT size, PVOID beg, PVOID stop)
{
	std::vector<DWORD64> res;

	PVOID cur = beg;
	while (true)
	{
		if (cur < stop)
		{
			if (KbFindSignature(prcess_id, cur, size, sig, mask, &beg))  res.push_back((DWORD64)cur);
			else break;
		}
		else break;
	}

	return res;
}

HANDLE open_device(LPCWSTR NativeDeviceName = symbols)
{
	g_device = CreateFileW(NativeDeviceName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	return g_device;
}

int get_process_id(const char* process)
{
	HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Snap == INVALID_HANDLE_VALUE) return false;

	PROCESSENTRY32 ProcessInfo{ 0 };
	ProcessInfo.dwSize = sizeof(ProcessInfo);

	if (Process32First(Snap, &ProcessInfo))
	{
		do
		{

		} while (Process32Next(Snap, &ProcessInfo));
	}

	CloseHandle(Snap);
	return 0;
}

MODULEENTRY32 find_module(const char* name, int pid)
{
	MODULEENTRY32 Result{ 0 };
	Result.dwSize = sizeof(Result);

	HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (Snap == INVALID_HANDLE_VALUE)
	{
		int code = GetLastError();
		return Result;
	}

	if (Module32First(Snap, &Result))
	{
		do
		{

		} while (Module32Next(Snap, &Result));
	}

	CloseHandle(Snap);
	return {};
}