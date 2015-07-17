#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include "mprtprsubflow.h"
#include "gstmprtcpbuffer.h"

GST_DEBUG_CATEGORY_STATIC (gst_mprtprsubflow_debug_category);
#define GST_CAT_DEFAULT gst_mprtprsubflow_debug_category

G_DEFINE_TYPE (MPRTPRSubflow, mprtpr_subflow, G_TYPE_OBJECT);

typedef struct _MPRTPRSubflowHeaderExtension{
  guint16 id;
  guint16 sequence;
}MPRTPRSubflowHeaderExtension;

typedef struct _Gap{
  GList   *at;
  guint16 start;
  guint16 end;
  guint16 total;
  guint16 filled;
}Gap;

static void mprtpr_subflow_process_rtpbuffer(MPRTPRSubflow* this, GstRTPBuffer* rtp, guint16 subflow_sequence);
static void mprtpr_subflow_finalize (GObject * object);
static void mprtpr_subflow_init (MPRTPRSubflow * subflow);
static void mprtpr_subflow_reset (MPRTPRSubflow * this);
static void mprtpr_subflow_proc_mprtcpblock(MPRTPRSubflow* this, GstMPRTCPSubflowBlock *block);
static gboolean mprtpr_subflow_is_active(MPRTPRSubflow* this);
static guint32 mprtpr_subflow_get_skew(MPRTPRSubflow* this);
static void _proc_rtcp_sr(MPRTPRSubflow* this, GstRTCPSR *sr);


static guint16
_mprtp_buffer_get_sequence_num(GstRTPBuffer* rtp,
		guint8 MPRTP_EXT_HEADER_ID);
static guint16 _mprtp_buffer_get_subflow_id(GstRTPBuffer* rtp,
		guint8 MPRTP_EXT_HEADER_ID);
static gboolean
_found_in_gaps(GList *gaps, guint16 actual_subflow_sequence,
		guint8 ext_header_id, GList **result_item, Gap **result_gap);
static Gap*
_make_gap(GList *at, guint16 start, guint16 end);
static gint
_cmp_seq(guint16 x, guint16 y);
static GList*
_merge_lists(GList *F, GList *L);



void
mprtpr_subflow_class_init (MPRTPRSubflowClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = mprtpr_subflow_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_mprtprsubflow_debug_category, "mprtpr_subflow", 0, "MPRTP Receiver Subflow");
}


MPRTPRSubflow*
make_mprtpr_subflow(guint16 id, GstPad* sinkpad, guint8 header_ext_id)
{
  MPRTPRSubflow *result;

  result = g_object_new (MPRTPR_SUBFLOW_TYPE, NULL);
  result->id = id;
  result->ext_header_id = header_ext_id;
  return result;
}

void
mprtpr_subflow_reset (MPRTPRSubflow * this)
{
  this->cycle_num = 0;
  this->sysclock = gst_system_clock_obtain();
  this->skews_write_index = 0;
  this->skews_read_index = 0;
}

void
mprtpr_subflow_init (MPRTPRSubflow * this)
{
  this->process_mprtp_packets = mprtpr_subflow_process_rtpbuffer;
  this->proc_mprtcpblock = mprtpr_subflow_proc_mprtcpblock;
  this->is_active = mprtpr_subflow_is_active;
  this->get_skew = mprtpr_subflow_get_skew;
  this->active = TRUE;
  g_mutex_init(&this->mutex);
  mprtpr_subflow_reset (this);

}


void
mprtpr_subflow_finalize (GObject * object)
{
  MPRTPRSubflow *subflow;
  subflow = MPRTPR_SUBFLOW_CAST (object);
  g_object_unref (subflow->sysclock);

}

void mprtpr_subflow_proc_mprtcpblock(MPRTPRSubflow* this,
  GstMPRTCPSubflowBlock *block)
{
  GstRTCPHeader *header = &block->block_header;
  guint8 type;

  g_mutex_lock(&this->mutex);

  gst_rtcp_header_getdown(header, NULL, NULL, NULL, &type, NULL, NULL);

  if(type == GST_RTCP_TYPE_SR){
    _proc_rtcp_sr(this, &block->sender_riport);
  }

  g_mutex_unlock(&this->mutex);
}

GList *
mprtpr_subflow_get_result(MPRTPRSubflow* this)
{
  GList *result;
  g_mutex_lock(&this->mutex);
  result = this->result;
  this->result = NULL;
  g_mutex_unlock(&this->mutex);
  return result;
}


gboolean
mprtpr_subflow_is_active(MPRTPRSubflow* this)
{
  gboolean result;
  g_mutex_lock(&this->mutex);
  result = this->active;
  g_mutex_unlock(&this->mutex);
  return result;
}


static gint _cmp_guint32(gconstpointer a, gconstpointer b, gpointer user_data)
{
  const guint32 *_a = a, *_b = b;
  if(*_a == *_b){
    return 0;
  }
  return *_a < *_b ? -1 : 1;
}

