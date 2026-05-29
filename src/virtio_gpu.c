#include "virtio_gpu.h"

#define VRING_SIZE  16
#define PACKED __attribute__((packed))

typedef struct { UINT64 Addr; UINT32 Len; UINT16 Flags; UINT16 Next; } PACKED vring_desc;
typedef struct { UINT16 Flags; UINT16 Idx; UINT16 Ring[VRING_SIZE]; } PACKED vring_avail;
typedef struct { UINT32 Id; UINT32 Len; } PACKED vring_used_elem;
typedef struct { UINT16 Flags; UINT16 Idx; vring_used_elem Ring[VRING_SIZE]; } PACKED vring_used;

static EFI_PCI_IO_PROTOCOL *g_pci;
static UINT8 g_ok;
static EFI_BOOT_SERVICES *g_bs;

static UINT32 g_c_bar, g_c_off;  /* common cfg */
static UINT32 g_n_bar, g_n_off;  /* notify */
static UINT32 g_n_mult;

static struct {
    void        *base;
    vring_desc  *desc;
    vring_avail *avail;
    vring_used  *used;
    UINT16      sz;
    UINT16      free_head;
} g_vq;

/* common config offsets */
#define C_DS    0x14
#define C_QSEL  0x16
#define C_QSZ   0x18
#define C_QEN   0x1C
#define C_QNO   0x1E
#define C_QDESC 0x20
#define C_QDRV  0x28
#define C_QDEV  0x30

static void my_memset(void *p, UINT8 c, UINTN n) {
    UINT8 *d = (UINT8*)p; for (UINTN i = 0; i < n; i++) d[i] = c;
}

static UINT32 rr32(UINT32 bar, UINT32 off) {
    UINT32 v; g_pci->MemRead(g_pci, 2, (UINT8)bar, off, 1, &v); return v;
}
static void rw32(UINT32 bar, UINT32 off, UINT32 v) {
    g_pci->MemWrite(g_pci, 2, (UINT8)bar, off, 1, &v);
}
static UINT16 rr16(UINT32 bar, UINT32 off) {
    UINT16 v; g_pci->MemRead(g_pci, 1, (UINT8)bar, off, 1, &v); return v;
}
static void rw16(UINT32 bar, UINT32 off, UINT16 v) {
    g_pci->MemWrite(g_pci, 1, (UINT8)bar, off, 1, &v);
}
static UINT8 rr8(UINT32 bar, UINT32 off) {
    UINT8 v; g_pci->MemRead(g_pci, 0, (UINT8)bar, off, 1, &v); return v;
}
static void rw8(UINT32 bar, UINT32 off, UINT8 v) {
    g_pci->MemWrite(g_pci, 0, (UINT8)bar, off, 1, &v);
}
static void rw64(UINT32 bar, UINT32 off, UINT64 v) {
    g_pci->MemWrite(g_pci, 3, (UINT8)bar, off, 1, &v);
}

static void cw8(UINT32 o, UINT8 v)  { rw8(g_c_bar, g_c_off + o, v); }
static UINT8 cr8(UINT32 o)          { return rr8(g_c_bar, g_c_off + o); }
static void cw16(UINT32 o, UINT16 v){ rw16(g_c_bar, g_c_off + o, v); }
static UINT16 cr16(UINT32 o)       { return rr16(g_c_bar, g_c_off + o); }
static void cw32(UINT32 o, UINT32 v){ rw32(g_c_bar, g_c_off + o, v); }
static UINT32 cr32(UINT32 o)       { return rr32(g_c_bar, g_c_off + o); }
static void cw64(UINT32 o, UINT64 v){ rw64(g_c_bar, g_c_off + o, v); }

static void mb(void) { __sync_synchronize(); }

UINT8 virtio_gpu_ok(void) { return g_ok; }

/* ---- PCI device ---- */
typedef EFI_STATUS (*EFI_LOC_HB)(
    UINTN, EFI_GUID*, void*, UINTN*, EFI_HANDLE**);

