/*
用户层连接IOCTL
*/

#include <iostream>
#include <thread>

#include <Windows.h>
#include <TlHelp32.h>

/* 读取 */
#define IOCTL_READ CTL_CODE(FILE_DEVICE_UNKNOWN, 0x999, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

/* 写入 */
#define IOCTL_WRITE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x998, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

/* 数据结构 */
struct DataStruct
{
	HANDLE PID;			//进程ID
	PVOID64 Addr;		//地址
	PVOID64 Result;		//结果
	SIZE_T Size;				//大小
};

class IOCTL
{
private:
	DWORD m_PID;

public:
	IOCTL() {}
	~IOCTL() {}

	bool attach(const char* process)
	{
		HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (Snap == INVALID_HANDLE_VALUE) return false;

		PROCESSENTRY32 ProcessInfo{ 0 };
		ProcessInfo.dwSize = sizeof(ProcessInfo);

		if (Process32First(Snap, &ProcessInfo))
		{
			do
			{
				if (strcmp(process, ProcessInfo.szExeFile) == 0)
				{
					CloseHandle(Snap);
					m_PID = ProcessInfo.th32ProcessID;
					return true;
				}
			} while (Process32Next(Snap, &ProcessInfo));
		}

		CloseHandle(Snap);
		return false;
	}

	MODULEENTRY32 find_module(const char* name)
	{
		MODULEENTRY32 Result{ 0 };
		Result.dwSize = sizeof(Result);

		HANDLE Snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_PID);
		if (Snap == INVALID_HANDLE_VALUE) return Result;

		if (Module32First(Snap, &Result))
		{
			do
			{
				if (strcmp(name, Result.szModule) == 0)
				{
					CloseHandle(Snap);
					return Result;
				}
			} while (Module32Next(Snap, &Result));
		}

		CloseHandle(Snap);
		return {};
	}

	HANDLE connect(const char* name)
	{
		return CreateFile(name, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	}

	template<class T>
	T read(uintptr_t addr)
	{
		T Result;

		DataStruct Data{ 0 };
		Data.PID = (HANDLE)m_PID;
		Data.Addr = (PVOID64)addr;
		Data.Result = &Result;
		Data.Size = sizeof(Result);

		HANDLE Device = connect("\\\\.\\Driver_IOCTL");
		if (Device != INVALID_HANDLE_VALUE)
		{
			DWORD Tips = 0;
			DeviceIoControl(Device, IOCTL_READ, &Data, sizeof(Data), &Result, sizeof(Result), &Tips, NULL);
			CloseHandle(Device);
		}
		else std::cout << "Error: " << GetLastError() << std::endl;

		return Result;
	}

	template<class T>
	void write(uintptr_t addr, T Buf)
	{
		DataStruct Data{ 0 };
		Data.PID = (HANDLE)m_PID;
		Data.Addr = (PVOID64)addr;
		Data.Result = &Buf;
		Data.Size = sizeof(Buf);

		HANDLE Device = connect("\\\\.\\Driver_IOCTL");
		if (Device != INVALID_HANDLE_VALUE)
		{
			DWORD Tips = 0;
			DeviceIoControl(Device, IOCTL_WRITE, &Data, sizeof(Data), &Buf, sizeof(Buf), &Tips, NULL);
			CloseHandle(Device);
		}
		else std::cout << "Error: " << GetLastError() << std::endl;
	}
};

void test_read()
{
	IOCTL* g = new IOCTL();
	if (g->attach("cmd.exe"))
	{
		MODULEENTRY32 mod = g->find_module("cmd.exe");
		DWORD addr = (DWORD)mod.modBaseAddr;

		for (int i = 0; i < 1000000; i++) g->read<int>(addr + 0x1C000);
	}
}

void test_write()
{
	IOCTL* g = new IOCTL();
	if (g->attach("cmd.exe"))
	{
		MODULEENTRY32 mod = g->find_module("cmd.exe");
		DWORD addr = (DWORD)mod.modBaseAddr;

		for (int i = 0; i < 1000000; i++) g->write<int>(addr + 0x1C000, 200);
	}
}

int main(int argc, char* argv[])
{
	IOCTL* g = new IOCTL();
	if (g->attach("cmd.exe"))
	{
		MODULEENTRY32 mod = g->find_module("cmd.exe");
		uintptr_t addr = (uintptr_t)mod.modBaseAddr;

		if (addr)
		{
			std::cout << "[+] Base: " << std::hex << addr << std::dec << std::endl;

			int res = g->read<int>(addr + 0x1C000);
			std::cout << "[+] Entry: " << res << std::endl;

			std::cout << "[+] Sucess" << std::endl;
			g->write<int>(addr + 0x1C000, 456);

			res = g->read<int>(addr + 0x1C000);
			std::cout << "[*] Read bytes: " << res << std::endl;
		}
	}
	else std::cout << "[+] Injected sucessfully" << std::endl;
	Sleep(234234234);
	return 0;
}