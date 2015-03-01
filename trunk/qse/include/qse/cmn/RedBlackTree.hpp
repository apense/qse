/*
 * $Id$
 *
    Copyright (c) 2006-2014 Chung, Hyung-Hwan. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _QSE_CMN_REDBLACKTREE_HPP_
#define _QSE_CMN_REDBLACKTREE_HPP_

#include <qse/Types.hpp>
#include <qse/cmn/Mpool.hpp>

/////////////////////////////////
QSE_BEGIN_NAMESPACE(QSE)
/////////////////////////////////

template <typename T, typename COMPARATOR> class RedBlackTree;

template <typename T, typename COMPARATOR>
class RedBlackTreeNode
{
public:
	friend class RedBlackTree<T,COMPARATOR>;
	typedef RedBlackTreeNode<T,COMPARATOR> SelfType;

	enum Color
	{
		RED,
		BLACK
	};

	T value; // you can use this variable or accessor functions below

protected:
	Color color;
	SelfType* parent;
	SelfType* left; // left child
	SelfType* right; // right child

	RedBlackTreeNode(): color (BLACK), parent (this), left (this), right (this)
	{
		// no initialization on 'value' in this constructor.
	}

	RedBlackTreeNode(const T& value, Color color, SelfType* parent, SelfType* left, SelfType* right):
		value (value), color (color), parent (parent), left (left), right (right)
	{
		QSE_ASSERT (parent != this);
		QSE_ASSERT (left != this);
		QSE_ASSERT (right != this);
	}

public:
	T& getValue () { return this->value; }
	const T& getValue () const { return this->value; }
	void setValue (const T& v) { this->value = v; }

	bool isNil () const
	{
		return this->parent == this; // && this->left == this && this->right == this;
	}

	bool notNil () const
	{
		return !this->isNil ();
	}

	bool isBlack () const  { return this->color == BLACK; }
	bool isRed () const { return this->color == RED; }

	SelfType* getParent () { return this->parent; }
	const SelfType* getParent () const { return this->parent; }

	SelfType* getLeft () { return this->left; }
	const SelfType* getLeft () const { return this->left; }

	SelfType* getRight () { return this->right; }
	const SelfType* getRight () const { return this->right; }

	SelfType* getChild (int idx) { return idx == 0? this->left: this->right; }
#if 0
	void setBlack () { this->color = BLACK; }
	void setRed () { this->color = RED; }
	void setParent (SelfType* node) { this->parent = node; }
	void setLeft (SelfType* node) { this->left = node; }
	void setRight (SelfType* node) { this->right = node; }
#endif
};

template <typename T>
struct RedBlackTreeComparator
{
	int operator() (const T& v1, const T& v2) const
	{
		return (v1 > v2)? 1:
		       (v1 < v2)? -1: 0;
	}
};

template <typename T, typename COMPARATOR, typename NODE, typename GET_T>
class RedBlackTreeIterator
{
public:
	typedef NODE Node;
	typedef RedBlackTreeIterator<T,COMPARATOR,NODE,GET_T> SelfType;

	RedBlackTreeIterator (): current (QSE_NULL), previous (QSE_NULL), next_action (0) {}
	RedBlackTreeIterator (Node* root): current (root) 
	{
		this->previous = root->getParent();
		this->__get_next_node ();
	}

protected:
	void __get_next_node ()
	{
		int l = 1, r = 0;  // TODO:

		while (/*this->current &&*/ this->current->notNil())
		{
			if (this->previous == this->current->getParent())
			{
				/* the previous node is the parent of the current node.
				 * it indicates that we're going down to the getChild(l) */
				if (this->current->getChild(l)->notNil())
				{
					/* go to the getChild(l) child */
					this->previous = this->current;
					this->current = this->current->getChild(l);
				}
				else
				{
					this->next_action = 1;
					break;
					//if (walker (rbt, this->current, ctx) == QSE_RBT_WALK_STOP) break;

#if 0
					if (this->current->getChild(r)->notNil())
					{
						/* go down to the right node if exists */
						this->previous = this->current;
						this->current = this->current->getChild(r);
					}
					else
					{
						/* otherwise, move up to the parent */
						this->previous = this->current;
						this->current = this->current->getParent();
					}
#endif
				}
			}
			else if (this->previous == this->current->getChild(l))
			{
				/* the left child has been already traversed */

				this->next_action = 2;
				break;
				//if (walker (rbt, this->current, ctx) == QSE_RBT_WALK_STOP) break;

#if 0
				if (this->current->getChild(r)->notNil())
				{
					/* go down to the right node if it exists */ 
					this->previous = this->current;
					this->current = this->current->getChild(r);
				}
				else
				{
					/* otherwise, move up to the parent */
					this->previous = this->current;
					this->current = this->current->getParent();
				}
#endif
			}
			else
			{
				/* both the left child and the right child have been traversed */
				QSE_ASSERT (this->previous == this->current->getChild(r));
				/* just move up to the parent */
				this->previous = this->current;
				this->current = this->current->getParent();
			}
		}
	}

	void get_next_node ()
	{
		int l = 1, r = 0;  // TODO:

		if (next_action ==  1)
		{
			if (this->current->getChild(r)->notNil())
			{
				/* go down to the right node if exists */
				this->previous = this->current;
				this->current = this->current->getChild(r);
			}
			else
			{
				/* otherwise, move up to the parent */
				this->previous = this->current;
				this->current = this->current->getParent();
			}
		}
		else if (next_action == 2)
		{
			if (this->current->getChild(r)->notNil())
			{
				/* go down to the right node if it exists */ 
				this->previous = this->current;
				this->current = this->current->getChild(r);
			}
			else
			{
				/* otherwise, move up to the parent */
				this->previous = this->current;
				this->current = this->current->getParent();
			}
		}

		this->__get_next_node ();
	}

