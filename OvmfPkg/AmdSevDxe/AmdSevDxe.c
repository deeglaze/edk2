/** @file

  AMD Sev Dxe driver. This driver is dispatched early in DXE, due to being list
  in APRIORI. It clears C-bit from MMIO and NonExistent Memory space when SEV
  is enabled.

  Copyright (c) 2017 - 2020, AMD Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Q35MchIch9.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/ConfidentialComputingSevSnpBlob.h>
#include <Library/PcdLib.h>
#include <Protocol/MemoryAccept.h>

STATIC CONFIDENTIAL_COMPUTING_SNP_BLOB_LOCATION  mSnpBootDxeTable = {
  SIGNATURE_32 ('A',                                    'M', 'D', 'E'),
  1,
  0,
  (UINT64)(UINTN)FixedPcdGet32 (PcdOvmfSnpSecretsBase),
  FixedPcdGet32 (PcdOvmfSnpSecretsSize),
  (UINT64)(UINTN)FixedPcdGet32 (PcdOvmfCpuidBase),
  FixedPcdGet32 (PcdOvmfCpuidSize),
};

STATIC EFI_HANDLE mAmdSevDxeHandle = NULL;

STATIC
EFI_STATUS
EFIAPI
AmdSevMemoryAccept (
  IN EFI_MEMORY_ACCEPT_PROTOCOL *This,
  IN EFI_PHYSICAL_ADDRESS StartAddress,
  IN UINTN Size
)
{
  MemEncryptSevSnpPreValidateSystemRam (
    StartAddress,
    EFI_SIZE_TO_PAGES (Size)
    );

  return EFI_SUCCESS;
}

STATIC EFI_MEMORY_ACCEPT_PROTOCOL mMemoryAcceptProtocol = {
  AmdSevMemoryAccept
};

EFI_STATUS
EFIAPI
AmdSevDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *AllDescMap;
  UINTN                            NumEntries;
  UINTN                            Index;

  //
  // Do nothing when SEV is not enabled
  //
  if (!MemEncryptSevIsEnabled ()) {
    return EFI_UNSUPPORTED;
  }

  //
  // Iterate through the GCD map and clear the C-bit from MMIO and NonExistent
  // memory space. The NonExistent memory space will be used for mapping the
  // MMIO space added later (eg PciRootBridge). By clearing both known MMIO and
  // NonExistent memory space can gurantee that current and furture MMIO adds
  // will have C-bit cleared.
  //
  Status = gDS->GetMemorySpaceMap (&NumEntries, &AllDescMap);
  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < NumEntries; Index++) {
      CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *Desc;

      Desc = &AllDescMap[Index];
      if ((Desc->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo) ||
          (Desc->GcdMemoryType == EfiGcdMemoryTypeNonExistent))
      {
        Status = MemEncryptSevClearMmioPageEncMask (
                   0,
                   Desc->BaseAddress,
                   EFI_SIZE_TO_PAGES (Desc->Length)
                   );
        ASSERT_EFI_ERROR (Status);
      }
    }

    FreePool (AllDescMap);
  }

  //
  // If PCI Express is enabled, the MMCONFIG area has been reserved, rather
  // than marked as MMIO, and so the C-bit won't be cleared by the above walk
  // through the GCD map. Check for the MMCONFIG area and clear the C-bit for
  // the range.
  //
  if (PcdGet16 (PcdOvmfHostBridgePciDevId) == INTEL_Q35_MCH_DEVICE_ID) {
    Status = MemEncryptSevClearMmioPageEncMask (
               0,
               FixedPcdGet64 (PcdPciExpressBaseAddress),
               EFI_SIZE_TO_PAGES (SIZE_256MB)
               );

    ASSERT_EFI_ERROR (Status);
  }

  //
  // When SMM is enabled, clear the C-bit from SMM Saved State Area
  //
  // NOTES: The SavedStateArea address cleared here is before SMBASE
  // relocation. Currently, we do not clear the SavedStateArea address after
  // SMBASE is relocated due to the following reasons:
  //
  // 1) Guest BIOS never access the relocated SavedStateArea.
  //
  // 2) The C-bit works on page-aligned address, but the SavedStateArea
  // address is not a page-aligned. Theoretically, we could roundup the address
  // and clear the C-bit of aligned address but looking carefully we found
  // that some portion of the page contains code -- which will causes a bigger
  // issues for SEV guest. When SEV is enabled, all the code must be encrypted
  // otherwise hardware will cause trap.
  //
  // We restore the C-bit for this SMM Saved State Area after SMBASE relocation
  // is completed (See OvmfPkg/Library/SmmCpuFeaturesLib/SmmCpuFeaturesLib.c).
  //
  if (FeaturePcdGet (PcdSmmSmramRequire)) {
    UINTN  MapPagesBase;
    UINTN  MapPagesCount;

    Status = MemEncryptSevLocateInitialSmramSaveStateMapPages (
               &MapPagesBase,
               &MapPagesCount
               );
    ASSERT_EFI_ERROR (Status);

    //
    // Although these pages were set aside (i.e., allocated) by PlatformPei, we
    // could be after a warm reboot from the OS. Don't leak any stale OS data
    // to the hypervisor.
    //
    ZeroMem ((VOID *)MapPagesBase, EFI_PAGES_TO_SIZE (MapPagesCount));

    Status = MemEncryptSevClearPageEncMask (
               0,             // Cr3BaseAddress -- use current CR3
               MapPagesBase,  // BaseAddress
               MapPagesCount  // NumPages
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: MemEncryptSevClearPageEncMask(): %r\n",
        __FUNCTION__,
        Status
        ));
      ASSERT (FALSE);
      CpuDeadLoop ();
    }
  }

  Status = gBS->InstallProtocolInterface (
                  &mAmdSevDxeHandle,
                  &gEfiMemoryAcceptProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &mMemoryAcceptProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Install EfiMemoryAcceptProtocol failed.\n"));
  }

  //
  // If its SEV-SNP active guest then install the CONFIDENTIAL_COMPUTING_SEV_SNP_BLOB.
  // It contains the location for both the Secrets and CPUID page.
  //
  if (MemEncryptSevSnpIsEnabled ()) {
    return gBS->InstallConfigurationTable (
                  &gConfidentialComputingSevSnpBlobGuid,
                  &mSnpBootDxeTable
                  );
  }

  return EFI_SUCCESS;
}
