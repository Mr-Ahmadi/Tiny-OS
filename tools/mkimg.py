#!/usr/bin/env python3
"""Create a bootable UEFI disk image (raw FAT32, no partition table).

Some UEFI firmwares (especially QEMU's AAVMF) handle raw FAT32 images
better than GPT-partitioned disks for virtio-blk drives. The firmware
treats the whole disk as a FAT32 removable media and looks for
EFI/BOOT/BOOTAA64.EFI directly.
"""

import os
import sys
import subprocess

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(PROJECT_DIR, 'build')
IMAGE_PATH = os.path.join(BUILD_DIR, 'disk.img')
EFI_PATH = os.path.join(BUILD_DIR, 'BOOTAA64.EFI')

# ~63 MB FAT32 partition (safe for mformat with h=64 s=32)
SECTORS = 129024  # 63 MB in 512-byte sectors
SECTOR_SIZE = 512


def run(cmd, **kwargs):
    print(f"  + {' '.join(cmd)}")
    return subprocess.run(cmd, capture_output=True, text=True, **kwargs)


def main():
    if not os.path.exists(EFI_PATH):
        print(f"ERROR: {EFI_PATH} not found. Run 'make' first.")
        sys.exit(1)

    print(f"Creating FAT32 disk image: {IMAGE_PATH}")

    if os.path.exists(IMAGE_PATH):
        os.unlink(IMAGE_PATH)

    # Step 1: Create empty image
    print("  Creating empty image...")
    with open(IMAGE_PATH, 'wb') as f:
        f.truncate(SECTORS * SECTOR_SIZE)  # ~63 MB

    # Step 2: Format as FAT32
    # Geometry: 64 heads x 32 sectors/track = 2048 sectors/cylinder
    # Number of cylinders = SECTORS / 2048 = 63
    h, s, t = 64, 32, SECTORS // (64 * 32)
    r = run(['mformat', f'-i{IMAGE_PATH}',
             '-h', str(h), '-s', str(s), '-t', str(t), '-F', '::'])
    if r.returncode != 0:
        print(f"  mformat failed: {r.stderr}")
        sys.exit(1)

    # Step 3: Create directory structure
    run(['mmd', f'-i{IMAGE_PATH}', '::/EFI'])
    run(['mmd', f'-i{IMAGE_PATH}', '::/EFI/BOOT'])

    # Step 4: Copy EFI application
    r = run(['mcopy', f'-i{IMAGE_PATH}', EFI_PATH, '::/EFI/BOOT/BOOTAA64.EFI'])
    if r.returncode != 0:
        print(f"  mcopy failed: {r.stderr}")
        sys.exit(1)

    # Step 5: Create startup.nsh for auto-boot
    startup_nsh = os.path.join(BUILD_DIR, 'startup.nsh')
    with open(startup_nsh, 'w') as f:
        f.write('\\EFI\\BOOT\\BOOTAA64.EFI\n')
    run(['mcopy', f'-i{IMAGE_PATH}', startup_nsh, '::/startup.nsh'])

    # Step 6: Create test files for notepad
    test_dir = os.path.join(BUILD_DIR, 'testfiles')
    os.makedirs(test_dir, exist_ok=True)

    files = {
        'readme.txt': 'Welcome to TinyOS!\n\n'
                       'This is the built-in text editor.\n'
                       'You can edit, save, and load files.\n\n'
                       'Shortcuts:\n'
                       '  Ctrl+S  Save\n'
                       '  Ctrl+O  Open (next demo file)\n'
                       '  Cursor  Navigate\n'
                       '  Home/End Line start/end\n',

        'notes.txt': 'TinyOS Notepad Demo Notes\n'
                     '========================\n\n'
                     'Features:\n'
                     '- Text editing with keyboard\n'
                     '- File save/load via UEFI FS\n'
                     '- Multi-line support\n'
                     '- Scroll follows cursor\n',

        'hello.txt': 'Hello, TinyOS World!\n'
                     '====================\n\n'
                     'This file was created to demonstrate\n'
                     'the file browser and notepad working\n'
                     'together on the TinyOS operating system.\n',

        'test.txt': 'This is a test file.\n'
                    'Line 2: Testing 123\n'
                    'Line 3: The quick brown fox jumps\n'
                    'Line 4: over the lazy dog.\n'
                    'Line 5: ABCDEFGHIJKLMNOPQRSTUVWXYZ\n'
                    'Line 6: 0123456789\n'
                    'Line 7: Special chars: !@#$%%^&*()\n'
    }

    for name, content in files.items():
        fpath = os.path.join(test_dir, name)
        with open(fpath, 'w') as f:
            f.write(content)
        r = run(['mcopy', f'-i{IMAGE_PATH}', fpath, f'::/{name}'])
        if r.returncode != 0:
            print(f"  Warning: could not copy {name}: {r.stderr}")

    # Verify
    r = run(['mdir', f'-i{IMAGE_PATH}', '::/EFI/BOOT/'])
    if r.returncode == 0:
        for line in r.stdout.split('\n'):
            if 'BOOTAA64' in line:
                print(f"  {line.strip()}")

    size = os.path.getsize(IMAGE_PATH)
    print(f"  Done: {size} bytes ({size//1024//1024} MB)")

    print()
    print("To use in UTM (raw FAT32, no GPT):")
    print("  1. Create ARM64 VM -> UEFI Boot")
    print("  2. Add build/disk.img as a Drive (IDE, NVMe, or virtio)")
    print("  3. Display: virtio-gpu or virtio-gpu-gl")
    print("  4. Start the VM")
    print()
    print("If still at Shell>, try:")
    print("  Shell> fs0:")
    print("  fs0:> EFI\\BOOT\\BOOTAA64.EFI")


if __name__ == '__main__':
    main()
