#ifndef __MY_LIST_H__
#define __MY_LIST_H__

#include "Mutex.h"

template <class DT>
class MyList;

template <class DT>
class MyListNode
{
private:
	MyListNode(DT *nodeData, MyListNode *nextPtr);

	DT *dataItem;
	MyListNode *next;
	MyListNode *prev;

	friend class MyList<DT>;
};

template<class DT>
MyListNode<DT>::MyListNode(DT *nodeData, MyListNode<DT> *nextPtr) : dataItem(nodeData)
{
	next = nextPtr;
	prev = NULL;
}

template<class DT>
class MyList
{
public:
	MyList();
	~MyList();

	void insert(DT *newData);
	void remove();
	void remove(DT *node);
	DT* deleteCursor();
	
	void clearList();
	void clear();

	void gotoBeginCursor();
	DT* getNextCursor();
	void getNext();
	DT* getCursor();
	int count() { return m_nCount; }
	
	void lock();
	void unlock();

private:
	MyListNode<DT>	*m_pHead, *m_pTail;
	MyListNode<DT>	*m_pCursor;
	MUTEX			m_hMutex;
	int	m_nCount;
};

template<class DT>
MyList<DT>::MyList()
{
	m_pHead = m_pTail = NULL;
	m_nCount = 0;
	MUTEX_INIT(&m_hMutex);
}

template<class DT>
MyList<DT>::~MyList()
{
	clear();
	MUTEX_DESTROY(&m_hMutex);
}

template<class DT>
void MyList<DT>::lock()
{
	MUTEX_LOCK(&m_hMutex);
}

template<class DT>
void MyList<DT>::unlock()
{
	MUTEX_UNLOCK(&m_hMutex);
}

template<class DT>
void MyList<DT>::insert(DT *newData)
{
	MyListNode<DT>* newNode = new MyListNode<DT>(newData,NULL);
 
	if(m_pHead == NULL)
	{
		m_pHead = newNode;
		m_pTail = newNode;
	}
	else 
	{
		newNode->prev = m_pTail;
		m_pTail->next = newNode;
		m_pTail = newNode;
	}

	m_nCount++;
}

template <class DT>
void MyList<DT>::remove()
{
	MyListNode<DT>* tmp = NULL;
	DT *data;

	if(m_pCursor == NULL) return;

	tmp = m_pCursor;

	if(m_pCursor == m_pHead )
	{
		if(m_pCursor->next == NULL)
			m_pHead = m_pTail = NULL;
		else
			m_pHead = m_pCursor->next;
		
	}
	else if(m_pCursor == m_pTail)
	{
		m_pTail = m_pTail->prev;
		m_pTail->next = NULL;
	}
	else 
	{
		m_pCursor->prev->next = m_pCursor->next;
		m_pCursor->next->prev = m_pCursor->prev;
	}
 
	// destroy the node
	if(tmp->dataItem) 
	{
		data = tmp->dataItem;
		delete data;
		data = NULL;
	}
	delete tmp;

	m_pCursor = NULL;
	m_nCount--;
}

template <class DT>
DT* MyList<DT>::deleteCursor()
{
	MyListNode<DT>* tmp = NULL;
	DT *data = NULL;
	MyListNode<DT>* tmp1= NULL;

	if(m_pCursor == NULL)
		return NULL;

	tmp1 = m_pCursor->next;

	tmp = m_pCursor;

	if(m_pCursor == m_pHead)
	{
		if(m_pCursor->next == NULL)
			m_pHead = m_pTail = NULL;
		else
			m_pHead = m_pCursor->next;
		
	}
	else if(m_pCursor == m_pTail)
	{
		m_pTail = m_pTail->prev;
		m_pTail->next = NULL;
	}
	else 
	{
		m_pCursor->prev->next = m_pCursor->next;
		m_pCursor->next->prev = m_pCursor->prev;
	}

	// destroy the node
	if(tmp->dataItem) 
	{
		data = tmp->dataItem;
	}
	delete tmp;

	m_pCursor = tmp1;

	m_nCount--;
	return data;
}

template <class DT>
void MyList<DT>::clear() 
{
	MyListNode<DT>* tmp;
	DT *data;

	m_pCursor = NULL;	
	while (m_pHead != NULL){
		tmp = m_pHead;
		m_pHead = m_pHead->next;
		data = tmp->dataItem;
		delete data;
		delete tmp;
	}

	m_pHead = m_pTail = m_pCursor= NULL;
	m_nCount = 0;
}

template <class DT>
void MyList<DT>::clearList() 
{
	MyListNode<DT>* tmp;

	m_pCursor = NULL;	
	while (m_pHead != NULL){
		tmp = m_pHead;
		m_pHead = m_pHead->next;
		delete tmp;
	}

	m_pHead = m_pTail = m_pCursor= NULL;
	m_nCount = 0;
}

template<class DT>
void MyList<DT>::gotoBeginCursor()
{
	m_pCursor = m_pHead;
}

template <class DT>
DT* MyList<DT>::getNextCursor()
{
	DT *data;
	
	if(m_pCursor != NULL)
	{
		data = m_pCursor->dataItem;
		m_pCursor = m_pCursor->next;
		return data;
	}
	else
	{
		return NULL;
	}
}

template <class DT>
void MyList<DT>::getNext()
{	
	if(m_pCursor != NULL)
		m_pCursor = m_pCursor->next;
}

template <class DT>
DT* MyList<DT>::getCursor()
{
	DT *data;
	
	if(m_pCursor != NULL)
	{
		data = m_pCursor->dataItem;
		return data;
	}
	else
	{
		return NULL;
	}
}

#endif
