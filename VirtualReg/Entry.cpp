#pragma once
#include "Header.h"
// *** TODO ***
// stuff with mutexes (in g_Data)

// VirtualRegs!OnPreRegisterCallback + 0xfd
Global g_Data;
extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\virtualreg");
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\virtualreg");
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT DeviceObject;
	DriverObject->MajorFunction[IRP_MJ_CREATE] =
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = VirtualRegCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = VirtualRegDeviceControl;
	DriverObject->DriverUnload = VirtualRegUnload;
	g_Data.Init();
	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName,
			FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("IoCreateDevice failed %x\n", status));
			break;
		}
		DeviceObject->Flags |= DO_BUFFERED_IO;
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("IoCreateSymbolicLink failed %x\n", status));
			break;
		}

		UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"1231.21");
		status = CmRegisterCallbackEx(OnPreRegisterCallback, &altitude, DriverObject, NULL, &g_Data.Cookie, NULL);
		if (!NT_SUCCESS(status))
		{
			IoDeleteSymbolicLink(&symLink);
			KdPrint(("CmRegisterCallbackEx failed %x\n", status));
			break;
		}

	} while (false);
	if (!NT_SUCCESS(status))
		if (DeviceObject) IoDeleteDevice(DeviceObject);

	return status;
}

NTSTATUS VirtualRegDeviceControl(PDEVICE_OBJECT, PIRP Irp)
{
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = STATUS_SUCCESS;
	ULONG len = 0;
	ULONG* buf = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{	
	case ADD_PID:
		status = AddNewProcessId(buf,
			stack->Parameters.DeviceIoControl.InputBufferLength, 
			len);

		break;

	case RESTORE_ALL:
		RestoreVirtualKeys();
		break;

	case CLEAR_ALL:
		g_Data.Clear();
		break;

	case DELETE_ALL:
		DeleteBaseVirtualKey();
		break;
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
	}
	return CompleteIrp(Irp, status, len);
}

NTSTATUS VirtualRegCreateClose(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp, STATUS_SUCCESS, 0);
}

void VirtualRegUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\virtualreg");
	IoDeleteSymbolicLink(&symLink); 
	IoDeleteDevice(DriverObject->DeviceObject);
	CmUnRegisterCallback(g_Data.Cookie);
	DeleteBaseVirtualKey();
	g_Data.Clear();
}


NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status, ULONG len)
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS AddNewProcessId(PULONG buf, ULONG InputBufferLegth, ULONG& Length)
{
	NTSTATUS status = STATUS_SUCCESS;
	if (buf == nullptr)
	{
		status = STATUS_BUFFER_ALL_ZEROS;
		return status;
	}

	if (sizeof(ULONG) != InputBufferLegth)
	{
		status = STATUS_INVALID_BUFFER_SIZE;
		return status;
	}

	Length = InputBufferLegth;
	if (g_Data.IsPidPresent(*buf))
		return status;
	g_Data.Add(*buf);
	return status;
}

void EnumGlobal()
{
	Node<TrackedProcess>* buf1 = g_Data.ProcessTrackingList.head;
	while (buf1 != nullptr)
	{
		KdPrint(("PID: %i\n", buf1->value.Pid));
		Node<WCHAR*>* tmp = buf1->value.KeysTrackingList.head;
		while (tmp != nullptr)
		{
			KdPrint(("\tpath: %ls\n", tmp->value));
			KdPrint(("\tpath_p: %p\n", tmp->value));
			tmp = tmp->next;
		}
		buf1 = buf1->next;
	}
}