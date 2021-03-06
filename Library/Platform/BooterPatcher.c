/*
 * Original idea of patching kernel by Evan Lojewsky, 2009
 *
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 * Correction and improvements by Clover team
 */

#include <Library/Platform/KernelPatcher.h>
#include <Library/PeCoffLib.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_BOOTER_PATCHER
#define DEBUG_BOOTER_PATCHER 0
#endif
#else
#ifdef DEBUG_BOOTER_PATCHER
#undef DEBUG_BOOTER_PATCHER
#endif
#define DEBUG_BOOTER_PATCHER DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_BOOTER_PATCHER, __VA_ARGS__)

BOOT_EFI_HEADER *
ParseBooterHeader (
  IN  VOID  *FileBuffer
) {
  EFI_IMAGE_SECTION_HEADER          *SectionHeader;
  EFI_IMAGE_DOS_HEADER              *DosHdr;
  EFI_IMAGE_OPTIONAL_HEADER_UNION   *PeHdr;
  BOOT_EFI_HEADER                   *Ret = NULL;
  UINT32                            Index;

  DBG ("%a: Start\n", __FUNCTION__);

  DosHdr = (EFI_IMAGE_DOS_HEADER *)FileBuffer;
  if (DosHdr->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
    // NO DOS header, check for PE/COFF header
    PeHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(FileBuffer);
    if (PeHdr->Pe32.Signature != EFI_IMAGE_NT_SIGNATURE) {
      DBG ("DOS header signature was not found.\n");
      goto Finish;
    }

    DosHdr = NULL;
  } else {
    PeHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)((UINT8 *)FileBuffer + DosHdr->e_lfanew);
    if (PeHdr->Pe32.Signature != EFI_IMAGE_NT_SIGNATURE) {
      DBG ("PE header signature was not found.\n");
      goto Finish;
    }
  }

  SectionHeader = (EFI_IMAGE_SECTION_HEADER *)((UINT8 *)&(PeHdr->Pe32.OptionalHeader) + PeHdr->Pe32.FileHeader.SizeOfOptionalHeader);

  if (PeHdr->Pe32.FileHeader.NumberOfSections) {
    Ret = AllocateZeroPool (sizeof (BOOT_EFI_HEADER));

    Ret->NumSections = PeHdr->Pe32.FileHeader.NumberOfSections;

    for (Index = 0; Index < PeHdr->Pe32.FileHeader.NumberOfSections; Index++, SectionHeader++) {
      if (AsciiStrCmp ((CHAR8 *)SectionHeader->Name, ".text") == 0) {
        Ret->TextVA = SectionHeader->VirtualAddress;
        Ret->TextOffset = SectionHeader->PointerToRawData;
        Ret->TextSize = SectionHeader->SizeOfRawData;
      }

      else if (AsciiStrCmp ((CHAR8 *)SectionHeader->Name, ".data") == 0) {
        Ret->DataVA = SectionHeader->VirtualAddress;
        Ret->DataOffset = SectionHeader->PointerToRawData;
        Ret->DataSize = SectionHeader->SizeOfRawData;
      }

      DBG (" - SectionHeader->Name: %a\n", (CHAR8 *)SectionHeader->Name);
    }
  }

  Finish:

  DBG ("%a: End\n", __FUNCTION__);

  return Ret;
}

VOID
PatchBooter (
  IN LOADER_ENTRY       *Entry,
  IN EFI_LOADED_IMAGE   *LoadedImage,
  IN BOOT_EFI_HEADER    *BootEfiHeader,
  IN CHAR8              *OSVer
) {
  if (
    gSettings.BooterPatchesAllowed &&
    (Entry->KernelAndKextPatches->BooterPatches != NULL) &&
    Entry->KernelAndKextPatches->NrBooters
  ) {
    UINTN  i = 0, Num = 0;

    MsgLog ("%a: Start\n", __FUNCTION__);

    for (; i < Entry->KernelAndKextPatches->NrBooters; ++i) {
      MsgLog (" - [%02d]: %a | [MatchOS: %a]",
        i,
        Entry->KernelAndKextPatches->BooterPatches[i].Label,
        Entry->KernelAndKextPatches->BooterPatches[i].MatchOS
          ? Entry->KernelAndKextPatches->BooterPatches[i].MatchOS
          : "All"
      );

      Entry->KernelAndKextPatches->BooterPatches[i].Disabled = !IsPatchEnabled (
        Entry->KernelAndKextPatches->BooterPatches[i].MatchOS, OSVer);

      if (!Entry->KernelAndKextPatches->BooterPatches[i].Disabled) {
        Num = SearchAndReplace (
          (UINT8 *)LoadedImage->ImageBase,
          (UINT32)LoadedImage->ImageSize,
          Entry->KernelAndKextPatches->BooterPatches[i].Data,
          Entry->KernelAndKextPatches->BooterPatches[i].DataLen,
          Entry->KernelAndKextPatches->BooterPatches[i].Patch,
          Entry->KernelAndKextPatches->BooterPatches[i].Wildcard,
          Entry->KernelAndKextPatches->BooterPatches[i].Count
        );
      }

      MsgLog (
        " | Allowed: %a | %r: %d replaces done\n",
        Entry->KernelAndKextPatches->BooterPatches[i].Disabled ? "No" : "Yes",
        Num ? EFI_SUCCESS : EFI_NOT_FOUND,
        Num
      );
    }

    MsgLog ("%a: End\n", __FUNCTION__);
  }
}
