/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#include <ntddk.h>
#include <ntstrsafe.h>
#include <stdarg.h>
#include <stdlib.h>
#include <xen.h>

#include <debug_interface.h>
#include <suspend_interface.h>

#include "pdo.h"
#include "hid.h"
#include "mrsw.h"
#include "thread.h"
#include "vkbd.h"
#include "dbg_print.h"
#include "assert.h"
#include "util.h"

struct _XENVKBD_HID_CONTEXT {
    PXENVKBD_PDO                Pdo;
    XENVKBD_MRSW_LOCK           Lock;
    LONG                        References;
    PXENVKBD_FRONTEND           Frontend;
    BOOLEAN                     Enabled;
    ULONG                       Version;
    XENHID_HID_CALLBACK         Callback;
    PVOID                       Argument;
    XENBUS_DEBUG_INTERFACE      DebugInterface;
    XENBUS_SUSPEND_INTERFACE    SuspendInterface;
    PXENBUS_DEBUG_CALLBACK      DebugCallback;
    PXENBUS_SUSPEND_CALLBACK    SuspendCallbackLate;

    XENVKBD_HID_KEYBOARD        KeyboardReport;
    XENVKBD_HID_ABSMOUSE        AbsMouseReport;
    BOOLEAN                     KeyboardPending;
    BOOLEAN                     AbsMousePending;
};

#define XENVKBD_VKBD_TAG  'FIV'

static FORCEINLINE NTSTATUS
HidCopyBuffer(
    IN  PVOID       Buffer,
    IN  ULONG       Length,
    IN  const VOID  *Source,
    IN  ULONG       SourceLength,
    OUT PULONG      Returned
    )
{
    if (Buffer == NULL)
        return STATUS_INVALID_PARAMETER;
    if (Length < SourceLength)
        return STATUS_NO_MEMORY;

    RtlCopyMemory(Buffer,
                  Source,
                  SourceLength);
    if (Returned)
        *Returned = SourceLength;
    return STATUS_SUCCESS;
}

static FORCEINLINE PVOID
__HidAllocate(
    IN  ULONG   Length
    )
{
    return __AllocatePoolWithTag(NonPagedPool, Length, XENVKBD_VKBD_TAG);
}

static FORCEINLINE VOID
__HidFree(
    IN  PVOID   Buffer
    )
{
    __FreePoolWithTag(Buffer, XENVKBD_VKBD_TAG);
}

static DECLSPEC_NOINLINE VOID
HidSuspendCallbackLate(
    IN  PVOID               Argument
    )
{
    PXENVKBD_HID_CONTEXT    Context = Argument;
    NTSTATUS                status;

    RtlZeroMemory(&Context->KeyboardReport, sizeof(XENVKBD_HID_KEYBOARD));
    RtlZeroMemory(&Context->AbsMouseReport, sizeof(XENVKBD_HID_ABSMOUSE));
    Context->KeyboardReport.ReportId = 1;
    Context->AbsMouseReport.ReportId = 2;

    if (!Context->Enabled)
        return;

    status = FrontendSetState(Context->Frontend, FRONTEND_ENABLED);
    ASSERT(NT_SUCCESS(status));
}

