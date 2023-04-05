#include "list.h"

#include "vma.h"

list_t *ll_create(unsigned int data_size)
{
	list_t *ll;

	ll = malloc(sizeof(*ll));
	DIE(!ll, "malloc failed");

	ll->head = NULL;
	ll->data_size = data_size;
	ll->total_elements = 0;

	return ll;
}

void ll_add_nth_node(list_t *list, unsigned int n, const void *new_data)
{
	node_t *prev, *curr;
	node_t *new_node;

	if (!list)
		return;

	/* n >= list->size inseamna adaugarea unui nou nod la finalul
	 * listei. */
	if (n > list->total_elements)
		n = list->total_elements;

	curr = list->head;
	prev = NULL;
	while (n > 0) {
		prev = curr;
		curr = curr->next;
		--n;
	}

	new_node = malloc(sizeof(*new_node));
	DIE(!new_node, "malloc failed");
	new_node->data = malloc(list->data_size);
	DIE(!new_node->data, "malloc failed");
	memcpy(new_node->data, new_data, list->data_size);

	new_node->next = curr;
	if (!prev) /* Adica n == 0. */
		list->head = new_node;
	else
		prev->next = new_node;

	list->total_elements++;
}

node_t *ll_remove_nth_node(list_t *list, unsigned int n)
{
	node_t *prev, *curr;

	if (!list || !list->head)
		return NULL;

	/* n >= list->size - 1 inseamna eliminarea nodului de la finalul
	 * listei.
	 */
	if (n > list->total_elements - 1)
		n = list->total_elements - 1;

	curr = list->head;
	prev = NULL;
	while (n > 0) {
		prev = curr;
		curr = curr->next;
		--n;
	}

	if (!prev) /* Adica n == 0. */
		list->head = curr->next;
	else
		prev->next = curr->next;

	list->total_elements--;

	return curr;
}

unsigned int ll_get_size(list_t *list)
{
	if (!list)
		return 0;

	return list->total_elements;
}

void ll_free(list_t **pp_list)
{
	if (!pp_list || !*pp_list)
		return;

	while (ll_get_size(*pp_list) > 0)
		free_node(*pp_list, 0);

	free(*pp_list);
	*pp_list = NULL;
}

void free_node(list_t *list, int idx)
{
	node_t *removed_node = ll_remove_nth_node(list, idx);
	if (removed_node->data) {
		free(removed_node->data);
		removed_node->data = NULL;
	}
	free(removed_node);
	removed_node = NULL;
}
