#include "fs.h"
#include "uefi.h"
#include <stdint.h>

static EFI_SYSTEM_TABLE *g_st;
static EFI_FILE_PROTOCOL *g_root;
static int g_fs_ok;

static void to_ascii(const CHAR16 *src, char *dst, int dst_max)
{
    int i;
    for (i = 0; src[i] && i < dst_max - 1; i++)
        dst[i] = (char)src[i];
    dst[i] = 0;
}

int fs_init(void *st)
{
    g_st = (EFI_SYSTEM_TABLE *)st;
    EFI_GUID sfsp_guid = GUID_SIMPLE_FILE_SYSTEM;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp = 0;
    EFI_STATUS s;

    s = g_st->BootServices->LocateProtocol(&sfsp_guid, 0, (void**)&sfsp);
    if (EFI_ERROR(s) || !sfsp) {
        g_fs_ok = 0;
        return 0;
    }
    s = sfsp->OpenVolume(sfsp, &g_root);
    if (EFI_ERROR(s) || !g_root) {
        g_fs_ok = 0;
        return 0;
    }
    g_fs_ok = 1;
    return 1;
}

int fs_list_root(dir_list_t *list)
{
    if (!g_fs_ok || !g_root) return 0;
    list->count = 0;

    EFI_FILE_PROTOCOL *dir = 0;
    if (EFI_ERROR(g_root->Open(g_root, &dir, L"\\", EFI_FILE_MODE_READ, 0)))
        return 0;
    if (!dir) return 0;

    char buf[512];
    UINTN sz = sizeof(buf);
    EFI_GUID finfo_guid = GUID_FILE_INFO;

    while (1) {
        sz = sizeof(buf);
        EFI_STATUS s = dir->Read(dir, &sz, buf);
        if (EFI_ERROR(s) || sz == 0) break;

        EFI_FILE_INFO *fi = (EFI_FILE_INFO *)buf;
        if (fi->Attribute & EFI_FILE_DIRECTORY) continue;

        char name[MAX_FILENAME];
        to_ascii(fi->FileName, name, MAX_FILENAME);

        if (name[0] == 0) continue;
        if (name[0] == '.') continue;

        if (list->count < MAX_FILES) {
            int i;
            for (i = 0; name[i] && i < MAX_FILENAME - 1; i++)
                list->names[list->count][i] = name[i];
            list->names[list->count][i] = 0;
            list->count++;
        }
    }
    dir->Close(dir);
    return list->count;
}

int fs_read(const char *filename, char *buf, int max_len)
{
    if (!g_fs_ok || !g_root) return 0;

    CHAR16 wpath[256];
    int i;
    wpath[0] = '\\'; wpath[1] = 0;
    for (i = 0; filename[i] && i < 126; i++)
        wpath[i+1] = (CHAR16)filename[i];
    wpath[i+1] = 0;

    EFI_FILE_PROTOCOL *f = 0;
    if (EFI_ERROR(g_root->Open(g_root, &f, wpath, EFI_FILE_MODE_READ, 0)))
        return 0;
    if (!f) return 0;

    UINTN sz = (UINTN)max_len;
    EFI_STATUS s = f->Read(f, &sz, buf);
    f->Close(f);

    if (EFI_ERROR(s)) return 0;
    if (sz >= (UINTN)max_len) sz = (UINTN)max_len - 1;
    buf[sz] = 0;
    return (int)sz;
}

int fs_write(const char *filename, const char *buf, int len)
{
    if (!g_fs_ok || !g_root) return 0;

    CHAR16 wpath[256];
    int i;
    wpath[0] = '\\'; wpath[1] = 0;
    for (i = 0; filename[i] && i < 126; i++)
        wpath[i+1] = (CHAR16)filename[i];
    wpath[i+1] = 0;

    EFI_FILE_PROTOCOL *f = 0;
    EFI_STATUS os = g_root->Open(g_root, &f, wpath,
                                EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(os) || !f) return 0;

    UINTN sz = (UINTN)len;
    EFI_STATUS ws = f->Write(f, &sz, (void*)buf);
    f->Close(f);

    return EFI_ERROR(ws) ? 0 : (int)sz;
}
