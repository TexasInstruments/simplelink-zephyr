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

/*!

@file list.c

@brief Module implementing a doubly-linked list.

 */

/********************************** Includes **********************************/

/* Module Header Files */
#include "utility/list/list.h"
#include "utility/utils.h"
#include "hal/platform_util.h"

//*****************************************************************************
//
//! \addtogroup UTILITY
//! @{
//
//! \addtogroup LIST
//! @{
//
//*****************************************************************************

/********************************** Defines ***********************************/

/*************************** Variable declarations ****************************/


/********************** Static Function Implementations ***********************/



/********************** Extern Function Implementations ***********************/

/*  Function to initialize the contents of a List.
 *
 *  Input:  list Pointer to a List structure that will be used to
 *               maintain a linked list.
 *
 *  Return: Cleared list.
 */
void Util_List_clear(Util_List *list)
{
    uintptr_t cskey;
    Plat_Util_startCriticalSection((uintptr_t)&cskey);
    list->head = list->tail = NULL;
    list->nEntries = 0;
    list->maxEntries = 0;
    Plat_Util_endCriticalSection((uintptr_t)&cskey);
}

/*  Function to atomically get the first entry in a linked list.
 *
 *  This function is used to process entries sequentially from head to tail.
 *  It will remove the entry from the linked list Util_List t retrieves them.
 *
 *  head -->[_n_]-->[_n_]-->[_n_]-->NULL
 *        ^ [_p_] ^ [_p_]  ^[_p_]
 *        |___|___|___|    |  |
 *            v   |________|__|
 *            NULL         |________tail
 *
 *  head -----------\
 *  entry-->[_n_]-\  \--->[_n_]-->[_n_]-->NULL
 *          [_p_] |     ^ [_p_]  ^[_p_]
 *            |___|     |   |    |  |
 *            v         |___|____|__|
 *          NULL            v    |________tail
 *                          NULL
 *
 *  Input:  list A pointer to a linked list.
 *
 *  Return: Pointer to the first entry in the linked list or NULL if empty.
 */
Util_List_Entry *Util_List_pop(Util_List *list)
{
    uintptr_t cskey;
    Util_List_Entry *entry;
    Plat_Util_startCriticalSection((uintptr_t)&cskey);

    entry = list->head;
    if (NULL != entry)
    {
        list->head = entry->next;
        if (NULL != entry->next)
        {
            entry->next->prev = NULL;
        }
        else
        {
            list->tail = NULL;
        }
        entry->next = NULL;
        list->nEntries--;
    }

    Plat_Util_endCriticalSection((uintptr_t)&cskey);
    return(entry);
}

Util_List_Entry *Util_List_popTail(Util_List *list)
{
    uintptr_t cskey;
    Util_List_Entry *entry;
    Plat_Util_startCriticalSection((uintptr_t)&cskey);

    entry = list->tail;
    if (NULL != entry)
    {
        list->tail = entry->prev;
        if (NULL == entry->prev)
        {
            // There was only one entry in the list
            list->head = NULL;
        }
        entry->prev = NULL;
        list->nEntries--;
    }

    Plat_Util_endCriticalSection((uintptr_t)&cskey);
    return(entry);
}

/*  Function to atomically remove a specified entry from a linked list
 *  using its pointer.
 *
 *  Input:  list A pointer to a linked list.
 *          entry A pointer to the specific entry to remove.
 *
 *  Return: None.
 */
void Util_List_delete(Util_List *list, Util_List_Entry *entry)
{
    uintptr_t cskey;
    bool bEntryInList = false;

    Plat_Util_startCriticalSection((uintptr_t)&cskey);
    Util_List_Entry *local = list->head;
    while ((NULL != local) && (local != entry))
    {
        // Walk the list till you find the entry
        local = Util_List_next(local);
    }
    if(NULL != local)
    {
        // If you find the entry before exhausting the list, go
        // ahead and remove it
        bEntryInList = true;
    }
    if(bEntryInList)
    {
        if (1 == list->nEntries)
        {
            list->tail = NULL;
            list->head = NULL;
        }
        else if (NULL == entry->next)
        {
            // Last entry
            list->tail = entry->prev;
            entry->prev->next = NULL;
        }
        else if (NULL == entry->prev)
        {
            // first entry
            list->head = entry->next;
            entry->next->prev = NULL;
        }
        else
        {
            // in-between entries
            entry->prev->next = entry->next;
            entry->next->prev = entry->prev;
        }

        entry->prev = NULL;
        entry->next = NULL;
        list->nEntries--;
    }

    Plat_Util_endCriticalSection((uintptr_t)&cskey);
}

