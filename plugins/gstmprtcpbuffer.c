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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstmprtcpreceiver
 *
 * The mprtcpreceiver element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! mprtcpreceiver ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include "gstmprtcpbuffer.h"

#define RTCPHEADER_BYTES 8
#define RTCPHEADER_WORDS (RTCPHEADER_BYTES>>2)
#define RTCPSRBLOCK_BYTES 20
#define RTCPSRBLOCK_WORDS (RTCPSRBLOCK_BYTES>>2)
#define RTCPRRBLOCK_BYTES 28
#define RTCPRRBLOCK_WORDS (RTCPRRBLOCK_BYTES>>2)
#define MPRTCPBLOCK_BYTES 4
#define MPRTCPBLOCK_WORDS (MPRTCPBLOCK_BYTES>>2)


void gst_rtcp_header_init(GstRTCPHeader *header)
{
  header->version = GST_RTCP_VERSION;
  gst_rtcp_header_setup(header, FALSE, 0, 0, 0, 0);
}

void gst_rtcp_header_setup(GstRTCPHeader *header, gboolean padding,
		guint8 reserved, guint8 payload_type, guint16 length, guint32 ssrc)
{
  header->padding = padding;
  header->reserved = reserved;
  header->payload_type = payload_type;
  header->length = g_htons(length);
  header->ssrc = g_htonl(ssrc);
}

void gst_rtcp_header_change(GstRTCPHeader *header, guint8 *version,
		gboolean *padding, guint8 *reserved, guint8 *payload_type,
		guint16 *length, guint32 *ssrc)
{
  if(version){
    header->version = *version;
  }
  if(padding){
	  header->padding = *padding;
  }
  if(reserved){
	  header->reserved = *reserved;
  }
  if(payload_type){
	  header->payload_type = *payload_type;
  }
  if(length){
	  header->length = g_htons(*length);
    }
  if(ssrc){
	  header->ssrc = g_htonl(*ssrc);
    }
}

void gst_rtcp_header_getdown(GstRTCPHeader *header, guint8 *version,
		gboolean *padding, guint8 *reserved, guint8 *payload_type,
		guint16 *length, guint32 *ssrc)
{
  if(version){
    *version = header->version;
  }
  if(padding){
    *padding = header->padding ? TRUE : FALSE;
  }
  if(reserved){
      *reserved = header->reserved;
  }
  if(payload_type){
      *payload_type = header->payload_type;
  }
  if(length){
        *length = g_ntohs(header->length);
    }
  if(ssrc){
        *ssrc = g_ntohl(header->ssrc);
    }
}

GstRTCPHeader* gst_rtcp_add_begin(GstRTCPBuffer *rtcp)
{
	GstRTCPHeader *header;
	guint offset;
	guint8 payload_type;
	guint16 length = 0;
	g_return_val_if_fail (rtcp != NULL, NULL);
	g_return_val_if_fail (GST_IS_BUFFER (rtcp->buffer), NULL);
	g_return_val_if_fail (rtcp->map.flags & GST_MAP_READWRITE, NULL);
	header = (GstRTCPHeader*)rtcp->map.data;
	/*It would be cool if the map.size have matched with the length of the riports
	for(offset = 0;	offset < rtcp->map.size;
		offset += (gst_rtcp_header_get_length(header) + 1) << 2)
	{
	  header = (GstRTCPHeader*)rtcp->map.data + offset;
	}
	*/
	for(offset = 0;	offset < rtcp->map.size;
			offset += (length + 1) << 2)
	{
	  header = (GstRTCPHeader*) (rtcp->map.data + offset);
	  gst_rtcp_header_getdown(header, NULL, NULL, NULL,
			  &payload_type, &length, NULL);

	  if(payload_type != MPRTCP_PACKET_TYPE_IDENTIFIER &&
			  payload_type != GST_RTCP_TYPE_SR &&
			  payload_type != GST_RTCP_TYPE_RR &&
			  payload_type != GST_RTCP_TYPE_XR)
	  {
		  break;
	  }
	}

	//header = rtcp->map.data + 28;
	//g_print("MÁR NEM AZ %d\n", offset);

    return header;
}

