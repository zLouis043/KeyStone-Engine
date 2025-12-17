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

/**
 * @brief Creates a new Virtual File System instance.
 * @return A valid handle to the new VFS, or NULL if allocation failed.
 */
KS_API Ks_VFS ks_vfs_create();

/**
 * @brief Destroys a VFS instance and frees all associated resources.
 * @param vfs The VFS handle to destroy. If NULL, this function does nothing.
 */
KS_API ks_no_ret ks_vfs_destroy(Ks_VFS vfs);

/**
 * @brief Mounts a physical disk path to a virtual alias.
 * * Example: ks_vfs_mount(vfs, "core", "./assets/core", ks_false);
 * This allows accessing files like "core://textures/logo.png".
 * * @param vfs The VFS handle.
 * @param alias The virtual prefix (without "://").
 * @param physical_path The real path on the disk (relative or absolute).
 * @param overwrite If ks_true, updates the path if the alias already exists.
 * If ks_false, fails if the alias is already mounted.
 * @return ks_true if mounted successfully, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_mount(Ks_VFS vfs, ks_str alias, ks_str physical_path, ks_bool overwrite);

/**
 * @brief Unmounts a virtual alias.
 * @param vfs The VFS handle.
 * @param alias The alias to remove (e.g., "core").
 * @return ks_true if the alias was found and removed, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_unmount(Ks_VFS vfs, ks_str alias);

/**
 * @brief Resolves a virtual path into a physical absolute path.
 * * Converts "alias://path/to/file" into "C:/Real/Path/to/file".
 * Useful for passing paths to external libraries that do not support VFS.
 * * @param vfs The VFS handle.
 * @param virtual_path The input virtual path (e.g., "core://config.ini").
 * @param out_path Buffer where the resolved physical path will be written.
 * @param max_len The capacity of the out_path buffer.
 * @return ks_true if resolution succeeded, ks_false if alias not found or buffer too small.
 */
KS_API ks_bool ks_vfs_resolve(Ks_VFS vfs, ks_str virtual_path, char* out_path, ks_size max_len);

/**
 * @brief Checks if a file exists at the given virtual path.
 * @param vfs The VFS handle.
 * @param virtual_path The virtual path to check.
 * @return ks_true if the file exists and is accessible, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_exists(Ks_VFS vfs, ks_str virtual_path);

/**
 * @brief Reads an entire file from the VFS into a newly allocated buffer.
 * * @note The returned buffer is null-terminated (safe for strings) but allocated
 * using the engine's memory manager. The caller OWNS this memory.
 * * @param vfs The VFS handle.
 * @param virtual_path The virtual path of the file to read.
 * @param[out] out_size Optional pointer to receive the size of the file in bytes. Can be NULL.
 * @return A pointer to the file data. IMPORTANT: Must be freed with ks_dealloc(). Returns NULL on failure.
 */
KS_API ks_str ks_vfs_read_file(Ks_VFS vfs, ks_str virtual_path, ks_size* out_size);

/**
 * @brief Writes a data buffer to a file in the VFS.
 * * Automatically creates any missing parent directories for the resolved path.
 * * @param vfs The VFS handle.
 * @param virtual_path The destination virtual path.
 * @param data Pointer to the data to write.
 * @param size Number of bytes to write.
 * @return ks_true if the write operation was successful, ks_false otherwise.
 */
KS_API ks_bool ks_vfs_write_file(Ks_VFS vfs, ks_str virtual_path, const void* data, ks_size size);

#ifdef __cplusplus
}
#endif