/*
 * The Gubbins Microcontroller Operating System
 *
 * Copyright 2025 Zynaptic Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/*
 * This header defines the API for the GubbinsMOS hash map data
 * structure, which supports fixed size key/value lookups.
 */

#ifndef GMOS_HASHMAP_H
#define GMOS_HASHMAP_H

#include <stdint.h>
#include <stdbool.h>

#include "gmos-config.h"
#include "gmos-scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Defines the function prototype for custom hash functions. A custom
 * hash function may be used for tables where the default hash function
 * is not considered suitable.
 * @param keyData This is a pointer to the key data that is to be used
 *     for generating the hash.
 * @param keySize This is the size of the key data, expressed as an
 *     integer number of bytes.
 * @return Returns a 16 bit integer hash value generated from the
 *     supplied key data.
 */
typedef uint16_t (*gmosHashMapHashingFunction_t) (
    uint8_t* keyData, uint8_t keySize);

/**
 * Defines the GubbinsMOS hash map data structure that is used when
 * implementing hash maps on devices that support conventional heap
 * based memory management.
 */
#if (GMOS_CONFIG_HEAP_SIZE > 0)
typedef struct gmosHashMapSubarray_t gmosHashMapSubarray_t;
typedef struct gmosHashMap_t {

    // Specifies a pointer to the start of the subarray list.
    gmosHashMapSubarray_t* subarrayList;

    // Specifies a pointer to the custom hash function.
    gmosHashMapHashingFunction_t hashingFunction;

    // Allocates memory for the background processing task.
    gmosTaskState_t backgroundTask;

    // Specifies the current number of entries in the hash map.
    uint32_t entryCount;

    // Holds the current state of the background processing task.
    uint16_t backgroundState;

    // Specifies the size of the hash map keys.
    uint8_t keySize;

    // Specifies the size of the hash map values.
    uint8_t valueSize;

} gmosHashMap_t;
#endif // GMOS_CONFIG_HEAP_SIZE

/**
 * Performs a one-time initialisation of a GubbinsMOS hash map. This
 * should be called during initialisation to set up the hash map for
 * subsequent use.
 * @param hashMap This is the hash map structure that is to be
 *     initialised.
 * @param keySize This is the size of the keys that are to be stored in
 *     the hash map, expressed as an integer number of bytes.
 * @param valueSize This is the size of the values that are to be stored
 *     in the hash map, expressed as an integer number of bytes.
 * @param hashingFunction This is a pointer to the hashing function to
 *     be used for the hash table, or a null reference if the default
 *     hashing function is to be used.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully initialising the hash map and 'false' otherwise.
 */
bool gmosHashMapInit (gmosHashMap_t* hashMap, uint8_t keySize,
    uint8_t valueSize, gmosHashMapHashingFunction_t hashingFunction);

/**
 * Puts a new entry into the hash map. This will either create a new
 * entry with the specified key and value or update an existing entry
 * with the same key.
 * @param hashMap This is the hash map structure into which the key
 *     value pair is to be stored.
 * @param key This is a pointer to a byte array that contains the key
 *     to be used when inserting the key value pair. It should match the
 *     fixed key length used by the hash map.
 * @param value This is a pointer to a byte array that contains the
 *     value to be used when inserting the key value pair. It should
 *     match the fixed value length used by the hash map.
 * @return Returns a boolean value which will be set to 'true' on
 *     successfully adding or updating the key value pair and 'false'
 *     on failure, usually due to a storage limitation.
 */
bool gmosHashMapPut (gmosHashMap_t* hashMap, uint8_t* key, uint8_t* value);

/**
 * Gets a entry from the hash map. If a matching entry exists, this will
 * read the entry value that corresponds to the specified key.
 * @param hashMap This is the hash map structure from which the
 *     requested entry should be read back.
 * @param key This is a pointer to a byte array that contains the key
 *     to be used for requesting the hash map entry. It should match the
 *     fixed key length used by the hash map.
 * @param value This is a pointer to a byte array that will be populated
 *     with the value of the requested entry. It should match the fixed
 *     value length used by the hash map. A null value may be used if
 *     the request is only being used to probe the map for an existing
 *     key.
 * @return Returns a boolean value which will be set to 'true' if an
 *     entry with the requested key exists in the hash table and 'false'
 *     otherwise.
 */
bool gmosHashMapGet (gmosHashMap_t* hashMap, uint8_t* key, uint8_t* value);

/**
 * Deletes an entry from the hash map. If a matching entry exists, this
 * will remove the entry that corresponds to the specified key.
 * @param hashMap This is the hash map structure from which the
 *     requested entry should be removed.
 * @param key This is a pointer to a byte array that contains the key
 *     to be used for removing the hash map entry. It should match the
 *     fixed key length used by the hash map.
 * @return Returns a boolean value which will be set to 'true' if an
 *     entry with the requested key was found and removed from the hash
 *     table and 'false' otherwise.
 */
bool gmosHashMapDelete (gmosHashMap_t* hashMap, uint8_t* key);

/**
 * Gets the load factor parameters for the hash map.
 * @param hashMap This is the hash map structure for which the load
 *     factor parameters are being requested.
 * @param capacity This is a pointer to an integer value which will be
 *     populated with the total hash map storage capacity. A null
 *     reference may be used if this information is not required.
 * @return Returns the total number of items currently stored in the
 *     hash map.
 */
uint32_t gmosHashMapLoadFactor (gmosHashMap_t* hashMap, uint32_t* capacity);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // GMOS_HASHMAP_H
