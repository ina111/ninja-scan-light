/**
 * u-blox GPS support routines.
 *  by fenrir.
 */

#include "c8051f380.h"
#include <string.h>

#include "main.h"
#include "f38x_uart0.h"
#include "data_hub.h"
#include "gps.h"
#include "util.h"

void gps_write(char *buf, int size){
  int written = 0;
  while(TRUE){
    written = uart0_write(buf, size);
    size -= written;
    if(size == 0){break;}
    buf += written;
    //P4 ^= 0x01;
  }
}

static void checksum(unsigned char ck[2], 
                        unsigned char *packet, 
                        int size){
  u8 a, b;
  a = b = 0x00;
  while(size-- > 0){
    a += *(packet++);
    b += a;
  }
  ck[0] = a;
  ck[1] = b;
}

static void gps_packet_write(char *packet, int packet_size){
  unsigned char ck[2];
  checksum(ck, &packet[2], packet_size - 2);
  gps_write(packet, packet_size);
  gps_write(ck, sizeof(ck));
}

#define swap(x) (\
(((x) & 0x80) ? 0x01 : 0x00) | \
(((x) & 0x40) ? 0x02 : 0x00) | \
(((x) & 0x20) ? 0x04 : 0x00) | \
(((x) & 0x10) ? 0x08 : 0x00) | \
(((x) & 0x08) ? 0x10 : 0x00) | \
(((x) & 0x04) ? 0x20 : 0x00) | \
(((x) & 0x02) ? 0x40 : 0x00) | \
(((x) & 0x01) ? 0x80 : 0x00) \
)

#define expand_16(x) \
((x) & 0xFF), \
((x) >> 8) & 0xFF

#define expand_32(x) \
((x) & 0xFF), \
((x) >> 8) & 0xFF, \
((x) >> 16) & 0xFF, \
((x) >> 24) & 0xFF

#define UBX_CFG_RATE_MEAS (u16)200 // 5Hz
#define UBX_CFG_RATE_NAV  (u16)1
#define UBX_CFG_RAET_TIME (u16)0

static void set_ubx_cfg_rate(){
  static const unsigned char packet[6 + 6] = {
    0xB5, // packet[0]
    0x62, // packet[1]
    0x06, // packet[2]
    0x08, // packet[3]
    expand_16(sizeof(packet) - 6),   // packet[4 + 0]
    expand_16(UBX_CFG_RATE_MEAS),     // packet[6 + 0]
    expand_16(UBX_CFG_RATE_NAV),      // packet[6 + 2]
    expand_16(UBX_CFG_RAET_TIME),     // packet[6 + 4]
  };
  gps_packet_write(packet, sizeof(packet));
}

#define UBX_CFG_TP_INTERVAL    (u32)1000000
#define UBX_CFG_TP_LENGTH      (u32)1000
#define UBX_CFG_TP_STATUS      (s8)-1
#define UBX_CFG_TP_TIMEREF     (u8)0
#define UBX_CFG_TP_CABLE_DELAY (u16)50
#define UBX_CFG_TP_RF_DELAY    (u16)820
#define UBX_CFG_TP_USER_DELAY  (u32)0

static void set_ubx_cfg_tp(){
  static const unsigned char packet[20 + 6] = {
    0xB5, // packet[0] 
    0x62, // packet[1] 
    0x06, // packet[2] 
    0x07, // packet[3] 
    expand_16(sizeof(packet) - 6),       // packet[4 + 0]    
    expand_32(UBX_CFG_TP_INTERVAL),       // packet[6 + 0]  
    expand_32(UBX_CFG_TP_LENGTH),         // packet[6 + 4]  
    (UBX_CFG_TP_STATUS & 0xFF),           // packet[6 + 8]  
    (UBX_CFG_TP_TIMEREF & 0xFF),          // packet[6 + 9]  
    0x00,                                 // packet[6 + 10] 
    0x00,                                 // packet[6 + 11] 
    expand_16(UBX_CFG_TP_CABLE_DELAY),    // packet[6 + 12]  
    expand_16(UBX_CFG_TP_RF_DELAY),       // packet[6 + 14]  
    expand_32(UBX_CFG_TP_USER_DELAY),     // packet[6 + 16] 
  };
  gps_packet_write(packet, sizeof(packet));
}

