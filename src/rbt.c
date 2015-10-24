#include "RBT.h"
#include "server_DS.h"
#include "misc.h"
/*
 * <doc>
 * RBT_tree* RBTreeCreate
 * All the inputs are names of functions.  CompFunc takes to
 * void pointers to keys and returns 1 if the first arguement is
 * "greater than" the second.
 * If RBTreePrint is never called the print functions don't have to be
 * defined and NullFunction can be used.
 *
 * This function returns a pointer to the newly created
 * red-black tree.
 * </doc>
 */
RBT_tree* RBTreeCreate( int (*CompFunc) (unsigned int,unsigned int),
            void (*DestFunc) (unsigned int),
            void (*PrintFunc) (unsigned int)) {
  RBT_tree* newTree;
  RBT_node* temp;

  newTree=(RBT_tree*) SafeMalloc(sizeof(RBT_tree));
  newTree->Compare =  CompFunc;
  newTree->DestroyKey = DestFunc;
  newTree->PrintKey = PrintFunc;

  /*  see the comment in the RBT_tree structure in red_black_tree.h */
  /*  for information on nil and root */
  temp=newTree->nil= (RBT_node*) SafeMalloc(sizeof(RBT_node));
  temp->parent=temp->left=temp->right=temp;
  temp->red=0;
  temp->key=0;
  temp=newTree->root= (RBT_node*) SafeMalloc(sizeof(RBT_node));
  temp->parent=temp->left=temp->right=newTree->nil;
  temp->key=0;
  temp->red=0;
  return(newTree);
}

/* <doc>
 * void LeftRotate(RBT_tree* tree, RBT_node* x)
 * This takes a tree so that it can access the appropriate
 * root and nil pointers, and the node to rotate on.
 * This makes the parent of x be to the left of x, x the parent of
 * its parent before the rotation and fixes other pointers
 * accordingly.
 *
 * </doc>
 */
void LeftRotate(RBT_tree* tree, RBT_node* x) {
  RBT_node* y;
  RBT_node* nil=tree->nil;

  y=x->right;
  x->right=y->left;

  if (y->left != nil) y->left->parent=x; /* used to use sentinel here */
  /* and do an unconditional assignment instead of testing for nil */
  
  y->parent=x->parent;   

  /* instead of checking if x->parent is the root as in the book, we */
  /* count on the root sentinel to implicitly take care of this case */
  if( x == x->parent->left) {
    x->parent->left=y;
  } else {
    x->parent->right=y;
  }
  y->left=x;
  x->parent=y;

}


/* <doc>
 * void RightRotate(RBT_tree* tree, RBT_node* y)
 * This takes a tree so that it can access the appropriate
 * root and nil pointers, and the node to rotate on.
 * This makes the parent of x be to the left of x, x the parent of
 * its parent before the rotation and fixes other pointers
 * accordingly.
 *
 * </doc>
 */
void RightRotate(RBT_tree* tree, RBT_node* y) {
  RBT_node* x;
  RBT_node* nil=tree->nil;

  x=y->left;
  y->left=x->right;

  if (nil != x->right)  x->right->parent=y; /*used to use sentinel here */
  /* and do an unconditional assignment instead of testing for nil */

  /* instead of checking if x->parent is the root as in the book, we */
  /* count on the root sentinel to implicitly take care of this case */
  x->parent=y->parent;
  if( y == y->parent->left) {
    y->parent->left=x;
  } else {
    y->parent->right=x;
  }
  x->right=y;
  y->parent=x;

}

/* <doc>
 * void TreeInsertHelp(RBT_tree* tree, RBT_node* z)
 * tree is the tree to insert into and z is the node to insert.
 * Inserts z into the tree as if it were a regular binary tree.
 * This function is only intended to be called by the RBTreeInsert
 * function and not by the user.
 *
 * </doc>
 */
void TreeInsertHelp(RBT_tree* tree, RBT_node* z) {

  /*  This function should only be called by InsertRBTree*/
  RBT_node* x;
  RBT_node* y;
  RBT_node* nil=tree->nil;
  
  z->left=z->right=nil;
  y=tree->root;
  x=tree->root->left;
  while( x != nil) {
    y=x;
    if (1 == tree->Compare(x->key,z->key)) { /* x.key > z.key */
      x=x->left;
    } else { /* x,key <= z.key */
      x=x->right;
    }
  }
  z->parent=y;
  if ( (y == tree->root) ||
       (1 == tree->Compare(y->key,z->key))) { /* y.key > z.key */
    y->left=z;
  } else {
    y->right=z;
  }

}

/* <doc>
 * RBT_node * RBTreeInsert(RBT_tree* tree, unsigned int key)
 * tree is the red-black tree to insert a node which has a key
 * pointed to by key and info pointed to by info.
 * This function returns a pointer to the newly inserted node
 * which is guarunteed to be valid until this node is deleted.
 * What this means is if another data structure stores this
 * pointer then the tree does not need to be searched when this
 * is to be deleted.
 * Creates a node node which contains the appropriate key and
 * info pointers and inserts it into the tree.
 *
 * </doc>
 */
