/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <OSTimer.h>
#include <EventScheduler.h>

#ifdef Serial
#undef Serial
#endif
#define Serial Serial0 // allows to call begin() etc.. since Serial is a Stream object not HardwareSerial, pointing to Serial0

typedef int esp_err_t;

#define ESP_OK          0       /*!< esp_err_t value indicating success (no error) */
#define ESP_FAIL        -1      /*!< Generic esp_err_t code indicating failure */

#define ESP_ERR_NO_MEM              0x101   /*!< Out of memory */
#define ESP_ERR_INVALID_ARG         0x102   /*!< Invalid argument */
#define ESP_ERR_INVALID_STATE       0x103   /*!< Invalid state */
#define ESP_ERR_INVALID_SIZE        0x104   /*!< Invalid size */
#define ESP_ERR_NOT_FOUND           0x105   /*!< Requested resource not found */
#define ESP_ERR_NOT_SUPPORTED       0x106   /*!< Operation or feature not supported */
#define ESP_ERR_TIMEOUT             0x107   /*!< Operation timed out */
#define ESP_ERR_INVALID_RESPONSE    0x108   /*!< Received response was invalid */
#define ESP_ERR_INVALID_CRC         0x109   /*!< CRC or checksum was invalid */
#define ESP_ERR_INVALID_VERSION     0x10A   /*!< Version was invalid */
#define ESP_ERR_INVALID_MAC         0x10B   /*!< MAC address was invalid */
#define ESP_ERR_NOT_FINISHED        0x10C   /*!< There are items remained to retrieve */

/**
    * Opaque pointer type representing non-volatile storage handle
    */
typedef uint32_t nvs_handle_t;

#define ESP_ERR_NVS_BASE                    0x1100                     /*!< Starting number of error codes */
#define ESP_ERR_NVS_NOT_INITIALIZED         (ESP_ERR_NVS_BASE + 0x01)  /*!< The storage driver is not initialized */
#define ESP_ERR_NVS_NOT_FOUND               (ESP_ERR_NVS_BASE + 0x02)  /*!< Id namespace doesn�t exist yet and mode is NVS_READONLY */
#define ESP_ERR_NVS_TYPE_MISMATCH           (ESP_ERR_NVS_BASE + 0x03)  /*!< The type of set or get operation doesn't match the type of value stored in NVS */
#define ESP_ERR_NVS_READ_ONLY               (ESP_ERR_NVS_BASE + 0x04)  /*!< Storage handle was opened as read only */
#define ESP_ERR_NVS_NOT_ENOUGH_SPACE        (ESP_ERR_NVS_BASE + 0x05)  /*!< There is not enough space in the underlying storage to save the value */
#define ESP_ERR_NVS_INVALID_NAME            (ESP_ERR_NVS_BASE + 0x06)  /*!< Namespace name doesn�t satisfy constraints */
#define ESP_ERR_NVS_INVALID_HANDLE          (ESP_ERR_NVS_BASE + 0x07)  /*!< Handle has been closed or is NULL */
#define ESP_ERR_NVS_REMOVE_FAILED           (ESP_ERR_NVS_BASE + 0x08)  /*!< The value wasn�t updated because flash write operation has failed. The value was written however, and update will be finished after re-initialization of nvs, provided that flash operation doesn�t fail again. */
#define ESP_ERR_NVS_KEY_TOO_LONG            (ESP_ERR_NVS_BASE + 0x09)  /*!< Key name is too long */
#define ESP_ERR_NVS_PAGE_FULL               (ESP_ERR_NVS_BASE + 0x0a)  /*!< Internal error; never returned by nvs API functions */
#define ESP_ERR_NVS_INVALID_STATE           (ESP_ERR_NVS_BASE + 0x0b)  /*!< NVS is in an inconsistent state due to a previous error. Call nvs_flash_init and nvs_open again, then retry. */
#define ESP_ERR_NVS_INVALID_LENGTH          (ESP_ERR_NVS_BASE + 0x0c)  /*!< String or blob length is not sufficient to store data */
#define ESP_ERR_NVS_NO_FREE_PAGES           (ESP_ERR_NVS_BASE + 0x0d)  /*!< NVS partition doesn't contain any empty pages. This may happen if NVS partition was truncated. Erase the whole partition and call nvs_flash_init again. */
#define ESP_ERR_NVS_VALUE_TOO_LONG          (ESP_ERR_NVS_BASE + 0x0e)  /*!< String or blob length is longer than supported by the implementation */
#define ESP_ERR_NVS_PART_NOT_FOUND          (ESP_ERR_NVS_BASE + 0x0f)  /*!< Partition with specified name is not found in the partition table */