static EFI_STATUS find_device(EFI_HANDLE ih, EFI_SYSTEM_TABLE *st, EFI_HANDLE *out_devh)
{
    EFI_GUID g = GUID_PCI_IO;
    UINTN n = 0;
    EFI_HANDLE *buf = 0;
    EFI_STATUS s;

    s = ((EFI_LOC_HB)st->BootServices->LocateHandleBuffer)(
        0, &g, 0, &n, &buf);
    if (EFI_ERROR(s) || !buf) return EFI_DEVICE_ERROR;

    for (UINTN i = 0; i < n; i++) {
        EFI_PCI_IO_PROTOCOL *pci = 0;
        s = st->BootServices->OpenProtocol(buf[i], &g, (void**)&pci, ih, 0, 2);
        if (EFI_ERROR(s) || !pci) continue;
        UINT32 vd;
        pci->ConfigRead(pci, 2, 0, 1, &vd);
        UINT16 ven = vd & 0xFFFF;
        UINT16 dev = vd >> 16;
        if (ven == 0x1AF4 && (dev == 0x1050 || dev == 0x1040)) {
            g_pci = pci;
            if (out_devh) *out_devh = buf[i];
            break;
        }
    }

    ((EFI_STATUS (*)(void*))st->BootServices->FreePool)(buf);
    return g_pci ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/* ---- Disconnect firmware GPU driver ---- */
static EFI_STATUS disconnect_gpu_driver(EFI_HANDLE devh, EFI_SYSTEM_TABLE *st)
{
    if (!devh || !st->BootServices->DisconnectController)
        return EFI_UNSUPPORTED;

    /* Disconnect all drivers from this controller handle */
    EFI_STATUS (*dc)(EFI_HANDLE, EFI_HANDLE, EFI_HANDLE) =
        (void*)st->BootServices->DisconnectController;
    return dc(devh, 0, 0);
}

/* ---- PCI caps ---- */
static EFI_STATUS find_caps(void)
{
    UINT32 sts;
    g_pci->ConfigRead(g_pci, 2, 4, 1, &sts);
    if (!(sts & (1 << 4))) return EFI_UNSUPPORTED;

    UINT32 cpp;
    g_pci->ConfigRead(g_pci, 2, 0x34, 1, &cpp);
    UINT32 cap = cpp & 0xFF;

    UINT8 fc = 0, fn = 0;
    while (cap >= 0x40) {
        UINT8 buf[16];
        g_pci->ConfigRead(g_pci, 0, cap, 16, buf);
        UINT8 cid = buf[0], ctype = buf[3];
        if (cid != 0x09 && cid != 0x10) break;
        switch (ctype) {
        case 1: g_c_bar = buf[4]; g_c_off = *(UINT32*)(buf+8); fc = 1; break;
        case 2: g_n_bar = buf[4]; g_n_off = *(UINT32*)(buf+8); fn = 1; break;
        }
        if (!buf[1]) break;
        cap = buf[1];
    }
    if (!fc || !fn) return EFI_UNSUPPORTED;

    UINT32 cmd;
    g_pci->ConfigRead(g_pci, 2, 4, 1, &cmd);
    cmd |= 6;
    g_pci->ConfigWrite(g_pci, 2, 4, 1, &cmd);
    return EFI_SUCCESS;
}

/* ---- Device init ---- */
static EFI_STATUS dev_init(void)
{
    cw8(C_DS, 0);
    int t = 500;
    while (cr8(C_DS) != 0 && --t);
    if (!t) return EFI_TIMEOUT;

    cw8(C_DS, 1);  if (!(cr8(C_DS) & 1)) return EFI_DEVICE_ERROR;
    cw8(C_DS, 3);  if (!(cr8(C_DS) & 2)) return EFI_DEVICE_ERROR;

    /* negotiate features - require VIRTIO_F_VERSION_1 (bit 32) */
    cw32(8, 1); UINT32 dfh = cr32(0x0C);
    cw32(8, 0); UINT32 dfl = cr32(0x0C);
    cw32(8, 1); cw32(0x0C, dfh);
    cw32(8, 0); cw32(0x0C, dfl);

    cw8(C_DS, 0x0B);  if (!(cr8(C_DS) & 8)) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

/* ---- Virtqueue ---- */
static EFI_STATUS vq_setup(void)
{
    cw16(C_QSEL, 0);
    UINT16 qsz = cr16(C_QSZ);
    if (qsz == 0) return EFI_DEVICE_ERROR;
    if (qsz > VRING_SIZE) qsz = VRING_SIZE;
    g_vq.sz = qsz;

    UINTN desc_sz = qsz * 16;
    UINTN avail_sz = 4 + qsz * 2;
    UINTN used_sz  = 4 + qsz * 8;
    UINTN total = desc_sz + avail_sz + used_sz + 64;
    total = (total + 0xFFF) & ~0xFFF;
    UINTN pages = total / 4096;
    if (pages == 0) pages = 1;

    EFI_PHYSICAL_ADDRESS phys;
    EFI_STATUS s = g_bs->AllocatePages(AllocateAnyPages, EfiBootServicesData, pages, &phys);
    if (EFI_ERROR(s)) return s;

    g_vq.base = (void*)(uintptr_t)phys;
    my_memset(g_vq.base, 0, total);

    UINTN avail_off = (desc_sz + 1) & ~1;
    UINTN used_off  = (avail_off + avail_sz + 1) & ~1;

    g_vq.desc  = (vring_desc*)g_vq.base;
    g_vq.avail = (vring_avail*)((UINT8*)g_vq.base + avail_off);
    g_vq.used  = (vring_used*)((UINT8*)g_vq.base + used_off);

    for (UINT16 i = 0; i < qsz - 1; i++) g_vq.desc[i].Next = i + 1;
    g_vq.desc[qsz - 1].Next = 0xFFFF;
    g_vq.free_head = 0;
    g_vq.avail->Flags = 1;
    g_vq.avail->Idx = 0;

    cw64(C_QDESC, phys);
    cw64(C_QDRV,  phys + avail_off);
    cw64(C_QDEV,  phys + used_off);

    g_n_mult = cr16(C_QNO);

    cw16(C_QEN, 1);
    if (!cr16(C_QEN)) return EFI_DEVICE_ERROR;

    cw8(C_DS, 0x0F);
    if (!(cr8(C_DS) & 4)) return EFI_DEVICE_ERROR;

    return EFI_SUCCESS;
}

/* ---- VQ ops ---- */
static UINT16 va_desc(void) {
    if (g_vq.free_head == 0xFFFF) return 0xFFFF;
    UINT16 i = g_vq.free_head;
    g_vq.free_head = g_vq.desc[i].Next;
    g_vq.desc[i].Next = 0xFFFF;
    return i;
}
static void vf_desc(UINT16 i) {
    g_vq.desc[i].Next = g_vq.free_head;
    g_vq.free_head = i;
}
static void ntfy(void) {
    rw16(g_n_bar, g_n_off + g_n_mult, 0);
}

static EFI_STATUS submit(void *cmd, UINT32 clen, void *rsp, UINT32 rlen, UINT32 *rtype)
{
    if (!g_vq.desc) return EFI_NOT_READY;
    UINT16 dc = va_desc();
    if (dc == 0xFFFF) return EFI_OUT_OF_RESOURCES;
    UINT16 dr = va_desc();
    if (dr == 0xFFFF) { vf_desc(dc); return EFI_OUT_OF_RESOURCES; }

    UINT64 ca = (UINT64)(uintptr_t)cmd;
    UINT64 ra = (UINT64)(uintptr_t)rsp;

    g_vq.desc[dc].Addr  = ca;  g_vq.desc[dc].Len = clen;
    g_vq.desc[dc].Flags = 1;   g_vq.desc[dc].Next = dr;
    g_vq.desc[dr].Addr  = ra;  g_vq.desc[dr].Len = rlen;
    g_vq.desc[dr].Flags = 2;   g_vq.desc[dr].Next = 0xFFFF;

    UINT16 idx = g_vq.avail->Idx;
    g_vq.avail->Ring[idx & (g_vq.sz - 1)] = dc;
    mb();
    g_vq.avail->Idx = idx + 1;
    mb();
    ntfy();

    int to = 5000000;
    while (to--) { mb(); if (g_vq.used->Idx != idx) break; }
    if (to <= 0) { vf_desc(dc); vf_desc(dr); return EFI_TIMEOUT; }

    if (rtype && rsp) *rtype = ((VIRTIO_GPU_CTRL_HDR*)rsp)->Type;
    return EFI_SUCCESS;
}

/* ---- GPU commands ---- */
static EFI_STATUS cmd_gdi(UINT32 *w, UINT32 *h) {
    VIRTIO_GPU_CTRL_HDR c; VIRTIO_GPU_RESP_DISPLAY_INFO r;
    my_memset(&c,0,sizeof(c)); c.Type=0x0100; my_memset(&r,0,sizeof(r));
    UINT32 rt; EFI_STATUS s=submit(&c,sizeof(c),&r,sizeof(r),&rt);
    if (EFI_ERROR(s)) return s;
    if (rt!=0x1101) return EFI_DEVICE_ERROR;
    if (r.Pinfo[0].Enabled && r.Pinfo[0].RectW) { *w=r.Pinfo[0].RectW; *h=r.Pinfo[0].RectH; }
    else return EFI_NOT_FOUND;
    return EFI_SUCCESS;
}

static EFI_STATUS cmd_rc2(UINT32 rid,UINT32 w,UINT32 h) {
    struct PACKED { VIRTIO_GPU_RESOURCE_CREATE_2D c; VIRTIO_GPU_CTRL_HDR r; } b;
    my_memset(&b,0,sizeof(b)); b.c.Hdr.Type=0x0101; b.c.ResourceId=rid;
    b.c.Format=1; b.c.Width=w; b.c.Height=h;
    UINT32 rt; EFI_STATUS s=submit(&b,sizeof(b.c),&b.r,sizeof(b.r),&rt);
    if (EFI_ERROR(s)) return s;
    if (rt!=0x1100 && rt!=0x1103) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

static EFI_STATUS cmd_ss(UINT32 rid,UINT32 w,UINT32 h) {
    struct PACKED { VIRTIO_GPU_SET_SCANOUT c; VIRTIO_GPU_CTRL_HDR r; } b;
    my_memset(&b,0,sizeof(b)); b.c.Hdr.Type=0x0103; b.c.RectW=w; b.c.RectH=h;
    b.c.ResourceId=rid;
    UINT32 rt; EFI_STATUS s=submit(&b,sizeof(b.c),&b.r,sizeof(b.r),&rt);
    if (EFI_ERROR(s)) return s; if (rt!=0x1100) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

static EFI_STATUS cmd_rab(UINT32 rid,UINT64 addr,UINT32 sz) {
    struct PACKED { VIRTIO_GPU_RESOURCE_ATTACH_BACKING c; VIRTIO_GPU_MEM_ENTRY e; VIRTIO_GPU_CTRL_HDR r; } b;
    my_memset(&b,0,sizeof(b)); b.c.Hdr.Type=0x0106; b.c.ResourceId=rid;
    b.c.NrEntries=1; b.e.Addr=addr; b.e.Length=sz;
    UINT32 rt; EFI_STATUS s=submit(&b,sizeof(b.c)+sizeof(b.e),&b.r,sizeof(b.r),&rt);
    if (EFI_ERROR(s)) return s; if (rt!=0x1100) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

static EFI_STATUS cmd_t2h(UINT32 rid,UINT32 w,UINT32 h,UINT64 off) {
    VIRTIO_GPU_TRANSFER_TO_HOST_2D c; VIRTIO_GPU_CTRL_HDR r;
    my_memset(&c,0,sizeof(c)); c.Hdr.Type=0x0105; c.ResourceId=rid;
    c.RectW=w; c.RectH=h; c.Offset=off;
    my_memset(&r,0,sizeof(r)); UINT32 rt;
    EFI_STATUS s=submit(&c,sizeof(c),&r,sizeof(r),&rt);
    if (EFI_ERROR(s)) return s; if (rt!=0x1100) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

static EFI_STATUS cmd_rf(UINT32 rid) {
    VIRTIO_GPU_RESOURCE_FLUSH c; VIRTIO_GPU_CTRL_HDR r;
    my_memset(&c,0,sizeof(c)); c.Hdr.Type=0x0104; c.ResourceId=rid;
    my_memset(&r,0,sizeof(r)); UINT32 rt;
    EFI_STATUS s=submit(&c,sizeof(c),&r,sizeof(r),&rt);
    if (EFI_ERROR(s)) return s; if (rt!=0x1100) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

/* ---- Public API ---- */
static UINT32 g_rid = 1;
static UINT32 g_fbsz;

void virtio_gpu_present(void *pixels, UINT32 w, UINT32 h, UINT32 pitch)
{
    if (!g_ok) return;

    EFI_PHYSICAL_ADDRESS pa = (EFI_PHYSICAL_ADDRESS)(uintptr_t)pixels;
    UINT32 sz = h * pitch;

    if (!g_fbsz) {
        if (EFI_ERROR(cmd_rc2(g_rid, w, h))) { g_ok = 0; return; }
        if (EFI_ERROR(cmd_rab(g_rid, pa, sz))) { g_ok = 0; return; }
        if (EFI_ERROR(cmd_ss(g_rid, w, h))) { g_ok = 0; return; }
        g_fbsz = sz;
    }

    if (EFI_ERROR(cmd_t2h(g_rid, w, h, 0))) { g_ok = 0; return; }
    if (EFI_ERROR(cmd_rf(g_rid))) { g_ok = 0; return; }
}

EFI_STATUS virtio_gpu_init(EFI_HANDLE ih, EFI_SYSTEM_TABLE *st,
                            UINT32 *ow, UINT32 *oh)
{
    EFI_STATUS s;
    g_bs = st->BootServices;
    g_ok = 0;
    g_pci = 0;

    EFI_HANDLE devh = 0;
    s = find_device(ih, st, &devh);
    if (EFI_ERROR(s)) return s;

    /* Disconnect firmware's GPU driver to avoid conflicts */
    s = disconnect_gpu_driver(devh, st);
    if (EFI_ERROR(s)) return s;

    s = find_caps();
    if (EFI_ERROR(s)) return s;

    s = dev_init();
    if (EFI_ERROR(s)) return s;

    s = vq_setup();
    if (EFI_ERROR(s)) return s;

    g_ok = 1;

    s = cmd_gdi(ow, oh);
    if (EFI_ERROR(s) || *ow == 0 || *oh == 0) {
        *ow = 640; *oh = 480;
    }

    return EFI_SUCCESS;
}