void gst_rtcp_add_end(GstRTCPBuffer *rtcp, GstRTCPHeader* header)
{
	guint16 length;
	g_return_if_fail (rtcp != NULL);
	g_return_if_fail (header != NULL);
	g_return_if_fail (GST_IS_BUFFER (rtcp->buffer));
	g_return_if_fail (rtcp->map.flags & GST_MAP_READWRITE);

	gst_rtcp_header_getdown(header, NULL, NULL, NULL, NULL, &length, NULL);
	length = (length + 1)<<2;
	g_return_if_fail(rtcp->map.size + length <= rtcp->map.maxsize);
	rtcp->map.size += length;
}

GstMPRTCPSubflowRiport* gst_mprtcp_add_riport(GstRTCPHeader* header)
{
  GstMPRTCPSubflowRiport* result = (GstMPRTCPSubflowRiport*) header;
  gst_mprtcp_riport_init(result);
  return result;
}

GstMPRTCPSubflowBlock *
gst_mprtcp_riport_add_block_begin(GstMPRTCPSubflowRiport* riport,
		guint16 subflow_id)
{
  guint8 i,src, *ptr;
  GstMPRTCPSubflowBlock *result = &riport->blocks;

  gst_rtcp_header_getdown(&riport->header, NULL, NULL, &src, NULL, NULL, NULL);
  for(ptr = (guint8*)result, i=0; i < src; ++i){
    guint8 block_length;
    gst_mprtcp_block_getdown(&result->info, NULL, &block_length, NULL);
    ptr += (block_length + 1)<<2;
  }
  result = ptr;
  src++;
  gst_mprtcp_block_setup(&result->info, MPRTCP_BLOCK_TYPE_RIPORT, 0, subflow_id);
  gst_rtcp_header_change(&riport->header, NULL, NULL, &src, NULL, NULL, NULL);
  return result;
}

GstRTCPSR*
gst_mprtcp_riport_block_add_sr(GstMPRTCPSubflowBlock * block)
{
	GstRTCPSR *result = &block->sender_riport;
	gst_rtcp_sr_init(result);
	return result;
}


void
gst_mprtcp_riport_add_block_end(GstMPRTCPSubflowRiport* riport,
		GstMPRTCPSubflowBlock *block)
{
  guint16 riport_length, block_header_length;
  guint8 block_length;
  GstMPRTCPSubflowInfo *info = &block->info;
  GstRTCPHeader *riport_header = &riport->header;
  GstRTCPHeader *block_header = &block->block_header;

  gst_rtcp_header_getdown(block_header, NULL,
  		 NULL, NULL, NULL, &block_header_length, NULL);

  block_length = (guint8) block_header_length + 1;

  gst_mprtcp_block_change(info, NULL, &block_length, NULL);


  gst_rtcp_header_getdown(riport_header, NULL,
  		 NULL, NULL, NULL, &riport_length, NULL);

  riport_length += (guint16)block_length + 1;

  gst_rtcp_header_change(riport_header, NULL, NULL, NULL,
		  NULL, &riport_length, NULL);

}
//---------------------------Iterator---------------------------

GstRTCPHeader* gst_rtcp_get_first_header(GstRTCPBuffer *rtcp)
{
	g_return_val_if_fail (rtcp != NULL, NULL);
	g_return_val_if_fail (GST_IS_BUFFER (rtcp->buffer), NULL);
	g_return_val_if_fail (rtcp->map.flags & GST_MAP_READ, NULL);
    if(rtcp->map.size == 0){
      return NULL;
    }
    return (GstRTCPHeader*) rtcp->map.data;
}

GstRTCPHeader* gst_rtcp_get_next_header(GstRTCPBuffer *rtcp, GstRTCPHeader* actual)
{
	guint8 *ref;
	guint8* start;
	guint offset = 0;
	g_return_val_if_fail (rtcp != NULL, NULL);
	g_return_val_if_fail (GST_IS_BUFFER (rtcp->buffer), NULL);
	g_return_val_if_fail (rtcp->map.flags & GST_MAP_READ, NULL);

	start = rtcp->map.data;
	ref = (guint8*) actual;
	offset = (ref - start) + ((g_ntohs(actual->length) + 1)<<2);
	if(rtcp->map.size < offset){
      return NULL;
	}
    return (GstRTCPHeader*) (rtcp->map.data + offset);
}