#define ESP_ERR_NVS_NEW_VERSION_FOUND       (ESP_ERR_NVS_BASE + 0x10)  /*!< NVS partition contains data in new format and cannot be recognized by this version of code */
#define ESP_ERR_NVS_XTS_ENCR_FAILED         (ESP_ERR_NVS_BASE + 0x11)  /*!< XTS encryption failed while writing NVS entry */
#define ESP_ERR_NVS_XTS_DECR_FAILED         (ESP_ERR_NVS_BASE + 0x12)  /*!< XTS decryption failed while reading NVS entry */
#define ESP_ERR_NVS_XTS_CFG_FAILED          (ESP_ERR_NVS_BASE + 0x13)  /*!< XTS configuration setting failed */
#define ESP_ERR_NVS_XTS_CFG_NOT_FOUND       (ESP_ERR_NVS_BASE + 0x14)  /*!< XTS configuration not found */
#define ESP_ERR_NVS_ENCR_NOT_SUPPORTED      (ESP_ERR_NVS_BASE + 0x15)  /*!< NVS encryption is not supported in this version */
#define ESP_ERR_NVS_KEYS_NOT_INITIALIZED    (ESP_ERR_NVS_BASE + 0x16)  /*!< NVS key partition is uninitialized */
#define ESP_ERR_NVS_CORRUPT_KEY_PART        (ESP_ERR_NVS_BASE + 0x17)  /*!< NVS key partition is corrupt */
#define ESP_ERR_NVS_WRONG_ENCRYPTION        (ESP_ERR_NVS_BASE + 0x19)  /*!< NVS partition is marked as encrypted with generic flash encryption. This is forbidden since the NVS encryption works differently. */

#define ESP_ERR_NVS_CONTENT_DIFFERS         (ESP_ERR_NVS_BASE + 0x18)  /*!< Internal error; never returned by nvs API functions.  NVS key is different in comparison */

#define NVS_DEFAULT_PART_NAME               "nvs"   /*!< Default partition name of the NVS partition in the partition table */

#define NVS_PART_NAME_MAX_SIZE              16   /*!< maximum length of partition name (excluding null terminator) */
#define NVS_KEY_NAME_MAX_SIZE               16   /*!< Maximal length of NVS key name (including null terminator) */

/**
 * @brief Mode of opening the non-volatile storage
 */
typedef enum {
    NVS_READONLY,  /*!< Read only */
    NVS_READWRITE  /*!< Read and write */
} nvs_open_mode_t;

/**
 * @brief Types of variables
 *
 */
typedef enum {
    NVS_TYPE_U8 = 0x01,  /*!< Type uint8_t */
    NVS_TYPE_I8 = 0x11,  /*!< Type int8_t */
    NVS_TYPE_U16 = 0x02,  /*!< Type uint16_t */
    NVS_TYPE_I16 = 0x12,  /*!< Type int16_t */
    NVS_TYPE_U32 = 0x04,  /*!< Type uint32_t */
    NVS_TYPE_I32 = 0x14,  /*!< Type int32_t */
    NVS_TYPE_U64 = 0x08,  /*!< Type uint64_t */
    NVS_TYPE_I64 = 0x18,  /*!< Type int64_t */
    NVS_TYPE_STR = 0x21,  /*!< Type string */
    NVS_TYPE_BLOB = 0x42,  /*!< Type blob */
    NVS_TYPE_ANY = 0xff   /*!< Must be last */
} nvs_type_t;

/**
 * @brief information about entry obtained from nvs_entry_info function
 */
typedef struct {
    char namespace_name[16];    /*!< Namespace to which key-value belong */
    char key[16];               /*!< Key of stored key-value pair */
    nvs_type_t type;            /*!< Type of stored key-value pair */
} nvs_entry_info_t;

/**
 * Opaque pointer type representing iterator to nvs entries
 */
typedef struct nvs_opaque_iterator_t *nvs_iterator_t;

/**
 * @brief      Open non-volatile storage with a given namespace from the default NVS partition
 *
 * Multiple internal ESP-IDF and third party application modules can store
 * their key-value pairs in the NVS module. In order to reduce possible
 * conflicts on key names, each module can use its own namespace.
 * The default NVS partition is the one that is labelled "nvs" in the partition
 * table.
 *
 * @param[in]  name        Namespace name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[in]  open_mode   NVS_READWRITE or NVS_READONLY. If NVS_READONLY, will
 *                         open a handle for reading only. All write requests will
 *             be rejected for this handle.
 * @param[out] out_handle  If successful (return code is zero), handle will be
 *                         returned in this argument.
 *
 * @return
 *             - ESP_OK if storage handle was opened successfully
 *             - ESP_ERR_NVS_NOT_INITIALIZED if the storage driver is not initialized
 *             - ESP_ERR_NVS_PART_NOT_FOUND if the partition with label "nvs" is not found
 *             - ESP_ERR_NVS_NOT_FOUND id namespace doesn't exist yet and
 *               mode is NVS_READONLY
 *             - ESP_ERR_NVS_INVALID_NAME if namespace name doesn't satisfy constraints
 *             - ESP_ERR_NO_MEM in case memory could not be allocated for the internal structures
 *             - other error codes from the underlying storage driver
 */
esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);

/**
 * @brief      Open non-volatile storage with a given namespace from specified partition
 *
 * The behaviour is same as nvs_open() API. However this API can operate on a specified NVS
 * partition instead of default NVS partition. Note that the specified partition must be registered
 * with NVS using nvs_flash_init_partition() API.
 *
 * @param[in]  part_name   Label (name) of the partition of interest for object read/write/erase
 * @param[in]  name        Namespace name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[in]  open_mode   NVS_READWRITE or NVS_READONLY. If NVS_READONLY, will
 *                         open a handle for reading only. All write requests will
 *             be rejected for this handle.
 * @param[out] out_handle  If successful (return code is zero), handle will be
 *                         returned in this argument.
 *
 * @return
 *             - ESP_OK if storage handle was opened successfully
 *             - ESP_ERR_NVS_NOT_INITIALIZED if the storage driver is not initialized
 *             - ESP_ERR_NVS_PART_NOT_FOUND if the partition with specified name is not found
 *             - ESP_ERR_NVS_NOT_FOUND id namespace doesn't exist yet and
 *               mode is NVS_READONLY
 *             - ESP_ERR_NVS_INVALID_NAME if namespace name doesn't satisfy constraints
 *             - ESP_ERR_NO_MEM in case memory could not be allocated for the internal structures
 *             - other error codes from the underlying storage driver
 */
esp_err_t nvs_open_from_partition(const char *part_name, const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);

/**@{*/
/**
 * @brief      set int8_t value for given key
 *
 * Set value for the key, given its name. Note that the actual storage will not be updated
 * until \c nvs_commit is called.
 *
 * @param[in]  handle  Handle obtained from nvs_open function.
 *                     Handles that were opened read only cannot be used.
 * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[in]  value   The value to set.
 *
 * @return
 *             - ESP_OK if value was set successfully
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *             - ESP_ERR_NVS_READ_ONLY if storage handle was opened as read only
 *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
 *             - ESP_ERR_NVS_NOT_ENOUGH_SPACE if there is not enough space in the
 *               underlying storage to save the value
 *             - ESP_ERR_NVS_REMOVE_FAILED if the value wasn't updated because flash
 *               write operation has failed. The value was written however, and
 *               update will be finished after re-initialization of nvs, provided that
 *               flash operation doesn't fail again.
 */
esp_err_t nvs_set_i8(nvs_handle_t handle, const char *key, int8_t value);

/**
 * @brief      set uint8_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value);

/**
 * @brief      set int16_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_i16(nvs_handle_t handle, const char *key, int16_t value);

/**
 * @brief      set uint16_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_u16(nvs_handle_t handle, const char *key, uint16_t value);

/**
 * @brief      set int32_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value);

/**
 * @brief      set uint32_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value);

/**
 * @brief      set int64_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_i64(nvs_handle_t handle, const char *key, int64_t value);

/**
 * @brief      set uint64_t value for given key
 *
 * This function is the same as \c nvs_set_i8 except for the data type.
 */
esp_err_t nvs_set_u64(nvs_handle_t handle, const char *key, uint64_t value);

/**
 * @brief      set string for given key
 *
 * Set value for the key, given its name. Note that the actual storage will not be updated
 * until \c nvs_commit is called.
 *
 * @param[in]  handle  Handle obtained from nvs_open function.
 *                     Handles that were opened read only cannot be used.
 * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[in]  value   The value to set.
 *                     For strings, the maximum length (including null character) is
 *                     4000 bytes, if there is one complete page free for writing.
 *                     This decreases, however, if the free space is fragmented.
 *
 * @return
 *             - ESP_OK if value was set successfully
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *             - ESP_ERR_NVS_READ_ONLY if storage handle was opened as read only
 *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
 *             - ESP_ERR_NVS_NOT_ENOUGH_SPACE if there is not enough space in the
 *               underlying storage to save the value
 *             - ESP_ERR_NVS_REMOVE_FAILED if the value wasn't updated because flash
 *               write operation has failed. The value was written however, and
 *               update will be finished after re-initialization of nvs, provided that
 *               flash operation doesn't fail again.
 *             - ESP_ERR_NVS_VALUE_TOO_LONG if the string value is too long
 */
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value);
/**@}*/

