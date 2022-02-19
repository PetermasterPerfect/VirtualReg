#pragma once
#include <ntddk.h>
#include "List.hpp"
#define MAX_KEY_PATH_LENGTH 0x100


struct RegistryKeyValue
{
	ULONG Type;
	void* Buffer;
	ULONG Length;

	bool Init(KEY_VALUE_FULL_INFORMATION* Info)
	{
		Type = Info->Type;
		Buffer = ExAllocatePool(PagedPool, Info->DataLength);
		if (Buffer == nullptr)
			return false;
		memmove(Buffer, (void*)((ULONG)Info + Info->DataOffset), Info->DataLength);
		Length = Info->DataLength;
		return true;
	}
};

void CleanRegistryKeyValueStruct(RegistryKeyValue*);

struct FastMutex {
public:

	void Initialize() {
		ExInitializeFastMutex(&_mutex);
	}

	void Release() {
		ExReleaseFastMutex(&_mutex);
	}

	void Acquire() {
		ExAcquireFastMutex(&_mutex);
	}

private:
	FAST_MUTEX _mutex;
};

struct TrackedProcess
{
	ULONG Pid;
	List<WCHAR*> KeysTrackingList;

	bool IsKeyPathPresent(WCHAR* Path)
	{
		Node<WCHAR*>* buf = KeysTrackingList.head;
		while (buf != nullptr)
		{
			if (!wcsncmp(buf->value, Path, wcsnlen(Path, MAX_KEY_PATH_LENGTH)))
				return true;
			buf = buf->next;
		}
		return false;
	}

	int LongestKeyLength(size_t& LeadingLength)
	{
		Node<WCHAR*>* buf = KeysTrackingList.head;
		int ret = -1, i = 1;
		size_t leadingLengthBuf;
		LeadingLength = 0;

		while (buf != nullptr)
		{
			leadingLengthBuf = wcsnlen(buf->value, MAX_KEY_PATH_LENGTH);
			if (LeadingLength < leadingLengthBuf)
			{
				ret = i;
				LeadingLength = leadingLengthBuf;
			}
			i++;
			buf = buf->next;
		}

		return ret;
	}

	void FreePaths()
	{
		Node<WCHAR*>* buf = KeysTrackingList.head;
		while (buf != nullptr)
		{
			ExFreePool(buf->value);
			buf = buf->next;
		}
	}
};

struct Global
{
	List<TrackedProcess> ProcessTrackingList;
	FastMutex Mutex;
	LARGE_INTEGER Cookie;

	void Init()
	{
		Mutex.Initialize();
	}
	void Add(ULONG Pid)
	{
		Mutex.Acquire();
		TrackedProcess buf;
		memset(&buf, 0, sizeof(TrackedProcess));
		buf.Pid = Pid;
		ProcessTrackingList.Add(buf, PagedPool);
		Mutex.Release();
	}
	void Clear()
	{
		Mutex.Acquire();
		CallbackHellClear();
		Mutex.Release();
	}

	void CallbackHellClear()
	{
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		while (buf != nullptr)
		{
			buf->value.FreePaths();
			buf->value.KeysTrackingList.ClearAll();
			buf = buf->next;
		}
		ProcessTrackingList.ClearAll();
	}

	void AddKeyPath(ULONG Pid, WCHAR* Str)
	{
		Mutex.Acquire();
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		while (buf != nullptr)
		{
			if (buf->value.Pid == Pid)
			{
				buf->value.KeysTrackingList.Add(Str, PagedPool);
				break;
			}
			buf = buf->next;
		}
		Mutex.Release();
	}

	WCHAR* GetKeyPath(ULONG Pid, int Idx)
	{
		Mutex.Acquire();
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		while (buf != nullptr)
		{
			if (Pid == buf->value.Pid)
			{
				Mutex.Release();
				return buf->value.KeysTrackingList.Get(Idx);
			}
			buf = buf->next;
		}
		Mutex.Release();
		return nullptr;
	}

	int LongestKeyIndex(ULONG& Pid)
	{
		Mutex.Acquire();
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		size_t leadingLengthBuf, leadingLength = 0;
		int retIdx = -1;

		while (buf != nullptr)
		{
			int idx = buf->value.LongestKeyLength(leadingLengthBuf);
			if (leadingLength < leadingLengthBuf)
			{
				retIdx = idx;
				leadingLength = leadingLengthBuf;
				Pid = Pid != buf->value.Pid ?
					buf->value.Pid : Pid;
			}
			buf = buf->next;
		}
		Mutex.Release();
		return retIdx;
	}

	bool RemoveKeyByIndex(ULONG Pid, int Idx)
	{
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		Mutex.Acquire();

		while (buf != nullptr)
		{
			if (Pid == buf->value.Pid)
			{
				Mutex.Release();
				return buf->value.KeysTrackingList.Remove(Idx);
			}
			buf = buf->next;
		}
		Mutex.Release();
		return false;
	}

	bool IsPidPresent(ULONG Pid)
	{
		Mutex.Acquire();
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		while (buf != nullptr)
		{
			if (buf->value.Pid == Pid)
			{
				Mutex.Release();
				return true;
			}
			buf = buf->next;
		}
		Mutex.Release();
		return false;
	}

	bool IsKeyPathPresent(WCHAR* Path)
	{
		Mutex.Acquire();
		Node<TrackedProcess>* buf = ProcessTrackingList.head;
		while (buf != nullptr)
		{
			if (buf->value.IsKeyPathPresent(Path))
			{
				Mutex.Release();
				return true;
			}
			buf = buf->next;
		}
		Mutex.Release();
		return false;
	}
};