GstMPRTCPSubflowBlock *gst_mprtcp_get_first_block(GstMPRTCPSubflowRiport *riport)
{
  return &riport->blocks;
}

GstMPRTCPSubflowBlock *gst_mprtcp_get_next_block(GstMPRTCPSubflowRiport *riport,
	GstMPRTCPSubflowBlock *actual)
{
  guint8 *next = &actual;
  guint8 *max_ptr = &riport->header;
  guint16 riport_length;
  guint8 block_length;
  gst_rtcp_header_getdown(&riport->header, NULL, NULL, NULL, NULL,
	&riport_length, NULL);
  gst_mprtcp_block_getdown(&actual->info, NULL, &block_length, NULL);
  max_ptr += riport_length<<2;
  next+= block_length<<2;
  return next < max_ptr ? (GstMPRTCPSubflowBlock *) next : NULL;
}

/*-------------------- Sender Riport ----------------------*/


void gst_rtcp_sr_init(GstRTCPSR *riport)
{
	gst_rtcp_header_init(&riport->header);

	gst_rtcp_header_setup(&riport->header, FALSE, 0,
		GST_RTCP_TYPE_SR, RTCPHEADER_WORDS + RTCPSRBLOCK_WORDS - 1, 0);
	gst_rtcp_srb_setup(&riport->sender_block, 0, 0, 0, 0);
}


void gst_rtcp_srb_setup(GstRTCPSRBlock *block,
		guint64 ntp, guint32 rtp, guint32 packet_count, guint32 octet_count)
{
	GST_WRITE_UINT64_BE(&block->ntptime, ntp);
	block->rtptime = g_htonl(rtp);
	block->packet_count = g_htonl(packet_count);
	block->octet_count = g_htonl(octet_count);
}

void gst_rtcp_srb_getdown(GstRTCPSRBlock *block,
		guint64 *ntp, guint32 *rtp, guint32 *packet_count, guint32 *octet_count)
{
  if(ntp != NULL){
    *ntp = GST_READ_UINT64_BE(&block->ntptime);
  }
  if(rtp != NULL){
    *rtp = g_ntohl(block->rtptime);
  }
  if(packet_count != NULL){
    *packet_count = g_ntohl(block->packet_count);
  }
  if(octet_count != NULL){
    *octet_count = g_ntohl(block->octet_count);
  }
}

//--------- RTCP Receiver Riport -----------------

void gst_rtcp_rr_init(GstRTCPRR *riport)
{
	gst_rtcp_header_init(&riport->header);
	gst_rtcp_header_setup(&riport->header, FALSE, 0,
		GST_RTCP_TYPE_RR, RTCPHEADER_WORDS - 1, 0);
}


void gst_rtcp_rrb_setup(
		GstRTCPRRBlock *block, guint32 ssrc,
		guint8 fraction_lost, guint32 cum_packet_lost,
		guint32 ext_hsn, guint32 jitter, guint32 LSR, guint32 DLSR
		)
{
	block->ssrc = g_htonl(ssrc);
	block->fraction_lost = fraction_lost;
	block->cum_packet_lost = g_htonl(cum_packet_lost);
	block->ext_hsn = g_htonl(ext_hsn);
	block->jitter = g_htonl(jitter);
	block->LSR = g_htonl(LSR);
	block->DLSR = g_htonl(DLSR);
}

