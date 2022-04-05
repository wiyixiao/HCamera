#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif
#ifndef LV_ATTRIBUTE_IMG_CACTUS_SMALL
#define LV_ATTRIBUTE_IMG_CACTUS_SMALL
#endif
const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_IMG_CACTUS_SMALL uint8_t cactus_small_map[] = {
  0x00, 0x00, 0x00, 0x00, 	/*Color of index 0*/
  0xff, 0xff, 0xff, 0x06, 	/*Color of index 1*/
  0xfb, 0xfb, 0xfb, 0x24, 	/*Color of index 2*/
  0xef, 0xef, 0xef, 0x48, 	/*Color of index 3*/
  0xe8, 0xe8, 0xe8, 0x70, 	/*Color of index 4*/
  0xba, 0xba, 0xba, 0x2e, 	/*Color of index 5*/
  0xd1, 0xd1, 0xd1, 0x77, 	/*Color of index 6*/
  0xb7, 0xb7, 0xb7, 0x94, 	/*Color of index 7*/
  0xa7, 0xa7, 0xa7, 0xaa, 	/*Color of index 8*/
  0x99, 0x99, 0x99, 0xfb, 	/*Color of index 9*/
  0x7e, 0x7e, 0x7e, 0xd6, 	/*Color of index 10*/
  0x57, 0x57, 0x57, 0xf7, 	/*Color of index 11*/
  0x48, 0x48, 0x48, 0xf6, 	/*Color of index 12*/
  0x1a, 0x1a, 0x1a, 0xff, 	/*Color of index 13*/
  0x00, 0x00, 0x00, 0xe8, 	/*Color of index 14*/
  0x00, 0x00, 0x00, 0xfe, 	/*Color of index 15*/

  0x00, 0x00, 0x13, 0xab, 0x82, 0x10, 0x00, 0x00, 
  0x00, 0x00, 0x4c, 0xff, 0xf9, 0x20, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfc, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x02, 0x10, 
  0x00, 0x00, 0x6f, 0xff, 0xfc, 0x20, 0x7c, 0x70, 
  0x00, 0x00, 0x6f, 0xff, 0xfc, 0x23, 0xdf, 0xf0, 
  0x78, 0x20, 0x6f, 0xff, 0xfb, 0x23, 0xdf, 0xe0, 
  0xff, 0x70, 0x6f, 0xff, 0xfc, 0x23, 0xdf, 0xe0, 
  0xef, 0x70, 0x6f, 0xff, 0xfb, 0x23, 0xdf, 0xe0, 
  0xef, 0x70, 0x6f, 0xff, 0xfc, 0x23, 0xdf, 0xe0, 
  0xef, 0x70, 0x6f, 0xff, 0xfb, 0x23, 0xdf, 0xe0, 
  0xef, 0x70, 0x6f, 0xff, 0xfc, 0x23, 0xdf, 0xe0, 
  0xef, 0x70, 0x6f, 0xff, 0xfb, 0x23, 0xdf, 0xf0, 
  0xef, 0x70, 0x6f, 0xff, 0xfd, 0xbc, 0xff, 0x80, 
  0xef, 0x70, 0x6f, 0xff, 0xff, 0xff, 0xd8, 0x10, 
  0xef, 0x70, 0x6f, 0xff, 0xfc, 0x87, 0x61, 0x00, 
  0xff, 0x81, 0x4f, 0xff, 0xfb, 0x20, 0x00, 0x00, 
  0xcf, 0xda, 0xaf, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x3c, 0xff, 0xff, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x03, 0x88, 0xaf, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x11, 0x4f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6f, 0xff, 0xfb, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x6e, 0xee, 0xeb, 0x50, 0x00, 0x00, 
};

const lv_img_dsc_t cactus_small = {
  .header.cf = LV_IMG_CF_INDEXED_4BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 15,
  .header.h = 30,
  .data_size = 305,
  .data = cactus_small_map,
};
