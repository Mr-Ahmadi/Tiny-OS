#ifndef UEFI_H
#define UEFI_H

#include <stdint.h>

/* Basic EFI types */
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t  UINT8;
typedef int64_t  INT64;
typedef int32_t  INT32;
typedef int16_t  INT16;
typedef int8_t   INT8;
typedef char     CHAR8;
typedef uint16_t CHAR16;
typedef uint64_t UINTN;
typedef int64_t  INTN;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;

typedef void *EFI_HANDLE;
typedef void *EFI_EVENT;

typedef UINTN EFI_STATUS;

#define EFI_SUCCESS               0
#define EFI_ERROR(status)        ((status) & 0x8000000000000000ULL)
#define EFI_ERR(code)            (0x8000000000000000ULL | (code))
#define EFI_NOT_FOUND            EFI_ERR(1)
#define EFI_DEVICE_ERROR         EFI_ERR(7)
#define EFI_INVALID_PARAMETER    EFI_ERR(2)
#define EFI_UNSUPPORTED          EFI_ERR(3)
#define EFI_TIMEOUT              EFI_ERR(14)
#define EFI_OUT_OF_RESOURCES     EFI_ERR(9)
#define EFI_NOT_READY            EFI_ERR(6)
#define EFI_SIZE_TO_PAGES(s)     (((s) + 0xFFF) >> 12)

/* EFI GUID */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/* EFI Table Header */
typedef struct {
    UINT64  Signature;
    UINT32  Revision;
    UINT32  HeaderSize;
    UINT32  CRC32;
    UINT32  Reserved;
} EFI_TABLE_HEADER;

/* Memory types */
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/* Allocation types */
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

/* Time */
typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

/* Graphics Output Protocol */
typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

