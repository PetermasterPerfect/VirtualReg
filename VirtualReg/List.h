#pragma once
#include <ntddk.h>

template<class T>
struct Node
{
	Node<T>* next;
	T value;
};

template<class T>
class List
{
public:
	Node<T>* head = nullptr;
	int length = 0;

	void Add(T, POOL_TYPE);
	bool Remove(int);
	void ClearAll();
	int Find(T);
	T Get(int i);
	bool IsEmpty();
};


template<class T>
Node<T>* CreateNode(T, POOL_TYPE);
