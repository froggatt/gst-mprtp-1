/*
 * mprtpssubflow.h
 *
 *  Created on: Jun 30, 2015
 *      Author: balazs
 */

#ifndef MPRTPSSUBFLOW_H_
#define MPRTPSSUBFLOW_H_

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include "gstmprtcpbuffer.h"

G_BEGIN_DECLS

typedef struct _MPRTPSenderSubflow MPRTPSSubflow;
typedef struct _MPRTPSenderSubflowClass MPRTPSSubflowClass;
typedef struct _MPRTPSubflowHeaderExtension MPRTPSubflowHeaderExtension;

#define MPRTPS_SUBFLOW_TYPE             (mprtps_subflow_get_type())
#define MPRTPS_SUBFLOW(src)             (G_TYPE_CHECK_INSTANCE_CAST((src),MPRTPS_SUBFLOW_TYPE,MPRTPSSubflow))
#define MPRTPS_SUBFLOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),MPRTPS_SUBFLOW_TYPE,MPRTPSSubflowClass))
#define MPRTPS_SUBFLOW_IS_SOURCE(src)          (G_TYPE_CHECK_INSTANCE_TYPE((src),MPRTPS_SUBFLOW_TYPE))
#define MPRTPS_SUBFLOW_IS_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),MPRTPS_SUBFLOW_TYPE))
#define MPRTPS_SUBFLOW_CAST(src)        ((MPRTPSSubflow *)(src))


typedef struct _MPRTPSubflowHeaderExtension{
  guint16 id;
  guint16 seq;
};

typedef enum{
	MPRTP_SENDER_SUBFLOW_STATE_NON_CONGESTED = 1,
	MPRTP_SENDER_SUBFLOW_STATE_CONGESTED     = 2,
	MPRTP_SENDER_SUBFLOW_STATE_PASSIVE       = 3,
} MPRTPSubflowStates;

typedef enum{
	MPRTP_SENDER_SUBFLOW_EVENT_DEAD       = 1,
	MPRTP_SENDER_SUBFLOW_EVENT_DISTORTION = 2,
	MPRTP_SENDER_SUBFLOW_EVENT_BID        = 3,
	MPRTP_SENDER_SUBFLOW_EVENT_SETTLED    = 4,
	MPRTP_SENDER_SUBFLOW_EVENT_CONGESTION = 5,
	MPRTP_SENDER_SUBFLOW_EVENT_KEEP       = 6,
	MPRTP_SENDER_SUBFLOW_EVENT_LATE       = 7,
	MPRTP_SENDER_SUBFLOW_EVENT_CHARGE     = 8,
	MPRTP_SENDER_SUBFLOW_EVENT_DISCHARGE  = 9,
	MPRTP_SENDER_SUBFLOW_EVENT_fi         = 10,
	MPRTP_SENDER_SUBFLOW_EVENT_JOINED     = 11,
	MPRTP_SENDER_SUBFLOW_EVENT_DETACHED   = 12,
} MPRTPSubflowEvent;


struct _MPRTPSenderSubflow{
  GObject              object;
  GstClock*            sysclock;
  guint16              id;
  GstPad*              outpad;
  gboolean             active;
  gboolean             linked;
  MPRTPSubflowStates   state;
  void               (*fire)(MPRTPSSubflow*,MPRTPSubflowEvent,void*);
  void               (*process_rtpbuf_out)(MPRTPSSubflow*, guint, GstRTPBuffer*);
  void               (*setup_sr_riport)(MPRTPSSubflow*, GstMPRTCPSubflowRiport*);
  guint16            (*get_id)(MPRTPSSubflow*);
  GstClockTime       (*get_sr_riport_time)(MPRTPSSubflow*);
  void               (*set_sr_riport_time)(MPRTPSSubflow*, GstClockTime);
  gboolean           (*is_active)(MPRTPSSubflow*);
  GstPad*            (*get_outpad)(MPRTPSSubflow*);
  //void               (*set_static_rtcp_outpad)(MPRTPSSubflow*,GstPad*);
  GMutex               mutex;
  GstClockTime         sr_riport_time;
  guint8               distortions;  //History of lost and discarded packet riports by using a continously shifted 8 byte value
  guint16              consequent_RR_missing;
  guint16              consequent_settlements;
  gboolean             increasement_request;
  guint16              consequent_distortions;
  //maintained by sending packets
  guint16              seq;               //The actual subflow specific sequence number
  guint16              cycle_num;         // the number of cycle the sequence number has
  guint32              ssrc;

  //refreshed by sending an SR
  guint16              HSN_s;             //HSN at the sender report time
  GstClockTime         SRT;               //Sending Report Time moments
  guint32              octet_count;       //
  gint32               packet_count;      //

  //refreshed by receiving a receiver report
  gboolean             RR_arrived;        //Indicate that a receiver report is arrived, used by the scheduler
  guint16              HSN_r;
  guint16              HCN_r;             //Highest cycle number received
  guint8               fraction_lost;
  guint32              total_lost_packets_num; //
  guint16              lost_packets_num;
  GstClockTimeDiff     RR_time_dif;      //the time difference between two consequtive RR
  guint32              jitter;
  guint32              LSR;
  guint32              DLSR;
  GstClockTime         RRT;
  guint32              delay;             //last reported delay of the path
  gfloat               LostRate;
  guint32              UB; //Utilized bytes
  guint32              DB; //Discarded bytes


  //refreshed by receiving Discarded packet report
  guint32              DP;
  GstClockTime         DPRT;

  //refreshed by sender after sending all reports out
  guint32              sent_report_size;
  guint32              received_report_size;

  //refreshed by the scheduler
  GstClockTime*        LRRT;
  guint32              LDPC; //Lost Discarded packet count
  gfloat               receive_rate;
  gfloat               SR; //Sending Rate
};

struct _MPRTPSenderSubflowClass {
  GObjectClass   parent_class;

};

GType mprtps_subflow_get_type (void);
MPRTPSSubflow* make_mprtps_subflow(guint16 id, GstPad* srcpad);

G_END_DECLS

#endif /* MPRTPSSUBFLOW_H_ */