static void set_ubx_cfg_sbas(){
  static const unsigned char packet[8 + 6] = {
    0xB5, // packet[0] 
    0x62, // packet[1] 
    0x06, // packet[2] 
    0x16, // packet[3] 
    expand_16(sizeof(packet) - 6), // packet[4 + 0]
    0, // packet[6 + 0]; mode, SBAS Disabled(bit0 = 0)
    0, // packet[6 + 1]; usage
    0, // packet[6 + 2]; maxsbas
    0, // packet[6 + 3]; reserved
    expand_32(0), // packet[6 + 4], scanmode
  };
  gps_packet_write(packet, sizeof(packet));
}

#define UBX_CFG_PRT_BAUDRATE  (u32)115200

static void set_ubx_cfg_prt(){
  // UBX
  static const unsigned char _packet[20 + 6] = {
    0xB5, // packet[0] 
    0x62, // packet[1] 
    0x06, // packet[2] 
    0x00, // packet[3] 
    expand_16(sizeof(_packet) - 6), // packet[4 + 0]  
    0x00, // packet[6 + 0]   // Port.NO
    0x00, // packet[6 + 1]   // Res0
    0x00, // packet[6 + 2]   // Res1
    0x00, // packet[6 + 3]  
    0xC0, // packet[6 + 4]   // (M)11000000(L)
    0x08, // packet[6 + 5]   // (M)00001000(L)
    0x00, // packet[6 + 6]   // (M)00000000(L)
    0x00, // packet[6 + 7]   //
    expand_32(UBX_CFG_PRT_BAUDRATE), // packet[6 + 8]  
    0x07, // packet[6 + 12]  // in - UBX,NMEA,RAW
    0x00, // packet[6 + 13] 
    0x01, // packet[6 + 14]  // out - UBX
    0x00, // packet[6 + 15] 
    0x00, // packet[6 + 16] 
    0x00, // packet[6 + 17] 
    0x00, // packet[6 + 18] 
    0x00, // packet[6 + 19] 
  };
  unsigned char packet[sizeof(_packet)];
  memcpy(packet, _packet, sizeof(packet));
  
  {
    u8 i;
    for(i = 0; i < 3; i++){
      packet[6 + 0]  = i;
      gps_packet_write(packet, sizeof(packet));
    }
  }
  
  // NMEA
  /*const char code *str1 = "$PUBX,41,0,0007,0003,115200,1*1A\r\n";
  const char code *str2 = "$PUBX,41,1,0007,0003,115200,1*1B\r\n";
  const char code *str3 = "$PUBX,41,2,0007,0003,115200,1*18\r\n";  
  gps_write(str1, strlen(str1));
  gps_write(str2, strlen(str2));  
  gps_write(str3, strlen(str3));*/
}

static void set_ubx_cfg_msg(u8 _class, u8 id, u8 rate1){
  unsigned char packet[6 + 6] = {
    0xB5, // packet[0] 
    0x62, // packet[1] 
    0x06, // packet[2] 
    0x01, // packet[3] 
    expand_16(sizeof(packet) - 6), // packet[4 + 0]  
    _class,   // packet[6 + 0]
    id,       // packet[6 + 1]  
    rate1,    // packet[6 + 2]  //(u8)0;
    rate1,    // packet[6 + 3]  
    rate1,    // packet[6 + 4]  //(u8)0;
    (u8)0,    // packet[6 + 5]  
  };
  gps_packet_write(packet, sizeof(packet));
}