/**
 * @brief       set variable length binary value for given key
 *
 * This family of functions set value for the key, given its name. Note that
 * actual storage will not be updated until nvs_commit function is called.
 *
 * @param[in]  handle  Handle obtained from nvs_open function.
 *                     Handles that were opened read only cannot be used.
 * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[in]  value   The value to set.
 * @param[in]  length  length of binary value to set, in bytes; Maximum length is
 *                     508000 bytes or (97.6% of the partition size - 4000) bytes
 *                     whichever is lower.
 *
 * @return
 *             - ESP_OK if value was set successfully
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *             - ESP_ERR_NVS_READ_ONLY if storage handle was opened as read only
 *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
 *             - ESP_ERR_NVS_NOT_ENOUGH_SPACE if there is not enough space in the
 *               underlying storage to save the value
 *             - ESP_ERR_NVS_REMOVE_FAILED if the value wasn't updated because flash
 *               write operation has failed. The value was written however, and
 *               update will be finished after re-initialization of nvs, provided that
 *               flash operation doesn't fail again.
 *             - ESP_ERR_NVS_VALUE_TOO_LONG if the value is too long
 */
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length);

/**@{*/
/**
 * @brief      get int8_t value for given key
 *
 * These functions retrieve value for the key, given its name. If \c key does not
 * exist, or the requested variable type doesn't match the type which was used
 * when setting a value, an error is returned.
 *
 * In case of any error, out_value is not modified.
 *
 * \c out_value has to be a pointer to an already allocated variable of the given type.
 *
 * \code{c}
 * // Example of using nvs_get_i32:
 * int32_t max_buffer_size = 4096; // default value
 * esp_err_t err = nvs_get_i32(my_handle, "max_buffer_size", &max_buffer_size);
 * assert(err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND);
 * // if ESP_ERR_NVS_NOT_FOUND was returned, max_buffer_size will still
 * // have its default value.
 *
 * \endcode
 *
 * @param[in]     handle     Handle obtained from nvs_open function.
 * @param[in]     key        Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param         out_value  Pointer to the output value.
 *                           May be NULL for nvs_get_str and nvs_get_blob, in this
 *                           case required length will be returned in length argument.
 *
 * @return
 *             - ESP_OK if the value was retrieved successfully
 *             - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
 *             - ESP_ERR_NVS_INVALID_LENGTH if length is not sufficient to store data
 */
esp_err_t nvs_get_i8(nvs_handle_t handle, const char *key, int8_t *out_value);

/**
 * @brief      get uint8_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value);

/**
 * @brief      get int16_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_i16(nvs_handle_t handle, const char *key, int16_t *out_value);

/**
 * @brief      get uint16_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_u16(nvs_handle_t handle, const char *key, uint16_t *out_value);

/**
 * @brief      get int32_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out_value);

/**
 * @brief      get uint32_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value);

/**
 * @brief      get int64_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_i64(nvs_handle_t handle, const char *key, int64_t *out_value);

/**
 * @brief      get uint64_t value for given key
 *
 * This function is the same as \c nvs_get_i8 except for the data type.
 */
esp_err_t nvs_get_u64(nvs_handle_t handle, const char *key, uint64_t *out_value);
/**@}*/

/**@{*/
/**
 * @brief      get string value for given key
 *
 * These functions retrieve the data of an entry, given its key. If key does not
 * exist, or the requested variable type doesn't match the type which was used
 * when setting a value, an error is returned.
 *
 * In case of any error, out_value is not modified.
 *
 * All functions expect out_value to be a pointer to an already allocated variable
 * of the given type.
 *
 * nvs_get_str and nvs_get_blob functions support WinAPI-style length queries.
 * To get the size necessary to store the value, call nvs_get_str or nvs_get_blob
 * with zero out_value and non-zero pointer to length. Variable pointed to
 * by length argument will be set to the required length. For nvs_get_str,
 * this length includes the zero terminator. When calling nvs_get_str and
 * nvs_get_blob with non-zero out_value, length has to be non-zero and has to
 * point to the length available in out_value.
 * It is suggested that nvs_get/set_str is used for zero-terminated C strings, and
 * nvs_get/set_blob used for arbitrary data structures.
 *
 * \code{c}
 * // Example (without error checking) of using nvs_get_str to get a string into dynamic array:
 * size_t required_size;
 * nvs_get_str(my_handle, "server_name", NULL, &required_size);
 * char* server_name = malloc(required_size);
 * nvs_get_str(my_handle, "server_name", server_name, &required_size);
 *
 * // Example (without error checking) of using nvs_get_blob to get a binary data
 * into a static array:
 * uint8_t mac_addr[6];
 * size_t size = sizeof(mac_addr);
 * nvs_get_blob(my_handle, "dst_mac_addr", mac_addr, &size);
 * \endcode
 *
 * @param[in]     handle     Handle obtained from nvs_open function.
 * @param[in]     key        Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 * @param[out]    out_value  Pointer to the output value.
 *                           May be NULL for nvs_get_str and nvs_get_blob, in this
 *                           case required length will be returned in length argument.
 * @param[inout]  length     A non-zero pointer to the variable holding the length of out_value.
 *                           In case out_value a zero, will be set to the length
 *                           required to hold the value. In case out_value is not
 *                           zero, will be set to the actual length of the value
 *                           written. For nvs_get_str this includes zero terminator.
 *
 * @return
 *             - ESP_OK if the value was retrieved successfully
 *             - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *             - ESP_ERR_NVS_INVALID_NAME if key name doesn't satisfy constraints
 *             - ESP_ERR_NVS_INVALID_LENGTH if \c length is not sufficient to store data
 */
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length);