void gst_rtcp_rrb_getdown(
		GstRTCPRRBlock *block, guint32* ssrc,
		guint8* fraction_lost, guint32* cum_packet_lost,
		guint32* ext_hsn, guint32* jitter, guint32* LSR, guint32* DLSR
		)
{
	if(ssrc){
	    *ssrc = g_ntohl(block->ssrc);
	  }
	if(fraction_lost){
	    *fraction_lost = block->fraction_lost;
	  }
	if(cum_packet_lost){
	    *cum_packet_lost = g_ntohl(block->cum_packet_lost);
	  }
	if(ext_hsn){
	    *ext_hsn = g_ntohl(block->ext_hsn);
	  }
	if(jitter){
	    *jitter = g_ntohl(block->jitter);
	  }
	if(LSR){
	    *LSR = g_ntohl(block->LSR);
	  }
	if(DLSR){
	    *DLSR = g_ntohl(block->DLSR);
	  }
}


//------------------ MPRTCP ------------------------


void gst_mprtcp_riport_init(GstMPRTCPSubflowRiport *riport)
{
	gst_rtcp_header_init(&riport->header);
	gst_rtcp_header_setup(&riport->header, FALSE, 0,
		MPRTCP_PACKET_TYPE_IDENTIFIER, RTCPHEADER_WORDS /*ssrc*/, 0);
}

void gst_mprtcp_riport_setup(GstMPRTCPSubflowRiport *riport, guint32 ssrc)
{
	riport->ssrc = g_htonl(ssrc);
}

void gst_mprtcp_riport_getdown(GstMPRTCPSubflowRiport *riport, guint32 *ssrc)
{
  if(ssrc){
	*ssrc = g_ntohl(riport->ssrc);
  }
}


void gst_mprtcp_block_init(GstMPRTCPSubflowBlock *block)
{
  gst_mprtcp_block_setup(&block->info, 0, 0, 0);

}

void gst_mprtcp_block_setup(GstMPRTCPSubflowInfo *info,
		guint8 type, guint8 block_length, guint16 subflow_id)
{
  info->block_length = block_length;
  info->subflow_id = g_htons(subflow_id);
  info->type = type;
}

void gst_mprtcp_block_change(GstMPRTCPSubflowInfo *info,
		guint8 *type, guint8 *block_length, guint16 *subflow_id)
{
	if(type){
		info->type = *type;
	  }
	if(subflow_id){
		info->subflow_id = g_ntohs(*subflow_id);
	  }
	if(block_length){
		info->block_length = *block_length;
	  }
}

void gst_mprtcp_block_getdown(GstMPRTCPSubflowInfo *info,
		guint8 *type, guint8 *block_length, guint16 *subflow_id)
{
	if(type){
	    *type = info->type;
	  }
	if(block_length){
	    *block_length = info->block_length;
	  }
	if(subflow_id){
	    *subflow_id = g_ntohs(info->subflow_id);
	  }
}

void gst_print_rtcp_check_sr(GstRTCPBuffer *rtcp, gint offset)
{
	GstRTCPHeader *header;
	GstRTCPSR *riport;
	GstRTCPRRBlock *block;
	GstRTCPPacket packet;
	gint index;
	guint32 ssrc, header_ssrc;
    guint8 version, payload_type, reserved;
    guint16 length;
    gboolean padding;

	g_return_if_fail (rtcp != NULL);
	g_return_if_fail (GST_IS_BUFFER (rtcp->buffer));
	g_return_if_fail (rtcp != NULL);
	g_return_if_fail (rtcp->map.flags & GST_MAP_READ);


	gst_rtcp_buffer_get_first_packet(rtcp, &packet);
	gst_rtcp_packet_sr_get_sender_info(&packet, &ssrc, NULL, NULL, NULL, NULL);
	header = (GstRTCPHeader *)(rtcp->map.data + offset);
	riport = (GstRTCPSR *)header;

	gst_rtcp_header_getdown(header, &version, &padding, &reserved, &payload_type,
			&length, &header_ssrc);
	g_print(
	   "0               1               2               3          \n"
	   "0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%3d|%1d|%9d|%15d|%14d?=?%14d|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29u ?=? %29u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
	   version,
	   padding,
	   reserved,
	   payload_type,
	   length,
       gst_rtcp_packet_get_length(&packet),
       header_ssrc,
	   ssrc
	);

	gst_print_rtcp_check_srb(&riport->sender_block, &packet);

    for(block = &riport->receiver_blocks, index = 0;
    	index < reserved;
    	++index, ++block){
    	gst_print_rtcp_check_rrb(block, index, &packet);
    }
}