void gps_init(){
  
  // init wait
  wait_ms(100);
  
  // set U-blox configuration
  set_ubx_cfg_prt();              // baudrate change
  while(uart0_tx_size() > 0);
  uart0_bauding(UBX_CFG_PRT_BAUDRATE);
  
  // baudrate change wait
  wait_ms(100);
  
  // clear buffer
  {
    char c;
    while(uart0_read(&c, 1) > 0);
  }
  
  set_ubx_cfg_rate();
  set_ubx_cfg_tp();
  set_ubx_cfg_sbas();
  
  set_ubx_cfg_msg(0x01, 0x02, 1);  // NAV-POSLLH  // 28 + 8 = 36 bytes
  set_ubx_cfg_msg(0x01, 0x03, 5);  // NAV-STATUS  // 16 + 8 = 24 bytes
  set_ubx_cfg_msg(0x01, 0x04, 5);  // NAV-DOP     // 18 + 8 = 26 bytes
  set_ubx_cfg_msg(0x01, 0x06, 1);  // NAV-SOL     // 52 + 8 = 60 bytes
  set_ubx_cfg_msg(0x01, 0x12, 1);  // NAV-VELNED  // 36 + 8 = 44 bytes
  set_ubx_cfg_msg(0x01, 0x20, 20);  // NAV-TIMEGPS  // 16 + 8 = 24 bytes
  set_ubx_cfg_msg(0x01, 0x21, 20);  // NAV-TIMEUTC  // 20 + 8 = 28 bytes
  set_ubx_cfg_msg(0x01, 0x30, 10);  // NAV-SVINFO  // (8 + 12 * x) + 8 = 112 bytes (@8)
  set_ubx_cfg_msg(0x02, 0x10, 1);  // RXM-RAW     // (8 + 24 * x) + 8 = 208 bytes (@8)
  set_ubx_cfg_msg(0x02, 0x11, 10);  // RXM-SFRB    // 42 + 8 = 50 bytes
  
  // NMEA-GGA
  // NMEA-GLL
  // NMEA-GSA
  // NMEA-GSV
  // NMEA-RMC
  // NMEA-VTG
  // NMEA-ZDA
}

static void poll_rxm_eph(u8 svid){
  unsigned char packet[1 + 6] = {
    0xB5, // packet[0] 
    0x62, // packet[1] 
    0x02, // packet[2] 
    0x31, // packet[3] 
    expand_16(sizeof(packet) - 6), // packet[4 + 0]  
    svid, // packet[6 + 0]
  };
  gps_packet_write(packet, sizeof(packet));
}

static void poll_aid_hui(){
  static const unsigned char packet[0 + 6] = {
    0xB5, // packet[0] 
    0x62, // packet[1] 
    0x0B, // packet[2] 
    0x02, // packet[3] 
    expand_16(sizeof(packet) - 6), // packet[4 + 0]  
  };
  gps_packet_write(packet, sizeof(packet));
}

volatile __bit gps_time_modified = FALSE;
__xdata u8 gps_num_of_sat = 0;
__xdata u16 gps_wn = 0;
__xdata s32 gps_ms = 0;

static __xdata s8 leap_seconds = 0;
#if USE_GPS_STD_TIME
time_t gps_std_time(time_t *timer) {
  time_t res = 0;
  if(gps_wn > 0){ // valid
   res = ((time_t)60 * 60 * 24 * 7) * gps_wn
       + (global_ms / 1000) 
       - leap_seconds
       + (time_t)315964800; // Jan 01, 1970, 00:00:00 UTC
  }
  if(timer){*timer = res;}
  return res;
}
#endif

__bit gps_utc_valid = FALSE;
__xdata struct tm gps_utc;