guint32
mprtpr_subflow_get_skew(MPRTPRSubflow* this)
{
  guint32 skews[100];
  gint c;
  GstClockTime treshold;
  guint8 i;

  g_mutex_lock(&this->mutex);
  treshold = gst_clock_get_time(this->sysclock) - 2 * GST_SECOND;
  g_print("\n");
  for(c = 0; this->skews_read_index != this->skews_write_index; )
  {
	i = this->skews_read_index;
	if(++this->skews_read_index == 100){
	  this->skews_read_index=0;
	}
	g_print("%lu-", this->skews[i]);
	if(this->reception_times[i] < treshold){
      continue;
    }
    skews[c++] = this->skews[i];
  }
  this->skews_read_index = this->skews_write_index = 0;
  g_qsort_with_data(skews, c, sizeof(guint32), _cmp_guint32, NULL);
  g_mutex_unlock(&this->mutex);
  g_print(":::::%lu\n", skews[c>>1]);
  return skews[c>>1];
}

void
mprtpr_subflow_process_rtpbuffer(MPRTPRSubflow* this, GstRTPBuffer* rtp, guint16 subflow_sequence)
{
  GList  *it;
  Gap    *gap;
  guint64 reception_time;
  guint32 rtptime;

  g_mutex_lock(&this->mutex);
  //printf("Packet is received by (%d-%d) path receiver with %d absolute sequence and %d subflow sequence\n",this->id, actual->subflow_id, actual->absolute_sequence, actual->subflow_sequence);
  if(this->seq_initialized == FALSE){
    this->actual_seq = subflow_sequence;
    this->HSN = 0;
    this->received = 1;
    this->seq_initialized = TRUE;
    this->result = g_list_prepend(this->result, rtp);
    this->last_RTP_timestamp = gst_rtp_buffer_get_timestamp(rtp);
    this->last_reception_time = gst_clock_get_time(this->sysclock);
    goto mprtpr_subflow_process_rtpbuffer_end;
  }

  //calculate the skew

  reception_time = gst_clock_get_time(this->sysclock);
  rtptime = gst_rtp_buffer_get_timestamp(rtp);

  this->skews[this->skews_write_index] =
    (guint32)(reception_time - this->last_reception_time) -
	(rtptime - this->last_RTP_timestamp);

  this->reception_times[this->skews_write_index] =
	reception_time;

  if(++this->skews_write_index == 100){
    this->skews_write_index = 0;
  }

  if(this->skews_write_index == this->skews_read_index){
    if(++this->skews_read_index == 100){
    	this->skews_read_index = 0;
    }
  }
  this->last_reception_time = reception_time;
  this->last_RTP_timestamp = rtptime;
  goto mprtpr_subflow_process_rtpbuffer_end;

  //calculate lost, discarded and received packets
  ++this->received;
  if(subflow_sequence == 0 && this->received_since_cycle_is_increased > 0x8000){
    this->received_since_cycle_is_increased = 0;
    ++this->cycle_num;
  }
  if(_cmp_seq(this->HSN, subflow_sequence) > 0){
    ++this->late_discarded;
    goto mprtpr_subflow_process_rtpbuffer_end;
  }
  if(subflow_sequence == (guint16)(this->actual_seq + 1)){
    ++this->received_since_cycle_is_increased;
    this->result = g_list_prepend(this->result, rtp);
    ++this->actual_seq;
    goto mprtpr_subflow_process_rtpbuffer_end;
  }
  if(_cmp_seq(this->actual_seq, subflow_sequence) < 0){//GAP
    this->result = g_list_prepend(this->result, rtp);
    gap = _make_gap(this->result, this->actual_seq, _mprtp_buffer_get_sequence_num(rtp, this->ext_header_id));
    this->gaps = g_list_append(this->gaps, gap);
    this->actual_seq = subflow_sequence;
    goto mprtpr_subflow_process_rtpbuffer_end;
  }
  if(_cmp_seq(this->actual_seq, subflow_sequence) > 0 &&
     _found_in_gaps(this->gaps,
    	_mprtp_buffer_get_sequence_num(rtp, this->ext_header_id),
		this->ext_header_id, &it, &gap) == TRUE)
  {//Discarded
	this->result = g_list_insert_before(this->result, it != NULL ? it : gap->at, rtp);
    //this->result = dlist_pre_insert(this->result, it != NULL ? it : gap->at, rtp);
    ++gap->filled;
    goto mprtpr_subflow_process_rtpbuffer_end;
  }
  ++this->duplicated;

mprtpr_subflow_process_rtpbuffer_end:
  g_mutex_unlock(&this->mutex);
  return;
}



