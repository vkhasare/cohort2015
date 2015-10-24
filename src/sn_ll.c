/*
 * Copyright (c) 1995-2001  Matt Harper. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modifications, are permitted provided that the above copyright notice
 * and this paragraph are duplicated in all such forms and that any
 * documentation, advertising materials, and other materials related to
 * such distribution and use acknowledge that the software was developed
 * by Matt Harper. The author's name may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission. THIS SOFTWARE IS PROVIDED ``AS IS''
 * AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE.
 */
#include "sn_ll.h"
/*
 * This file contains a generic implementation of doubly-linked lists
 *
 * Two basic types are used:
 *            sn_list_t - holds the head of a list
 *            sn_list_element_t - holds the list pointers in a list element
 *
 */


/*
 * Initialize a fresh doubly linked list
 * The result is an empty list
 */
void
SN_LIST_INIT(sn_list_t *listhead)
{
  SN_LIST_ASSERT(listhead != MY_NULL);
  listhead->length = 0;
  SN_LIST_ELEMENT_INIT(&listhead->list);
}



/*
 * Initialize the fields in an element used
 * to hold the list pointers which are use to hold objects
 * in the list
 */
void
SN_LIST_ELEMENT_INIT(sn_list_element_t *element)
{
  SN_LIST_ASSERT(element != MY_NULL);
  element->next = MY_NULL;
  element->prev = MY_NULL;
  SN_LIST_DEBUG_ONLY(element->flags = 0);
}



/*
 * Given a list element, return the next element in the list
 */
sn_list_element_t *
SN_LIST_ELEMENT_NEXT(sn_list_element_t *element)
{
  SN_LIST_ASSERT(element != MY_NULL);
  return element->next;
}



/*
 * Given a list element, return the previous element in the list
 */
sn_list_element_t *
SN_LIST_ELEMENT_PREV(sn_list_element_t *element)
{
  SN_LIST_ASSERT(element != MY_NULL);
  return element->prev;
}



/*
 * Return the first element of a list
 */
sn_list_element_t *
SN_LIST_HEAD(sn_list_t *listhead)
{
  SN_LIST_ASSERT(listhead != MY_NULL);
  return listhead->list.prev;
}



/*
 * Return the last element of a list
 */
sn_list_element_t *
SN_LIST_TAIL(sn_list_t *listhead)
{
  SN_LIST_ASSERT(listhead != MY_NULL);
  return listhead->list.next;
}



/*
 * Return the number of elements in a list
 */
unsigned long
SN_LIST_LENGTH(sn_list_t *listhead)
{
  SN_LIST_ASSERT(listhead != MY_NULL);
  return listhead->length;
}



/*
 * Remove the specified element from the specified list
 */
void
SN_LIST_ELEMENT_UNLINK(sn_list_t *listhead, sn_list_element_t *element)
{
  SN_LIST_ASSERT(listhead != MY_NULL);
  SN_LIST_ASSERT(listhead->list.next != MY_NULL);
  SN_LIST_ASSERT(listhead->list.prev != MY_NULL);
  SN_LIST_ASSERT(element != MY_NULL);
  SN_LIST_ASSERT(listhead->length > 0);
  SN_LIST_ASSERT(sn_list_contains_element(listhead,element));
  SN_LIST_ASSERT(sn_list_length(listhead) == listhead->length);

  if (element->prev) {
    element->prev->next = element->next;
  }
  else {
    SN_LIST_ASSERT(listhead->list.prev == element);
    listhead->list.prev = element->next;
  }
  if (element->next) {
    element->next->prev = element->prev;
  }
  else {
    SN_LIST_ASSERT(listhead->list.next == element);
    listhead->list.next = element->prev;
  }
  element->next = MY_NULL;
  element->prev = MY_NULL;
  listhead->length--;
    
  SN_LIST_DEBUG_ONLY(element->flags &= ~(SN_LIST_ELEMENT_FLAG_ON_LIST));
  SN_LIST_ASSERT(!sn_list_contains_element(listhead,element));
  SN_LIST_ASSERT(sn_list_length(listhead) == listhead->length);
}



/*
 * Remove the first element from a list
 */
sn_list_element_t *
SN_LIST_REMOVE_HEAD(sn_list_t *listhead)
{
  sn_list_element_t *head;
  SN_LIST_ASSERT(listhead != MY_NULL);

  head = SN_LIST_HEAD(listhead);
  if (head != MY_NULL) {
    SN_LIST_ELEMENT_UNLINK(listhead, head);
  }
  return head;
}


/*
 * Remove the first element from a list
 */
void
SN_LIST_REMOVE_HEAD_NORETURN(sn_list_t *listhead)
{
  sn_list_element_t *head;
  SN_LIST_ASSERT(listhead != MY_NULL);

  head = SN_LIST_HEAD(listhead);
  if (head != MY_NULL) {
    SN_LIST_ELEMENT_UNLINK(listhead, head);
  }
}



/*
 * Remove the last element from a list
 */
sn_list_element_t *
SN_LIST_REMOVE_TAIL(sn_list_t *listhead)
{
  sn_list_element_t *tail;
  SN_LIST_ASSERT(listhead != MY_NULL);
  
  tail = SN_LIST_TAIL(listhead);
  if (tail != MY_NULL) {
    SN_LIST_ELEMENT_UNLINK(listhead, tail);
  }
  return tail;
}