static DECLSPEC_NOINLINE VOID
HidDebugCallback(
    IN  PVOID               Argument,
    IN  BOOLEAN             Crashing
    )
{
    PXENVKBD_HID_CONTEXT    Context = Argument;

    UNREFERENCED_PARAMETER(Crashing);

    XENBUS_DEBUG(Printf,
                 &Context->DebugInterface,
                 "%u 0x%p(0x%p)%s\n",
                 Context->Version,
                 Context->Callback,
                 Context->Argument,
                 Context->Enabled ? " ENABLED" : "");

    XENBUS_DEBUG(Printf,
                 &Context->DebugInterface,
                 "KBD: %02x %02x %02x %02x %02x %02x %02x %02x%s\n",
                 Context->KeyboardReport.ReportId,
                 Context->KeyboardReport.Modifiers,
                 Context->KeyboardReport.Keys[0],
                 Context->KeyboardReport.Keys[1],
                 Context->KeyboardReport.Keys[2],
                 Context->KeyboardReport.Keys[3],
                 Context->KeyboardReport.Keys[4],
                 Context->KeyboardReport.Keys[5],
                 Context->KeyboardPending ? " PENDING" : "");

    XENBUS_DEBUG(Printf,
                 &Context->DebugInterface,
                 "MOU: %02x %02x %04x %04x %02x%s\n",
                 Context->AbsMouseReport.ReportId,
                 Context->AbsMouseReport.Buttons,
                 Context->AbsMouseReport.X,
                 Context->AbsMouseReport.Y,
                 Context->AbsMouseReport.dZ,
                 Context->AbsMousePending ? " PENDING" : "");
}

static NTSTATUS
HidEnable(
    IN  PINTERFACE          Interface,
    IN  XENHID_HID_CALLBACK Callback,
    IN  PVOID               Argument
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    KIRQL                   Irql;
    BOOLEAN                 Exclusive;
    NTSTATUS                status;

    Trace("====>\n");

    AcquireMrswLockExclusive(&Context->Lock, &Irql);
    Exclusive = TRUE;

    if (Context->Enabled)
        goto done;

    Context->Callback = Callback;
    Context->Argument = Argument;

    Context->Enabled = TRUE;

    KeMemoryBarrier();

    status = XENBUS_SUSPEND(Acquire, &Context->SuspendInterface);
    if (!NT_SUCCESS(status))
        goto fail1;

    status = FrontendSetState(Context->Frontend, FRONTEND_ENABLED);
    if (!NT_SUCCESS(status))
        goto fail2;

    status = XENBUS_SUSPEND(Register,
                            &Context->SuspendInterface,
                            SUSPEND_CALLBACK_LATE,
                            HidSuspendCallbackLate,
                            Context,
                            &Context->SuspendCallbackLate);
    if (!NT_SUCCESS(status))
        goto fail3;

    status = XENBUS_DEBUG(Acquire, &Context->DebugInterface);
    if (!NT_SUCCESS(status))
        goto fail4;

    status = XENBUS_DEBUG(Register,
                          &Context->DebugInterface,
                          __MODULE__"|DEBUG",
                          HidDebugCallback,
                          Context,
                          &Context->DebugCallback);
    if (!NT_SUCCESS(status))
        goto fail5;

done:
    ASSERT(Exclusive);
    ReleaseMrswLockExclusive(&Context->Lock, Irql, FALSE);

    Trace("<====\n");

    return STATUS_SUCCESS;

fail5:
    Error("fail5\n");

    XENBUS_DEBUG(Release, &Context->DebugInterface);

fail4:
    Error("fail4\n");

    XENBUS_SUSPEND(Deregister,
                   &Context->SuspendInterface,
                   Context->SuspendCallbackLate);
    Context->SuspendCallbackLate = NULL;

fail3:
    Error("fail3\n");

    (VOID) FrontendSetState(Context->Frontend, FRONTEND_CONNECTED);

    ReleaseMrswLockExclusive(&Context->Lock, Irql, TRUE);
    Exclusive = FALSE;

fail2:
    Error("fail2\n");

    XENBUS_SUSPEND(Release, &Context->SuspendInterface);

fail1:
    Error("fail1 (%08x)\n", status);

    Context->Enabled = FALSE;

    KeMemoryBarrier();

    Context->Argument = NULL;
    Context->Callback = NULL;

    if (Exclusive)
        ReleaseMrswLockExclusive(&Context->Lock, Irql, FALSE);
    else
        ReleaseMrswLockShared(&Context->Lock);

    return status;
}