/**
 * @brief      get blob value for given key
 *
 * This function behaves the same as \c nvs_get_str, except for the data type.
 */
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
/**@}*/

/**
 * @brief      Erase key-value pair with given key name.
 *
 * Note that actual storage may not be updated until nvs_commit function is called.
 *
 * @param[in]  handle  Storage handle obtained with nvs_open.
 *                     Handles that were opened read only cannot be used.
 *
 * @param[in]  key     Key name. Maximal length is (NVS_KEY_NAME_MAX_SIZE-1) characters. Shouldn't be empty.
 *
 * @return
 *              - ESP_OK if erase operation was successful
 *              - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *              - ESP_ERR_NVS_READ_ONLY if handle was opened as read only
 *              - ESP_ERR_NVS_NOT_FOUND if the requested key doesn't exist
 *              - other error codes from the underlying storage driver
 */
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key);

/**
 * @brief      Erase all key-value pairs in a namespace
 *
 * Note that actual storage may not be updated until nvs_commit function is called.
 *
 * @param[in]  handle  Storage handle obtained with nvs_open.
 *                     Handles that were opened read only cannot be used.
 *
 * @return
 *              - ESP_OK if erase operation was successful
 *              - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *              - ESP_ERR_NVS_READ_ONLY if handle was opened as read only
 *              - other error codes from the underlying storage driver
 */
esp_err_t nvs_erase_all(nvs_handle_t handle);

/**
 * @brief      Write any pending changes to non-volatile storage
 *
 * After setting any values, nvs_commit() must be called to ensure changes are written
 * to non-volatile storage. Individual implementations may write to storage at other times,
 * but this is not guaranteed.
 *
 * @param[in]  handle  Storage handle obtained with nvs_open.
 *                     Handles that were opened read only cannot be used.
 *
 * @return
 *             - ESP_OK if the changes have been written successfully
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL
 *             - other error codes from the underlying storage driver
 */
esp_err_t nvs_commit(nvs_handle_t handle);

/**
 * @brief      Close the storage handle and free any allocated resources
 *
 * This function should be called for each handle opened with nvs_open once
 * the handle is not in use any more. Closing the handle may not automatically
 * write the changes to nonvolatile storage. This has to be done explicitly using
 * nvs_commit function.
 * Once this function is called on a handle, the handle should no longer be used.
 *
 * @param[in]  handle  Storage handle to close
 */
void nvs_close(nvs_handle_t handle);

/**
 * @note Info about storage space NVS.
 */
typedef struct {
    size_t used_entries;      /**< Amount of used entries. */
    size_t free_entries;      /**< Amount of free entries. */
    size_t total_entries;     /**< Amount all available entries. */
    size_t namespace_count;   /**< Amount name space. */
} nvs_stats_t;

/**
 * @brief      Fill structure nvs_stats_t. It provides info about used memory the partition.
 *
 * This function calculates to runtime the number of used entries, free entries, total entries,
 * and amount namespace in partition.
 *
 * \code{c}
 * // Example of nvs_get_stats() to get the number of used entries and free entries:
 * nvs_stats_t nvs_stats;
 * nvs_get_stats(NULL, &nvs_stats);
 * printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
          nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);
 * \endcode
 *
 * @param[in]   part_name   Partition name NVS in the partition table.
 *                          If pass a NULL than will use NVS_DEFAULT_PART_NAME ("nvs").
 *
 * @param[out]  nvs_stats   Returns filled structure nvs_states_t.
 *                          It provides info about used memory the partition.
 *
 *
 * @return
 *             - ESP_OK if the changes have been written successfully.
 *               Return param nvs_stats will be filled.
 *             - ESP_ERR_NVS_PART_NOT_FOUND if the partition with label "name" is not found.
 *               Return param nvs_stats will be filled 0.
 *             - ESP_ERR_NVS_NOT_INITIALIZED if the storage driver is not initialized.
 *               Return param nvs_stats will be filled 0.
 *             - ESP_ERR_INVALID_ARG if nvs_stats equal to NULL.
 *             - ESP_ERR_INVALID_STATE if there is page with the status of INVALID.
 *               Return param nvs_stats will be filled not with correct values because
 *               not all pages will be counted. Counting will be interrupted at the first INVALID page.
 */
