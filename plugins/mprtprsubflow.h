/*
 * mprtpssubflow.h
 *
 *  Created on: Jun 30, 2015
 *      Author: balazs
 */

#ifndef MPRTPRSUBFLOW_H_
#define MPRTPRSUBFLOW_H_

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include "gstmprtcpbuffer.h"

G_BEGIN_DECLS

typedef struct _MPRTPReceiverSubflow MPRTPRSubflow;
typedef struct _MPRTPReceiverSubflowClass MPRTPRSubflowClass;


#define MPRTPR_SUBFLOW_TYPE             (mprtpr_subflow_get_type())
#define MPRTPR_SUBFLOW(src)             (G_TYPE_CHECK_INSTANCE_CAST((src),MPRTPR_SUBFLOW_TYPE,MPRTPRSubflow))
#define MPRTPR_SUBFLOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),MPRTPR_SUBFLOW_TYPE,MPRTPRSubflowClass))
#define MPRTPR_SUBFLOW_IS_SOURCE(src)          (G_TYPE_CHECK_INSTANCE_TYPE((src),MPRTPR_SUBFLOW_TYPE))
#define MPRTPR_SUBFLOW_IS_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),MPRTPR_SUBFLOW_TYPE))
#define MPRTPR_SUBFLOW_CAST(src)        ((MPRTPRSubflow *)(src))



struct _MPRTPReceiverSubflow{
  GObject              object;
  guint16              id;
  GList*               gaps;
  GList*               result;
  guint16              received_since_cycle_is_increased;
  guint16              late_discarded;
  guint16              early_discarded;
  guint16              duplicated;
  guint16              actual_seq;
  guint8               ext_header_id;
  gboolean             seq_initialized;
  guint16              cycle_num;
  guint16              received;
  guint16              HSN;
  guint16              losts;
  GRWLock              rwmutex;

  GstClock*            sysclock;
  GMutex               mutex;
  GstClockTime         last_reception_time;
  guint32              last_RTP_timestamp;
  guint32              skews[100];
  guint64              reception_times[100];
  guint8               skews_counter;
  guint8               skews_write_index;
  guint8               skews_read_index;
  gboolean             active;

  void                 (*process_mprtp_packets)(MPRTPRSubflow*,GstRTPBuffer*, guint16 subflow_sequence);
  void                 (*process_mprtcpsr_packets)(MPRTPRSubflow*,GstRTCPBuffer*);
  void                 (*setup_mprtcpsr_packets)(MPRTPRSubflow*,GstRTCPBuffer*);
  void                 (*proc_mprtcpblock)(MPRTPRSubflow*,GstMPRTCPSubflowBlock*);
  gboolean             (*is_active)(MPRTPRSubflow*);
  guint32              (*get_skew)(MPRTPRSubflow*);
};

struct _MPRTPReceiverSubflowClass {
  GObjectClass   parent_class;
};


GType mprtpr_subflow_get_type (void);
MPRTPRSubflow* make_mprtpr_subflow(guint16 id, GstPad* sinkpad, guint8 header_ext_id);

G_END_DECLS

#endif /* MPRTPRSUBFLOW_H_ */
