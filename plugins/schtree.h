/*
 * schtree.h
 *
 *  Created on: Jun 30, 2015
 *      Author: balazs
 */

#ifndef SCHTREE_H_
#define SCHTREE_H_

#include <gst/gst.h>
#include "mprtpssubflow.h"

typedef struct _SchTree SchTree;
typedef struct _SchTreeClass SchTreeClass;
typedef struct _SchNode SchNode;

#define SCHTREE_TYPE             (schtree_get_type())
#define SCHTREE(src)             (G_TYPE_CHECK_INSTANCE_CAST((src),SCHTREE_TYPE,SchTree))
#define SCHTREE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),SCHTREE_TYPE,SchTreeClass))
#define SCHTREE_IS_SOURCE(src)          (G_TYPE_CHECK_INSTANCE_TYPE((src),SCHTREE_TYPE))
#define SCHTREE_IS_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),SCHTREE_TYPE))
#define SCHTREE_CAST(src)        ((SchTree *)(src))


#define MPRTP_SENDER_SCHTREE_MAX_PATH_NUM 32

struct _SchTree
{
  GObject          object;
  SchNode*         root;
  SchNode*         actual;
  GRWLock          rwmutex;
  MPRTPSSubflow*   paths[MPRTP_SENDER_SCHTREE_MAX_PATH_NUM];
  gint             path_values[MPRTP_SENDER_SCHTREE_MAX_PATH_NUM];
  gfloat           path_weights[MPRTP_SENDER_SCHTREE_MAX_PATH_NUM];
  void           (*set_rate)(SchTree*,MPRTPSSubflow*,float);
  MPRTPSSubflow* (*get_actual)(SchTree*);
  MPRTPSSubflow* (*get_next)(SchTree*);
  void           (*print)(SchTree*);
  void           (*set_path_and_value)(SchTree*, MPRTPSSubflow*, guint16);
  void           (*create_and_replace)(SchTree*);
  gint             max_value;
};

struct _SchTreeClass{
  GObjectClass parent_class;
};

struct _SchNode{
  SchNode*       left;
  SchNode*       right;
  SchNode*       next;
  MPRTPSSubflow* path;
};

GType schtree_get_type (void);

#endif /* SCHTREE_H_ */