static VOID
HidDisable(
    IN  PINTERFACE          Interface
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    KIRQL                   Irql;

    Trace("====>\n");

    AcquireMrswLockExclusive(&Context->Lock, &Irql);

    if (!Context->Enabled) {
        ReleaseMrswLockExclusive(&Context->Lock, Irql, FALSE);
        goto done;
    }

    Context->Enabled = FALSE;

    KeMemoryBarrier();

    XENBUS_DEBUG(Deregister,
                 &Context->DebugInterface,
                 Context->DebugCallback);
    Context->DebugCallback = NULL;

    XENBUS_DEBUG(Release, &Context->DebugInterface);

    XENBUS_SUSPEND(Deregister,
                   &Context->SuspendInterface,
                   Context->SuspendCallbackLate);
    Context->SuspendCallbackLate = NULL;

    (VOID) FrontendSetState(Context->Frontend, FRONTEND_CONNECTED);

    ReleaseMrswLockExclusive(&Context->Lock, Irql, TRUE);

    XENBUS_SUSPEND(Release, &Context->SuspendInterface);

    Context->Argument = NULL;
    Context->Callback = NULL;

    ReleaseMrswLockShared(&Context->Lock);

done:
    Trace("<====\n");
}

static NTSTATUS
HidGetDeviceAttributes(
    IN  PINTERFACE          Interface,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = HidCopyBuffer(Buffer,
                           Length,
                           &VkbdDeviceAttributes,
                           sizeof(VkbdDeviceAttributes),
                           Returned);

    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    return status;
}

static NTSTATUS
HidGetDeviceDescriptor(
    IN  PINTERFACE          Interface,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = HidCopyBuffer(Buffer,
                           Length,
                           &VkbdDeviceDescriptor,
                           sizeof(VkbdDeviceDescriptor),
                           Returned);

    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    return status;
}

static NTSTATUS
HidGetReportDescriptor(
    IN  PINTERFACE          Interface,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = HidCopyBuffer(Buffer,
                           Length,
                           VkbdReportDescriptor,
                           sizeof(VkbdReportDescriptor),
                           Returned);

    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    return status;
}

static NTSTATUS
HidGetString(
    IN  PINTERFACE          Interface,
    IN  ULONG               Identifier,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    PXENVKBD_PDO            Pdo;
    PXENVKBD_FDO            Fdo;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    Pdo = FrontendGetPdo(Context->Frontend);
    Fdo = PdoGetFdo(Pdo);

    // Ignore LangID
    switch (Identifier & 0xFF) {
    case HID_STRING_ID_IMANUFACTURER:
        status = HidCopyBuffer(Buffer,
                               Length,
                               FdoGetVendorName(Fdo),
                               (ULONG)strlen(FdoGetVendorName(Fdo)),
                               Returned);
        break;
    case HID_STRING_ID_IPRODUCT:
        status = HidCopyBuffer(Buffer,
                               Length,
                               "PV HID Device",
                               sizeof("PV HID Device"),
                               Returned);
        break;
    //case HID_STRING_ID_ISERIALNUMBER:
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    return status;
}

static NTSTATUS
HidGetIndexedString(
    IN  PINTERFACE          Interface,
    IN  ULONG               Index,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = STATUS_NOT_SUPPORTED;
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    UNREFERENCED_PARAMETER(Index);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Returned);
    return status;
}

static NTSTATUS
HidGetFeature(
    IN  PINTERFACE          Interface,
    IN  ULONG               ReportId,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = STATUS_NOT_SUPPORTED;
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    UNREFERENCED_PARAMETER(ReportId);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    UNREFERENCED_PARAMETER(Returned);
    return status;
}

static NTSTATUS
HidSetFeature(
    IN  PINTERFACE          Interface,
    IN  ULONG               ReportId,
    IN  PVOID               Buffer,
    IN  ULONG               Length
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = STATUS_NOT_SUPPORTED;
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    UNREFERENCED_PARAMETER(ReportId);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    return status;
}