void gst_print_rtcp_check_rrb(GstRTCPRRBlock *block, gint index, GstRTCPPacket *packet)
{
	guint32 ssrc, exthighestseq, jitter, lsr, dlsr;
		guint32 packetslost;
		guint8 fraction_lost;

	guint32 r_ssrc, r_exthighestseq, r_jitter, r_lsr, r_dlsr;
		gint32 r_packetslost;
		guint8 r_fraction_lost;
	gst_rtcp_packet_get_rb(packet, index, &r_ssrc, &r_fraction_lost, &r_packetslost,
            &r_exthighestseq, &r_jitter,
            &r_lsr, &r_dlsr);

	gst_rtcp_rrb_getdown(block, &ssrc, &fraction_lost, &packetslost,
			&exthighestseq, &jitter, &lsr, &dlsr);
	g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|%29u ?=? %29u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%7d?=?%6d|%21d ?=? %20d|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29u ?=? %29u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29u ?=? %29u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29u ?=? %29u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29u ?=? %29u|\n"
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n",
	   ssrc,
	   r_ssrc,
	   fraction_lost,
	   r_fraction_lost,
	   packetslost,
	   r_packetslost,
	   exthighestseq,
	   r_exthighestseq,
	   jitter,
	   r_jitter,
	   lsr,
	   r_lsr,
	   dlsr,
	   r_dlsr
	);
}

void gst_print_rtcp_check_srb(GstRTCPSRBlock *block, GstRTCPPacket *packet)
{
	guint32 r_ssrc, r_rtptime, r_packet_count, r_octet_count;
	guint64 r_ntptime;
	guint32 ssrc, rtptime, packet_count, octet_count;
	guint64 ntptime;

	gst_rtcp_packet_sr_get_sender_info(packet, &r_ssrc, &r_ntptime,
			&r_rtptime, &r_packet_count, &r_octet_count);
	gst_rtcp_srb_getdown(block, &ntptime, &rtptime, &packet_count, &octet_count);
	g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|%29llX ?=? %29llX|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29X ?=? %29X|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29X ?=? %29X|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%29X ?=? %29X|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n",
	   ntptime,
	   r_ntptime,
	   rtptime,
	   r_rtptime,
	   packet_count,
	   r_packet_count,
	   octet_count,
	   r_octet_count
	);
}




//------------------------------------

void gst_print_rtcp_buffer(GstRTCPBuffer *buffer)
{
  g_print("0               1               2               3          \n"
	   "0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2\n");
  gst_print_rtcp((GstRTCPHeader*)buffer->map.data);
  g_print("+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n");
}

void gst_print_rtcp(GstRTCPHeader *header)
{
  gboolean ok = TRUE;
  guint16 offset = 0;
  guint8 payload_type;
  guint16 length;

  for(; ok; header=(guint8*) header + offset){
	gst_rtcp_header_getdown(header, NULL, NULL, NULL, &payload_type, &length, NULL);
	switch(payload_type){
	  case MPRTCP_PACKET_TYPE_IDENTIFIER:
	    gst_print_mprtcp(header);
		break;
	  case GST_RTCP_TYPE_SR:
		gst_print_rtcp_sr((GstRTCPSR*)header);
		break;
	  case GST_RTCP_TYPE_RR:
		gst_print_rtcp_rr((GstRTCPRR*)header);
		break;
	  default:
		  ok = FALSE;
		  break;
	}
	offset = length + 1;
	offset<<=2;
  }
}


void gst_print_mprtcp(GstMPRTCPSubflowRiport *riport)
{
  gint index;
  GstMPRTCPSubflowBlock *block = &riport->blocks;
  GstMPRTCPSubflowInfo *info;
  GstRTCPHeader *riport_header = &riport->header;
  guint32 ssrc;
  guint8 src;
  gst_mprtcp_riport_getdown(riport, &ssrc);

  gst_print_rtcp_header(riport_header);
  gst_rtcp_header_getdown(riport_header, NULL, NULL,
		&src, NULL, NULL, NULL);

  g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|%63u|\n",
	   ssrc
	   );

  for(index = 0; index < src; ++index, ++block){
	  guint8 type, block_length;
	  guint16 subflow_id;
	  info = &block->info;
	  gst_mprtcp_block_getdown(info, &type, &block_length, &subflow_id);

	  g_print(
	  	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	  	   "|%15u|%15u|%31d|\n",
		   type,
		   block_length,
		   subflow_id
	  );

	  gst_print_rtcp(&block->block_header);
  }
}


