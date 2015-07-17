/* GStreamer Mprtp sender subflow
 * Copyright (C) 2015 Balázs Kreith (contact: balazs.kreith@gmail.com)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include "mprtpssubflow.h"
#include "gstmprtcpbuffer.h"

GST_DEBUG_CATEGORY_STATIC (gst_mprtpssubflow_debug_category);
#define GST_CAT_DEFAULT gst_mprtpssubflow_debug_category

G_DEFINE_TYPE (MPRTPSSubflow, mprtps_subflow, G_TYPE_OBJECT);


static void mprtps_subflow_finalize (GObject * object);
static void mprtps_subflow_process_rtpbuffer_out(MPRTPSSubflow* subflow,
	guint ext_header_id, GstRTPBuffer* rtp);
static void mprtps_subflow_proc_mprtcprr(MPRTPSSubflow* subflow,
	GstRTCPPacket *packet);
static void mprtps_subflow_FSM_fire(MPRTPSSubflow *subflow,
	MPRTPSubflowEvent event, void* data);
static void _print_rtp_packet_info(GstRTPBuffer *rtp);

static guint16 mprtps_subflow_get_id(MPRTPSSubflow* this);
static GstClockTime mprtps_subflow_get_sr_riport_time(MPRTPSSubflow* this);
static void mprtps_subflow_set_sr_riport_time(MPRTPSSubflow*,GstClockTime);
static gboolean mprtps_subflow_is_active(MPRTPSSubflow* this);
static GstPad* mprtps_subflow_get_outpad(MPRTPSSubflow* this);

static void
mprtps_subflow_setup_sr_riport(MPRTPSSubflow *this, GstMPRTCPSubflowRiport *header);

void
mprtps_subflow_class_init (MPRTPSSubflowClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = mprtps_subflow_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_mprtpssubflow_debug_category, "mprtpssubflow", 0, "MPRTP Sender Subflow");
}

/**
 * mprtp_source_new:
 *
 * Create a #MPRTPSource with @ssrc.
 *
 * Returns: a new #MPRTPSource. Use g_object_unref() after usage.
 */
MPRTPSSubflow*
make_mprtps_subflow(guint16 id, GstPad* srcpad)
{
  MPRTPSSubflow *result;

  result = g_object_new (MPRTPS_SUBFLOW_TYPE, NULL);
  result->id = id;
  result->outpad = srcpad;
  return result;


}

/**
 * mprtps_subflow_reset:
 * @src: an #MPRTPSSubflow
 *
 * Reset the subflow of @src.
 */
void
mprtps_subflow_reset (MPRTPSSubflow * subflow)
{
  subflow->seq = 0;
  subflow->cycle_num = 0;
  subflow->octet_count = 0;
  subflow->packet_count = 0;
  subflow->LRRT = NULL;
  subflow->SRT = 0;
  subflow->active = FALSE;
  subflow->linked = FALSE;
  subflow->ssrc = g_random_int ();
}


void
mprtps_subflow_init (MPRTPSSubflow * subflow)
{
  subflow->fire = mprtps_subflow_FSM_fire;
  subflow->process_rtpbuf_out = mprtps_subflow_process_rtpbuffer_out;
  subflow->setup_sr_riport = mprtps_subflow_setup_sr_riport;
  mprtps_subflow_reset (subflow);
  subflow->get_id = mprtps_subflow_get_id;
  subflow->get_sr_riport_time = mprtps_subflow_get_sr_riport_time;
  subflow->set_sr_riport_time = mprtps_subflow_set_sr_riport_time;
  subflow->is_active = mprtps_subflow_is_active;
  subflow->get_outpad = mprtps_subflow_get_outpad;

  g_mutex_init(&subflow->mutex);
  subflow->sysclock = gst_system_clock_obtain();
}


void
mprtps_subflow_finalize (GObject * object)
{
  MPRTPSSubflow *subflow = MPRTPS_SUBFLOW(object);
  g_object_unref (subflow->sysclock);
  subflow = MPRTPS_SUBFLOW_CAST (object);

}

