/* Copyright (c) 1998 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  list.h
 *  Linked list data structure and routines 
 */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef struct _LIST {
    int d;
    struct _LIST *next;
}LIST;


#define LIST_NEXT(l) ((l)->next)
#define LIST_DATA(l) ((l)->d)
#define SET_LIST_NEXT(l,d) ((l)->next = (d))
#define SET_LIST_DATA(l,id) ((l)->d = (int)(id))
/*
LIST *new_list(void);
LIST *free_list(LIST *l);
LIST *append_list(LIST *a, LIST *b);
LIST *push_data(LIST *l,int d);
int pop_data(LIST **l);
LIST *add_data_to_circular_list(LIST *l,LIST **end,int d)
int remove_from_list(int d,LIST **list)
*/
LIST *new_list();
LIST *free_list();
LIST *append_list();
LIST *push_data();
int  pop_data();
LIST *add_data_to_circular_list();
int remove_from_list();
LIST *add_data();