GList* _merge_lists(GList *F, GList *L)
{
  GList *head = NULL,*tail = NULL,*p = NULL,**s;
  GstRTPBuffer *F_rtp,*L_rtp;
  while(F != NULL && L != NULL){
	F_rtp = (GstRTPBuffer*) F->data;
	L_rtp = (GstRTPBuffer*) L->data;
	if(_cmp_seq(gst_rtp_buffer_get_seq(F_rtp), gst_rtp_buffer_get_seq(L_rtp)) < 0){
	  s = &F;
	}else{
	  s = &L;
	}
	//s = (((packet_t*)F->data)->absolute_sequence > ((packet_t*)L->data)->absolute_sequence) ? &F : &L;
    if(head == NULL){
      head = *s;
      p = NULL;
    }else{
      tail->next = *s;
      tail->prev = p;
      p = tail;
    }
    tail = *s;
    *s = (*s)->next;
  }
  if(F != NULL){
	  tail->next = F;
	  F->prev = tail;
  }else if(L != NULL){
	  tail->next = L;
	  L->prev = tail;
  }
  return head;
}

gint _cmp_seq(guint16 x, guint16 y)
{
  if(x == y){
	  return 0;
  }
  if(x < y || (0x8000 < x && y < 0x8000)){
	  return -1;
  }
  return 1;

  //return ((gint16) (x - y)) < 0 ? -1 : 1;
}

Gap* _make_gap(GList *at, guint16 start, guint16 end)
{
  Gap *result = g_new0(Gap, 1);
  guint16 counter;

  result->at = at;
  result->start = start;
  result->end = end;
  //_mprtp_buffer_get_sequence_num(at->data, &result->end);
  //result->active = BOOL_TRUE;
  result->total = 1;
  for(counter = result->start+1;
	  counter != (guint16)(result->end-1);
	  ++counter, ++result->total);
  //printf("Make Gap: start: %d - end: %d, missing: %d\n",result->start, result->end, result->total);
  return result;
}

gboolean _found_in_gaps(GList *gaps,
		              guint16 actual_subflow_sequence,
					  guint8 ext_header_id,
					  GList **result_item,
					  Gap **result_gap)
{

  Gap *gap;
  GList *it;
  GstRTPBuffer *rtp;
  int32_t cmp;
  guint16 rtp_subflow_sequence;
  for(it = gaps; it != NULL; it = it->next){
    gap = (Gap*) it->data;
    /*
    printf("\nGap total: %d; Filled: %d Start seq:%d End seq:%d\n",
    		gap->total, gap->filled, gap->start, gap->end);
    */
    //if(gap->active == BOOL_FALSE){
    if(gap->filled == gap->total){
    	continue;
    }
    if(_cmp_seq(gap->start, actual_subflow_sequence) <= 0 && _cmp_seq(actual_subflow_sequence, gap->end) <= 0){
      break;
    }
  }
  if(it == NULL){
    return FALSE;
  }
  if(result_gap != NULL){
    *result_gap = gap;
  }

  for(it = gap->at; it != NULL; it = it->next){
    rtp = it->data;
    rtp_subflow_sequence = _mprtp_buffer_get_sequence_num(rtp, ext_header_id);
	cmp = _cmp_seq(rtp_subflow_sequence, actual_subflow_sequence);
	//printf("packet_s: %d, actual_s: %d\n",packet->sequence, actual->sequence);
	if(cmp > 0){
	  continue;
	}
	if(cmp == 0){
      return FALSE;
    }
    break;
  }
  if(result_item != NULL){
    *result_item = it;
    //printf("result_item: %d",((packet_t*)it->next->data)->sequence);
  }
  return TRUE;
}


guint16 _mprtp_buffer_get_subflow_id(GstRTPBuffer* rtp, guint8 MPRTP_EXT_HEADER_ID)
{
	gpointer pointer = NULL;
	guint size = 0;
	MPRTPRSubflowHeaderExtension *ext_header;
	if(!gst_rtp_buffer_get_extension_onebyte_header(rtp, MPRTP_EXT_HEADER_ID, 0, &pointer, &size)){
	  GST_WARNING("The requested rtp buffer doesn't contain one byte header extension with id: %d", MPRTP_EXT_HEADER_ID);
	  return FALSE;
	}
	ext_header = (MPRTPRSubflowHeaderExtension*) pointer;
	return ext_header->id;
}

guint16 _mprtp_buffer_get_sequence_num(GstRTPBuffer* rtp, guint8 MPRTP_EXT_HEADER_ID)
{
	gpointer pointer = NULL;
	guint size = 0;
	MPRTPRSubflowHeaderExtension *ext_header;
	if(!gst_rtp_buffer_get_extension_onebyte_header(rtp, MPRTP_EXT_HEADER_ID, 0, &pointer, &size)){
	  GST_WARNING("The requested rtp buffer doesn't contain one byte header extension with id: %d", MPRTP_EXT_HEADER_ID);
	  return FALSE;
	}
	ext_header = (MPRTPRSubflowHeaderExtension*) pointer;
	return ext_header->sequence;
}

void _proc_rtcp_sr(MPRTPRSubflow* this, GstRTCPSR *sr)
{
  GstRTCPSRBlock *srblock = &sr->sender_block;
}
