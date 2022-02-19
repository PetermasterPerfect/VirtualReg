#pragma once
#include <ntddk.h>

struct _CM_NAME_HASH
{
	ULONG ConvKey;                                                          //0x0
	struct _CM_NAME_HASH* NextHash;                                         //0x4
	USHORT NameLength;                                                      //0x8
	WCHAR Name[1];                                                          //0xa
};

//0x10 bytes (sizeof)
struct _CM_NAME_CONTROL_BLOCK
{
    UCHAR Compressed;                                                       //0x0
    USHORT RefCount;                                                        //0x2
    union
    {
        struct _CM_NAME_HASH NameHash;                                      //0x4
        //struct
        //{
        //    ULONG ConvKey;                                                  //0x4
        //    struct _CM_KEY_HASH* NextHash;                                  //0x8
        //    USHORT NameLength;                                              //0xc
        //    WCHAR Name[1];                                                  //0xe
        //};
    };
};

typedef struct _CM_KEY_BODY
{
	ULONG Type;
	struct _CM_KEY_CONTROL_BLOCK* KeyControlBlock;
	struct _CM_NOTIFY_BLOCK* NotifyBlock;
	HANDLE ProcessID;
	LIST_ENTRY KeyBodyList;
} CM_KEY_BODY, * PCM_KEY_BODY;