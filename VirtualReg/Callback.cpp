#include "Header.h"
#define MAXSIZE 1024
#define TEMP_TAG 'Yeii'

NTSTATUS OnPreRegisterCallback(
	PVOID CallbackContext,
	PVOID Argument1,
	PVOID Argument2
)
{
	ULONG pid;
	pid = (ULONG)PsGetCurrentProcessId();
	if (!g_Data.IsPidPresent(pid))
		return STATUS_SUCCESS;

	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)Argument1)
	{
	case RegNtPreSetValueKey:
		return HookSettingValue(Argument2, pid);
		break;

	case RegNtPreCreateKeyEx:
		return HookRegistryCreation(Argument2, pid);
		break;
	}
	return STATUS_SUCCESS;
}

NTSTATUS HookSettingValue(PVOID Argument2, ULONG Pid)
{
	NTSTATUS status;
	PVOID originalKeyBody = ((REG_SET_VALUE_KEY_INFORMATION*)Argument2)->Object;
	ULONG len;
	PUNICODE_STRING originalKeyPath = (PUNICODE_STRING)ExAllocatePool(PagedPool, MAX_KEY_PATH_LENGTH * sizeof(WCHAR));
	if (originalKeyPath == nullptr)
		return STATUS_INSUFFICIENT_RESOURCES;


	status = QueryForRegistryString(((REG_SET_VALUE_KEY_INFORMATION*)Argument2)->Object,
		originalKeyPath);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(originalKeyPath);
		return status;
	}

	if (!wcsncmp(originalKeyPath->Buffer, LOCAL_KEY_ROOT, wcslen(LOCAL_KEY_ROOT)))
	{
		WCHAR* relativeKeyPath;
		PVOID newKeyObject;

		status = WrapAllRegistryModificationsProcess(originalKeyPath, Pid,
			relativeKeyPath, newKeyObject);
		if (!NT_SUCCESS(status))
		{
			ExFreePool(originalKeyPath);
			return status;
		}

		if (!g_Data.IsKeyPathPresent(relativeKeyPath))
			g_Data.AddKeyPath(Pid, relativeKeyPath);

		// check if newKeyObject or originalKeyBody are free later
		memmove(originalKeyBody, newKeyObject, sizeof(_CM_KEY_BODY));
	}
	return STATUS_SUCCESS;
}


NTSTATUS HookRegistryCreation(PVOID Argument2, ULONG Pid)
{
	NTSTATUS status;
	PVOID originalKeyBody = ((REG_CREATE_KEY_INFORMATION*)Argument2)->RootObject;
	ULONG len;
	PUNICODE_STRING originalKeyPath = (PUNICODE_STRING)ExAllocatePool(PagedPool, MAX_KEY_PATH_LENGTH * sizeof(WCHAR));
	if (originalKeyPath == nullptr)
		return STATUS_INSUFFICIENT_RESOURCES;


	status = QueryForRegistryString(((REG_CREATE_KEY_INFORMATION*)Argument2)->RootObject,
		originalKeyPath);
	if (!NT_SUCCESS(status))
	{
		ExFreePool(originalKeyPath);
		return status;
	}

	if (!wcsncmp(originalKeyPath->Buffer, LOCAL_KEY_ROOT, wcslen(LOCAL_KEY_ROOT)))
	{
		WCHAR *relativeKeyPath;
		PVOID newKeyObject;

		status = WrapAllRegistryModificationsProcess(originalKeyPath, Pid,
			relativeKeyPath, newKeyObject);
		if (!NT_SUCCESS(status))
		{
			ExFreePool(originalKeyPath);
			return status;
		}

		relativeKeyPath = CatPathsAtCreateRegistryEvent(relativeKeyPath,
			((REG_CREATE_KEY_INFORMATION*)Argument2)->CompleteName->Buffer);
		if (relativeKeyPath == nullptr)
		{
			ExFreePool(originalKeyPath);
			ExFreePool(relativeKeyPath);
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		if (!g_Data.IsKeyPathPresent(relativeKeyPath))
			g_Data.AddKeyPath(Pid, relativeKeyPath);

		// check if newKeyObject or originalKeyBody are free later
		memmove(originalKeyBody, newKeyObject, sizeof(_CM_KEY_BODY));
	}
	return STATUS_SUCCESS;
}

NTSTATUS WrapAllRegistryModificationsProcess(PUNICODE_STRING OriginalKeyPath, ULONG Pid, 
	WCHAR*& RelativeKeyPath, PVOID& NewKeyObject)
{
		
	if (!CreateBaseVirtualKey())
			return STATUS_ACCESS_DENIED;

	RelativeKeyPath = CutKeyRoot(OriginalKeyPath);
	if (RelativeKeyPath == nullptr)
		return STATUS_INSUFFICIENT_RESOURCES;
	
	NewKeyObject = WrapCreatingVirtualKeyProcess(RelativeKeyPath, Pid);
	if (NewKeyObject == NULL)
	{
		ExFreePool(RelativeKeyPath);
		return STATUS_ACCESS_DENIED;
	}
	return STATUS_SUCCESS;
}


NTSTATUS QueryForRegistryString(PVOID Object, PUNICODE_STRING UnicodeString)
{
	ULONG len;
	NTSTATUS status = ObQueryNameString(Object, (POBJECT_NAME_INFORMATION)UnicodeString,
		MAX_KEY_PATH_LENGTH * sizeof(WCHAR), &len);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("QueryForRegistryString ObQueryNameString failed %x\n", status));
		return status;
	}

	if (!wcsncmp(UnicodeString->Buffer, BASE_VIRTUAL_KEY, wcslen(BASE_VIRTUAL_KEY)))
		return STATUS_INSUFFICIENT_RESOURCES;
	
	return STATUS_SUCCESS;
}

WCHAR *CatPathsAtCreateRegistryEvent(WCHAR* str1, WCHAR* str2)
{
	//str1 need to be dynamiclly alocated
	// because of ExFreePool at the end
	WCHAR* buf = (WCHAR*)ExAllocatePool(PagedPool, (wcslen(str1) + wcslen(str2)+2) * sizeof(WCHAR));
	if (buf == nullptr)
		return nullptr;

	memmove(buf, str1, (wcslen(str1)+1)*sizeof(WCHAR));
	wcscat(buf, L"\\");
	wcscat(buf, str2);
	ExFreePool(str1);
	return buf;
}

WCHAR* CutKeyRoot(UNICODE_STRING* path)
{
	WCHAR* buf = &path->Buffer[wcslen(LOCAL_KEY_ROOT) + 1];
	WCHAR* KeyPath = (WCHAR*)ExAllocatePool(PagedPool, wcslen(buf) * sizeof(WCHAR)+2);
	if (KeyPath != nullptr)
		memmove(KeyPath, buf, wcslen(buf) * 2 + 2);
	return KeyPath;
}

