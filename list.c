/** \file list.c \version 1.0

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "base.h"

/** Initilize a new list
\arg el_size sizeof(element)
\arg buf_size initial buffer size
\pre \a el_size must be <= MAX_EL_SIZE
\pre \a buf_size must be <= MAX_BUF_SIZE
*/
List *list_init(List *self, UInt el_size, UInt buf_size)
{
	ASSERT(el_size <= MAX_EL_SIZE, "Max element size is %d", MAX_EL_SIZE);
	ASSERT(buf_size <= MAX_BUF_SIZE, "Max buffer size is %d", MAX_BUF_SIZE);
	self->length = 0;
	self->buffer = buf_size;
	self->el_size = el_size;
	self->data = safe_malloc(self->buffer * self->el_size);
	return self;
}

void list_fini(List *self)
{
	if(self->data) {
		safe_free(self->data);
		self->data = NULL;
	}
	memset(self, 0, sizeof(List));
}

List *list_save(List *self, char *filename)
{
	UInt16 MAGIC = 0xabcd;
	FILE *f = fopen(filename, "wb");
	if(!f)
		return 0;
	fwrite(&MAGIC, sizeof(UInt16), 1, f);
	fwrite(&self->el_size, sizeof(UInt16), 1, f);
	fwrite(&self->length, sizeof(UInt32), 1, f);
	fwrite(self->data, sizeof(UInt8), self->el_size * self->length, f);
	fclose(f);
	return self;
}

List *list_load(List *self, char *filename)
{
	UInt16 MAGIC, el_size;
	UInt32 length;
	FILE *f = fopen(filename, "rb");
	if(!f)
		return 0;
	fread(&MAGIC, sizeof(UInt16), 1, f);
	if(MAGIC != 0xabcd) {
		fclose(f);
		return 0;
	}
	fread(&el_size, sizeof(UInt16), 1, f);
	fread(&length, sizeof(UInt32), 1, f);
	list_init(self, el_size, length);
	list_append(self, length);
	fread(self->data, sizeof(UInt8), el_size * length, f);
	fclose(f);
	return self;
}

/** returns the index of obj in self.  If the object is not found then a negative index of 
where it should be put is returned (except shifted by 1).  So list_insert(list, 1, (-ret)-1)
*/
int list_find_bsearch(List *self, void *obj, int(*compar)(List *, UInt32, const void *))
{
	if(self->length == 0)
		return -1;
	if(compar(self, self->length-1, obj) < 0)
		return -(self->length + 1);
	// binary search
	int L = 0;
	int H = self->length;
	int c, i = (H - L)>>1;
	while(H > L) {
		c = compar(self, i, obj);
		if(c==0)	return i; 
		if(c < 0)	L = i+1;
		else	H = i;
		i = ((H - L)>>1) + L;
	}
	// not found
	return -(i+1);
}

/** Adjust the buffer (i.e. memory) to  the buffer with a hysterisis
\a diff is the NEW buffer size (+ for a shrinking buffer, - for a growing buffer)
\note change the length \em before calling this function
\note self->buffer contains irrelevent data upon entering this function
but it is valid after exiting
\todo add smarter algorithm for growth and compaction
*/
static List *mem_manage(List *self, int diff)
{
	if(diff >= 0) { /* We have extra buffer space */
		/* Do we have too much buffer.  Need to truncate it ? */
		if(diff > MIN_THRESHOLD && (diff > MAX_BUF_SIZE || (diff>>1) >= self->length)) {
			self->data = safe_realloc(self->data, self->length * self->el_size);
			self->buffer = 0;
		} else {
			self->buffer = diff;
		}
	} else { /* We don't have enough buffer */
		/* How much should we give it? */
		/* exponential growth */
		self->buffer = (self->length > MAX_BUF_SIZE) ? MAX_BUF_SIZE : self->length/2 + (self->length >= 2 ? 0:1);
		self->data = safe_realloc(self->data, (self->buffer + self->length) * self->el_size);
	}
	return self;
}

