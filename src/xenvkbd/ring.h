/* Copyright (c) Xen Project.
 * Copyright (c) Cloud Software Group, Inc.
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

#ifndef _XENVKBD_RING_H
#define _XENVKBD_RING_H

#include <ntddk.h>

#include <hid_interface.h>

#include "frontend.h"

typedef struct _XENVKBD_RING XENVKBD_RING, *PXENVKBD_RING;

extern NTSTATUS
RingInitialize(
    _In_ PXENVKBD_FRONTEND  Frontend,
    _Outptr_ PXENVKBD_RING  *Ring
    );

extern NTSTATUS
RingConnect(
    _Inout_ PXENVKBD_RING   Ring
    );

extern NTSTATUS
RingStoreWrite(
    _In_ PXENVKBD_RING              Ring,
    _In_ PXENBUS_STORE_TRANSACTION  Transaction
    );

extern NTSTATUS
RingEnable(
    _Inout_ PXENVKBD_RING   Ring
    );

extern VOID
RingDisable(
    _Inout_ PXENVKBD_RING   Ring
    );

extern VOID
RingDisconnect(
    _Inout_ PXENVKBD_RING   Ring
    );

extern VOID
RingTeardown(
    _Inout_ PXENVKBD_RING   Ring
    );

extern VOID
RingNotify(
    _Inout_ PXENVKBD_RING   Ring
    );

extern NTSTATUS
RingGetInputReport(
    _In_ PXENVKBD_RING                      Ring,
    _In_ ULONG                              ReportId,
    _Out_writes_bytes_opt_(Length) PVOID    Buffer,
    _In_ ULONG                              Length,
    _Out_ PULONG                            Returned
    );

extern VOID
RingReadReport(
    _Inout_ PXENVKBD_RING   Ring
    );

#endif  // _XENVKBD_RING_H
