#pragma once
#pragma warning(disable: 4996)
#include <ntifs.h>
#include <ntddk.h>
#include <string.h>
#include <stdlib.h>
#include "Struct.h"
#include "CmRegistry.h"
#define VIRTUAL_REGISTRY_DEVICE 0x8069
#define ADD_PID CTL_CODE(VIRTUAL_REGISTRY_DEVICE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CLEAR_ALL CTL_CODE(VIRTUAL_REGISTRY_DEVICE, 0x803, METHOD_NEITHER, FILE_ANY_ACCESS)
#define RESTORE_ALL CTL_CODE(VIRTUAL_REGISTRY_DEVICE, 0x804, METHOD_NEITHER, FILE_ANY_ACCESS)
#define DELETE_ALL CTL_CODE(VIRTUAL_REGISTRY_DEVICE, 0x805, METHOD_NEITHER, FILE_ANY_ACCESS)
#define LOCAL_KEY_ROOT L"\\REGISTRY\\MACHINE"
#define BASE_VIRTUAL_KEY L"\\REGISTRY\\MACHINE\\SOFTWARE\\VIRTUAL"

extern Global g_Data;
extern HANDLE g_BaseKey;

NTSTATUS VirtualRegCreateClose(PDEVICE_OBJECT, PIRP);
NTSTATUS VirtualRegDeviceControl(PDEVICE_OBJECT, PIRP);
void VirtualRegUnload(PDRIVER_OBJECT);
NTSTATUS CompleteIrp(PIRP, NTSTATUS, ULONG);
void DeleteBaseVirtualKey();
void EnumGlobal();
NTSTATUS AddNewProcessId(PULONG, ULONG, ULONG&);

NTSTATUS OnPreRegisterCallback(PVOID, PVOID,PVOID );


NTSTATUS HookSettingValue(PVOID, ULONG);
NTSTATUS HookRegistryCreation(PVOID, ULONG);
NTSTATUS WrapAllRegistryModificationsProcess(PUNICODE_STRING, ULONG,
	WCHAR*&, PVOID&);
NTSTATUS QueryForRegistryString(PVOID, PUNICODE_STRING);
WCHAR* CatPathsAtCreateRegistryEvent(WCHAR*, WCHAR*);
WCHAR* CutKeyRoot(UNICODE_STRING*);

bool CreateBaseVirtualKey();
PVOID CreateVirtualKey(WCHAR*, ULONG);
PVOID CreateKey(WCHAR*);
WCHAR* CreateKeyPathWithPid(WCHAR*, ULONG);

PVOID WrapCreatingVirtualKeyProcess(WCHAR*, ULONG);
PVOID WrapRestoringKeyProcess(WCHAR*);
HANDLE OpenRegistryKeyHandle(WCHAR*);
WCHAR* EnumKeyValues(WCHAR*, ULONG, ULONG length=sizeof(KEY_VALUE_BASIC_INFORMATION));
int CountSlashes(WCHAR*);

void DeleteVirtualKeys();
bool DeleteKey(WCHAR*);
void DeletePidKeys();
void DeleteBaseVirtualKey();
bool DeleteKeyBackward(WCHAR*, ULONG pd);
bool WrapDeletingKey(WCHAR*, ULONG);

WCHAR* CreateKeyPathWithPrefix(WCHAR*, WCHAR*);
void RestoreVirtualKeys();
void RestoreValuesForSingleKey(WCHAR*, WCHAR*);
RegistryKeyValue* QueryKeyValue(HANDLE, UNICODE_STRING*, ULONG len = sizeof(KEY_VALUE_FULL_INFORMATION));