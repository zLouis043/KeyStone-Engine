/**
 * @file vfs.h
 * @brief Virtual File System.
 * Manages mount points and file abstraction (core://, mod://).
 * @ingroup Filesystem
 */
#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a Virtual File System instance.
 */
typedef ks_ptr Ks_VFS;

enum Ks_VFS_Error {
	KS_VFS_ERROR_FAILED_TO_RESOLVE_PATH,
	KS_VFS_ERROR_PATH_ALREADY_MOUNTED,
	KS_VFS_ERROR_PATH_DOES_NOT_EXIST,
	KS_VFS_ERROR_FAILED_TO_OPEN_FILE
};

/**
 * @brief Init a the Virtual File System.
 */
KS_API ks_bool ks_vfs_init();

/**
 * @brief Shuts down the VFS and frees all associated resources.
 */
KS_API ks_no_ret ks_vfs_shutdown();

/**
 * @brief Mounts a physical disk path to a virtual alias.
 * * Example: ks_vfs_mount(vfs, "core", "./assets/core", ks_false);
 * This allows accessing files like "core://textures/logo.png".
 * @param alias The virtual prefix (without "://").
 * @param physical_path The real path on the disk (relative or absolute).
 * @param overwrite If ks_true, updates the path if the alias already exists.
 * If ks_false, fails if the alias is already mounted.
 * @return ks_true if mounted successfully, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_mount(ks_str alias, ks_str physical_path, ks_bool overwrite);

/**
 * @brief Unmounts a virtual alias.
 * @param alias The alias to remove (e.g., "core").
 * @return ks_true if the alias was found and removed, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_unmount(ks_str alias);

/**
 * @brief Resolves a virtual path into a physical absolute path.
 * * Converts "alias://path/to/file" into "C:/Real/Path/to/file".
 * Useful for passing paths to external libraries that do not support VFS.
 * @param virtual_path The input virtual path (e.g., "core://config.ini").
 * @param out_path Buffer where the resolved physical path will be written.
 * @param max_len The capacity of the out_path buffer.
 * @return ks_true if resolution succeeded, ks_false if alias not found or buffer too small.
 */
KS_API ks_bool ks_vfs_resolve(ks_str virtual_path, char* out_path, ks_size max_len);

/**
 * @brief Checks if a file exists at the given virtual path.
 * @param virtual_path The virtual path to check.
 * @return ks_true if the file exists and is accessible, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_exists(ks_str virtual_path);

/**
 * @brief Reads an entire file from the VFS into a newly allocated buffer.
 * * @note The returned buffer is null-terminated (safe for strings) but allocated
 * using the engine's memory manager. The caller OWNS this memory.
 * @param virtual_path The virtual path of the file to read.
 * @param[out] out_size Optional pointer to receive the size of the file in bytes. Can be NULL.
 * @return A pointer to the file data. IMPORTANT: Must be freed with ks_dealloc(). Returns NULL on failure.
 */
KS_API ks_str ks_vfs_read_file(ks_str virtual_path, ks_size* out_size);

/**
 * @brief Writes a data buffer to a file in the VFS.
 * * Automatically creates any missing parent directories for the resolved path.
 * @param virtual_path The destination virtual path.
 * @param data Pointer to the data to write.
 * @param size Number of bytes to write.
 * @return ks_true if the write operation was successful, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_write_file(ks_str virtual_path, const void* data, ks_size size);

#ifdef __cplusplus
}
#endif