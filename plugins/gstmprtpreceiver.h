/* GStreamer
 * Copyright (C) 2015 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_MPRTPRECEIVER_H_
#define _GST_MPRTPRECEIVER_H_

#include <gst/gst.h>
#include "gstmprtcpbuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_MPRTPRECEIVER   (gst_mprtpreceiver_get_type())
#define GST_MPRTPRECEIVER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPRTPRECEIVER,GstMprtpreceiver))
#define GST_MPRTPRECEIVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPRTPRECEIVER,GstMprtpreceiverClass))
#define GST_IS_MPRTPRECEIVER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPRTPRECEIVER))
#define GST_IS_MPRTPRECEIVER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPRTPRECEIVER))

typedef struct _GstMprtpreceiver GstMprtpreceiver;
typedef struct _GstMprtpreceiverClass GstMprtpreceiverClass;

struct _GstMprtpreceiver
{
  GstElement base_mprtpreceiver;

  guint8         ext_header_id;
  GList*         subflows;
  GstPad*        rtp_srcpad;
  GstPad*        rtcp_srcpad;
  GstPad*        rtcp_sinkpad;

  gfloat         playout_delay;
  GMutex         mutex;
  guint32        path_skews[256];
  gint           path_skew_index;
  gint           path_skew_counter;

  GstTask*       playouter;
  GRecMutex      playouter_mutex;
  GstTask*       riporter;
  GRecMutex      riporter_mutex;
};

struct _GstMprtpreceiverClass
{
  GstElementClass base_mprtpreceiver_class;
};

GType gst_mprtpreceiver_get_type (void);

G_END_DECLS

#endif