static NTSTATUS
HidGetInputReport(
    IN  PINTERFACE          Interface,
    IN  ULONG               ReportId,
    IN  PVOID               Buffer,
    IN  ULONG               Length,
    OUT PULONG              Returned
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    switch (ReportId) {
    case 1:
        status = HidCopyBuffer(Buffer,
                               Length,
                               &Context->KeyboardReport,
                               sizeof(XENVKBD_HID_KEYBOARD),
                               Returned);
        break;
    case 2:
        status = HidCopyBuffer(Buffer,
                               Length,
                               &Context->AbsMouseReport,
                               sizeof(XENVKBD_HID_ABSMOUSE),
                               Returned);
        break;
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    UNREFERENCED_PARAMETER(ReportId);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    return status;
}

static NTSTATUS
HidSetOutputReport(
    IN  PINTERFACE          Interface,
    IN  ULONG               ReportId,
    IN  PVOID               Buffer,
    IN  ULONG               Length
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = STATUS_NOT_SUPPORTED;
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    UNREFERENCED_PARAMETER(ReportId);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    return status;
}

static BOOLEAN
HidSendReadReport(
    IN  PXENVKBD_HID_CONTEXT    Context,
    IN  PVOID                   Buffer,
    IN  ULONG                   Length
    )
{
    if (!Context->Enabled)
        return TRUE; // flag as pending

    // Callback returns TRUE on success, FALSE when Irp could not be completed
    // Invert the result to indicate Pending state
    return !Context->Callback(Context->Argument,
                              Buffer,
                              Length);
}

static VOID
HidReadReport(
    IN  PINTERFACE          Interface
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;

    AcquireMrswLockShared(&Context->Lock);

    // Check for pending reports, push 1 pending report to subscriber
    if (Context->KeyboardPending)
        Context->KeyboardPending = HidSendReadReport(Context,
                                                     &Context->KeyboardReport,
                                                     sizeof(XENVKBD_HID_KEYBOARD));
    else if (Context->AbsMousePending)
        Context->AbsMousePending = HidSendReadReport(Context,
                                                     &Context->AbsMouseReport,
                                                     sizeof(XENVKBD_HID_ABSMOUSE));

    ReleaseMrswLockShared(&Context->Lock);
}

static NTSTATUS
HidWriteReport(
    IN  PINTERFACE          Interface,
    IN  ULONG               ReportId,
    IN  PVOID               Buffer,
    IN  ULONG               Length
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    NTSTATUS                status;

    Trace("=====>\n");
    AcquireMrswLockShared(&Context->Lock);

    status = STATUS_NOT_SUPPORTED;
  
    ReleaseMrswLockShared(&Context->Lock);
    Trace("<=====\n");

    UNREFERENCED_PARAMETER(ReportId);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(Length);
    return status;
}

static NTSTATUS
HidAcquire(
    PINTERFACE              Interface
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    KIRQL                   Irql;

    AcquireMrswLockExclusive(&Context->Lock, &Irql);

    if (Context->References++ != 0)
        goto done;

    Trace("====>\n");

    Context->Frontend = PdoGetFrontend(Context->Pdo);
    Context->Version = Interface->Version;

    Trace("<====\n");

done:
    ReleaseMrswLockExclusive(&Context->Lock, Irql, FALSE);

    return STATUS_SUCCESS;
}

VOID
HidRelease(
    IN  PINTERFACE          Interface
    )
{
    PXENVKBD_HID_CONTEXT    Context = Interface->Context;
    KIRQL                   Irql;

    AcquireMrswLockExclusive(&Context->Lock, &Irql);

    if (--Context->References > 0)
        goto done;

    Trace("====>\n");

    ASSERT(!Context->Enabled);

    Context->Version = 0;
    Context->Frontend = NULL;

    Trace("<====\n");

done:
    ReleaseMrswLockExclusive(&Context->Lock, Irql, FALSE);
}

static struct _XENHID_HID_INTERFACE_V1 HidInterfaceVersion1 = {
    { sizeof (struct _XENHID_HID_INTERFACE_V1), 1, NULL, NULL, NULL },
    HidAcquire,
    HidRelease,
    HidEnable,
    HidDisable,
    HidGetDeviceAttributes,
    HidGetDeviceDescriptor,
    HidGetReportDescriptor,
    HidGetString,
    HidGetIndexedString,
    HidGetFeature,
    HidSetFeature,
    HidGetInputReport,
    HidSetOutputReport,
    HidReadReport,
    HidWriteReport
};

NTSTATUS
HidInitialize(
    IN  PXENVKBD_PDO            Pdo,
    OUT PXENVKBD_HID_CONTEXT    *Context
    )
{
    NTSTATUS                    status;

    Trace("====>\n");

    *Context = __HidAllocate(sizeof (XENVKBD_HID_CONTEXT));

    status = STATUS_NO_MEMORY;
    if (*Context == NULL)
        goto fail1;

    InitializeMrswLock(&(*Context)->Lock);

    FdoGetDebugInterface(PdoGetFdo(Pdo),&(*Context)->DebugInterface);
    FdoGetSuspendInterface(PdoGetFdo(Pdo),&(*Context)->SuspendInterface);

    (*Context)->Pdo = Pdo;
    (*Context)->KeyboardReport.ReportId = 1;
    (*Context)->AbsMouseReport.ReportId = 2;

    Trace("<====\n");

    return STATUS_SUCCESS;

fail1:
    Error("fail1 (%08x)\n", status);

    return status;
}

NTSTATUS
HidGetInterface(
    IN      PXENVKBD_HID_CONTEXT    Context,
    IN      ULONG                   Version,
    IN OUT  PINTERFACE              Interface,
    IN      ULONG                   Size
    )
{
    NTSTATUS                        status;

    switch (Version) {
    case 1: {
        struct _XENHID_HID_INTERFACE_V1 *HidInterface;

        HidInterface = (struct _XENHID_HID_INTERFACE_V1 *)Interface;

        status = STATUS_BUFFER_OVERFLOW;
        if (Size < sizeof (struct _XENHID_HID_INTERFACE_V1))
            break;

        *HidInterface = HidInterfaceVersion1;

        ASSERT3U(Interface->Version, ==, Version);
        Interface->Context = Context;

        status = STATUS_SUCCESS;
        break;
    }
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    return status;
}   

VOID
HidTeardown(
    IN  PXENVKBD_HID_CONTEXT    Context
    )
{
    Trace("====>\n");

    RtlZeroMemory(&Context->KeyboardReport, sizeof(XENVKBD_HID_KEYBOARD));
    RtlZeroMemory(&Context->AbsMouseReport, sizeof(XENVKBD_HID_ABSMOUSE));
    Context->KeyboardPending = FALSE;
    Context->AbsMousePending = FALSE;

    Context->Pdo = NULL;
    Context->Version = 0;

    RtlZeroMemory(&Context->SuspendInterface,
                  sizeof (XENBUS_SUSPEND_INTERFACE));
    RtlZeroMemory(&Context->DebugInterface,
                  sizeof (XENBUS_DEBUG_INTERFACE));

    RtlZeroMemory(&Context->Lock, sizeof (XENVKBD_MRSW_LOCK));

    ASSERT(IsZeroMemory(Context, sizeof (XENVKBD_HID_CONTEXT)));
    __HidFree(Context);

    Trace("<====\n");
}

static FORCEINLINE LONG
Constrain(
    IN  LONG    Value,
    IN  LONG    Min,
    IN  LONG    Max
    )
{
    if (Value < Min)
        return Min;
    if (Value > Max)
        return Max;
    return Value;
}

static FORCEINLINE UCHAR
SetBit(
    IN  UCHAR   Value,
    IN  UCHAR   BitIdx,
    IN  BOOLEAN Pressed
    )
{
    if (Pressed) {
        return Value | (1 << BitIdx);
    } else {
        return Value & ~(1 << BitIdx);
    }
}

static FORCEINLINE VOID
SetArray(
    IN  PUCHAR  Array,
    IN  ULONG   Size,
    IN  UCHAR   Value,
    IN  BOOLEAN Pressed
    )
{
    ULONG       Idx;
    if (Pressed) {
        for (Idx = 0; Idx < Size; ++Idx) {
            if (Array[Idx] == Value)
                break;
            if (Array[Idx] != 0)
                continue;
            Array[Idx] = Value;
            break;
        }
    } else {
        for (Idx = 0; Idx < Size; ++Idx) {
            if (Array[Idx] == 0)
                break;
            if (Array[Idx] != Value)
                continue;
            for (; Idx < Size - 1; ++Idx)
                Array[Idx] = Array[Idx + 1];
            Array[Size - 1] = 0;
            break;
        }
    }
}

static FORCEINLINE USHORT
KeyCodeToUsage(
    IN  ULONG   KeyCode
    )
{
    if (KeyCode < sizeof(VkbdKeyCodeToUsage)/sizeof(VkbdKeyCodeToUsage[0]))
        return VkbdKeyCodeToUsage[KeyCode];
    return 0;
}

VOID
HidEventMotion(
    IN  PXENVKBD_HID_CONTEXT    Context,
    IN  LONG                    dX,
    IN  LONG                    dY,
    IN  LONG                    dZ
    )
{
    Context->AbsMouseReport.X = (USHORT)Constrain(Context->AbsMouseReport.X + dX, 0, 32767);
    Context->AbsMouseReport.Y = (USHORT)Constrain(Context->AbsMouseReport.Y + dY, 0, 32767);
    Context->AbsMouseReport.dZ = -(CHAR)Constrain(dZ, -127, 127);

    Context->AbsMousePending = HidSendReadReport(Context,
                                                 &Context->AbsMouseReport,
                                                 sizeof(XENVKBD_HID_ABSMOUSE));
}

VOID
HidEventKeypress(
    IN  PXENVKBD_HID_CONTEXT    Context,
    IN  ULONG                   KeyCode,
    IN  BOOLEAN                 Pressed
    )
{
    if (KeyCode >= 0x110 && KeyCode <= 0x114) {
        // Mouse Buttons
        Context->AbsMouseReport.Buttons = SetBit(Context->AbsMouseReport.Buttons,
                                                 (UCHAR)(KeyCode - 0x110),
                                                 Pressed);

        Context->AbsMousePending = HidSendReadReport(Context,
                                                    &Context->AbsMouseReport,
                                                    sizeof(XENVKBD_HID_ABSMOUSE));

    } else {
        // map KeyCode to Usage
        USHORT  Usage = KeyCodeToUsage(KeyCode);
        if (Usage == 0)
            return; // non-standard key

        if (Usage >= 0xE0 && Usage <= 0xE7) {
            // Modifier
            Context->KeyboardReport.Modifiers = SetBit(Context->KeyboardReport.Modifiers,
                                                       (UCHAR)(Usage - 0xE0),
                                                       Pressed);
        } else {
            // Standard Key
            SetArray(Context->KeyboardReport.Keys,
                     6,
                     (UCHAR)Usage,
                     Pressed);
        }
        Context->KeyboardPending = HidSendReadReport(Context,
                                                    &Context->KeyboardReport,
                                                    sizeof(XENVKBD_HID_KEYBOARD));

    }
}

VOID
HidEventPosition(
    IN  PXENVKBD_HID_CONTEXT    Context,
    IN  ULONG                   X,
    IN  ULONG                   Y,
    IN  LONG                    dZ
    )
{
    Context->AbsMouseReport.X = (USHORT)Constrain(X, 0, 32767);
    Context->AbsMouseReport.Y = (USHORT)Constrain(Y, 0, 32767);
    Context->AbsMouseReport.dZ = -(CHAR)Constrain(dZ, -127, 127);

    Context->AbsMousePending = HidSendReadReport(Context,
                                                 &Context->AbsMouseReport,
                                                 sizeof(XENVKBD_HID_ABSMOUSE));
}