public:
	SelfType& operator++ () // prefix increment
	{
		this->get_next_node ();
		return *this;
	}

	SelfType operator++ (int) // postfix increment
	{
		SelfType saved (*this);
		this->get_next_node ();
		return saved;
	}

	bool isLegit() const
	{
		return current->notNil();
	}

	GET_T& operator* () // dereference
	{
		return this->current->getValue();
	}

	GET_T& getValue ()
	{
		return this->current->getValue();
	}

	// no setValue().

	Node* getNode ()
	{
		return this->current;
	}

protected:
	Node* current;
	Node* previous;
	int next_action;
};


///
///
///   A node is either red or black.
///   The root is black.
///   All leaves (NIL) are black. (All leaves are same color as the root.)
///   Every red node must have two black child nodes.
///   Every path from a given node to any of its descendant NIL nodes contains the same number of black nodes.
///
template <typename T, typename COMPARATOR = RedBlackTreeComparator<T> >
class RedBlackTree: public Mmged
{
public:
	typedef RedBlackTree<T,COMPARATOR> SelfType;
	typedef RedBlackTreeNode<T,COMPARATOR> Node;
	typedef RedBlackTreeIterator<T,COMPARATOR,Node,T> Iterator;
	typedef RedBlackTreeIterator<T,COMPARATOR,const Node,const T> ConstIterator;

	typedef RedBlackTreeComparator<T> DefaultComparator;

	RedBlackTree (Mmgr* mmgr = QSE_NULL, qse_size_t mpb_size = 0): Mmged(mmgr),  mp (mmgr, QSE_SIZEOF(Node), mpb_size), node_count (0)
	{
		// initialize nil
		this->nil = new(&this->mp) Node();
//		this->nil->setAll (Node::BLACK, this->nil, this->nil, this->nil);

		// set root to nil
		this->root = this->nil;
	}

	RedBlackTree (const RedBlackTree& rbt)
	{
		/* TODO */
	}

	~RedBlackTree ()
	{
		this->clear ();
		this->dispose_node (this->nil);
	}

	RedBlackTree& operator= (const RedBlackTree& rbt)
	{
		/* TODO */
		return *this;
	}

	Mpool& getMpool ()
	{
		return this->mp;
	}

	const Mpool& getMpool () const
	{
		return this->mp;
	}

	qse_size_t getSize () const
	{
		return this->node_count;
	}

	bool isEmpty () const
	{
		return this->node_count <= 0;
	}

	Node* getRoot ()
	{
		return this->root;
	}

	const Node* getRoot () const
	{
		return this->root;
	}

protected:
	void dispose_node (Node* node)
	{
		//call the destructor
		node->~Node (); 
		// free the memory
		::operator delete (node, &this->mp);
	}

	Node* find_node (const T& datum) const
	{
		Node* node = this->root;

		// normal binary tree search
		while (node->notNil())
		{
			int n = this->comparator (datum, node->value);
			if (n == 0) return node;

			if (n > 0) node = node->right;
			else /* if (n < 0) */ node = node->left;
		}

		return QSE_NULL;
	}

