#include "header.h"

typedef enum {
  RB_BUSY = 44,
  RB_FREE = 45
} avail_state;

typedef struct RBT_node {
  unsigned int key;
  struct sockaddr client_addr;
  unsigned int port;
  void *client_grp_list;
  avail_state av_status;  
  uint8_t is_moderator:1;
  int red; /* if red=0 then the node is black */
  struct RBT_node* left;
  struct RBT_node* right;
  struct RBT_node* parent;
  unsigned int capability;
} RBT_node;


/* Compare(a,b) should return 1 if *a > *b, -1 if *a < *b, and 0 otherwise */
/* Destroy(a) takes a pointer to whatever key might be and frees it accordingly */
typedef struct RBT_tree {
  int (*Compare)(unsigned int a, unsigned int b); 
  void (*DestroyKey)(unsigned int a);
  void (*PrintKey)(unsigned int a);
  /*  These nodes are created when RBTreeCreate is called.  root->left should always */
  /*  point to the node which is the root of the tree.  nil points to a */
  /*  node which should always be black but has aribtrary children and */
  /*  parent and no key or info.  The point of using these nodes is so */
  /*  that the root and nil nodes do not require special cases in the code */
  RBT_node* root;             
  RBT_node* nil;              
} RBT_tree;

RBT_tree* RBTreeCreate(int  (*CompFunc)(unsigned int, unsigned int),
           void (*DestFunc)(unsigned int), 
           void (*PrintFunc)(unsigned int));
RBT_node * RBTreeInsert(RBT_tree*, unsigned int , struct sockaddr *, unsigned int, avail_state, void *, unsigned int);
void RBTreePrint(RBT_tree*);
void RBDelete(RBT_tree* , RBT_node* );
void RBTreeDestroy(RBT_tree*);
RBT_node* TreePredecessor(RBT_tree*,RBT_node*);
RBT_node* TreeSuccessor(RBT_tree*,RBT_node*);
RBT_node* RBFindNodeByID(RBT_tree*, unsigned int);
void NullFunction(unsigned int);

