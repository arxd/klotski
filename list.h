/** \file list.h \version 1.0

Dependencies:
	+ base
	
*/
#ifndef LIST_H
#define LIST_H
#include "base.h"

#define MIN_THRESHOLD 16
#define MAX_BUF_SIZE (1<<16)-1 /**< largest the buffer can get */
#define MAX_EL_SIZE (1<<16)-1 /**< largest element possible */

typedef struct {
	unsigned int length; /**< number of elements. READ ONLY*/
	unsigned short buffer; /**< free buffer elements. READ ONLY */
	unsigned short el_size; /**< sizeof(element). READ ONLY */
	void *data; /** Data Elements + Buffered elements. RW */
} List;

List *list_init(List *self, UInt32 el_size, UInt32 buf_size);
void list_fini(List *self);

List *list_save(List *self, char *filename);
List *list_load(List *self, char *filename);

List *list_compact(List *self);
List *list_buffer(List *self, UInt num);

List *list_append(List *self, UInt num);
List *list_insert(List *self, UInt num, UInt index);
int list_find_bsearch(List *self, void *obj, int(*compar)(List *, UInt32, const void *));

List *list_truncate(List *self, UInt num);
List *list_clear(List *self);
List *list_remove(List *self, UInt num, UInt index);

void *listp_tail_call(List *self);

#define foreach(type, list, el, index) \
	for(index=0;\
		index < (list).length \
			&& ((el = &list_el(type, list, index)) || 1);\
		++index) \

#define foreach_from(from, type, list, el, index) \
	for(index=(from);\
		index < (list).length \
			&& ((el = &list_el(type, list, index)) || 1);\
		++index) \

/* WARNING: No bounds checking on the following */
/** Use these if you have a list object 
*/
#define list_el(type, list, index) ((type*)(list).data)[index]
#define list_tail(type, list) list_el(type, list, (list).length-1) /* WARNING: NOT SIDE_EFFECT SAFE on list*/
#define list_head(type, list) list_el(type, list, 0)
#define list_push(type, list) listp_tail(type, list_append(&(list), 1)) /* len + 1 then listp_tail */

/** Use these if you have a pointer to a list
*/
#define listp_el(type, listp, index) ((type*)((listp)->data))[index]
#define listp_tail(type, listp) (*(type*)listp_tail_call(listp)) /* This is side-effect safe */
#define listp_head(type, listp) listp_el(type, listp, 0)
#define listp_push(type, listp) listp_tail(type, list_append(listp, 1)) /* len + 1 then listp_tail */

/** Use these if your el_size != sizeof(type)
 */
#define listv_el(type, listp, index) (*(type*)((listp)->data + (index)*(listp)->el_size))
#define listv_tail(type, listp) (*(type*)listp_tail_call(listp))
#define listv_push(type, listp) listv_tail(type, list_append(listp, 1))
#endif /* LIST_H */