RBT_node * RBTreeInsert(RBT_tree* tree, unsigned int key, struct sockaddr *c_addr, unsigned int port, avail_state status, void *grp_ptr) {
  RBT_node * y;
  RBT_node * x;
  RBT_node * newNode;

  x=(RBT_node*) SafeMalloc(sizeof(RBT_node));

  x->key=key;
  /*copying sockaddr struct*/
  x->client_addr.sa_family = c_addr->sa_family;
  strcpy(x->client_addr.sa_data, c_addr->sa_data);
  rb_info_t *rb_info;

  /* generation of LL for group node pointers*/
  allocate_rb_info(&rb_info);
  rb_cl_grp_node_t *rb_grp_node = allocate_rb_cl_node(&rb_info);
  rb_grp_node->grp_addr = grp_ptr;

  /* storing grp node pointer in RB group list*/
  x->client_grp_list = rb_info;

  x->port = port;
  x->av_status = status;

  TreeInsertHelp(tree,x);
  newNode=x;
  x->red=1;
  while(x->parent->red) { /* use sentinel instead of checking for root */
    if (x->parent == x->parent->parent->left) {
      y=x->parent->parent->right;
      if (y->red) {
  x->parent->red=0;
  y->red=0;
  x->parent->parent->red=1;
  x=x->parent->parent;
      } else {
  if (x == x->parent->right) {
    x=x->parent;
    LeftRotate(tree,x);
  }
  x->parent->red=0;
  x->parent->parent->red=1;
  RightRotate(tree,x->parent->parent);
      } 
    } else { /* case for x->parent == x->parent->parent->right */
      y=x->parent->parent->left;
      if (y->red) {
  x->parent->red=0;
  y->red=0;
  x->parent->parent->red=1;
  x=x->parent->parent;
      } else {
  if (x == x->parent->left) {
    x=x->parent;
    RightRotate(tree,x);
  }
  x->parent->red=0;
  x->parent->parent->red=1;
  LeftRotate(tree,x->parent->parent);
      } 
    }
  }
  tree->root->left->red=0;
  return(newNode);

}

/* <doc>
 * RBT_node* TreeSuccessor(RBT_tree* tree,RBT_node* x)
 * tree is the tree in question, and x is the node we want the
 * the successor of.
 * This function returns the successor of x or NULL if no
 * successor exists.
 *
 * </doc>
 */  
RBT_node* TreeSuccessor(RBT_tree* tree,RBT_node* x) { 
  RBT_node* y;
  RBT_node* nil=tree->nil;
  RBT_node* root=tree->root;

  if (nil != (y = x->right)) { /* assignment to y is intentional */
    while(y->left != nil) { /* returns the minium of the right subtree of x */
      y=y->left;
    }
    return(y);
  } else {
    y=x->parent;
    while(x == y->right) { /* sentinel used instead of checking for nil */
      x=y;
      y=y->parent;
    }
    if (y == root) return(nil);
    return(y);
  }
}

/* <doc>
 * RBT_node* TreePredecessor(RBT_tree* tree, RBT_node* x)
 * tree is the tree in question, and x is the node we want the
 * the predecessor of.
 * This function returns the predecessor of x or NULL if no
 * predecessor exists.
 *
 * </doc>
 */
RBT_node* TreePredecessor(RBT_tree* tree, RBT_node* x) {
  RBT_node* y;
  RBT_node* nil=tree->nil;
  RBT_node* root=tree->root;

  if (nil != (y = x->left)) { /* assignment to y is intentional */
    while(y->right != nil) { /* returns the maximum of the left subtree of x */
      y=y->right;
    }
    return(y);
  } else {
    y=x->parent;
    while(x == y->left) { 
      if (y == root) return(nil); 
      x=y;
      y=y->parent;
    }
    return(y);
  }
}

/* <doc>
 * void InorderTreePrint(RBT_tree* tree, RBT_node* x)
 * This function recursively prints the nodes of the tree
 * inorder using the PrintKey functions.
 *
 * </doc>
 */
void InorderTreePrint(RBT_tree* tree, RBT_node* x) {
  RBT_node* nil=tree->nil;
  RBT_node* root=tree->root;
  if (x != tree->nil) {
    InorderTreePrint(tree,x->left);
    printf("  key="); 
    tree->PrintKey(x->key);
    printf("  l->key=");
    if( x->left == nil) printf("NULL"); else tree->PrintKey(x->left->key);
    printf("  r->key=");
    if( x->right == nil) printf("NULL"); else tree->PrintKey(x->right->key);
    printf("  p->key=");
    if( x->parent == root) printf("NULL"); else tree->PrintKey(x->parent->key);
    printf("  red=%i\n",x->red);
    InorderTreePrint(tree,x->right);
  }
}


/* <doc>
 * void TreeDestHelper(RBT_tree* tree, RBT_node* x)
 * This function recursively destroys the nodes of the tree
 * postorder using the DestroyKey functions.
 *
 * </doc>
 */
