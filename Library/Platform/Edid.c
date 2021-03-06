//Slice 2013

#include <Library/Platform/Platform.h>

#ifndef DEBUG_ALL
#ifndef DEBUG_EDID
#define DEBUG_EDID -1
#endif
#else
#ifdef DEBUG_EDID
#undef DEBUG_EDID
#endif
#define DEBUG_EDID DEBUG_ALL
#endif

#define DBG(...) DebugLog (DEBUG_EDID, __VA_ARGS__)

EFI_EDID_DISCOVERED_PROTOCOL     *EdidDiscovered;

EFI_STATUS
EFIAPI
GetEdidImpl (
  IN      EFI_EDID_OVERRIDE_PROTOCOL    *This,
  IN      EFI_HANDLE                    *ChildHandle,
  OUT     UINT32                        *Attributes,
  IN OUT  UINTN                         *EdidSize,
  IN OUT  UINT8                         **Edid
) {
  *Edid = gSettings.CustomEDID;
  *EdidSize = 128;
  *Attributes = 0;

  return *Edid ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_EDID_OVERRIDE_PROTOCOL gEdidOverride = { GetEdidImpl };

EFI_STATUS
InitializeEdidOverride () {
  EFI_STATUS                    Status;
  EFI_EDID_OVERRIDE_PROTOCOL    *EdidOverride;

  EdidOverride = AllocateCopyPool (sizeof (EFI_EDID_OVERRIDE_PROTOCOL), &gEdidOverride);

  Status = gBS->InstallMultipleProtocolInterfaces (
                   &gImageHandle,
                   &gEfiEdidOverrideProtocolGuid,
                   EdidOverride,
                   NULL
                 );

  if (EFI_ERROR (Status)) {
    DBG ("Can't install EdidOverride on ImageHandle\n");
  }

  return Status;
}

#if 0
UINT8 *
GetCurrentEdid () {
  EFI_STATUS                      Status;
  EFI_EDID_ACTIVE_PROTOCOL        *EdidProtocol;
  UINT8                           *Edid;

  DBG ("Edid:");
  Edid = NULL;

  Status = gBS->LocateProtocol (&gEfiEdidActiveProtocolGuid, NULL, (VOID **)&EdidProtocol);
  if (!EFI_ERROR (Status)) {
    DBG (" size=%d", EdidProtocol->SizeOfEdid);
    if (EdidProtocol->SizeOfEdid > 0) {
      Edid = AllocateCopyPool (EdidProtocol->SizeOfEdid, EdidProtocol->Edid);
    }
  }

  DBG (" %a\n", (Edid != NULL) ? "found" : "not found");

  return Edid;
}
#endif

EFI_STATUS
GetEdidDiscovered () {
  EFI_STATUS    Status;
  UINTN         i, j, N;

  gSettings.EDID = NULL;

  Status = gBS->LocateProtocol (&gEfiEdidDiscoveredProtocolGuid, NULL, (VOID **)&EdidDiscovered);

  if (!EFI_ERROR (Status)) {
    N = EdidDiscovered->SizeOfEdid;

    DBG ("EdidDiscovered size=%d\n", N);

    if (N == 0) {
      return EFI_NOT_FOUND;
    }

    gSettings.EDID = AllocateAlignedPages (EFI_SIZE_TO_PAGES (N), 128);
    if (!gSettings.CustomEDID) {
      gSettings.CustomEDID = gSettings.EDID; //copy pointer but data if no CustomEDID
    }

    CopyMem (gSettings.EDID, EdidDiscovered->Edid, N);

    for (i = 0; i < N; i += 16) {
      DBG ("%03d | ", i);

      for (j = 0; j < 16; j++) {
        DBG ("%02x%a", EdidDiscovered->Edid[i + j], (j < 15) ? " " : "");
      }

      DBG ("\n");
    }
  }

  return Status;
}
