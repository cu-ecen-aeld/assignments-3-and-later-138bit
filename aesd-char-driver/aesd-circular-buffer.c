/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

#include <stdio.h>

#define INCREASE_OFFS(n) n = ((n + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
	if char_offset > strlen(), increase out and  char_offset -= strlen
    */
	uint8_t start = buffer->out_offs;

	printf("input: out_offs = %d, char_offset = %lu\n", start, char_offset);
	
	// Catch "empty" buffers
	if (start == buffer->in_offs && !buffer->full) {
		printf("Buffer is empty. Exiting..\n");
		return NULL;
	} 

	while (char_offset >= buffer->entry[start].size) {
		printf(" ... %lu > %lu ... \n", char_offset, buffer->entry[start].size);
		char_offset -= buffer->entry[start].size;
		INCREASE_OFFS(start);

		if (start == buffer->in_offs) {
			printf("char_offset too long!\n");
			return NULL;
		}
	}
	*entry_offset_byte_rtn = char_offset;
	printf("Returning out_offs (%d) with offset %lu (str = %s, size = %lu)\n",
		start, char_offset, buffer->entry[start].buffptr, buffer->entry[start].size);

    return &buffer->entry[start];
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
	if (buffer->full) {
		// If buffer is full, increase out_offs
		//buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		printf("increasing out_offs to %d\n", buffer->out_offs);
		INCREASE_OFFS(buffer->out_offs);
	}

	printf("Pos %d set to %s\n", buffer->in_offs, add_entry->buffptr);

	// Add entry to the array and increase in_offs
	memcpy(&buffer->entry[buffer->in_offs], add_entry, sizeof(struct aesd_buffer_entry));
	//buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
	INCREASE_OFFS(buffer->in_offs);

	// Check if buffer is full on the last line to make sure the starting condition doesn't mark the
	// buffer as full from the start.
	buffer->full = buffer->in_offs == buffer->out_offs;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
