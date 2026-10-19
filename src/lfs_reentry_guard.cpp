/**
 * Author: sascha_lammers@gmx.de
 */

// ESP8266 diagnostic for FIXED-7 (docs/webui_crash_report.md): LittleFS has no lock on the ESP8266 and the
// core's flash_hal_write()/flash_hal_erase() call optimistic_yield(10000) before every flash access. A
// LittleFS call from the loop task that runs longer than 10 ms (the LED show of 1024 pixels alone takes
// ~30 ms with interrupts off) yields in the middle of a commit, and the SDK runs the network callbacks
// (async web server) and the scheduled functions, which can call LittleFS again.
//
// With KFC_LFS_REENTRY_GUARD=1 every LittleFS API call is wrapped (-Wl,--wrap=lfs_..., all of them are
// called from LittleFS.cpp only) and a call that starts while another one is running is printed on
// Serial0 with the outer call and the last operations. A heap watch prints the free heap low water mark
// and failed allocations.

#include <global.h>

#if ESP8266 && KFC_LFS_REENTRY_GUARD

#include <Arduino.h>
#include <LittleFS.h>
#include <serial_compat.h>

extern "C" {
    bool can_yield();
    size_t umm_free_heap_size_lw(void);
    extern void *umm_last_fail_alloc_addr;
    extern int umm_last_fail_alloc_size;
}

namespace {

    struct LfsOp {
        uint32_t ms;
        const char *op;
        const void *handle;
        uint16_t heap;
        uint8_t depth;
        bool cont;
        char name[26];
    };

    struct LfsOpenFile {
        const void *handle;
        char name[26];
    };

    constexpr uint8_t kHistorySize = 12;
    constexpr uint8_t kMaxDepth = 4;
    constexpr uint8_t kMaxOpenFiles = 12;

    LfsOp history[kHistorySize];
    LfsOp active[kMaxDepth];
    LfsOpenFile openFiles[kMaxOpenFiles];
    uint8_t historyPos = 0;
    volatile uint8_t depth = 0;
    uint32_t reentryCount = 0;
    uint32_t outerReentries = 0; // re-entries during the running outer call

    void copyName(char *dst, size_t size, const char *path)
    {
        if (!path) {
            dst[0] = 0;
            return;
        }
        // keep the end of the path, the file name is the interesting part
        auto len = strlen(path);
        if (len >= size) {
            path += len - (size - 1);
        }
        strncpy(dst, path, size - 1);
        dst[size - 1] = 0;
    }

    const char *fileName(const void *handle)
    {
        for (auto &file: openFiles) {
            if (file.handle == handle) {
                return file.name;
            }
        }
        return nullptr;
    }

    void rememberFile(const void *handle, const char *path)
    {
        for (auto &file: openFiles) {
            if (!file.handle) {
                file.handle = handle;
                copyName(file.name, sizeof(file.name), path);
                return;
            }
        }
    }

    void forgetFile(const void *handle)
    {
        for (auto &file: openFiles) {
            if (file.handle == handle) {
                file.handle = nullptr;
                return;
            }
        }
    }

    void printOp(const char *prefix, const LfsOp &op)
    {
        Serial0.printf_P(PSTR("LFS %s %8u %-5s %-9s d=%u heap=%u %p %s\n"), prefix, op.ms, op.cont ? "cont" : "sys", op.op, op.depth, op.heap, op.handle, op.name);
    }

    // heap watch: prints the low water mark of the free heap when it drops below kHeapWatchLimit and every
    // failed allocation (the SDK crashed with a NULL memcpy in the WiFi code during large form POSTs)
    constexpr uint32_t kHeapWatchLimit = 8192;
    constexpr uint32_t kHeapWatchStep = 256;
    ETSTimer heapWatchTimer;
    bool heapWatchStarted = false;
    uint32_t heapWatchLast = kHeapWatchLimit;
    void *heapWatchFailAddr = nullptr;
    int heapWatchFailSize = 0;

