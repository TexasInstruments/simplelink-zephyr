/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc13xx_cc26xx

 ******************************************************************************
 
 Copyright (c) 2021-2026, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

#ifndef LIST_H_
#define LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Includes **********************************/

/* Standard C Libraries */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Module Header Files */

//*****************************************************************************
//
//! \addtogroup UTILITY
//! @{
//
//! \defgroup LIST
//! \brief Module for a doubly-linked list.
//! \details To use the List module, include the header file as follows:
//! @code
//! #include <ti/trx/host/utility/list.h>
//! @endcode
//!
//! The List module is an adaptation of the driver utility found under
//! \<SIMPLELINK_SDK\>\\source\\ti\\drivers\\utils. It has been modified to have a
//! finite set of entries.
//!
//! This document describes the features and usage of the list module. The key
//! features are:
//!
//! - @ref Util_List_init    "List Initialization"
//! - @ref Util_List_manage  "List Management"
//! - @ref Util_List_search  "List Search"
//!
//! @anchor Util_List_init
//! List Initialization
//! -------------------
//! A linked list is initialized by first clearing the list instance and
//! then setting the maximum number of acceptable entries.
//!
//! @code
//! #define MAX_ENTRIES (4U)
//! List list;
//!
//! Util_List_clear(&list);
//! Util_List_setMaxEntries(&list, MAX_ENTRIES);
//! @endcode
//!
//! The list is empty at this point with the "head" and "tail" pointer set to NULL.
//!
//! @anchor Util_List_manage
//! List Management
//! -------------------
//! Entries may be added or removed from the head of the list using the "push/pop"
//! APIs. The size of the list is amended accordingly.
//!
//! @code
//! Util_List_entry entry;
//! Util_List_entry *retval;
//! uint8_t size;
//!
//! Util_List_push(&list, &entry);
//! size = Util_List_size(&list) // size = 1
//!
//! retval = Util_List_pop(&list);
//! size = Util_List_size(&list) // size = 0
//!
//! if (Util_List_empty(&list))
//! {
//!     printf("empty");
//! }
//! @endcode
//!
//! Additionally, entries can be inserted into the list after a given entry using
//! Util_List_ins().
//!
//! The user can walk the list, without removing any entries, using the following
//! API:
//!
//!  | API      | Description                       |
//!  |----------|-----------------------------------|
//!  |Util_List_head |Get the head entry of the list     |
//!  |Util_List_next |Get the next entry in the list     |
//!  |Util_List_prev |Get the previous entry in the list |
//!  |Util_List_tail |Get the last entry in the list     |
//!
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/


/*********************************** Enums ************************************/


/********************************** Typedefs **********************************/


/********************************** Structs ***********************************/

/** Structure implementing a list entry. */
typedef struct Util_List_Entry_t
{
    /** Pointer to the next entry in the linked list. */
    struct Util_List_Entry_t *next;
    /** Pointer to the previous entry in the linked list. */
    struct Util_List_Entry_t *prev;
}Util_List_Entry;

/** Structure implementing a doubly-linked list */
typedef struct Util_List_t
{
    /** Pointer to the first entry in the list. */
    Util_List_Entry *head;
    /** Pointer to the last entry in the list. */
    Util_List_Entry *tail;
    /** Maximum number of entries */
    uint8_t maxEntries;
    /** Number of entries in the list */
    uint8_t nEntries;
}Util_List;

/***************************** Static Inline Functions ************************/

/** Function to test whether a linked list is empty
 *
 *  @param[in] list A pointer to a linked list
 *
 *  @retval true    if list is empty
 *  @retval false   if list is not empty
 */
static inline bool Util_List_empty(const Util_List *list)
{
    return (list->head == NULL);
}

/** Function to return the head of a linked list
 *
 *  This function does not remove the head, it simply returns a pointer to
 *  it. This function is typically used when traversing a linked list.
 *
 *  @param[in]  list A pointer to the linked list.
 *
 *  @return Pointer to the entry at the head of the linked list or NULL if
 *          empty.
 */
static inline Util_List_Entry *Util_List_head(const Util_List *list)
{
    return (list->head);
}

/** Function to return the next entry in a linked list.
 *
 *  This function does not remove the entry, it simply returns a pointer to
 *  it. This function is typically used when traversing a linked list.
 *
 *  @param[in] entry Entry in the list.
 *
 *  @return Pointer to the next entry in linked list or NULL if at the end.
 */
static inline Util_List_Entry *Util_List_next(const Util_List_Entry *entry)
{
    return (entry->next);
}