void TreeDestHelper(RBT_tree* tree, RBT_node* x) {
  RBT_node* nil=tree->nil;
  if (x != nil) {
    TreeDestHelper(tree,x->left);
    TreeDestHelper(tree,x->right);
    tree->DestroyKey(x->key);
    free(x);
  }
}


/* <doc>
 * void RBTreeDestroy(RBT_tree* tree)
 * tree is the tree to destroy. Destroys the key and frees memory.
 * </doc>
 */
void RBTreeDestroy(RBT_tree* tree) {
  TreeDestHelper(tree,tree->root->left);
  free(tree->root);
  free(tree->nil);
  free(tree);
}


/* <doc>
 * void RBTreePrint(RBT_tree* tree)
 * This function recursively prints the nodes of the tree
 * inorder using the PrintKey functions.
 *
 * </doc>
 */
void RBTreePrint(RBT_tree* tree) {
  InorderTreePrint(tree,tree->root->left);
}


/* <doc>
 * RBT_node* RBExactQuery(RBT_tree* tree, unsigned int q)
 * Returns the a node with key equal to q.  If there are
 * multiple nodes with key equal to q this function returns
 * the one highest in the tree.
 *
 * </doc>
 */  
RBT_node* RBExactQuery(RBT_tree* tree, unsigned int q) {
  RBT_node* x=tree->root->left;
  RBT_node* nil=tree->nil;
  int compVal;
  if (x == nil) return(0);
  compVal=tree->Compare(x->key, q);
  while(0 != compVal) {/*assignemnt*/
    if (1 == compVal) { /* x->key > q */
      x=x->left;
    } else {
      x=x->right;
    }
    if ( x == nil) return(0);
    compVal=tree->Compare(x->key, q);
  }
  return(x);
}


/* <doc>
 * void RBDeleteFixUp(RBT_tree* tree, RBT_node* x)
 * Performs rotations and changes colors to restore red-black
 * properties after a node is deleted.
 *
 * </doc>
 */
void RBDeleteFixUp(RBT_tree* tree, RBT_node* x) {
  RBT_node* root=tree->root->left;
  RBT_node* w;

  while( (!x->red) && (root != x)) {
    if (x == x->parent->left) {
      w=x->parent->right;
      if (w->red) {
  w->red=0;
  x->parent->red=1;
  LeftRotate(tree,x->parent);
  w=x->parent->right;
      }
      if ( (!w->right->red) && (!w->left->red) ) { 
  w->red=1;
  x=x->parent;
      } else {
  if (!w->right->red) {
    w->left->red=0;
    w->red=1;
    RightRotate(tree,w);
    w=x->parent->right;
  }
  w->red=x->parent->red;
  x->parent->red=0;
  w->right->red=0;
  LeftRotate(tree,x->parent);
  x=root; /* this is to exit while loop */
      }
    } else { /* the code below is has left and right switched from above */
      w=x->parent->left;
      if (w->red) {
  w->red=0;
  x->parent->red=1;
  RightRotate(tree,x->parent);
  w=x->parent->left;
      }
      if ( (!w->right->red) && (!w->left->red) ) { 
  w->red=1;
  x=x->parent;
      } else {
  if (!w->left->red) {
    w->right->red=0;
    w->red=1;
    LeftRotate(tree,w);
    w=x->parent->left;
  }
  w->red=x->parent->red;
  x->parent->red=0;
  w->left->red=0;
  RightRotate(tree,x->parent);
  x=root; /* this is to exit while loop */
      }
    }
  }
  x->red=0;

}


/* <doc>
 * void RBDelete(RBT_tree* tree, RBT_node* z)
 * Deletes z from tree and frees the key and info of z
 * using DestoryKey .  Then calls
 * RBDeleteFixUp to restore red-black properties
 *
 * </doc>
 */
void RBDelete(RBT_tree* tree, RBT_node* z){
  RBT_node* y;
  RBT_node* x;
  RBT_node* nil=tree->nil;
  RBT_node* root=tree->root;

  y= ((z->left == nil) || (z->right == nil)) ? z : TreeSuccessor(tree,z);
  x= (y->left == nil) ? y->right : y->left;
  if (root == (x->parent = y->parent)) { /* assignment of y->p to x->p is intentional */
    root->left=x;
  } else {
    if (y == y->parent->left) {
      y->parent->left=x;
    } else {
      y->parent->right=x;
    }
  }
  if (y != z) { /* y should not be nil in this case */

    /* y is the node to splice out and x is its child */

    if (!(y->red)) RBDeleteFixUp(tree,x);
  
    tree->DestroyKey(z->key);
    y->left=z->left;
    y->right=z->right;
    y->parent=z->parent;
    y->red=z->red;
    z->left->parent=z->right->parent=y;
    if (z == z->parent->left) {
      z->parent->left=y; 
    } else {
      z->parent->right=y;
    }
    free(z); 
  } else {
    tree->DestroyKey(y->key);
    if (!(y->red)) RBDeleteFixUp(tree,x);
    free(y);
  }
  
}