void
mprtps_subflow_process_rtpbuffer_out(MPRTPSSubflow* subflow, guint ext_header_id, GstRTPBuffer* rtp)
{
	MPRTPSubflowHeaderExtension data;
	data.id = subflow->id;
	if(++subflow->seq == 0){
		++subflow->cycle_num;
	}
	data.seq = subflow->seq;
	++subflow->packet_count;

	subflow->octet_count += gst_rtp_buffer_get_payload_len(rtp);
	gst_rtp_buffer_add_extension_onebyte_header(rtp, ext_header_id, (gpointer) &data, sizeof(data));
	//_print_rtp_packet_info(rtp);
}

void
mprtps_subflow_proc_mprtcprr(MPRTPSSubflow* subflow, GstRTCPPacket *packet)
{
  guint8 *data = packet->rtcp->map.data;
  guint32 offset = packet->offset;
  GstClockTime now;
  guint32 lost_packet_now;
  now = gst_clock_get_time (subflow->sysclock);

  subflow->fraction_lost = data[offset + 8];
  lost_packet_now = (*((guint32*) (data + offset + 8)))>>8;
  subflow->lost_packets_num = lost_packet_now - subflow->total_lost_packets_num;
  subflow->total_lost_packets_num = lost_packet_now;
  subflow->HSN_r = *((guint16*) (data + offset + 12));
  subflow->HCN_r = *((guint16*) (data + offset + 14));
  subflow->jitter = *((guint32*) (data + offset + 16));
  subflow->LSR = *((guint32*) (data + offset + 20));
  subflow->DLSR = *((guint32*) (data + offset + 24));


  subflow->RR_time_dif = GST_CLOCK_DIFF(subflow->RRT, now);
  subflow->RRT = now;
  subflow->LRRT = &subflow->RRT;

  subflow->delay = now - subflow->DLSR - subflow->LSR;
}


void
mprtps_subflow_setup_sr_riport(MPRTPSSubflow *this,
		GstMPRTCPSubflowRiport *riport)
{
  GstMPRTCPSubflowBlock *block;
  GstRTCPSR *sr;
  guint64 ntptime;
  guint32 rtptime;

  ntptime = gst_clock_get_time(this->sysclock);

  rtptime = (guint32)(gst_rtcp_ntp_to_unix (ntptime)>>32), //rtptime

  block = gst_mprtcp_riport_add_block_begin(riport, this->id);
  sr = gst_mprtcp_riport_block_add_sr(block);
  gst_rtcp_header_change(&sr->header, NULL, NULL, NULL, NULL, NULL, &this->ssrc);

  gst_rtcp_srb_setup(&sr->sender_block, ntptime, rtptime,
	this->packet_count, this->octet_count);

  gst_mprtcp_riport_add_block_end(riport, block);
}


