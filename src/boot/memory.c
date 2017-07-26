#include <efi.h>
#include <efilib.h>

#include "memory.h"
#include "utility.h"

const UINTN PAGE_SIZE = 4096;

const CHAR16 *memory_type_names[] = {
    L"EfiReservedMemoryType",
    L"EfiLoaderCode",
    L"EfiLoaderData",
    L"EfiBootServicesCode",
    L"EfiBootServicesData",
    L"EfiRuntimeServicesCode",
    L"EfiRuntimeServicesData",
    L"EfiConventionalMemory",
    L"EfiUnusableMemory",
    L"EfiACPIReclaimMemory",
    L"EfiACPIMemoryNVS",
    L"EfiMemoryMappedIO",
    L"EfiMemoryMappedIOPortSpace",
    L"EfiPalCode",
    L"EfiPersistentMemory",
};

static const CHAR16 *memory_type_name(UINT32 value) {
    if (value >= (sizeof(memory_type_names)/sizeof(CHAR16*)))
        return L"Bad Type Value";
    return memory_type_names[value];
}

void EFIAPI memory_update_addresses(EFI_EVENT UNUSED *event, void UNUSED *context) {
    ST->RuntimeServices->ConvertPointer(0, (void **)&BS);
    ST->RuntimeServices->ConvertPointer(0, (void **)&ST);
}

EFI_STATUS memory_virtualize() {
    EFI_STATUS status;
    EFI_EVENT event;

    status = ST->BootServices->CreateEvent(EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
            TPL_CALLBACK, (EFI_EVENT_NOTIFY)&memory_update_addresses, NULL, &event);
    CHECK_EFI_STATUS_OR_RETURN(status, "Failed to create memory update event");

    return status;
}

EFI_STATUS memory_get_map(EFI_MEMORY_DESCRIPTOR **buffer, UINTN *buffer_size,
                          UINTN *key, UINTN *desc_size, UINT32 *desc_version) {
    EFI_STATUS status;

    UINTN needs_size = 0;
    status = ST->BootServices->GetMemoryMap(&needs_size, 0, key, desc_size, desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        CHECK_EFI_STATUS_OR_RETURN(status, "Failed to load memory map");
    }

    // Give some extra buffer to account for changes.
    *buffer_size = needs_size + 256;
    Print(L"Trying to load memory map with size %d.\n", *buffer_size);
    status = ST->BootServices->AllocatePool(EfiLoaderData, *buffer_size, (void**)buffer);
    CHECK_EFI_STATUS_OR_RETURN(status, "Failed to allocate space for memory map");

    status = ST->BootServices->GetMemoryMap(buffer_size, *buffer, key, desc_size, desc_version);
    CHECK_EFI_STATUS_OR_RETURN(status, "Failed to load memory map");
    return EFI_SUCCESS;
}

EFI_STATUS memory_dump_map() {
    EFI_MEMORY_DESCRIPTOR *buffer;
    UINTN buffer_size, desc_size, key;
    UINT32 desc_version;

    EFI_STATUS status = memory_get_map(&buffer, &buffer_size, &key,
        &desc_size, &desc_version);
    CHECK_EFI_STATUS_OR_RETURN(status, "Failed to get memory map");

    const UINTN count = buffer_size / desc_size;

    Print(L"Memory map:\n");
    Print(L"\t  Descriptor Count: %d (%d bytes)\n", count, buffer_size);
    Print(L"\t       Version Key: %d\n", key);
    Print(L"\tDescriptor Version: %d (%d bytes)\n\n", desc_version, desc_size);

    EFI_MEMORY_DESCRIPTOR *end = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)buffer + buffer_size);
    EFI_MEMORY_DESCRIPTOR *d = buffer;
    while (d < end) {
        UINTN size_bytes = d->NumberOfPages * PAGE_SIZE;

        Print(L"Type: %s  Attr: 0x%x\n", memory_type_name(d->Type), d->Attribute);
        Print(L"\t Physical  %016llx - %016llx\n", d->PhysicalStart, d->PhysicalStart + size_bytes);
        Print(L"\t  Virtual  %016llx - %016llx\n\n", d->VirtualStart, d->VirtualStart + size_bytes);

        d = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)d + desc_size);
    }

    ST->BootServices->FreePool(buffer);
    return EFI_SUCCESS;
}