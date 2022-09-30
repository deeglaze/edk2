/** @file

  Confidential Compute Dxe driver. This driver installs protocols that are
  generic over confidential compute techonology.

  Copyright (c) 2022, Google LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/MemEncryptTdxLib.h>
#include <Protocol/AcceptAllMemory.h>
#include <Protocol/ExitBootServicesCallback.h>
#include <Protocol/MemoryAccept.h>

STATIC EFI_HANDLE mCocoDxeHandle = NULL;

STATIC
EFI_STATUS
AcceptAllUnacceptedMemory (
  IN EFI_MEMORY_ACCEPT_PROTOCOL *AcceptMemory
  )
{
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *AllDescMap;
  UINTN                            NumEntries;
  UINTN                            Index;
  EFI_STATUS                       Status;
  BOOLEAN                          AcceptedAny;

  DEBUG ((DEBUG_INFO, "Accepting all memory\n"));
  AcceptedAny = FALSE;
  /*
   * Get a copy of the memory space map to iterate over while
   * changing the map.
   */
  Status = gDS->GetMemorySpaceMap (&NumEntries, &AllDescMap);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  for (Index = 0; Index < NumEntries; Index++) {
    CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *Desc;

    Desc = &AllDescMap[Index];
    if (Desc->GcdMemoryType != EfiGcdMemoryTypeUnaccepted) {
      continue;
    }

    Status = AcceptMemory->AcceptMemory (
      AcceptMemory,
      Desc->BaseAddress,
      Desc->Length
      );
    if (EFI_ERROR(Status)) {
      goto done;
    }

    Status = gDS->RemoveMemorySpace(Desc->BaseAddress, Desc->Length);
    if (EFI_ERROR(Status)) {
      goto done;
    }

    Status = gDS->AddMemorySpace (
      EfiGcdMemoryTypeSystemMemory,
      Desc->BaseAddress,
      Desc->Length,
      EFI_MEMORY_CPU_CRYPTO | EFI_MEMORY_XP | EFI_MEMORY_RO | EFI_MEMORY_RP
      );
    if (EFI_ERROR(Status)) {
      goto done;
    }

    AcceptedAny = TRUE;
  }

  // If any memory is accepted, cause ExitBootServices to fail with
  // EFI_INVALID_PARAMETER in order to force the caller to refresh
  // their view of the MemoryMap.
  if (AcceptedAny) {
    Status = EFI_INVALID_PARAMETER;
  }

done:
  gBS->FreePool (AllDescMap);
  return Status;
}

EFI_STATUS
EFIAPI
ResolveUnacceptedMemory (
  IN EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL  *This
  )
{
  EFI_MEMORY_ACCEPT_PROTOCOL *AcceptMemory;
  EFI_STATUS                 Status;

  if (This->Disabled) {
    return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (&gEfiMemoryAcceptProtocolGuid, NULL,
    (VOID **)&AcceptMemory);
  if (Status == EFI_NOT_FOUND) {
    return EFI_SUCCESS;
  }
  ASSERT_EFI_ERROR (Status);

  return AcceptAllUnacceptedMemory(AcceptMemory);
}

STATIC EDKII_EXIT_BOOT_SERVICES_CALLBACK_PROTOCOL mExitBootServicesCallbackProcotol = {
  ResolveUnacceptedMemory,
  FALSE,
};

STATIC
EFI_STATUS
EFIAPI
DisableAcceptAllMemory (
  IN  BZ3987_ACCEPT_ALL_MEMORY_PROTOCOL  *This
  )
{
  mExitBootServicesCallbackProcotol.Disabled = TRUE;
  return EFI_SUCCESS;
}

STATIC BZ3987_ACCEPT_ALL_MEMORY_PROTOCOL mAcceptAllMemoryProtocol = {
  DisableAcceptAllMemory,
};

EFI_STATUS
EFIAPI
CocoDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  //
  // Do nothing when confidential compute technologies that require memory
  // acceptance are not enabled.
  //
  if (!MemEncryptSevSnpIsEnabled () &&
      !MemEncryptTdxIsEnabled ()) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->InstallProtocolInterface (&mCocoDxeHandle,
                  &gEdkiiExitBootServicesCallbackProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mExitBootServicesCallbackProcotol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Install EdkiiExitBootServicesCallbackProtocol failed.\n"));
  }

  Status = gBS->InstallProtocolInterface (&mCocoDxeHandle,
                  &gBz3987AcceptAllMemoryProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mAcceptAllMemoryProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Install Bz3987AcceptAllMemoryProtocol failed.\n"));
  }

  return EFI_SUCCESS;
}