/* Blt operations */
#define BltVideoFill         0
#define BltVideoToBltBuffer  1
#define BltBufferToVideo     2
#define BltVideoToVideo      3

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    EFI_STATUS (*QueryMode)(void *this, UINT32 ModeNumber, UINTN *SizeOfInfo,
                            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
    EFI_STATUS (*SetMode)(void *this, UINT32 ModeNumber);
    EFI_STATUS (*Blt)(void *this, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
                      UINT32 BltOperation, UINTN SourceX, UINTN SourceY,
                      UINTN DestinationX, UINTN DestinationY,
                      UINTN Width, UINTN Height, UINTN Delta);
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

/* Simple Text Input Protocol (basic) */
typedef struct {
    EFI_STATUS (*Reset)(void *this, UINT8 ExtendedVerification);
    EFI_STATUS (*ReadKeyStroke)(void *this, EFI_INPUT_KEY *Key);
    EFI_EVENT  WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* Simple Text Output Protocol */
typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    UINT32 Attribute;
    INT32  CursorColumn;
    INT32  CursorRow;
    UINT8  CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef struct {
    EFI_STATUS (*Reset)(void *this, UINT8 ExtendedVerification);
    EFI_STATUS (*OutputString)(void *this, CHAR16 *String);
    EFI_STATUS (*TestString)(void *this, CHAR16 *String);
    EFI_STATUS (*QueryMode)(void *this, UINTN ModeNumber, UINTN *Columns, UINTN *Rows);
    EFI_STATUS (*SetMode)(void *this, UINTN ModeNumber);
    EFI_STATUS (*SetAttribute)(void *this, UINTN Attribute);
    EFI_STATUS (*ClearScreen)(void *this);
    EFI_STATUS (*SetCursorPosition)(void *this, UINTN Column, UINTN Row);
    EFI_STATUS (*EnableCursor)(void *this, UINT8 Visible);
    SIMPLE_TEXT_OUTPUT_MODE *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/* Boot Services */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    EFI_STATUS (*AllocatePages)(EFI_ALLOCATE_TYPE Type, EFI_MEMORY_TYPE MemoryType,
                                UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory);
    EFI_STATUS (*FreePages)(EFI_PHYSICAL_ADDRESS Memory, UINTN Pages);
    EFI_STATUS (*GetMemoryMap)(UINTN *MemoryMapSize, void *MemoryMap,
                               UINTN *MapKey, UINTN *DescriptorSize,
                               UINT32 *DescriptorVersion);
    EFI_STATUS (*AllocatePool)(EFI_MEMORY_TYPE PoolType, UINTN Size, void **Buffer);
    EFI_STATUS (*FreePool)(void *Buffer);
    void *CreateEvent;
    void *SetTimer;
    EFI_STATUS (*WaitForEvent)(UINTN NumberOfEvents, EFI_EVENT *Event, UINTN *Index);
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    void *HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    EFI_STATUS (*Exit)(EFI_HANDLE ImageHandle, EFI_STATUS ExitStatus,
                       UINTN ExitDataSize, CHAR16 *ExitData);
    void *UnloadImage;
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void *GetNextMonotonicCount;
    EFI_STATUS (*Stall)(UINTN Microseconds);
    void *SetWatchdogTimer;
    void *ConnectController;
    void *DisconnectController;
    EFI_STATUS (*OpenProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                               void **Interface, EFI_HANDLE AgentHandle,
                               EFI_HANDLE ControllerHandle, UINT32 Attributes);
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    void *LocateHandleBuffer;
    EFI_STATUS (*LocateProtocol)(EFI_GUID *Protocol, void *Registration,
                                 void **Interface);
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;
    void *CalculateCrc32;
    void *CopyMem;
    void *SetMem;
    void *CreateEventEx;
} EFI_BOOT_SERVICES;

/* Reset types */
typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

/* Runtime Services */
typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *GetTime;
    void *SetTime;
    void *GetWakeupTime;
    void *SetWakeupTime;
    void *SetVirtualAddressMap;
    void *ConvertPointer;
    void *GetVariable;
    void *GetNextVariableName;
    void *SetVariable;
    void *GetNextHighMonotonicCount;
    void (*ResetSystem)(EFI_RESET_TYPE ResetType, EFI_STATUS ResetStatus,
                        UINTN DataSize, void *ResetData);
    void *UpdateCapsule;
    void *QueryCapsuleCapabilities;
    void *QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

/* System Table */
typedef struct {
    EFI_TABLE_HEADER               Hdr;
    CHAR16                         *FirmwareVendor;
    UINT32                         FirmwareRevision;
    EFI_HANDLE                     ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE                     ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE                     StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    EFI_RUNTIME_SERVICES           *RuntimeServices;
    EFI_BOOT_SERVICES              *BootServices;
    UINTN                          NumberOfTableEntries;
    void                           *ConfigurationTable;
} EFI_SYSTEM_TABLE;

/* Protocol GUIDs */
#define GUID_GRAPHICS_OUTPUT \
    {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}}
#define GUID_ABSOLUTE_POINTER \
    {0x8D59D32B, 0xC332, 0x4DF3, {0x8B, 0x5E, 0xE1, 0x6A, 0xFA, 0x56, 0x7C, 0x11}}

/* Absolute Pointer Protocol */
typedef struct {
    UINT64 AbsoluteMinX;
    UINT64 AbsoluteMinY;
    UINT64 AbsoluteMinZ;
    UINT64 AbsoluteMaxX;
    UINT64 AbsoluteMaxY;
    UINT64 AbsoluteMaxZ;
    UINT32 Attributes;
} EFI_ABSOLUTE_POINTER_MODE;

typedef struct {
    UINT64 CurrentX;
    UINT64 CurrentY;
    UINT64 CurrentZ;
    UINT32 ActiveButtons;
} EFI_ABSOLUTE_POINTER_STATE;

typedef struct {
    EFI_STATUS (*Reset)(void *this, UINT8 ExtendedVerification);
    EFI_STATUS (*GetState)(void *this, EFI_ABSOLUTE_POINTER_STATE *State);
    EFI_EVENT  WaitForInput;
    EFI_ABSOLUTE_POINTER_MODE *Mode;
} EFI_ABSOLUTE_POINTER_PROTOCOL;

/* Simple File System Protocol */
typedef struct EFI_FILE_PROTOCOL_s EFI_FILE_PROTOCOL;

#define EFI_FILE_MODE_READ      0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE     0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE    0x8000000000000000ULL

#define EFI_FILE_READ_ONLY      0x0000000000000001ULL
#define EFI_FILE_HIDDEN         0x0000000000000002ULL
#define EFI_FILE_SYSTEM         0x0000000000000004ULL
#define EFI_FILE_RESERVED       0x0000000000000008ULL
#define EFI_FILE_DIRECTORY      0x0000000000000010ULL
#define EFI_FILE_ARCHIVE        0x0000000000000020ULL
#define EFI_FILE_VALID_ATTR     0x0000000000000037ULL

typedef struct {
    EFI_EVENT  Event;
    EFI_STATUS Status;
    UINTN      BufferSize;
    void       *Buffer;
} EFI_FILE_IO_TOKEN;

struct EFI_FILE_PROTOCOL_s {
    UINT64 Revision;
    EFI_STATUS (*Open)(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **NewHandle,
                       CHAR16 *FileName, UINT64 OpenMode, UINT64 Attributes);
    EFI_STATUS (*Close)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (*Delete)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (*Read)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize, void *Buffer);
    EFI_STATUS (*Write)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize, void *Buffer);
    EFI_STATUS (*GetPosition)(EFI_FILE_PROTOCOL *This, UINT64 *Position);
    EFI_STATUS (*SetPosition)(EFI_FILE_PROTOCOL *This, UINT64 Position);
    EFI_STATUS (*GetInfo)(EFI_FILE_PROTOCOL *This, EFI_GUID *InformationType,
                          UINTN *BufferSize, void *Buffer);
    EFI_STATUS (*SetInfo)(EFI_FILE_PROTOCOL *This, EFI_GUID *InformationType,
                          UINTN *BufferSize, void *Buffer);
    EFI_STATUS (*Flush)(EFI_FILE_PROTOCOL *This);
    EFI_STATUS (*OpenEx)(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **NewHandle,
                         CHAR16 *FileName, UINT64 OpenMode, UINT64 Attributes,
                         EFI_FILE_IO_TOKEN *Token);
    EFI_STATUS (*ReadEx)(EFI_FILE_PROTOCOL *This, EFI_FILE_IO_TOKEN *Token);
    EFI_STATUS (*WriteEx)(EFI_FILE_PROTOCOL *This, EFI_FILE_IO_TOKEN *Token);
    EFI_STATUS (*FlushEx)(EFI_FILE_PROTOCOL *This, EFI_FILE_IO_TOKEN *Token);
};