    void heapWatchCallback(void *)
    {
        auto lw = umm_free_heap_size_lw();
        if (lw + kHeapWatchStep <= heapWatchLast) {
            heapWatchLast = lw;
            Serial0.printf_P(PSTR("HEAP %u low water=%u free=%u block=%u\n"), millis(), lw, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
        }
        if (umm_last_fail_alloc_addr != heapWatchFailAddr || umm_last_fail_alloc_size != heapWatchFailSize) {
            heapWatchFailAddr = umm_last_fail_alloc_addr;
            heapWatchFailSize = umm_last_fail_alloc_size;
            Serial0.printf_P(PSTR("HEAP %u alloc failed size=%d caller=%p free=%u block=%u\n"), millis(), heapWatchFailSize, heapWatchFailAddr, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
        }
    }

    void heapWatchStart()
    {
        if (!heapWatchStarted) {
            heapWatchStarted = true;
            os_timer_setfn(&heapWatchTimer, heapWatchCallback, nullptr);
            os_timer_arm(&heapWatchTimer, 100, true);
        }
    }

    struct LfsGuard {
        LfsGuard(const char *op, const void *handle, const char *path)
        {
            heapWatchStart();
            LfsOp cur;
            cur.ms = millis();
            cur.op = op;
            cur.handle = handle;
            cur.heap = static_cast<uint16_t>(std::min<uint32_t>(ESP.getMaxFreeBlockSize(), 0xffff));
            cur.depth = depth;
            cur.cont = can_yield();
            copyName(cur.name, sizeof(cur.name), path ? path : fileName(handle));

            // the full block is printed once per outer call: kilobytes of UART output from the network
            // context starved the loop task and triggered the soft WDT
            if (depth && outerReentries++ == 0) {
                reentryCount++;
                Serial0.printf_P(PSTR("LFS RE-ENTRY #%u\n"), reentryCount);
                printOp("inner", cur);
                for (uint8_t i = 0; i < depth && i < kMaxDepth; i++) {
                    printOp("outer", active[i]);
                }
                for (uint8_t i = 0; i < kHistorySize; i++) {
                    auto &item = history[(historyPos + i) % kHistorySize];
                    if (item.op) {
                        printOp("prev ", item);
                    }
                }
            }

            history[historyPos] = cur;
            historyPos = (historyPos + 1) % kHistorySize;
            if (depth < kMaxDepth) {
                active[depth] = cur;
            }
            depth++;
        }

        ~LfsGuard()
        {
            depth--;
            if (depth == 0 && outerReentries) {
                Serial0.printf_P(PSTR("LFS outer %s %s done after %ums, %u re-entries\n"), active[0].op, active[0].name, millis() - active[0].ms, outerReentries);
                outerReentries = 0;
            }
        }
    };

}

extern "C" {

    int __real_lfs_mount(lfs_t *lfs, const struct lfs_config *config);
    int __real_lfs_unmount(lfs_t *lfs);
    int __real_lfs_format(lfs_t *lfs, const struct lfs_config *config);
    int __real_lfs_remove(lfs_t *lfs, const char *path);
    int __real_lfs_rename(lfs_t *lfs, const char *oldpath, const char *newpath);
    int __real_lfs_stat(lfs_t *lfs, const char *path, struct lfs_info *info);
    lfs_ssize_t __real_lfs_getattr(lfs_t *lfs, const char *path, uint8_t type, void *buffer, lfs_size_t size);
    int __real_lfs_setattr(lfs_t *lfs, const char *path, uint8_t type, const void *buffer, lfs_size_t size);
    int __real_lfs_file_open(lfs_t *lfs, lfs_file_t *file, const char *path, int flags);
    int __real_lfs_file_close(lfs_t *lfs, lfs_file_t *file);
    int __real_lfs_file_sync(lfs_t *lfs, lfs_file_t *file);
    lfs_ssize_t __real_lfs_file_read(lfs_t *lfs, lfs_file_t *file, void *buffer, lfs_size_t size);
    lfs_ssize_t __real_lfs_file_write(lfs_t *lfs, lfs_file_t *file, const void *buffer, lfs_size_t size);
    lfs_soff_t __real_lfs_file_seek(lfs_t *lfs, lfs_file_t *file, lfs_soff_t off, int whence);
    int __real_lfs_file_truncate(lfs_t *lfs, lfs_file_t *file, lfs_off_t size);
    lfs_soff_t __real_lfs_file_tell(lfs_t *lfs, lfs_file_t *file);
    lfs_soff_t __real_lfs_file_size(lfs_t *lfs, lfs_file_t *file);
    int __real_lfs_mkdir(lfs_t *lfs, const char *path);
    int __real_lfs_dir_open(lfs_t *lfs, lfs_dir_t *dir, const char *path);
    int __real_lfs_dir_close(lfs_t *lfs, lfs_dir_t *dir);
    int __real_lfs_dir_read(lfs_t *lfs, lfs_dir_t *dir, struct lfs_info *info);
    int __real_lfs_dir_rewind(lfs_t *lfs, lfs_dir_t *dir);
    lfs_ssize_t __real_lfs_fs_size(lfs_t *lfs);

    int __wrap_lfs_mount(lfs_t *lfs, const struct lfs_config *config)
    {
        LfsGuard guard(PSTR("mount"), lfs, nullptr);
        return __real_lfs_mount(lfs, config);
    }

    int __wrap_lfs_unmount(lfs_t *lfs)
    {
        LfsGuard guard(PSTR("unmount"), lfs, nullptr);
        return __real_lfs_unmount(lfs);
    }

    int __wrap_lfs_format(lfs_t *lfs, const struct lfs_config *config)
    {
        LfsGuard guard(PSTR("format"), lfs, nullptr);
        return __real_lfs_format(lfs, config);
    }

    int __wrap_lfs_remove(lfs_t *lfs, const char *path)
    {
        LfsGuard guard(PSTR("remove"), nullptr, path);
        return __real_lfs_remove(lfs, path);
    }

    int __wrap_lfs_rename(lfs_t *lfs, const char *oldpath, const char *newpath)
    {
        LfsGuard guard(PSTR("rename"), nullptr, oldpath);
        return __real_lfs_rename(lfs, oldpath, newpath);
    }

    int __wrap_lfs_stat(lfs_t *lfs, const char *path, struct lfs_info *info)
    {
        LfsGuard guard(PSTR("stat"), nullptr, path);
        return __real_lfs_stat(lfs, path, info);
    }

    lfs_ssize_t __wrap_lfs_getattr(lfs_t *lfs, const char *path, uint8_t type, void *buffer, lfs_size_t size)
    {
        LfsGuard guard(PSTR("getattr"), nullptr, path);
        return __real_lfs_getattr(lfs, path, type, buffer, size);
    }

    int __wrap_lfs_setattr(lfs_t *lfs, const char *path, uint8_t type, const void *buffer, lfs_size_t size)
    {
        LfsGuard guard(PSTR("setattr"), nullptr, path);
        return __real_lfs_setattr(lfs, path, type, buffer, size);
    }

    int __wrap_lfs_file_open(lfs_t *lfs, lfs_file_t *file, const char *path, int flags)
    {
        LfsGuard guard((flags & LFS_O_WRONLY) ? PSTR("open_w") : PSTR("open_r"), file, path);
        auto result = __real_lfs_file_open(lfs, file, path, flags);
        if (result >= 0) {
            rememberFile(file, path);
        }
        return result;
    }

    int __wrap_lfs_file_close(lfs_t *lfs, lfs_file_t *file)
    {
        LfsGuard guard(PSTR("close"), file, nullptr);
        auto result = __real_lfs_file_close(lfs, file);
        forgetFile(file);
        return result;
    }

    int __wrap_lfs_file_sync(lfs_t *lfs, lfs_file_t *file)
    {
        LfsGuard guard(PSTR("sync"), file, nullptr);
        return __real_lfs_file_sync(lfs, file);
    }

    lfs_ssize_t __wrap_lfs_file_read(lfs_t *lfs, lfs_file_t *file, void *buffer, lfs_size_t size)
    {
        LfsGuard guard(PSTR("read"), file, nullptr);
        return __real_lfs_file_read(lfs, file, buffer, size);
    }

    lfs_ssize_t __wrap_lfs_file_write(lfs_t *lfs, lfs_file_t *file, const void *buffer, lfs_size_t size)
    {
        LfsGuard guard(PSTR("write"), file, nullptr);
        return __real_lfs_file_write(lfs, file, buffer, size);
    }

    lfs_soff_t __wrap_lfs_file_seek(lfs_t *lfs, lfs_file_t *file, lfs_soff_t off, int whence)
    {
        LfsGuard guard(PSTR("seek"), file, nullptr);
        return __real_lfs_file_seek(lfs, file, off, whence);
    }

    int __wrap_lfs_file_truncate(lfs_t *lfs, lfs_file_t *file, lfs_off_t size)
    {
        LfsGuard guard(PSTR("truncate"), file, nullptr);
        return __real_lfs_file_truncate(lfs, file, size);
    }

    lfs_soff_t __wrap_lfs_file_tell(lfs_t *lfs, lfs_file_t *file)
    {
        LfsGuard guard(PSTR("tell"), file, nullptr);
        return __real_lfs_file_tell(lfs, file);
    }

    lfs_soff_t __wrap_lfs_file_size(lfs_t *lfs, lfs_file_t *file)
    {
        LfsGuard guard(PSTR("size"), file, nullptr);
        return __real_lfs_file_size(lfs, file);
    }

    int __wrap_lfs_mkdir(lfs_t *lfs, const char *path)
    {
        LfsGuard guard(PSTR("mkdir"), nullptr, path);
        return __real_lfs_mkdir(lfs, path);
    }

    int __wrap_lfs_dir_open(lfs_t *lfs, lfs_dir_t *dir, const char *path)
    {
        LfsGuard guard(PSTR("dir_open"), dir, path);
        return __real_lfs_dir_open(lfs, dir, path);
    }

    int __wrap_lfs_dir_close(lfs_t *lfs, lfs_dir_t *dir)
    {
        LfsGuard guard(PSTR("dir_close"), dir, nullptr);
        return __real_lfs_dir_close(lfs, dir);
    }

    int __wrap_lfs_dir_read(lfs_t *lfs, lfs_dir_t *dir, struct lfs_info *info)
    {
        LfsGuard guard(PSTR("dir_read"), dir, nullptr);
        return __real_lfs_dir_read(lfs, dir, info);
    }

    int __wrap_lfs_dir_rewind(lfs_t *lfs, lfs_dir_t *dir)
    {
        LfsGuard guard(PSTR("dir_rewnd"), dir, nullptr);
        return __real_lfs_dir_rewind(lfs, dir);
    }

    lfs_ssize_t __wrap_lfs_fs_size(lfs_t *lfs)
    {
        LfsGuard guard(PSTR("fs_size"), lfs, nullptr);
        return __real_lfs_fs_size(lfs);
    }

}

#endif