	void rotate (Node* pivot, bool leftwise)
	{
		/*
		 * == leftwise rotation
		 * move the pivot pair down to the poistion of the pivot's original
		 * left child(x). move the pivot's right child(y) to the pivot's original
		 * position. as 'c1' is between 'y' and 'pivot', move it to the right
		 * of the new pivot position.
		 *       parent                   parent
		 *        | | (left or right?)      | |
		 *       pivot                      y
		 *       /  \                     /  \
		 *     x     y    =====>      pivot   c2
		 *          / \               /  \
		 *         c1  c2            x   c1
		 *
		 * == rightwise rotation
		 * move the pivot pair down to the poistion of the pivot's original
		 * right child(y). move the pivot's left child(x) to the pivot's original
		 * position. as 'c2' is between 'x' and 'pivot', move it to the left
		 * of the new pivot position.
		 *
		 *       parent                   parent
		 *        | | (left or right?)      | |
		 *       pivot                      x
		 *       /  \                     /  \
		 *     x     y    =====>        c1   pivot
		 *    / \                            /  \
		 *   c1  c2                         c2   y
		 *
		 *
		 * the actual implementation here resolves the pivot's relationship to
		 * its parent by comparaing pointers as it is not known if the pivot pair
		 * is the left child or the right child of its parent,
		 */

		Node* parent, * z, * c;

		QSE_ASSERT (pivot != QSE_NULL);

		parent = pivot->parent;
		if (leftwise)
		{
			// y for leftwise rotation
			z = pivot->right;
			// c1 for leftwise rotation
			c = z->left;
		}
		else
		{
			// x for rightwise rotation
			z = pivot->left;
			// c2 for rightwise rotation
			c = z->right;
		}

		z->parent = parent;
		if (parent->notNil())
		{
			if (parent->left == pivot)
			{
				parent->left = z;
			}
			else
			{
				QSE_ASSERT (parent->right == pivot);
				parent->right = z;
			}
		}
		else
		{
			QSE_ASSERT (this->root == pivot);
			this->root = z;
		}

		if (leftwise)
		{
			z->left = pivot;
			pivot->right = c;
		}
		else
		{
			z->right = pivot;
			pivot->left = c;
		}

		if (pivot->notNil()) pivot->parent = z;
		if (c->notNil()) c->parent = pivot;
	}

	void rotate_left (Node* pivot)
	{
		this->rotate (pivot, true);
	}

	void rotate_right (Node* pivot)
	{
		this->rotate (pivot, false);
	}

	void rebalance_for_injection (Node* node)
	{
		while (node != this->root)
		{
			Node* tmp, * tmp2, * x_par, * x_grand_par;
			bool leftwise;

			x_par = node->parent;
			if (x_par->color == Node::BLACK) break;

			QSE_ASSERT (x_par->parent->notNil());

			x_grand_par = x_par->parent;
			if (x_par == x_grand_par->left)
			{
				tmp = x_grand_par->right;
				tmp2 = x_par->right;
				leftwise = true;
			}
			else
			{
				tmp = x_grand_par->left;
				tmp2 = x_par->left;
				leftwise = false;
			}

			if (tmp->color == Node::RED)
			{
				x_par->color = Node::BLACK;
				tmp->color = Node::BLACK;
				x_grand_par->color = Node::RED;
				node = x_grand_par;
			}
			else
			{
				if (node == tmp2)
				{
					node = x_par;
					this->rotate (node, leftwise);
					x_par = node->parent;
					x_grand_par = x_par->parent;
				}

				x_par->color = Node::BLACK;
				x_grand_par->color = Node::RED;
				this->rotate (x_grand_par, !leftwise);
			}
		}
	}

	void rebalance_for_removal (Node* node, Node* par)
	{
		while (node != this->root && node->color == Node::BLACK)
		{
			Node* tmp;

			if (node == par->left)
			{
				tmp = par->right;
				if (tmp->color == Node::RED)
				{
					tmp->color = Node::BLACK;
					par->color = Node::RED;
					this->rotate_left (par);
					tmp = par->right;
				}

				if (tmp->left->color == Node::BLACK &&
				    tmp->right->color == Node::BLACK)
				{
					if (tmp->notNil()) tmp->color = Node::RED;
					node = par;
					par = node->parent;
				}
				else
				{
					if (tmp->right->color == Node::BLACK)
					{
						if (tmp->left->notNil())
							tmp->left->color = Node::BLACK;
						tmp->color = Node::RED;
						this->rotate_right (tmp);
						tmp = par->right;
					}

					tmp->color = par->color;
					if (par->notNil()) par->color = Node::BLACK;
					if (tmp->right->color == Node::RED)
						tmp->right->color = Node::BLACK;

					this->rotate_left (par);
					node = this->root;
				}
			}
			else
			{
				QSE_ASSERT (node == par->right);
				tmp = par->left;
				if (tmp->color == Node::RED)
				{
					tmp->color = Node::BLACK;
					par->color = Node::RED;
					this->rotate_right (par);
					tmp = par->left;
				}

				if (tmp->left->color == Node::BLACK &&
				    tmp->right->color == Node::BLACK)
				{
					if (tmp->notNil()) tmp->color = Node::RED;
					node = par;
					par = node->parent;
				}
				else
				{
					if (tmp->left->color == Node::BLACK)
					{
						if (tmp->right->notNil())
							tmp->right->color = Node::BLACK;
						tmp->color = Node::RED;
						this->rotate_left (tmp);
						tmp = par->left;
					}
					tmp->color = par->color;
					if (par->notNil()) par->color = Node::BLACK;
					if (tmp->left->color == Node::RED)
						tmp->left->color = Node::BLACK;

					this->rotate_right (par);
					node = this->root;
				}
			}
		}

		node->color = Node::BLACK;
	}