#define GUID_FILE_INFO \
    {0x09576E92, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

typedef struct {
    UINT64 Size;
    UINT64 ReadOnly;
    UINT64 VolumeSize;
    UINT64 FreeSpace;
    UINT32 BlockSize;
    CHAR16 VolumeLabel[1];
} EFI_FILE_SYSTEM_INFO;

typedef struct {
    UINT64 Revision;
    EFI_STATUS (*OpenVolume)(void *This, EFI_FILE_PROTOCOL **Root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

#define GUID_SIMPLE_FILE_SYSTEM \
    {0x0964E5B22, 0x6459, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

/* Application entry point */
typedef EFI_STATUS (*EFI_ENTRY_POINT)(EFI_HANDLE ImageHandle,
                                      EFI_SYSTEM_TABLE *SystemTable);

#define EFIAPI

/* PCI IO Protocol */
#define GUID_PCI_IO \
    {0x4cf5b200, 0x68b8, 0x4ca5, {0x9e, 0xec, 0xb2, 0x3e, 0x3f, 0x50, 0x02, 0x9a}}

#define EFI_PCI_IO_PROTOCOL_WIDTH uint64_t   /* forced to UINT64 */
#define EfiPciIoWidthUint8   0
#define EfiPciIoWidthUint16  1
#define EfiPciIoWidthUint32  2
#define EfiPciIoWidthUint64  3
#define EfiPciIoWidthMax     4

#define EFI_PCI_IO_ATTRIBUTE_MEMORY_WRITE_COMBINE  0x0000000000000001ULL
#define EFI_PCI_IO_ATTRIBUTE_ENABLE                 0x0000000000000040ULL

/* Minimal EFI_PCI_IO_PROTOCOL interface */
typedef struct EFI_PCI_IO_PROTOCOL_s EFI_PCI_IO_PROTOCOL;
struct EFI_PCI_IO_PROTOCOL_s {
    void *PollMem;
    void *PollIo;
    EFI_STATUS (*MemRead)(EFI_PCI_IO_PROTOCOL *This, UINTN Width, UINT8 BarIndex,
                          UINT64 Offset, UINTN Count, void *Buffer);
    EFI_STATUS (*MemWrite)(EFI_PCI_IO_PROTOCOL *This, UINTN Width, UINT8 BarIndex,
                           UINT64 Offset, UINTN Count, void *Buffer);
    void *IoRead;
    void *IoWrite;
    EFI_STATUS (*ConfigRead)(EFI_PCI_IO_PROTOCOL *This, UINTN Width, UINT32 Offset,
                             UINTN Count, void *Buffer);
    EFI_STATUS (*ConfigWrite)(EFI_PCI_IO_PROTOCOL *This, UINTN Width, UINT32 Offset,
                              UINTN Count, void *Buffer);
    void *CopyMem;
    EFI_STATUS (*Map)(EFI_PCI_IO_PROTOCOL *This, UINTN Operation,
                      void *HostAddress, UINTN *NumberOfBytes, UINT64 *DeviceAddress);
    EFI_STATUS (*Unmap)(EFI_PCI_IO_PROTOCOL *This, UINT64 DeviceAddress);
    EFI_STATUS (*AllocateBuffer)(EFI_PCI_IO_PROTOCOL *This, UINTN Type,
                                 UINTN MemoryType, UINTN Pages, void **HostAddress,
                                 UINT64 Attributes);
    EFI_STATUS (*FreeBuffer)(EFI_PCI_IO_PROTOCOL *This, UINTN Pages, void *HostAddress);
    void *Flush;
    EFI_STATUS (*GetLocation)(EFI_PCI_IO_PROTOCOL *This, UINTN *SegmentNumber,
                              UINTN *BusNumber, UINTN *DeviceNumber,
                              UINTN *FunctionNumber);
    EFI_STATUS (*Attributes)(EFI_PCI_IO_PROTOCOL *This, UINTN Operation,
                             UINT64 Attributes, UINT64 *Result);
    void *GetBarAttributes;
    void *SetBarAttributes;
    UINT64 RomSize;
    void *RomImage;
};

/* Virtio PCI capability */
typedef struct {
    UINT8  CapVndr;     /* 0x09 */
    UINT8  CapNext;
    UINT8  CapLen;
    UINT8  CfgType;     /* 1=Common, 2=Notify, 3=ISR, 4=Device, 5=PCI */
    UINT8  Bar;
    UINT8  Padding[3];
    UINT32 Offset;
    UINT32 Length;
} VIRTIO_PCI_CAP;

/* Modern virtio common config registers (offset from CommonCfg BAR+offset) */
typedef volatile struct {
    UINT32 DeviceFeatureSelect;
    UINT32 DeviceFeature;
    UINT32 DriverFeatureSelect;
    UINT32 DriverFeature;
    UINT16 ConfigMSIXVector;
    UINT16 NumQueues;
    UINT8  DeviceStatus;
    UINT8  ConfigGeneration;
    UINT16 QueueSelect;
    UINT16 QueueSize;
    UINT16 QueueMSIXVector;
    UINT16 QueueEnable;
    UINT16 QueueNotifyOff;
    UINT64 QueueDesc;
    UINT64 QueueDriver;
    UINT64 QueueDevice;
    UINT16 QueueReset;
} VIRTIO_PCI_COMMON_CFG;

/* Virtio device status bits */
#define VIRTIO_STATUS_RESET         0x00
#define VIRTIO_STATUS_ACKNOWLEDGE   0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_FAILED        0x80
#define VIRTIO_STATUS_FEATURES_OK   0x08
#define VIRTIO_STATUS_DRIVER_OK     0x04

/* Virtio feature bits */
#define VIRTIO_F_VERSION_1          0x80000000  /* bit 32 */

/* Virtio-gpu device and vendor IDs */
#define VIRTIO_VENDOR               0x1AF4
#define VIRTIO_GPU_DEVICE_TRANS     0x1050
#define VIRTIO_GPU_DEVICE_MODERN    0x1040

/* Virtqueue descriptor flags */
#define VRING_DESC_F_NEXT          0x01
#define VRING_DESC_F_WRITE         0x02

/* Virtqueue avail/used ring flags */
#define VRING_AVAIL_F_NO_INTERRUPT 0x01
#define VRING_USED_F_NO_NOTIFY     0x01

/* Virtio-gpu commands */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO      0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D    0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF        0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT           0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH        0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D   0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107

/* Virtio-gpu responses */
#define VIRTIO_GPU_RESP_OK_NODATA            0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO      0x1101
#define VIRTIO_GPU_RESP_OK_CREATE_2D         0x1103
#define VIRTIO_GPU_RESP_ERR_UNSPEC           0x1200

/* Virtio-gpu formats */
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM     1

/* Virtio-gpu control header */
typedef struct {
    UINT32 Type;
    UINT32 Flags;
    UINT64 FenceId;
    UINT32 CtxId;
    UINT32 Padding;
} VIRTIO_GPU_CTRL_HDR;

typedef struct {
    VIRTIO_GPU_CTRL_HDR Hdr;
    UINT32 RectX;
    UINT32 RectY;
    UINT32 RectW;
    UINT32 RectH;
    UINT32 ScanoutId;  /* padding/resource_id for SET_SCANOUT */
    UINT32 ResourceId;
} VIRTIO_GPU_SET_SCANOUT;

typedef struct {
    VIRTIO_GPU_CTRL_HDR Hdr;
    UINT32 ResourceId;
    UINT32 Format;
    UINT32 Width;
    UINT32 Height;
} VIRTIO_GPU_RESOURCE_CREATE_2D;

typedef struct {
    VIRTIO_GPU_CTRL_HDR Hdr;
    UINT32 ResourceId;
    UINT32 NrEntries;
    UINT32 Padding;
} VIRTIO_GPU_RESOURCE_ATTACH_BACKING;

typedef struct {
    UINT64 Addr;
    UINT32 Length;
    UINT32 Padding;
} VIRTIO_GPU_MEM_ENTRY;

typedef struct {
    VIRTIO_GPU_CTRL_HDR Hdr;
    UINT32 ResourceId;
    UINT32 RectX;
    UINT32 RectY;
    UINT32 RectW;
    UINT32 RectH;
    UINT64 Offset;
} VIRTIO_GPU_TRANSFER_TO_HOST_2D;

typedef struct {
    VIRTIO_GPU_CTRL_HDR Hdr;
    UINT32 ResourceId;
    UINT32 Padding;
} VIRTIO_GPU_RESOURCE_FLUSH;

/* Display info response */
typedef struct {
    UINT32 RectX;
    UINT32 RectY;
    UINT32 RectW;
    UINT32 RectH;
    UINT32 Enabled;
    UINT32 Flags;
} VIRTIO_GPU_DISPLAY_INFO;

#define VIRTIO_GPU_MAX_SCANOUTS 16

typedef struct {
    VIRTIO_GPU_CTRL_HDR Hdr;
    VIRTIO_GPU_DISPLAY_INFO Pinfo[VIRTIO_GPU_MAX_SCANOUTS];
} VIRTIO_GPU_RESP_DISPLAY_INFO;

/* Helper to compare GUIDs */
static inline int guid_eq(EFI_GUID *a, EFI_GUID *b) {
    return a->Data1 == b->Data1 && a->Data2 == b->Data2 &&
           a->Data3 == b->Data3 &&
           a->Data4[0] == b->Data4[0] && a->Data4[1] == b->Data4[1] &&
           a->Data4[2] == b->Data4[2] && a->Data4[3] == b->Data4[3] &&
           a->Data4[4] == b->Data4[4] && a->Data4[5] == b->Data4[5] &&
           a->Data4[6] == b->Data4[6] && a->Data4[7] == b->Data4[7];
}

#endif