esp_err_t nvs_get_stats(const char *part_name, nvs_stats_t *nvs_stats);

/**
 * @brief      Calculate all entries in a namespace.
 *
 * An entry represents the smallest storage unit in NVS.
 * Strings and blobs may occupy more than one entry.
 * Note that to find out the total number of entries occupied by the namespace,
 * add one to the returned value used_entries (if err is equal to ESP_OK).
 * Because the name space entry takes one entry.
 *
 * \code{c}
 * // Example of nvs_get_used_entry_count() to get amount of all key-value pairs in one namespace:
 * nvs_handle_t handle;
 * nvs_open("namespace1", NVS_READWRITE, &handle);
 * ...
 * size_t used_entries;
 * size_t total_entries_namespace;
 * if(nvs_get_used_entry_count(handle, &used_entries) == ESP_OK){
 *     // the total number of entries occupied by the namespace
 *     total_entries_namespace = used_entries + 1;
 * }
 * \endcode
 *
 * @param[in]   handle              Handle obtained from nvs_open function.
 *
 * @param[out]  used_entries        Returns amount of used entries from a namespace.
 *
 *
 * @return
 *             - ESP_OK if the changes have been written successfully.
 *               Return param used_entries will be filled valid value.
 *             - ESP_ERR_NVS_NOT_INITIALIZED if the storage driver is not initialized.
 *               Return param used_entries will be filled 0.
 *             - ESP_ERR_NVS_INVALID_HANDLE if handle has been closed or is NULL.
 *               Return param used_entries will be filled 0.
 *             - ESP_ERR_INVALID_ARG if used_entries equal to NULL.
 *             - Other error codes from the underlying storage driver.
 *               Return param used_entries will be filled 0.
 */
esp_err_t nvs_get_used_entry_count(nvs_handle_t handle, size_t *used_entries);

/**
 * @brief       Create an iterator to enumerate NVS entries based on one or more parameters
 *
 * \code{c}
 * // Example of listing all the key-value pairs of any type under specified partition and namespace
 * nvs_iterator_t it = nvs_entry_find(partition, namespace, NVS_TYPE_ANY);
 * while (it != NULL) {
 *         nvs_entry_info_t info;
 *         nvs_entry_info(it, &info);
 *         it = nvs_entry_next(it);
 *         printf("key '%s', type '%d' \n", info.key, info.type);
 * };
 * // Note: no need to release iterator obtained from nvs_entry_find function when
 * //       nvs_entry_find or nvs_entry_next function return NULL, indicating no other
 * //       element for specified criteria was found.
 * }
 * \endcode
 *
 * @param[in]   part_name       Partition name
 *
 * @param[in]   namespace_name  Set this value if looking for entries with
 *                              a specific namespace. Pass NULL otherwise.
 *
 * @param[in]   type            One of nvs_type_t values.
 *
 * @return
 *          Iterator used to enumerate all the entries found,
 *          or NULL if no entry satisfying criteria was found.
 *          Iterator obtained through this function has to be released
 *          using nvs_release_iterator when not used any more.
 */
nvs_iterator_t nvs_entry_find(const char *part_name, const char *namespace_name, nvs_type_t type);

/**
 * @brief       Returns next item matching the iterator criteria, NULL if no such item exists.
 *
 * Note that any copies of the iterator will be invalid after this call.
 *
 * @param[in]   iterator     Iterator obtained from nvs_entry_find function. Must be non-NULL.
 *
 * @return
 *          NULL if no entry was found, valid nvs_iterator_t otherwise.
 */
nvs_iterator_t nvs_entry_next(nvs_iterator_t iterator);

/**
 * @brief       Fills nvs_entry_info_t structure with information about entry pointed to by the iterator.
 *
 * @param[in]   iterator     Iterator obtained from nvs_entry_find or nvs_entry_next function. Must be non-NULL.
 *
 * @param[out]  out_info     Structure to which entry information is copied.
 */
void nvs_entry_info(nvs_iterator_t iterator, nvs_entry_info_t *out_info);

/**
 * @brief       Release iterator
 *
 * @param[in]   iterator    Release iterator obtained from nvs_entry_find function. NULL argument is allowed.
 *
 */
