#include "Header.h"

PVOID WrapCreatingVirtualKeyProcess(WCHAR* Ending, ULONG Pid)
{
	size_t len = wcsnlen(Ending, 255 - wcslen(LOCAL_KEY_ROOT));
	PVOID ret = nullptr;
	WCHAR keyBuf[MAX_KEY_PATH_LENGTH];
	ret = CreateVirtualKey(L"", Pid);
	if (ret == nullptr)
		return ret;

	for (size_t i = 0; i < len; i++)
	{
		if (Ending[i] == L'\\')
		{
			memset(keyBuf, 0, sizeof(WCHAR) * (i+1));
			memmove(keyBuf, Ending, sizeof(WCHAR) * i);
			ret = CreateVirtualKey(keyBuf, Pid);
			if (ret == nullptr)
				return ret;
		}
	}
	ret = CreateVirtualKey(Ending, Pid);
	return ret;
}

PVOID WrapRestoringKeyProcess(WCHAR* Str)
{
	size_t len = wcsnlen(Str, MAX_KEY_PATH_LENGTH);
	if (wcslen(LOCAL_KEY_ROOT) >= len)
		return nullptr;
	WCHAR *buf = (WCHAR*)ExAllocatePool(PagedPool, sizeof(WCHAR)*(len+1));
	if (buf == nullptr)
		ExFreePool(buf);
	PVOID ret;

	for (size_t i = 1; i < len; i++)
	{
		if (Str[i] == L'\\')
		{
			memset(buf, 0, sizeof(WCHAR) * (i + 1));
			memmove(buf, Str, sizeof(WCHAR) * i);
			ret = CreateKey(buf);
			if (ret == nullptr)
				goto EXIT;
		}
	}

	ret = CreateKey(Str);
EXIT:
	ExFreePool(buf);
	return ret;
}

int CountSlashes(WCHAR *str)
{
	int len = wcsnlen(str, 255 - wcslen(LOCAL_KEY_ROOT));
	int count = 0;
	for (int i = 0; i < len; i++)
		str[i] == L'\\' ? count++ : count;
	return count;
}

bool CreateBaseVirtualKey()
{
	HANDLE hKey = OpenRegistryKeyHandle(BASE_VIRTUAL_KEY);
	if (hKey == NULL)
		return false;
	ZwClose(hKey);
	return true;
}

PVOID CreateKey(WCHAR* KeyPath)
{
	PVOID keyObject;
	HANDLE hKey;
	NTSTATUS status;


	hKey = OpenRegistryKeyHandle(KeyPath);
	if (hKey == NULL)
		return nullptr;

	status = ObReferenceObjectByHandle(hKey,
		KEY_SET_VALUE,
		*CmKeyObjectType,
		KernelMode,
		&keyObject,
		NULL);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("ObReferenceObjectByHandle in CreateVirtualKey FAILED %x\n", status));
		ZwClose(hKey);
		return nullptr;
	}

	ZwClose(hKey);
	return keyObject;
}

PVOID CreateVirtualKey(WCHAR *Ending, ULONG Pid)
{
	PVOID keyObject;
	HANDLE hKey;
	NTSTATUS status;
	WCHAR* keyName = CreateKeyPathWithPid(Ending, Pid);
	if (keyName == nullptr)
		return nullptr;

	hKey = OpenRegistryKeyHandle(keyName);
	if (hKey == NULL)
		return nullptr;

	status = ObReferenceObjectByHandle(hKey,
		KEY_SET_VALUE,
		*CmKeyObjectType,
		KernelMode,
		&keyObject,
		NULL);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("ObReferenceObjectByHandle in CreateVirtualKey FAILED %x\n", status));
		ExFreePool(keyName);
		ZwClose(hKey);
		return nullptr;
	}

	ZwClose(hKey);
	ExFreePool(keyName);
	return keyObject;
}

HANDLE OpenRegistryKeyHandle(WCHAR *KeyName)
{
	HANDLE hKey;
	UNICODE_STRING keyPath;
	OBJECT_ATTRIBUTES keyAttributes;
	NTSTATUS status;
	RtlInitUnicodeString(&keyPath, KeyName);
	InitializeObjectAttributes(&keyAttributes,
		&keyPath,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	status = ZwCreateKey(&hKey, KEY_ALL_ACCESS, &keyAttributes, 0, NULL, 0, NULL);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("ZwCreateKey in CreateVirtualKey failed: %x\n", status));
		return NULL;
	}
	return hKey;
}

