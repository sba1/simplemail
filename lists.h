#ifndef SM__LISTS_H
#define SM__LISTS_H

struct node
{
	struct node *next;
	struct node *prev;
	struct list *list; /* To which list belongs this node? */
};

struct list
{
	struct node *first;
	struct node *last;
};

/* Prototypes */
void list_init(struct list *list);
void list_insert(struct list *list, struct node *newnode, struct node *prednode);
void list_insert_tail(struct list *list, struct node *prednode);
struct node *list_remove_tail(struct list *list);
struct node *list_find(struct list *list, int num);
int list_length(struct list *list);
int node_index(struct node *node);
void node_remove(struct node *node);

#ifdef INLINEING
#define list_first(x) ((x)->first)
#define list_last(x) ((x)->last)
#define node_next(x) ((x)->next)
#define node_prev(x) ((x)->prev)
#define node_list(x) ((x)->list)
#else
struct node *list_first(struct list *list);
struct node *list_last(struct list *last);
struct node *node_next(struct node *node);
struct node *node_prev(struct node *node);
struct list *node_list(struct node *node);
#endif

#endif