#define UBX_SAT_MAX_ID 32
static void make_packet(packet_t *packet){
  payload_t *dst = packet->current;
  u8 size = packet->buf_end - dst;
  
  //if(size == 0){return;} // guranteed that size > 0
  
  *(dst++) = 'G';
    
  // packet�ւ̓o�^
  size = uart0_read(dst, --size);
    
  // GPS�����̎擾, ���q���ɑ΂���G�t�F�����X�̗v��
  while(size-- > 0){
    
    static __xdata struct {
      u16 index, size;
      u8 ck_a, ck_b; // �`�F�b�N�T���p
      enum {NAV_TIMEGPS, NAV_TIMEUTC, RXM_RAW, UNKNOWN} packet_type;
    } ubx_state = {0};
    
    u8 c = *(dst++);
    
    switch(++ubx_state.index){
      case 1:
        if(c == 0xB5){
          ubx_state.size = 8;
        }else{
          ubx_state.index = 0;
        }
        continue; // jump to while loop.
      case 2: 
        if(c != 0x62){ubx_state.index = 0;} 
        continue; // jump to while loop.
      case 3: 
        ubx_state.ck_a = ubx_state.ck_b = c;
        continue; // jump to while loop.
      case 4: 
        ubx_state.packet_type = UNKNOWN;
        switch(ubx_state.ck_a){
          case 0x01:
            switch(c){
              case 0x20: ubx_state.packet_type = NAV_TIMEGPS; break;
              case 0x21: ubx_state.packet_type = NAV_TIMEUTC; break;
            }
            break;
          case 0x02:
            switch(c){
              case 0x10: ubx_state.packet_type = RXM_RAW; break;
            }
            break;
        }
        break;
      case 5:
        ubx_state.size += c;
        break;
      case 6:
        ubx_state.size += ((u16)c << 8);
        break;
      default: {
        if(ubx_state.index == ubx_state.size){
          if(ubx_state.ck_b == c){ // correct checksum
            if(ubx_state.packet_type == RXM_RAW){
              gps_ms = ((gps_ms / 1000) + 1) * 1000;
              if(gps_ms >= (u32)60 * 60 * 24 * 7 * 1000){
                gps_ms = 0;
                gps_wn++;
              }
              gps_time_modified = TRUE;
            }else if(ubx_state.packet_type == NAV_TIMEGPS){
              static __xdata u8 sv_eph_selector = 0;
              if((++sv_eph_selector) > UBX_SAT_MAX_ID){
                poll_aid_hui();
                sv_eph_selector = 0;
              }else{
                poll_rxm_eph(sv_eph_selector);
              }
            }
          }else{
            if(ubx_state.packet_type == NAV_TIMEUTC){
              gps_utc.tm_year -= 1900;
              gps_utc_valid = FALSE;
            }
          }
          ubx_state.index = 0;
          continue; // jump to while loop.
        }else if(ubx_state.index == (ubx_state.size - 1)){
          if(ubx_state.ck_a != c){ // incorrect checksum
            ubx_state.index = 0;
          }
          continue; // jump to while loop.
        }else{
          switch(ubx_state.packet_type){
            case NAV_TIMEGPS:
              if(ubx_state.index == 17){
                leap_seconds = (s8)c;
              }
              break;
            case NAV_TIMEUTC:
              switch(ubx_state.index){
                case 19:
                  gps_utc_valid = FALSE;
                case 20:
                  *((u8 *)(((u8 *)&gps_utc.tm_year) + (ubx_state.index - 19))) = c;
                  break;
                case 21: gps_utc.tm_mon = c - 1; break;
                case 22: gps_utc.tm_mday = c; break;
                case 23: gps_utc.tm_hour = c; break;
                case 24: gps_utc.tm_min = c; break;
                case 25: gps_utc.tm_sec = c; break;
                case 26: if((c & 0x03) == 0x03){gps_utc_valid = TRUE;} break;
              }
              break;
            case RXM_RAW:
              switch(ubx_state.index){
                case 7:
                case 8:
                case 9:
                case 10:
                  *((u8 *)(((u8 *)&gps_ms) + (ubx_state.index - 7))) = c;
                  break;
                case 11:
                case 12:
                  *((u8 *)(((u8 *)&gps_wn) + (ubx_state.index - 11))) = c;
                  break;
                case 13:
                  gps_num_of_sat = c;
                  break;
              }
              break;
          }
        }
        break; 
      }
    }
    
    // check sum update
    ubx_state.ck_a += c;
    ubx_state.ck_b += ubx_state.ck_a;
  }
  
  packet->current = dst;
}

void gps_polling(){
  u8 buf_size = uart0_rx_size();
  for(; buf_size >= (PAGE_SIZE - 1); buf_size -= (PAGE_SIZE - 1)){
    if(!data_hub_assign_page(make_packet)){break;}
  }
}