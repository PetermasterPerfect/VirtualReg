#include "List.h"

template<class T>
Node<T>* CreateNode(T value, POOL_TYPE pool)
{
	Node<T>* node = (Node<T>*)ExAllocatePool(pool, sizeof(Node<T>));
	if (node == nullptr)
		return nullptr;
	memmove(&node->value, &value, sizeof(value));
	node->next = nullptr;
	return node;
}

template <class T>
void List<T>::Add(T value, POOL_TYPE pool)
{
	Node<T>* i;
	Node<T>* buf = CreateNode(value, pool);
	if (head == nullptr)
		head = buf;
	else
	{
		i = head;
		while (i->next != nullptr)
			i = i->next;
		i->next = buf;
	}
	length++;
}

template <class T>
bool List<T>::Remove(int idx)
{

	Node<T>* tmp;
	Node<T>* buf = head;

	if (idx > length || idx < 1)
		return false;

	if (idx == 1)
	{
		buf = buf->next;
		ExFreePool(head);
		head = buf;
	}
	else
	{
		for (int i = 1; i < idx - 1; i++)
			buf = buf->next;
		tmp = buf->next;
		buf->next = tmp->next;

		if (idx == 1)
			head = buf;

		ExFreePool(tmp);
	}

	length--;
	return true;
}

template <class T>
void List<T>::ClearAll()
{
	Node<T>* buf = head;
	Node<T>* tmp;
	while (buf != nullptr)
	{
		tmp = buf;
		buf = buf->next;
		ExFreePool(tmp);
	}
	head = nullptr;
	length = 0;
}


template <class T>
int List<T>::Find(T value)
{
	Node<T>* buf = head;
	int i = 1;
	while (buf != nullptr)
	{
		if (buf->value == value)
			return i;
		buf = buf->next;
		i++;
	}
	return -1;
}

template <class T>
T List<T>::Get(int idx)
{
	if (idx > length)
		return 0;

	Node<T>* buf = head;
	for (int i = 1; i < idx; i++)
		buf = buf->next;

	return buf->value;
}


template <class T>
bool List<T>::IsEmpty()
{
	if (head == nullptr)
		return true;
	return false;
}