void gst_print_rtcp_header(GstRTCPHeader *header)
{
	guint32 ssrc;
	guint8 version, payload_type, reserved;
	guint16 length;
	gboolean padding;

	gst_rtcp_header_getdown(header, &version, &padding,
		&reserved, &payload_type, &length, &ssrc);

	g_print(
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%3d|%1d|%9d|%15d|%31d|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%63u|\n",
	   version,
	   padding,
	   reserved,
	   payload_type,
	   length,
	   ssrc
	);
}
/*
void gst_print_mprtcp(GstMPRTCPSubflowRiport *riport_ptr)
{
  gint index;
  GstMPRTCPSubflowBlock *block;
  GstMPRTCPSubflowInfo *info;
  guint8 rc = gst_mprtcp_riport_get_rc(riport_ptr);
  gst_print_rtcp_header(&riport_ptr->header);
  g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|%63u|\n",
	   gst_mprtcp_riport_get_ssrc(riport_ptr)
	   );

  for(index = 0; index < rc; ++index){
	  block = &riport_ptr->blocks[index];
	  info = &block->subflow_info;
	  g_print(
	  	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	  	   "|%15u|%15u|%31d|\n",
	  	   gst_mprtcp_block_get_type(info),
		   gst_mprtcp_block_get_length(info),
		   gst_mprtcp_block_get_subflow_id(info)
	  );
	  gst_print_rtcp(&block->block_header);
  }
}
*/
void gst_print_rtcp_sr(GstRTCPSR *riport)
{
	gint index;
	guint8 rc;
	GstRTCPRRBlock *block = &riport->receiver_blocks;
    gst_rtcp_header_getdown(&riport->header, NULL, NULL, &rc, NULL, NULL, NULL);

	gst_print_rtcp_header(&riport->header);

	gst_print_rtcp_srb(&riport->sender_block);

    for(index = 0; index < rc; ++index, ++block){
    	gst_print_rtcp_rrb(block);
    }
}

void gst_print_rtcp_rr(GstRTCPRR *riport)
{
	gint index;
	guint8 rc;
	GstRTCPRRBlock *block = &riport->blocks;
	gst_rtcp_header_getdown(&riport->header, NULL, NULL, &rc, NULL, NULL, NULL);

	gst_print_rtcp_header(&riport->header);

	for(index = 0; index < rc; ++index, ++block){
		gst_print_rtcp_rrb(block);
	}
}

void gst_print_rtcp_rrb(GstRTCPRRBlock *block)
{
	guint32 ssrc, exthighestseq, jitter, lsr, dlsr;
			guint32 packetslost;
			guint8 fraction_lost;


	gst_rtcp_rrb_getdown(block, &ssrc, &fraction_lost, &packetslost,
				&exthighestseq, &jitter, &lsr, &dlsr);

	g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|%63u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%15d|%47d|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
			"|%63u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
			"|%63u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
			"|%63u|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
			"|%63u|\n",
	   ssrc,
	   fraction_lost,
	   packetslost,
	   exthighestseq,
	   jitter,
	   lsr,
	   dlsr
	);
}

void gst_print_rtcp_srb(GstRTCPSRBlock *block)
{
	guint32 ssrc, rtptime, packet_count, octet_count;
	guint64 ntptime;

	gst_rtcp_srb_getdown(block, &ntptime, &rtptime, &packet_count, &octet_count);

	g_print(
	   "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+\n"
	   "|%63llX|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%63X|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%63X|\n"
	   "+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n"
	   "|%63X|\n",
	   ntptime,
	   rtptime,
	   packet_count,
	   octet_count
	);
}