void
mprtps_subflow_FSM_fire(MPRTPSSubflow *subflow, MPRTPSubflowEvent event, void *data)
{
  gfloat *rate;
  switch(subflow->state){
    case MPRTP_SENDER_SUBFLOW_STATE_NON_CONGESTED:
	  switch(event){
	    case MPRTP_SENDER_SUBFLOW_EVENT_JOINED:
	      rate = data;
		  subflow->active = TRUE;
		  subflow->SR = *rate;
	    break;
	    case MPRTP_SENDER_SUBFLOW_EVENT_KEEP:
	      subflow->SR = subflow->receive_rate;
	    break;
	    case MPRTP_SENDER_SUBFLOW_EVENT_DISTORTION:
	      rate = (gfloat*) data;
	      subflow->SR = *rate;
	    break;
	    case MPRTP_SENDER_SUBFLOW_EVENT_DETACHED:
	      subflow->active = FALSE;
	      subflow->linked = FALSE;
	    case MPRTP_SENDER_SUBFLOW_EVENT_LATE:
	      subflow->SR = 0.0;
	      subflow->state = MPRTP_SENDER_SUBFLOW_STATE_PASSIVE;
	    break;
	    case MPRTP_SENDER_SUBFLOW_EVENT_BID:
	      rate = (gfloat*) data;
	      subflow->SR = *rate;
	    break;
	    case MPRTP_SENDER_SUBFLOW_EVENT_CONGESTION:
	      rate = (gfloat*) data;
          subflow->SR = *rate;
	      subflow->state = MPRTP_SENDER_SUBFLOW_STATE_CONGESTED;
	    break;
	    default:
	    break;
	  }
  	  break;
    case MPRTP_SENDER_SUBFLOW_STATE_CONGESTED:
	  switch(event){
		case MPRTP_SENDER_SUBFLOW_EVENT_KEEP:
		  subflow->SR = subflow->receive_rate;
		break;
		case MPRTP_SENDER_SUBFLOW_EVENT_DETACHED:
		  subflow->active = FALSE;
		  subflow->linked = FALSE;
		case MPRTP_SENDER_SUBFLOW_EVENT_LATE:
		  subflow->SR = 0.0;
		  subflow->state = MPRTP_SENDER_SUBFLOW_STATE_PASSIVE;
		break;
		case MPRTP_SENDER_SUBFLOW_EVENT_SETTLED:
		  subflow->SR = subflow->receive_rate;
		  subflow->state = MPRTP_SENDER_SUBFLOW_STATE_NON_CONGESTED;
		break;
		default:
	    break;
	  }
  	  break;
    case MPRTP_SENDER_SUBFLOW_STATE_PASSIVE:
	  switch(event){
	    case MPRTP_SENDER_SUBFLOW_EVENT_DETACHED:
	      subflow->linked = FALSE;
	    case MPRTP_SENDER_SUBFLOW_EVENT_DEAD:
	      subflow->active = FALSE;
	      subflow->SR = 0;
	    break;
	    case MPRTP_SENDER_SUBFLOW_EVENT_SETTLED:
	      subflow->state = MPRTP_SENDER_SUBFLOW_STATE_NON_CONGESTED;
	    break;
	    default:
	    break;
	  }
  	  break;
	default:
	  break;
  }
}


guint16 mprtps_subflow_get_id(MPRTPSSubflow* this)
{
	guint16 result;
	g_mutex_lock(&this->mutex);
	result = this->id;
	g_mutex_unlock(&this->mutex);
	return result;
}

GstClockTime mprtps_subflow_get_sr_riport_time(MPRTPSSubflow* this)
{
	GstClockTime result;
	g_mutex_lock(&this->mutex);
	result = this->sr_riport_time;
	g_mutex_unlock(&this->mutex);
	return result;
}
void mprtps_subflow_set_sr_riport_time(MPRTPSSubflow* this, GstClockTime time)
{
	g_mutex_lock(&this->mutex);
	this->sr_riport_time = time;
	g_mutex_unlock(&this->mutex);
}

gboolean mprtps_subflow_is_active(MPRTPSSubflow* this)
{
	gboolean result;
	g_mutex_lock(&this->mutex);
	result = this->active;
	g_mutex_unlock(&this->mutex);
	return result;
}


GstPad* mprtps_subflow_get_outpad(MPRTPSSubflow* this)
{
	GstPad* result;
	g_mutex_lock(&this->mutex);
	result = this->outpad;
	g_mutex_unlock(&this->mutex);
	return result;
}