/*  Function to add an entry to the end of a linked list.
 *
 *  Input:  list     A pointer to the linked list.
 *          newEntry New entry to append.
 *
 *  Return:  0  if successful
 *          -1  unsuccessful: max entries reached
 */
int8_t Util_List_push(Util_List *list, Util_List_Entry *newEntry)
{
    uintptr_t cskey;
    int8_t retval = 0;
    if (list->nEntries < list->maxEntries)
    {
        Plat_Util_startCriticalSection((uintptr_t)&cskey);
        newEntry->next = NULL;
        newEntry->prev = list->tail;
        if (NULL != list->tail)
        {
            list->tail->next = newEntry;
        }
        else
        {
            list->head = newEntry;
        }
        list->tail = newEntry;
        list->nEntries++;
        Plat_Util_endCriticalSection((uintptr_t)&cskey);
    }
    else
    {
        retval = -1;
    }

    return(retval);
}


/*  Function to insert an entry into a linked list after another entry. If NULL
 * is provided as the entry to insert after, the new entry will become the head
 *
 *                        Util_List_ins here
 *                        | after entry[k]
 *                        v
 *  head -->[___] ->[_k_] ->[___] ->NULL
 *        ^ [_n_]/ ^[_n_]/ ^[_n_]/
 *        | [_p_]  |[_p_]  |[_p_]
 *        |___|____|__|    |  |
 *            v    |_______|__|
 *            NULL         |________tail
 *
 *
 *                         ->[___]
 *                        | ^[_n_]\
 *                        | |[_p_]|
 *                        | |  |  |
 *  head -->[___] ->[___] | |  |   ->[___] ->NULL
 *        ^ [_n_]/ ^[_n_]/  |  |    ^[_n_]/
 *        | [_p_]  |[_p_]   |  |    |[_p_]
 *        |___|____|__|     |  |    | |
 *            v    |        |__|____|_|
 *            NULL |___________|    |_______tail
 *
 *  Input:  list     A pointer to the linked list.
 *          newEntry New entry to insert.
 *          curEntry Entry that will precede the newEntry.
 *
 *  Return:  0  if successful
 *          -1  unsuccessful: max entries reached
 */
int8_t Util_List_ins(Util_List *list, Util_List_Entry *newEntry, Util_List_Entry *curEntry)
{
    uintptr_t cskey;
    int8_t retval = 0;
    if (list->nEntries < list->maxEntries)
    {
        Plat_Util_startCriticalSection((uintptr_t)&cskey);

        if((NULL == curEntry) && (NULL != list->head))
        {
            // Insert at head
            newEntry->next = list->head;
            newEntry->prev = NULL;
            list->head->prev = newEntry;
            list->head = newEntry;
        }
        else if(NULL == curEntry)
        {
            // Empty List
            list->head = newEntry;
            newEntry->next = NULL;
            newEntry->prev = NULL;
            list->tail = newEntry;
        }
        else
        {
            // could be in the middle or at the end
            newEntry->next = curEntry->next;
            newEntry->prev = curEntry;
            if (NULL == curEntry->next)
            {
                // Tail end of the list
                list->tail = newEntry;
            }
            else
            {
                // somewhere in the middle
                (curEntry->next)->prev = newEntry;
            }
            curEntry->next = newEntry;
        }
        list->nEntries++;
        Plat_Util_endCriticalSection((uintptr_t)&cskey);
    }
    else
    {
        // List is full
        retval = -1;
    }

    return(retval);
}

int8_t Util_List_concat(Util_List *list, Util_List *listToConcat)
{
    uintptr_t cskey;
    int8_t retval = 0;
    if (list->nEntries < list->maxEntries)
    {
        Plat_Util_startCriticalSection((uintptr_t)&cskey);
        const uint8_t newListLength = list->nEntries + listToConcat->nEntries;
        if(newListLength < list->maxEntries)
        {
            if(NULL != list->tail)
            {
                // Append list to the tail
                list->tail->next = listToConcat->head;
            }
            else
            {
                // List is empty
                list->head = listToConcat->head;
            }
            list->nEntries = newListLength;
            list->tail = listToConcat->tail;

            // Clear the list that was appended
            const uint8_t oldMaxEntries = listToConcat->maxEntries;
            Util_List_clear(listToConcat);
            Util_List_setMaxEntries(listToConcat, oldMaxEntries);
        }
        else
        {
            retval = -1;
        }
        Plat_Util_endCriticalSection((uintptr_t)&cskey);
    }
    else
    {
        retval = -1;
    }

    return(retval);
}

//*****************************************************************************
//
//! Close the Doxygen group.
//! @}
//
//! Close the Doxygen group.
//! @}
//
//*****************************************************************************