WCHAR* EnumKeyValues(WCHAR* Path, ULONG Idx, ULONG Length)
{
	NTSTATUS status;
	KEY_VALUE_BASIC_INFORMATION  *info;
	ULONG retLength;
	WCHAR *ret;
	HANDLE hKey = OpenRegistryKeyHandle(Path);
	if (hKey == nullptr)
		return nullptr;

	info =(PKEY_VALUE_BASIC_INFORMATION)ExAllocatePool(PagedPool, Length);
	if (info == nullptr)
	{
		ZwClose(hKey);
		return nullptr;
	}
	status = ZwEnumerateValueKey(hKey, Idx, KeyValueBasicInformation, info,
		Length, &retLength);

	if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
	{
		ZwClose(hKey);
		ExFreePool(info);
		return EnumKeyValues(Path, Idx, retLength);
	}
	
	if (!NT_SUCCESS(status) && status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL)
	{
		ZwClose(hKey);
		ExFreePool(info);
		KdPrint(("ZwEnumerateValueKey failed: %x, %i\n", status, retLength));
		return nullptr;
	
	}

	ret = (WCHAR*)ExAllocatePool(PagedPool, info->NameLength+1);
	if (ret == nullptr)
	{
		ZwClose(hKey);
		ExFreePool(info);
		KdPrint(("EnumKeyValues alloc NULL\n"));
		return nullptr;
	}
	memmove(ret, info->Name, info->NameLength);
	memset((PVOID)((ULONG)ret + info->NameLength), 0x0, sizeof(WCHAR));

	ZwClose(hKey);
	ExFreePool(info);
	return ret;
}

bool DeleteKey(WCHAR* Path)
{
	NTSTATUS status;
	HANDLE hKey = OpenRegistryKeyHandle(Path);
	if (hKey == NULL)
		return false;

	status = ZwDeleteKey(hKey);
	ZwClose(hKey);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("DeleteKey, ZwDeleteKey failed: %x\n", status));
		return false;
	}
	return true;
}

bool DeleteKeyBackward(WCHAR* Path, ULONG Pid)
{
	size_t len = wcsnlen(Path, MAX_KEY_PATH_LENGTH);
	bool ret = false;
	WCHAR* buf = (WCHAR*)ExAllocatePool(PagedPool, MAX_KEY_PATH_LENGTH*sizeof(WCHAR));
	if (buf == nullptr)
		return false;

	if (!WrapDeletingKey(Path, Pid))
		goto Exit;

	for (int i = len - 1; i >= 0; i--)
	{
		if (Path[i] == L'\\')
		{
			memset(buf, 0, MAX_KEY_PATH_LENGTH);
			memmove(buf, Path, i*sizeof(WCHAR));

			if (!WrapDeletingKey(buf, Pid))
				goto Exit;
		}
	}	
	ret = true;
Exit:
	ExFreePool(buf);
	return ret;
}

bool WrapDeletingKey(WCHAR* Path, ULONG Pid)
{
	WCHAR* fullPath;
	fullPath = CreateKeyPathWithPid(Path, Pid);
	if (fullPath == nullptr)
		return false;

	if (!DeleteKey(fullPath))
	{
		ExFreePool(fullPath);
		return false;
	}
	ExFreePool(fullPath);
	return true;
}

void DeleteBaseVirtualKey()
{
	if (!DeleteKey(BASE_VIRTUAL_KEY))
	{
		DeleteVirtualKeys();
		DeletePidKeys();
		DeleteKey(BASE_VIRTUAL_KEY);
	}
}

void DeletePidKeys()
{
	Node<TrackedProcess>* buf = g_Data.ProcessTrackingList.head;

	while (buf != nullptr)
	{
		if(!WrapDeletingKey(L"", buf->value.Pid))
			KdPrint(("in DeletePidKeys, WrapDeletingKey failed \n"));

		buf = buf->next;
	}
}
void DeleteVirtualKeys()
{
	ULONG pid;
	WCHAR *path;
	int longestKey = g_Data.LongestKeyIndex(pid);
	while (longestKey != -1)
	{
		path = g_Data.GetKeyPath(pid, longestKey);
		if (path != nullptr)
		{
			DeleteKeyBackward(path, pid);
			g_Data.RemoveKeyByIndex(pid, longestKey);
		}
		longestKey = g_Data.LongestKeyIndex(pid);
	}
}

void RestoreVirtualKeys()
{
	Node<TrackedProcess>* buf = g_Data.ProcessTrackingList.head;
	while (buf != nullptr)
	{

		int i = 1;
		WCHAR* keyPath;
		for (keyPath = g_Data.GetKeyPath(buf->value.Pid, i); keyPath != nullptr;
			keyPath = g_Data.GetKeyPath(buf->value.Pid, i), i++)
		{
			WCHAR* virtualKeyPath = CreateKeyPathWithPid(keyPath, buf->value.Pid);
			if (virtualKeyPath == nullptr)
				return;
			WCHAR* restoredKeyPath = CreateKeyPathWithPrefix(LOCAL_KEY_ROOT, keyPath);
			if (restoredKeyPath == nullptr)
				return;

			RestoreValuesForSingleKey(virtualKeyPath, restoredKeyPath);

		}
		buf = buf->next;
	}
}

