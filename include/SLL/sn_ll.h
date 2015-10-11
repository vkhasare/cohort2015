//#include "header.h"

#ifdef  SN_LIST_DEBUG
#define SN_LIST_DEBUG_ONLY(x) x
#else
#define SN_LIST_DEBUG_ONLY(x)
#endif

#ifdef SN_LIST_ASSERT_ENABLE
#define SN_LIST_ASSERT(X) ASSERT(X)
#else
#define SN_LIST_ASSERT(X)
#endif

#define MY_NULL 0

#ifndef FALSE
#define FALSE  (0)
#endif

#ifndef TRUE
#define TRUE   (!FALSE)
#endif

typedef struct sn_list_element {
  struct sn_list_element *next;
  struct sn_list_element *prev;
} sn_list_element_t;

typedef struct {
  sn_list_element_t list;
  unsigned long length;
} sn_list_t;

#define SN_LIST_ELEMENT_PTR(list_type, list_element_ptr, element ) \
        ((list_type *) (((unsigned long) element) -                \
        ((unsigned long)&(((list_type *)0)->list_element_ptr))))

#define SN_LIST_MEMBER_INSERT_HEAD(list, value, element) SN_LIST_INSERT_HEAD(list, &value->element)

#define SN_LIST_MEMBER_REMOVE(list, value, element) SN_LIST_ELEMENT_UNLINK(list, &value->element)

#define SN_LIST_MEMBER_HEAD(list, type, element) \
(SN_LIST_HEAD(list) ? SN_LIST_ELEMENT_PTR(type, element, SN_LIST_HEAD(list)) : MY_NULL)

#define SN_LIST_MEMBER_NEXT(value, type, element) \
(SN_LIST_ELEMENT_NEXT(&value->element) ? SN_LIST_ELEMENT_PTR(type, element, SN_LIST_ELEMENT_NEXT(&value->element)) : MY_NULL)

void SN_LIST_INIT(sn_list_t *listhead);
void SN_LIST_ELEMENT_INIT(sn_list_element_t *element);
sn_list_element_t *SN_LIST_ELEMENT_NEXT(sn_list_element_t *element);
sn_list_element_t *SN_LIST_ELEMENT_PREV(sn_list_element_t *element);
sn_list_element_t *SN_LIST_HEAD(sn_list_t *listhead);
sn_list_element_t *SN_LIST_TAIL(sn_list_t *listhead);
unsigned long SN_LIST_LENGTH(sn_list_t *listhead);
void SN_LIST_ELEMENT_UNLINK(sn_list_t *listhead, sn_list_element_t *element);
sn_list_element_t *SN_LIST_REMOVE_HEAD(sn_list_t *listhead);
void SN_LIST_REMOVE_HEAD_NORETURN(sn_list_t *listhead);
sn_list_element_t *SN_LIST_REMOVE_TAIL(sn_list_t *listhead);
void SN_LIST_ELEMENT_INSERT_BEFORE(sn_list_t *listhead,
           sn_list_element_t *element,
           sn_list_element_t *insert_before_element);
void SN_LIST_INSERT_HEAD(sn_list_t *listhead,
       sn_list_element_t *element);
void SN_LIST_INSERT_TAIL(sn_list_t *listhead,
       sn_list_element_t *element);
void SN_LIST_PUSH(sn_list_t *listhead,
      sn_list_element_t *element);
sn_list_element_t *SN_LIST_POP(sn_list_t *listhead);
void SN_LIST_ENQUEUE(sn_list_t *listhead,
         sn_list_element_t *element);
sn_list_element_t *SN_LIST_DEQUEUE(sn_list_t *listhead);
void SN_LIST_PUTBACKQUEUE(sn_list_t *listhead,
        sn_list_element_t *element);
