#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include "uefi.h"

EFI_STATUS virtio_gpu_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable,
                           UINT32 *out_w, UINT32 *out_h);
void virtio_gpu_present(void *pixels, UINT32 w, UINT32 h, UINT32 pitch);
uint8_t virtio_gpu_ok(void);

#endif