	void remove_node (Node* node)
	{
		Node* x, * y, * par;

		QSE_ASSERT (node && node->notNil());

		if (node->left->isNil() || node->right->isNil())
		{
			y = node;
		}
		else
		{
			/* find a successor with NIL as a child */
			y = node->right;
			while (y->left->notNil()) y = y->left;
		}

		x = (y->left->isNil())? y->right: y->left;

		par = y->parent;
		if (x->notNil()) x->parent = par;

		if (par->notNil()) // if (par)
		{
			if (y == par->left)
				par->left = x;
			else
				par->right = x;
		}
		else
		{
			this->root = x;
		}

		if (y == node)
		{
			if (y->color == Node::BLACK && x->notNil())
				this->rebalance_for_removal (x, par); 

			this->dispose_node (y);
		}
		else
		{
			if (y->color == Node::BLACK && x->notNil())
				this->rebalance_for_removal (x, par);

			if (node->parent->notNil()) //if (node->parent)
			{
				if (node->parent->left == node) node->parent->left = y;
				if (node->parent->right == node) node->parent->right = y;
			}
			else
			{
				this->root = y;
			}

			y->parent = node->parent;
			y->left = node->left;
			y->right = node->right;
			y->color = node->color;

			if (node->left->parent == node) node->left->parent = y;
			if (node->right->parent == node) node->right->parent = y;

			this->dispose_node (node);
		}

		this->node_count--;
	}

public:
	Node* search (const T& datum)
	{
		return this->find_node (datum);
	}

	const Node* search (const T& datum) const
	{
		return this->find_node (datum);
	}

	Node* inject (const T& datum, int mode, bool* injected = QSE_NULL)
	{
		Node* x_cur = this->root;
		Node* x_par = this->nil;

		while (x_cur->notNil())
		{
			int n = this->comparator (datum, x_cur->value);
			if (n == 0)
			{
				if (injected) *injected = false;
				if (mode <= -1) return QSE_NULL; // return failure
				if (mode >= 1) x_cur->value = datum;
				return x_cur;
			}

			x_par = x_cur;

			if (n > 0) x_cur = x_cur->right;
			else /* if (n < 0) */ x_cur = x_cur->left;
		}

		Node* x_new = new(&this->mp) Node (datum, Node::RED, this->nil, this->nil, this->nil);
		if (x_par->isNil())
		{
			QSE_ASSERT (this->root->isNil());
			this->root = x_new;
		}
		else
		{
			int n = this->comparator (datum, x_par->value);
			if (n > 0)
			{
				QSE_ASSERT (x_par->right->isNil());
				x_par->right = x_new;
			}
			else
			{
				QSE_ASSERT (x_par->left->isNil());
				x_par->left = x_new;
			}

			x_new->parent = x_par;
			this->rebalance_for_injection (x_new);
		}

		this->root->color = Node::BLACK;
		this->node_count++;

		if (injected) *injected = true; // indicate that a new node has been injected
		return x_new;
	}

	Node* insert (const T& datum)
	{
		return this->inject (datum, -1, QSE_NULL);
	}

	Node* ensert (const T& datum)
	{
		return this->inject (datum, 0, QSE_NULL);
	}

	Node* upsert (const T& datum)
	{
		return this->inject (datum, 1, QSE_NULL);
	}

	Node* update (const T& datum)
	{
		Node* node = this->find_node (datum);
		if (node) node->value = datum;
		return node;
	}

	int remove (const T& datum)
	{
		Node* node = this->find_node (datum);
		if (node == QSE_NULL) return -1;

		this->remove_node (node);
		return 0;
	}

	void clear (bool clear_mpool = false)
	{
		while (this->root->notNil()) this->remove_node (this->root);
	}

	Iterator getIterator () const
	{
		return Iterator (this->root);
	}

	void dump (Node* node)
	{
		printf ("%d %d\n", node->value.getX(), node->value.getY());
		if (node->left->notNil()) this->dump (node->left);
		if (node->right->notNil()) this->dump (node->right);
	}

protected:
	Mpool      mp;
	COMPARATOR comparator;

	qse_size_t node_count;
	Node*      nil; // internal node to present nil
	Node*      root; // root node.

};


/////////////////////////////////
QSE_END_NAMESPACE(QSE)
/////////////////////////////////

#endif