void nvs_release_iterator(nvs_iterator_t iterator);



class TestTimer : public OSTimer /* ETSTimer implementation */ {
public:
    TestTimer() : OSTimer(OSTIMER_NAME("TestTimer")) {
    }

    virtual void run() override {
        Serial.printf_P(PSTR("OSTimer %.3fms\n"), micros() / 1000.0);
    }
};

TestTimer testTimer1;

void setup()
{
    Serial.begin(115200);

    //nvs_handle_t handle;
    //if (nvs_open("test", nvs_open_mode_t::NVS_READWRITE, &handle) == ESP_OK) {
    //    nvs_close(handle);
    //}

    // ETSTimer/OSTimer test
    testTimer1.startTimer(15000, true);

    // Scheduler test
    _Scheduler.add(Event::seconds(30), 10, [](Event::CallbackTimerPtr timer) {
        Serial.printf_P(PSTR("Scheduled event #1 %.3fs, %u/%u\n"), micros() / 1000000.0, timer->_repeat.getRepeatsLeft() + 1, 10);
    });

    _Scheduler.add(Event::seconds(10), true, [](Event::CallbackTimerPtr timer) {
        Serial.printf_P(PSTR("Scheduled event #2 %.3fs\n"), micros() / 1000000.0);
        timer->disarm();
    });

    //ets_dump_timer(Serial); // #include "ets_timer_win32.h"
    Serial.println(F("Press any key to display millis(). Ctrl+C to end program..."));
}



void loop()
{
    if (Serial.available()) {
        Serial.printf_P(PSTR("millis()=%u\n"), millis());
        Serial.read();
    }
    delay(50);
}

#if 0

#include "section_defines.h"
#include "push_pack.h"

namespace NVSFlashStorage {

    static constexpr uint16_t kSectorMagic = 0x7315;
    static constexpr uint32_t kInvalidAddress = 0xffffff;

    enum class IndexType : uint8_t {
        NAMESPACE,
        INVALID = 0xff
    };

    struct Index __attribute__packed__ {
        Index(IndexType type = IndexType::INVALID, uint32_t address = kInvalidAddress, uint16_t length = 0xffff) : 
            _address(address), 
            _length(length), 
            _crc(0xffffffffU)
        {
            _type = type;
        }

        uint32_t getCrc() const {
            return crc32(reinterpret_cast<const uint8_t *>(this), dataSize());
        }

        bool validateCrc(uint32_t crc) const
        {
            return getCrc() == crc;
        }

        bool validateCrc() const
        {
            return getCrc() == _crc;
        }

        constexpr size_t size() const
        {
            return sizeof(Index);
        }

        constexpr size_t dataSize() const
        {
            return size() - sizeof(_crc);
        }

        union {
            uint32_t _address : 24;
            struct {
                uint8_t _nextBytes[3];
                IndexType _type;
            };
        };
        uint16_t _length;
        uint32_t _crc;
    };

    static constexpr size_t kIndexSize = sizeof(Index);

    struct NamespaceIndex __attribute__packed__ : Index {
        NamespaceIndex(const char *name, uint32_t address) :
            Index(IndexType::NAMESPACE, size()),
            _address(address),
            _name{}
        {
            memmove_P(_name, name, std::min(sizeof(_name), strlen_P(name)));
        }

        bool validateCrc(uint32_t crc) const
        {
            return crc == crc32(reinterpret_cast<const uint8_t *>(this) + Index::size(), dataSize(), Index::getCrc());
        }

        bool validateCrc() const
        {
            return validateCrc(_crc);
        }

        void finalize()
        {
            _crc = crc32(reinterpret_cast<const uint8_t *>(this) + Index::size(), dataSize(), Index::getCrc());
        }

        constexpr size_t dataSize() const
        {
            return size() - Index::size();
        }

        constexpr size_t size() const
        {
            return sizeof(NamespaceIndex);
        }

        uint32_t getAddress() const 
        {
            return _address;
        }

        size_t getSectorAddress() const
        {
            return _address / 4096;
        }

        String getName() const 
        {
            String tmp;
            auto ptr = _name;
            auto end = _name + sizeof(_name);
            while (ptr < end && *ptr) {
                tmp += *ptr++;
            }
            return tmp;
        }

        bool nameEquals(const char *name) const 
        {
            return strncmp_P(_name, name, NVS_PART_NAME_MAX_SIZE) == 0;
        }

        uint32_t _address;
        char _name[NVS_PART_NAME_MAX_SIZE];
    };

    static constexpr size_t kNamespaceSize = sizeof(NamespaceIndex);

    union IndexTuple {
        Index _index;
        NamespaceIndex _namespace;

        IndexTuple() {}
        ~IndexTuple() {}
    };