/** Appends \a num uninitilized elements to the end of the list
*/
List *list_append(List *self, UInt num)
{
	self->length += num;
	if(((int)self->buffer - (int)num) < 0)
		return mem_manage(self, (int)self->buffer - (int)num);
	self->buffer -= num;
	return self;
}

/** Insert \a num uninitilized elements starting at \a index
\pre index must be <= length
*/
List *list_insert(List *self, UInt num, UInt index)
{
	ASSERT(index <= self->length, "Out of bounds (%d > %d)", index, self->length);
	
	list_append(self, num);
	void *src = self->data + (self->length - num) * self->el_size;
	void *dest = self->data + self->length * self->el_size;
	int size = (self->length - num - index) * self->el_size;
	
	/* shift elements down */
	/*
	if(!(size & 0x3) && !((UInt32)dest & 0x3)) { // move by 4 byte words 
		UInt32 *dest32 = dest, *src32 = src;
		size += 4;
		while(size -= 4)  *(--dest32) = *(--src32);
	} else if(!(size & 0x1) && !((UInt32)dest & 0x1)) { // move by 2 byte words 
		UInt16 *dest16 = dest, *src16 = src;
		size += 2;
		while(size -= 2)  *(--dest16) = *(--src16);
	} else { // move by bytes 
		UInt8 *dest8 = dest, *src8 = src;
		size += 1;
		while(size -= 1)  *(--dest8) = *(--src8);
	}
	*/
	// just move bytes (slow)
	UInt8 *dest8 = dest, *src8 = src;
	size +=1;
	while(size -= 1) *(--dest8) = *(--src8);


	return self;
}

/** Removes \a num elements from the end of the list
\pre num must be <= self->length
*/
List *list_truncate(List *self, UInt num)
{
	ASSERT(num <= self->length, "Num must be <= %d", self->length);
	self->length -= num;
	return mem_manage(self, num + self->buffer);
}

/** Remove all the elements from the list
*/
List *list_clear(List *self)
{
	return list_truncate(self, self->length);
}

/** Removes \a num elements starting at \a index
\pre Range must not be out of bounds
*/
List *list_remove(List *self, UInt num, UInt index)
{
	ASSERT(index + num <= self->length, "Out of bounds (%d)", self->length);

	void *dest = self->data + index * self->el_size;
	void *src = self->data + (index+num) * self->el_size;
	int size = (self->length - (index+num)) * self->el_size;
	
	/* Shift elements down */
	//if(!((UInt32)src & 0x3) && !(size&0x3)) { /* by words */
	//	UInt32 *src32 = src, *dest32 = dest;
	//	size += 4;
	//	while(size -= 4)  *(dest32++) = *(src32++);
	//} else if(!((UInt32)src & 0x1) && !(size&0x1)) { /* by half-words */
	//	UInt16 *src16 = src, *dest16 = dest;
	//	size += 2;
	//	while(size -= 2)  *(dest16++) = *(src16++);
	//} else { /* by bytes */
		UInt8 *src8 = src, *dest8 = dest;
		size += 1;
		while(size -= 1)  *(dest8++) = *(src8++);
	//}

	self->length -= num;
	return mem_manage(self, num + self->buffer);
}

/** Make sure the buffer can hold at least \a num items.
\note The list length does not change (use list_append for that)
\note If the buffer is already >= \a num then this function is a no-op
*/
List *list_buffer(List *self, UInt num)
{
	ASSERT(num <= MAX_BUF_SIZE, "MAX_BUF_SIZE is %d", MAX_BUF_SIZE);
	if(self->buffer < num) {
		self->buffer = num;
		self->data = safe_realloc(self->data, (self->buffer + self->length) * self->el_size);
	}
	return self;
}

/** Truncate the buffer to zero.
This is usefull if you know you won't be adding any more elements.
*/
List *list_compact(List *self)
{
	if(self->buffer) {
		self->data = safe_realloc(self->data, self->length * self->el_size);
		self->buffer = 0;
	}
	return self;
}

void *listp_tail_call(List *self)
{
	return self->data + (self->length-1)*self->el_size;
}
