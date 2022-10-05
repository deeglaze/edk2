/** @file
  The file provides the protocol that disables the behavior that all memory
  gets accepted at ExitBootServices(). This protocol is only meant to be called
  by the OS loader, and not EDK2 itself.

  Copyright (c) 2022, Google LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef _ACCEPT_ALL_UNACCEPTED_MEMORY_H_
#define _ACCEPT_ALL_UNACCEPTED_MEMORY_H_

#define BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL_GUID \
  {0xc5a010fe, \
   0x38a7, \
   0x4531, \
   {0x8a, 0x4a, 0x05, 0x00, 0xd2, 0xfd, 0x16, 0x49}}

typedef struct _BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL
    BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL;

/**
  @param This A pointer to a BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL.
**/
typedef
EFI_STATUS
(EFIAPI *BZ3987_DISABLE_ACCEPT_ALL_UNACCEPTED_MEMORY)(
  IN  BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL  *This
  );

///
/// The BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL allows the OS loader to
/// indicate to EDK2 that ExitBootServices should not accept all memory.
///
struct _BZ3987_ACCEPT_ALL_UNACCEPTED_MEMORY_PROTOCOL {
  BZ3987_DISABLE_ACCEPT_ALL_UNACCEPTED_MEMORY  Disable;
};

extern EFI_GUID  gBz3987AcceptAllUnacceptedMemoryProtocolGuid;

#endif