    struct Sector {
        Sector() : _writePosition(0)
        {
        }
        uint16_t _writePosition;
    };

    using SectorVector = std::vector<Sector>;

    class Container {
    public:
        Container(const char *name, nvs_open_mode_t mode);
        ~Container();

        bool readIndex();
        void createIndex();

    private:
        uint32_t _getStartAddress() const
        {
            return reinterpret_cast<uint32_t>(&_KFCFW_start) - SECTION_FLASH_START_ADDRESS;
        }

        uint32_t _getEndAddress() const
        {
            return reinterpret_cast<uint32_t>(&_KFCFW_end) - SECTION_FLASH_START_ADDRESS;
        }

        size_t _getNumSectors() const {
            return (reinterpret_cast<uint32_t>(&_KFCFW_end) - reinterpret_cast<uint32_t>(&_KFCFW_start)) / 4096;
        }

    private:
        nvs_open_mode_t _mode;
        NamespaceIndex _index;
        SectorVector _sectors;
    };

    Container::Container(const char *name, nvs_open_mode_t mode) :
        _mode(mode)
        //_index(name)
    {
    }

    Container::~Container()
    {
    }

    bool Container::readIndex()
    {
        auto startAddress = _getStartAddress();
        auto endAddress = _getEndAddress();
        auto startSector = startAddress / 4096;
        auto address = startAddress;
        _index._address = startAddress;
        _index.finalize();

        _sectors.clear();
        _sectors.resize(_getNumSectors());

        do {
            IndexTuple index;
            uint16_t sector = (address - startAddress) / 4096;
            if (!ESP.flashRead(address, reinterpret_cast<uint8_t *>(&index._index), index._index.size())) {
                __DBG_printf("failed to read index address=%08x", address);
                return false;
            }
            // last index in sector?
            if (index._index._type == IndexType::INVALID) {
                _sectors[sector - startSector]._writePosition = address + index._index._length;
                sector++;
                address = sector * 4096;
                if (address >= endAddress) {
                    __DBG_printf("last sector reached");
                    return false;
                }
            }
            else {
                // validate data
                if (!index._index.validateCrc()) {
                    __DBG_printf("invalid crc index address=%08x", address);
                    return false;
                }
                if (index._index._type == IndexType::NAMESPACE) {
                    if (!ESP.flashRead(address + index._index.size(), reinterpret_cast<uint8_t *>(&index._namespace) + index._index.size(), index._namespace.dataSize())) {
                        __DBG_printf("failed to read namespace address=%08x", address);
                        return false;
                    }
                    if (!index._namespace.validateCrc()) {
                        __DBG_printf("invalid crc namespace address=%08x", address);
                        return false;
                    }
                    // compare name
                    if (index._namespace.nameEquals(index._namespace._name)) {
                        _index = index._namespace;
                        return true;
                    }
                }
            }
            // try next index
            address += index._index._length;
            if (address + 4096 >= endAddress) {
                __DBG_printf("invalid address=%08x (%u) end=%08x", address, address - startAddress, endAddress);
                return false;
            }
        } while (true);

        return false;
    }

    void Container::createIndex()
    {
        //_sectors.clear();
        //_sectors.resize(_getNumSectors());

        //auto startAddress = _getStartAddress();
        //_index._address = startAddress + _index.size();
        //_index.finalize();
        //if (!ESP.flashEraseSector(startAddress)) {
        //    __DBG_printf("failed to erase sector %u", _index._sector);
        //    return;
        //}
        //if (!ESP.flashWrite(startAddress, _index, _index.size())) {
        //    __DBG_printf("failed to write index %08x (%u)", startAddress, 0);
        //    return;
        //}
        //IndexHeader header;
        //strncpy(header._name, _name, NVS_PART_NAME_MAX_SIZE - 1);
        //header.finalize();
    }

};

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    auto nameLen = strlen_P(name);
    if (nameLen < 1 || nameLen >= NVS_PART_NAME_MAX_SIZE) {
        return ESP_ERR_NVS_INVALID_NAME;
    }
    auto container = new NVSFlashStorage::Container(name, open_mode);
    if (!container) {
        return ESP_ERR_NO_MEM;
    }
    if (!container->readIndex()) {
        if (open_mode == nvs_open_mode_t::NVS_READONLY) {
            delete container;
            return ESP_ERR_NVS_NOT_FOUND;
        }
        container->createIndex();
    }
    *out_handle = reinterpret_cast<nvs_handle_t>(container);
    return ESP_OK;
}


void nvs_close(nvs_handle_t handle)
{
    if (handle) {
        delete reinterpret_cast<NVSFlashStorage::Container *>(handle);
    }
}

#include "pop_pack.h"

#endif
