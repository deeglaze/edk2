/** @file
  The file provides the protocol that allows callbacks in ExitBootServices
  immediately before TerminateMemoryMap.

  Copyright (c) 2022, Google LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef EXIT_BOOT_SERVICES_CALLBACK_H_
#define EXIT_BOOT_SERVICES_CALLBACK_H_

/* This protocol is internal to EDK2 only */

#define EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL_GUID   {0xf5684799,                                             0x9a33,                                                 0x40f7,                                                 {0xa1, 0x5c, 0x10, 0x8e, 0x0e, 0x6b, 0x45, 0x25}}

typedef struct _EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL
    EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL;

/**
  @param This A pointer to a EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL.

  The returned status may only be EFI_SUCCESS or EFI_INVALID_PARAMETER.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_TERMINATE_MEMORY_MAP_PREHOOK)(
  IN  EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL  *This
  );

///
/// The EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL allows callbacks in
/// ExitBootServices immediately before TerminateMemoryMap.
///
struct _EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL {
  EDKII_TERMINATE_MEMORY_MAP_PREHOOK  TerminateMemoryMapPrehook;
  BOOLEAN                             Disabled;
};

extern EFI_GUID  gEdkiiExitBootServicesCallbackProtocolGuid;

#endif