void RestoreValuesForSingleKey(WCHAR* VirtualKeyPath, WCHAR* KeyPathToRestore)
{
	int i = 0;
	WCHAR* keyValue;
	WrapRestoringKeyProcess(KeyPathToRestore);
	for (keyValue = EnumKeyValues(VirtualKeyPath, i, wcsnlen(VirtualKeyPath, MAX_KEY_PATH_LENGTH)); keyValue != nullptr;
		keyValue = EnumKeyValues(VirtualKeyPath, i, wcsnlen(VirtualKeyPath, MAX_KEY_PATH_LENGTH)), i++)
	{
		HANDLE hKey = OpenRegistryKeyHandle(VirtualKeyPath);
		if (!hKey)
		{
			ExFreePool(keyValue);
			return;
		}
		UNICODE_STRING valueUnicode;
		valueUnicode.Buffer = keyValue;
		valueUnicode.Length = (USHORT)(wcsnlen(keyValue, MAX_KEY_PATH_LENGTH) * sizeof(WCHAR));
		valueUnicode.MaximumLength = MAX_KEY_PATH_LENGTH;

		RegistryKeyValue* queriedValue = QueryKeyValue(hKey, &valueUnicode);
		if (queriedValue == nullptr)
		{
			ExFreePool(keyValue);
			ZwClose(hKey);
			continue;
		}

		HANDLE hNewKey = OpenRegistryKeyHandle(KeyPathToRestore);
		if (!hNewKey)
		{
			ExFreePool(keyValue);
			ZwClose(hKey);
			return;
		}
		NTSTATUS status = ZwSetValueKey(hNewKey, &valueUnicode, 0,
			queriedValue->Type, queriedValue->Buffer, queriedValue->Length);

		if (!NT_SUCCESS(status))
		{
			ExFreePool(keyValue);
			ZwClose(hKey);
			KdPrint(("ZwSetValueKey failed %x\n", status));
			return;
		}

		CleanRegistryKeyValueStruct(queriedValue);
		ExFreePool(keyValue);
	}
}

RegistryKeyValue *QueryKeyValue(HANDLE hKey, UNICODE_STRING *ValueStr, ULONG Len)
{

	RegistryKeyValue* ret;
	KEY_VALUE_FULL_INFORMATION* valueInfo;
	ULONG retLen;
	NTSTATUS status;

	ret = (RegistryKeyValue*)ExAllocatePool(PagedPool, sizeof(RegistryKeyValue));
	if (ret == nullptr)
		return nullptr;

	valueInfo = (KEY_VALUE_FULL_INFORMATION*)ExAllocatePool(PagedPool, Len);

	if (valueInfo == nullptr)
	{
		ExFreePool(ret);
		return nullptr;
	}

	status = ZwQueryValueKey(hKey, ValueStr, KeyValueFullInformation,
		valueInfo, Len, &retLen);

	if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_BUFFER_OVERFLOW)
	{
		ExFreePool(ret);
		ExFreePool(valueInfo);
		return QueryKeyValue(hKey, ValueStr, retLen);
	}

	// these are not all fail reason but they were choosen just to save code with goto
	//(cleaning after if statement)
	if (!NT_SUCCESS(status))
	{
		KdPrint(("ZwQueryValueKey failed %x\n", status));
		goto RETURN_FAILED;
	}
	if (!ret->Init(valueInfo))
		goto RETURN_FAILED;

	ExFreePool(valueInfo);
	return ret;

RETURN_FAILED:
	ExFreePool(ret);
	ExFreePool(valueInfo);
	return nullptr;
}

WCHAR* CreateKeyPathWithPrefix(WCHAR* Prefix, WCHAR* KeyPathEnding)
{;
	WCHAR* createdKeyPath;

	if ((wcsnlen(KeyPathEnding, MAX_KEY_PATH_LENGTH) + wcsnlen(Prefix, MAX_KEY_PATH_LENGTH) - 1) > MAX_KEY_PATH_LENGTH)
		return NULL;

	createdKeyPath = (WCHAR*)ExAllocatePool(PagedPool, MAX_KEY_PATH_LENGTH*sizeof(WCHAR));
	if (createdKeyPath == NULL)
		return NULL;

	wcsncpy(createdKeyPath, Prefix, MAX_KEY_PATH_LENGTH);
	wcscat(createdKeyPath, L"\\");
	wcscat(createdKeyPath, KeyPathEnding);

	return createdKeyPath;
}

WCHAR* CreateKeyPathWithPid(WCHAR* KeyPathEnding, ULONG Pid)
{
	char buf[33];
	wchar_t keyBuf[33];
	WCHAR * createdKeyPath = (WCHAR*)ExAllocatePool(PagedPool, MAX_KEY_PATH_LENGTH * sizeof(WCHAR));
	if (createdKeyPath == NULL)
		return NULL;

	_itoa(Pid, buf, 10);
	mbstowcs(keyBuf, buf, sizeof(buf));

	wcscpy(createdKeyPath, BASE_VIRTUAL_KEY);
	wcscat(createdKeyPath, L"\\");
	wcscat(createdKeyPath, keyBuf);
	wcscat(createdKeyPath, L"\\");
	wcscat(createdKeyPath, KeyPathEnding);
	return createdKeyPath;
}

void CleanRegistryKeyValueStruct(RegistryKeyValue* Info)
{
	if (Info == nullptr)
		return;

	if (Info->Buffer != nullptr)
		ExFreePool(Info->Buffer);

	ExFreePool(Info);
}