/*
void _mprtps_subflow_checker_func(void* data)
{
  MPRTPSSubflow *this = data;
  GstClockTime next_time, now;
  GstClockID clock_id;
  GstPad *outpad;
  GstRTCPHeader *header;
  GstMPRTCPSubflowRiport *riport;
  GstRTCPBuffer rtcp = {NULL, };
  GstBuffer *outbuf;
  GstMPRTCPSubflowBlock *block;
  GstRTCPSR *sr;
g_print("subflow checker\n");
  g_mutex_lock(&this->mutex);
  outpad = this->rtcp_outpad != NULL ? this->rtcp_outpad : this->outpad;
  if(!gst_pad_is_linked(outpad)){
	GST_WARNING_OBJECT(this, "The outpad is not linked");
    goto mprtps_subflow_checker_func_end;
  }
  if(!this->active){
	GST_LOG_OBJECT(this, "The subflow (%d) is not active", this->id);
	goto mprtps_subflow_checker_func_end;
  }
  now = gst_clock_get_time(this->sysclock);
  outbuf = gst_rtcp_buffer_new(1400);
  gst_rtcp_buffer_map (outbuf, GST_MAP_READWRITE, &rtcp);

  header = gst_rtcp_add_begin(&rtcp);
  gst_rtcp_header_change(header, NULL, NULL, NULL, NULL, NULL, &this->ssrc);
  riport = gst_mprtcp_add_riport(header);

  block = gst_mprtcp_riport_add_block_begin(riport, this->id);
  sr = gst_mprtcp_riport_block_add_sr(block);
  gst_rtcp_header_change(&sr->header, NULL, NULL, NULL, NULL, NULL, &this->ssrc);

  gst_rtcp_srb_setup(&sr->sender_block,
		    now, //ntptime
		    (guint32)(gst_rtcp_ntp_to_unix (now)>>32), //rtptime
            this->packet_count, //packet count
			this->octet_count //octet count
			);

  gst_mprtcp_riport_add_block_end(riport, block);

  gst_rtcp_add_end(&rtcp, header);
  gst_print_rtcp_buffer(&rtcp);
  gst_rtcp_buffer_unmap (&rtcp);
  //goto mprtps_subflow_checker_func_end;
  if(gst_pad_push(outpad, outbuf) != GST_FLOW_OK){
	GST_WARNING_OBJECT(this, "The outpad does not work");
  }

mprtps_subflow_checker_func_end:
  g_mutex_unlock(&this->mutex);

  next_time = now + 5 * GST_SECOND;
  clock_id = gst_clock_new_single_shot_id (this->sysclock, next_time);

  if(gst_clock_id_wait (clock_id, NULL) == GST_CLOCK_UNSCHEDULED){
    GST_WARNING_OBJECT(this, "The scheduler clock wait is interrupted");
  }
  gst_clock_id_unref (clock_id);
}

*/


void _print_rtp_packet_info(GstRTPBuffer *rtp)
{
	gboolean extended;
	g_print(
   "0               1               2               3          \n"
   "0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 \n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%3d|%1d|%1d|%7d|%1d|%13d|%31d|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63u|\n"
   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
   "|%63u|\n",
			gst_rtp_buffer_get_version(rtp),
			gst_rtp_buffer_get_padding(rtp),
			extended = gst_rtp_buffer_get_extension(rtp),
			gst_rtp_buffer_get_csrc_count(rtp),
			gst_rtp_buffer_get_marker(rtp),
			gst_rtp_buffer_get_payload_type(rtp),
			gst_rtp_buffer_get_seq(rtp),
			gst_rtp_buffer_get_timestamp(rtp),
			gst_rtp_buffer_get_ssrc(rtp)
			);

	if(extended){
		guint16 bits;
		guint8 *pdata;
		guint wordlen;
		gulong index = 0;

		gst_rtp_buffer_get_extension_data (rtp, &bits, (gpointer) & pdata, &wordlen);


		g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|0x%-29X|%31d|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
	   bits,
	   wordlen);

	   for(index = 0; index < wordlen; ++index){
		 g_print("|0x%-5X = %5d|0x%-5X = %5d|0x%-5X = %5d|0x%-5X = %5d|\n"
				 "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
				 *(pdata+index*4), *(pdata+index*4),
				 *(pdata+index*4+1),*(pdata+index*4+1),
				 *(pdata+index*4+2),*(pdata+index*4+2),
				 *(pdata+index*4+3),*(pdata+index*4+3));
	  }
	  g_print("+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n");
	}
}