/*
 * Insert an element into a list at the specified location
 *
 * If insert_before_element is MY_NULL, then insert the element
 * at the tail of the list
 */
void SN_LIST_ELEMENT_INSERT_BEFORE(sn_list_t *listhead,
				    sn_list_element_t *element,
				    sn_list_element_t *insert_before_element)
{
  SN_LIST_ASSERT(element->next == MY_NULL);
  SN_LIST_ASSERT(element->prev == MY_NULL);
  SN_LIST_ASSERT((insert_before_element == MY_NULL) ||
	 sn_list_contains_element(listhead,insert_before_element));
  SN_LIST_DEBUG_ONLY(SN_LIST_ASSERT(!(element->flags & SN_LIST_ELEMENT_FLAG_ON_LIST));)
  if (insert_before_element == listhead->list.prev) {
    /* insert element at the head of the list */
    element->prev = MY_NULL;
    element->next = listhead->list.prev;
    listhead->list.prev = element;
    if (element->next) {
      element->next->prev = element;
    }
    else {
      listhead->list.next = element;
      SN_LIST_ASSERT(listhead->list.next == listhead->list.prev);
    }
  }
  else if (insert_before_element == MY_NULL) {
    /* insert element at tail of list */
    element->next = MY_NULL;
    element->prev = listhead->list.next;
    if (element->prev) {
      element->prev->next = element;
    }
    listhead->list.next = element;
  }
  else {
    /* insert element in middle of list */
    element->next = insert_before_element;
    element->prev = insert_before_element->prev;
    element->prev->next = (element);
    element->next->prev = (element);
  }
  SN_LIST_DEBUG_ONLY(element->flags |= SN_LIST_ELEMENT_FLAG_ON_LIST;)
  listhead->length++;
}



/*
 * Insert the specified element at the head of the list
 */
void
SN_LIST_INSERT_HEAD(sn_list_t *listhead,
			  sn_list_element_t *element)
{
  SN_LIST_ELEMENT_INSERT_BEFORE((listhead), (element), SN_LIST_HEAD((listhead)));
}



/*
 * Insert the specified element at the tail of the list
 */
void
SN_LIST_INSERT_TAIL(sn_list_t *listhead,
			  sn_list_element_t *element)
{
  SN_LIST_ELEMENT_INSERT_BEFORE((listhead), (element), MY_NULL);
}



/*
 * For a list being treated in LIFO fashion (a stack),
 * insert the element at the head of the list
 */
void
SN_LIST_PUSH(sn_list_t *listhead,
		   sn_list_element_t *element)
{
  SN_LIST_INSERT_HEAD(listhead,element);
}



/*
 * For a list being treated in LIFO fashion (a stack),
 * remove the first element at the head of the list
 * and return the element as the result
 */
sn_list_element_t *
SN_LIST_POP(sn_list_t *listhead)
{
  return SN_LIST_REMOVE_HEAD(listhead);
}



/*
 * For a list being treated in FIFO fashion (a queue),
 * insert the element at the tail of the list
 */
void
SN_LIST_ENQUEUE(sn_list_t *listhead, sn_list_element_t *element)
{
  SN_LIST_INSERT_TAIL(listhead,element);
}



/*
 * For a list being treated in FIFO fashion (a queue),
 * remove the element at the tail of the list
 */
sn_list_element_t *
SN_LIST_DEQUEUE(sn_list_t *listhead)
{
  return SN_LIST_REMOVE_HEAD(listhead);
}



/*
 * For a list being treated in FIFO fashion (a queue),
 * return a recently dequeued element to the head of the queue
 */
void SN_LIST_PUTBACKQUEUE(sn_list_t *listhead,
			   sn_list_element_t *element)
{
  SN_LIST_INSERT_HEAD(listhead,element);
}


/*
 ********************************************************
 * List debuggging functions
 ********************************************************
 */



/*
 * Determine if a list contains the specified element
 */
int
sn_list_contains_element(sn_list_t *listhead, sn_list_element_t *element)
{
  int result = FALSE;
  sn_list_element_t *elem_p;
  SN_LIST_ASSERT(listhead != MY_NULL);  
  SN_LIST_ASSERT(element != MY_NULL);

  for(elem_p=listhead->list.prev;elem_p;elem_p=elem_p->next) {
    if (elem_p == element) {
      result = TRUE;
      break;
    }
  }
  return result;
}



/*
 * A debug function to verify that the specified list actually
 * contains the correct number of elements
 */
unsigned long
sn_list_length(sn_list_t *listhead)
{
  unsigned long length = 0;
  sn_list_element_t *elem_p;
  SN_LIST_ASSERT(listhead != MY_NULL);
  
  for(elem_p=listhead->list.prev;elem_p;elem_p=elem_p->next) {
    SN_LIST_ASSERT((elem_p->next != MY_NULL) || (listhead->list.next == elem_p));
    length++;
  }

  SN_LIST_ASSERT((length > 0) || (listhead->list.next == MY_NULL));
  SN_LIST_ASSERT(length == listhead->length);
  return length;
}

