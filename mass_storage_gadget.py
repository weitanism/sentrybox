import subprocess
from argparse import ArgumentParser
from pathlib import Path
from typing import List


class GadgetException(Exception):
    pass


def run_shell_command(command: List[str], raise_on_error: bool = True):
    print(f"execute shell command: {' '.join(command)}")
    ret_code = subprocess.call(command)
    if ret_code != 0 and raise_on_error:
        raise GadgetException(f"failed to run command, exit code: {ret_code}.")
    return ret_code


def initialize_backing_file(path: Path, size_gb: int):
    if path.exists():
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    run_shell_command(["fallocate", "-l", f"{size_gb}GiB", str(path)])
    run_shell_command(["mkfs.vfat", "-F", "32", "-I", str(path)])


def enable_gadget(backing_file: Path, size_gb: int):
    initialize_backing_file(backing_file, size_gb)

    run_shell_command(
        [
            "modprobe",
            "g_mass_storage",
            f'file="{backing_file}"',
        ]
    )


def disable_gadget():
    run_shell_command(
        [
            "modprobe",
            "-r",
            "g_mass_storage",
        ]
    )


def is_mounted(mount_path: Path):
    return (
        run_shell_command(["findmnt", str(mount_path)], raise_on_error=False)
        == 0
    )


def mount(path: Path, mount_path: Path, readwrite: bool):
    if not mount_path.exists():
        mount_path.mkdir(parents=True, exist_ok=True)

    if is_mounted(mount_path):
        unmount(mount_path)

    run_shell_command(
        [
            "mount",
            "-t",
            "vfat",
            str(path),
            str(mount_path),
            "-o",
            "rw,noatime,nodiratime" if readwrite else "ro",
        ]
    )


def unmount(mount_path: Path):
    if not is_mounted(mount_path):
        return

    run_shell_command(
        [
            "umount",
            str(mount_path),
        ]
    )


def main():
    parser = ArgumentParser("mass-storage-gadget")
    parser.add_argument(
        "action", choices=["host-mode", "client-mode", "disable", "remount"]
    )
    parser.add_argument("-f", "--backing-file", required=True, type=str)
    parser.add_argument("-s", "--size-gb", default=64, type=int)
    parser.add_argument("-m", "--mount-path", default=None, type=str)
    parser.add_argument(
        "-w", "--mount-read-write", default=False, action="store_true"
    )
    args = parser.parse_args()
    backing_file = Path(args.backing_file)
    mount_path = Path(args.mount_path) if args.mount_path else None
    if args.action == "host-mode":
        enable_gadget(backing_file, args.size_gb)
        if mount_path:
            mount(backing_file, mount_path, readwrite=False)
    elif args.action == "client-mode":
        disable_gadget()
        if mount_path is None:
            print("error: mount path required.")
            exit(1)
        mount(backing_file, mount_path, readwrite=True)
    elif args.action == "disable":
        disable_gadget()
        if mount_path:
            unmount(mount_path)
    elif args.action == "remount":
        if mount_path is None:
            print("error: mount path required.")
            exit(1)
        unmount(mount_path)
        mount(backing_file, mount_path, args.mount_read_write)
    else:
        print(f"unsupported action: {args.action}")


if __name__ == "__main__":
    main()