/** Function to return the previous entry in a linked list.
 *
 *  This function does not remove the entry, it simply returns a pointer to
 *  it. This function is typically used when traversing a linked list.
 *
 *  @param[in] entry Entry in the list.
 *
 *  @return Pointer to the prev entry in linked list or NULL if at the
 *          beginning.
 */
static inline Util_List_Entry *Util_List_prev(const Util_List_Entry *entry)
{
    return (entry->prev);
}


/** Function to return the tail of a linked list.
 *
 *  This function does not remove the tail, it simply returns a pointer to
 *  it. This function is typically used when traversing a linked list.
 *
 *  @param[in] list A pointer to the linked list
 *
 *  @return Pointer to the last entry in the linked list or NULL if empty.
 */
static inline Util_List_Entry *Util_List_tail(const Util_List *list)
{
    return (list->tail);
}


/** Function to get the current number of entries in the linked list.
 *
 *  @param[in] list A pointer to the linked list
 *
 *  @return NEntries Maximum number of linked list entries.
 */
static inline uint8_t Util_List_size(const Util_List *list)
{
    return (list->nEntries);
}

/** Function to get the number of available entries in the linked list.
 *
 *  @param[in] list A pointer to the linked list
 *
 *  @return  Number of available entries in the linked list.
 */
static inline uint8_t Util_List_available(const Util_List *list)
{
    return (list->maxEntries - list->nEntries);
}

/** Function to set the maximum number of entries in the linked list.
 *
 *  @param[in] list A pointer to the linked list
 *  @param[in] maxEntries Maximum number of linked list entries.
 */
static inline void Util_List_setMaxEntries(Util_List *list, uint8_t maxEntries)
{
    list->maxEntries = maxEntries;
}

/***************************** Extern Prototypes ******************************/

/** Function to initialize the contents of a List.
 *
 *  @param[in, out]  list Pointer to a List structure that will be used to
 *                        maintain a linked list.
 */
extern void Util_List_clear(Util_List *list);

/** Function to atomically get the first entry in a linked list.
 *
 *  This function is used to process entries sequentially from head to tail.
 *  It will remove the entry from the linked list as it retrieves them.
 *
 *  @param[in]  list A pointer to a linked list.
 *
 *  @return Pointer to the first entry in the linked list or NULL if empty.
 */
extern Util_List_Entry *Util_List_pop(Util_List *list);

/** Function to atomically get the last entry in a linked list.
 *
 *  This function is used to process entries sequentially from tail to head.
 *  It will remove the entry from the linked list as it retrieves them.
 *
 *  @param[in]  list A pointer to a linked list.
 *
 *  @return Pointer to the last entry in the linked list or NULL if empty.
 */
extern Util_List_Entry *Util_List_popTail(Util_List *list);

/** Function to atomically remove a specified entry from a linked list
 *  using its pointer.
 *
 *  @note entry cannot be NULL nor can it be an entry that isn't already in the
 *        list; this behavior is undefined
 *
 *  @pre  Util_List_seek to find the entry prior to deleting it.
 *
 *  @param[in] list A pointer to a linked list.
 *  @param[in] entry A pointer to the specific entry to remove.
 */
void Util_List_delete(Util_List *list, Util_List_Entry *entry);

/** Function to add an entry to the end of a linked list.
 *
 *  @param[in]  list A pointer to the linked list.
 *  @param[in]  newEntry New entry to append.
 *
 *  @retval  0  if successful
 *  @retval -1  unsuccessful: max entries reached
 */
extern int8_t Util_List_push(Util_List *list, Util_List_Entry *newEntry);

/** Function to atomically append one list to the end of another linked list.
 * The provided listToConcat will be emptied if this operation is successful.
 *
 *  @param[in]  list A pointer to the linked list to append to.
 *  @param[in]  listToConcat A pointer to the list to append from.
 *
 *  @retval  0  if successful
 *  @retval -1  unsuccessful: max entries reached
 */
extern int8_t Util_List_concat(Util_List *list, Util_List *listToConcat);

/** Function to insert an entry into a linked list after another entry. If NULL
 * is provided as the entry to insert after, the new entry will become the head
 *
 *  @param[in]  list A pointer to the linked list.
 *  @param[in]  newEntry New entry to insert.
 *  @param[in]  curEntry Entry that will precede the newEntry.
 *
 *  @retval  0  if successful
 *  @retval -1  unsuccessful: max entries reached
 */
extern int8_t Util_List_ins(Util_List *list, Util_List_Entry *newEntry, Util_List_Entry *curEntry);

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************

#ifdef __cplusplus
}
#endif

#endif // LIST_H_
