#include "app_controller.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "driver/ledc.h"
#include "sdkconfig.h"

#include "fr_flash.h"
#include "esp_partition.h"
#include "app_board.h" // 2
#include "app_camera.h"
#include <time.h> // 2

camera_buf_t camera_buf;
camera_cfg_t camera_cfg;
control_param_t cp_t;
control_handle_t handlers[5];

// EXT_RAM_ATTR static uint8_t gs_buf[240 * 240]; // gray to rgb565缓冲区//使用外部内存

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define TAG "app controller"
#else
#include "esp_log.h"
static const char *TAG = "HCamera";
#endif

#if CONFIG_ESP_FACE_DETECT_ENABLED

#if CONFIG_ESP_FACE_DETECT_MTMN
#include "fd_forward.h"
#endif

#if CONFIG_ESP_FACE_DETECT_LSSH
#include "lssh_forward.h"
#endif

#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
#include "fr_forward.h"

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 7
#endif

#define FACE_COLOR_WHITE 0x00FFFFFF
#define FACE_COLOR_BLACK 0x00000000
#define FACE_COLOR_RED 0x000000FF
#define FACE_COLOR_GREEN 0x0000FF00
#define FACE_COLOR_BLUE 0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)
#endif

#ifdef CONFIG_LED_ILLUMINATOR_ENABLED
int led_duty = 0;
bool isStreaming = false;
#ifdef CONFIG_LED_LEDC_LOW_SPEED_MODE
#define CONFIG_LED_LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#else
#define CONFIG_LED_LEDC_SPEED_MODE LEDC_HIGH_SPEED_MODE
#endif
#endif

#if CONFIG_ESP_FACE_DETECT_ENABLED

static int8_t detection_enabled = 0;

#if CONFIG_ESP_FACE_DETECT_MTMN
static mtmn_config_t mtmn_config = {0};
#endif

#if CONFIG_ESP_FACE_DETECT_LSSH
static lssh_config_t lssh_config;
static int min_face = 80;
#endif

#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
static int8_t recognition_enabled = 0;
static int8_t is_enrolling = 0;
static face_id_list id_list = {0};
#endif

#endif

typedef struct
{
    size_t size;  // number of values used for filtering
    size_t index; // current value index
    size_t count; // value count
    int sum;
    int *values; // array to be filled with values
} ra_filter_t;

static ra_filter_t ra_filter;

static ra_filter_t *ra_filter_init(ra_filter_t *filter, size_t sample_size)
{
    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int *)malloc(sample_size * sizeof(int));
    if (!filter->values)
    {
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
}

static int ra_filter_run(ra_filter_t *filter, int value)
{
    if (!filter->values)
    {
        return value;
    }
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index++;
    filter->index = filter->index % filter->size;
    if (filter->count < filter->size)
    {
        filter->count++;
    }
    return filter->sum / filter->count;
}

#if CONFIG_ESP_FACE_DETECT_ENABLED
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
static void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char *str)
{
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, 10, color, str);
}

static int rgb_printf(dl_matrix3du_t *image_matrix, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(copy);
    if (len >= sizeof(loc_buf))
    {
        temp = (char *)malloc(len + 1);
        if (temp == NULL)
        {
            return 0;
        }
    }
    vsnprintf(temp, len + 1, format, arg);
    va_end(arg);
    rgb_print(image_matrix, color, temp);
    if (len > 64)
    {
        free(temp);
    }
    return len;
}
#endif
static void draw_face_boxes(dl_matrix3du_t *image_matrix, box_array_t *boxes, int face_id)
{
    int x, y, w, h, i;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0)
    {
        color = FACE_COLOR_RED;
    }
    else if (face_id > 0)
    {
        color = FACE_COLOR_GREEN;
    }
    fb_data_t fb;
    fb.width = image_matrix->w;
    fb.height = image_matrix->h;
    fb.data = image_matrix->item;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    for (i = 0; i < boxes->len; i++)
    {
        // rectangle box
        x = (int)boxes->box[i].box_p[0];
        y = (int)boxes->box[i].box_p[1];
        w = (int)boxes->box[i].box_p[2] - x + 1;
        h = (int)boxes->box[i].box_p[3] - y + 1;
        fb_gfx_drawFastHLine(&fb, x, y, w, color);
        fb_gfx_drawFastHLine(&fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(&fb, x, y, h, color);
        fb_gfx_drawFastVLine(&fb, x + w - 1, y, h, color);
#if 0
        // landmark
        int x0, y0, j;
        for (j = 0; j < 10; j+=2) {
            x0 = (int)boxes->landmark[i].landmark_p[j];
            y0 = (int)boxes->landmark[i].landmark_p[j+1];
            fb_gfx_fillRect(&fb, x0, y0, 3, 3, color);
        }
#endif
    }
}
#if 0

static int8_t save_face_id_to_flash_name(face_id_name_list *id_list,char * name )
{
    if(0==l->count){
        ESP_LOGE(TAG, "list is empty");
        return -3;
    }
    // left_sample == 0
    const esp_partition_t *pt = esp_partition_find_first(FR_FLASH_TYPE, FR_FLASH_SUBTYPE, FR_FLASH_PARTITION_NAME);
    if (pt == NULL){
        ESP_LOGE(TAG, "Not found");
        return -2;
    }

    uint8_t enroll_id_idx = l->count - 1;

    // flash can only be erased divided by 4K
    const int block_len = FACE_ID_SIZE * sizeof(float);
    const int block_num = (4096 + block_len - 1) / block_len;

    if(enroll_id_idx % block_num == 0)
    {
        // save the other block TODO: if block != 2
        esp_partition_erase_range(pt, 4096 + enroll_id_idx * block_len, 4096);

        esp_partition_write(pt, 4096 + enroll_id_idx * block_len, l->tail->id_vec->item, block_len);
    }
    else
    {
        // save the other block TODO: if block != 2
        float *backup_buf = (float *)dl_lib_calloc(1, block_len, 0);
        esp_partition_read(pt, 4096 + (enroll_id_idx - 1) * block_len, backup_buf, block_len);

        esp_partition_erase_range(pt, 4096 + (enroll_id_idx - 1) * block_len, 4096);

        esp_partition_write(pt, 4096 + (enroll_id_idx - 1) * block_len, backup_buf, block_len);
        esp_partition_write(pt, 4096 + enroll_id_idx * block_len, l->tail->id_vec->item, block_len); 
        dl_lib_free(backup_buf);
    }

    const int name_len = ENROLL_NAME_LEN * sizeof(char);
    char *backup_name = (char *)dl_lib_calloc(l->count, name_len, 0);
    esp_partition_read(pt, sizeof(int) + sizeof(uint8_t), backup_name, name_len * (l->count - 1));
    memcpy(backup_name + (l->count - 1) * name_len, l->tail->id_name, name_len);
    esp_partition_erase_range(pt, 0, 4096);
    int flash_info_flag = FR_FLASH_INFO_FLAG;
    esp_partition_write(pt, 0, &flash_info_flag, sizeof(int));
    esp_partition_write(pt, sizeof(int), &l->count, sizeof(uint8_t));
    esp_partition_write(pt, sizeof(int) + sizeof(uint8_t), backup_name, name_len * l->count);
    dl_lib_free(backup_name);

    return 0;
    }
static int8_t save_face_id_to_flash(face_id_name_list *id_list)
{
        if(0==id_list->count){
            ESP_LOGE(TAG, "list is empty");
            return -3;
        }
        const esp_partition_t *pt = esp_partition_find_first(FR_FLASH_TYPE, FR_FLASH_SUBTYPE, FR_FLASH_PARTITION_NAME);
        if (pt == NULL){
            ESP_LOGE(TAG, "Not found");
        return -2;
        }

            const int block_len = FACE_ID_SIZE * sizeof(float);
            const int block_num = (4096 + block_len - 1) / block_len;
            float *backup_buf = (float *)dl_lib_calloc(1, block_len, 0);
            int flash_info_flag = FR_FLASH_INFO_FLAG;
            uint8_t enroll_id_idx = id_list->tail == 0 ? (id_list->size - 1) : (id_list->tail - 1) % id_list->size;

            if(enroll_id_idx % block_num == 0)
            {
                // save the other block TODO: if block != 2
                esp_partition_read(pt, 4096 + (enroll_id_idx + 1) * block_len, backup_buf, block_len);

                esp_partition_erase_range(pt, 4096 + enroll_id_idx * block_len, 4096);

                esp_partition_write(pt, 4096 + enroll_id_idx * block_len, id_list->id_list[enroll_id_idx]->item, block_len);
                esp_partition_write(pt, 4096 + (enroll_id_idx + 1) * block_len, backup_buf, block_len); 
            }
            else
            {
                // save the other block TODO: if block != 2
                esp_partition_read(pt, 4096 + (enroll_id_idx - 1) * block_len, backup_buf, block_len);

                esp_partition_erase_range(pt, 4096 + (enroll_id_idx - 1) * block_len, 4096);

                esp_partition_write(pt, 4096 + (enroll_id_idx - 1) * block_len, backup_buf, block_len);
                esp_partition_write(pt, 4096 + enroll_id_idx * block_len, id_list->id_list[enroll_id_idx]->item, block_len); 
            }
            dl_lib_free(backup_buf);

            esp_partition_erase_range(pt, 0, 4096);
            esp_partition_write(pt, 0, &flash_info_flag, sizeof(int));
            esp_partition_write(pt, sizeof(int), id_list, sizeof(face_id_name_list));
        return 0;
}
int8_t e_save_face_id_to_flash(void)
{
        if(0)
            return save_face_id_to_flash_name(&id_list,NULL);
        else        
            return save_face_id_to_flash(&id_list);
}

int8_t e_delete_face_id_in_flash(void)
{
        return delete_face_id_in_flash(&id_list);
}
int8_t e_read_face_id_from_flash(uint8_t sv_id_num,uint8_t cf_times)
{
	uint8_t sv_id_num_t=sv_id_num&0x0f;
	uint8_t cf_times_t=cf_times&0xff;
	
	while(delete_face(&id_list));
	face_id_init(&id_list, sv_id_num_t, cf_times_t);

    	return read_face_id_from_flash(&id_list);
}
#endif
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
static int run_face_recognition(dl_matrix3du_t *image_matrix, box_array_t *net_boxes)
{
    dl_matrix3du_t *aligned_face = NULL;
    int matched_id = 0;

    aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
    if (!aligned_face)
    {
        ESP_LOGE(TAG, "Could not allocate face recognition buffer");
        return matched_id;
    }
    if (align_face(net_boxes, image_matrix, aligned_face) == ESP_OK)
    {
        if (is_enrolling == 1)
        {
            int8_t left_sample_face;

            left_sample_face = enroll_face(&id_list, aligned_face);

            if (left_sample_face == (ENROLL_CONFIRM_TIMES - 1))
            {
                ESP_LOGI(TAG, "Enrolling Face ID: %d", id_list.tail); // 2
            }
            ESP_LOGI(TAG, "Enrolling Face ID: %d sample %d", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);
            rgb_printf(image_matrix, FACE_COLOR_CYAN, "ID[%u] Sample[%u]", id_list.tail, ENROLL_CONFIRM_TIMES - left_sample_face);
            if (left_sample_face == 0)
            {
                is_enrolling = 0;
                ESP_LOGI(TAG, "Enrolled Face ID: %d", id_list.tail);
            }
        }
        else
        {
            matched_id = recognize_face(&id_list, aligned_face);
            if (matched_id >= 0)
            {
                ESP_LOGW(TAG, "Match Face ID: %u", matched_id);
                rgb_printf(image_matrix, FACE_COLOR_GREEN, "Hello Subject %u", matched_id);
            }
            else
            {
                ESP_LOGW(TAG, "No Match Found");
                rgb_print(image_matrix, FACE_COLOR_RED, "Intruder Alert!");
                matched_id = -1;
            }
        }
    }
    else
    {
        ESP_LOGW(TAG, "Face Not Aligned");
        // rgb_print(image_matrix, FACE_COLOR_YELLOW, "Human Detected");
    }

    dl_matrix3du_free(aligned_face);
    return matched_id;
}
#endif
#endif

#ifdef CONFIG_LED_ILLUMINATOR_ENABLED
void enable_led(bool en)
{ // Turn LED On or Off
    int duty = en ? led_duty : 0;
    if (en && isStreaming && (led_duty > CONFIG_LED_MAX_INTENSITY))
    {
        duty = CONFIG_LED_MAX_INTENSITY;
    }
    ledc_set_duty(CONFIG_LED_LEDC_SPEED_MODE, CONFIG_LED_LEDC_CHANNEL, duty);
    ledc_update_duty(CONFIG_LED_LEDC_SPEED_MODE, CONFIG_LED_LEDC_CHANNEL);
    ESP_LOGI(TAG, "Set LED intensity to %d", duty);
}
#endif

static void stream_handler(void *pvParameters)
{
    uint8_t task_id = *((uint8_t *)pvParameters);
    sensor_t *s = esp_camera_sensor_get();

    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    // char *part_buf[128];
#if CONFIG_ESP_FACE_DETECT_ENABLED
    dl_matrix3du_t *image_matrix = NULL;
    bool detected = false;
    int face_id = 0;
    int64_t fr_start = 0;
    int64_t fr_ready = 0;
    int64_t fr_face = 0;
    int64_t fr_recognize = 0;
    int64_t fr_encode = 0;
#endif

    static int64_t last_frame = 0;
    if (!last_frame)
    {
        last_frame = esp_timer_get_time();
    }

#ifdef CONFIG_LED_ILLUMINATOR_ENABLED
    enable_led(true);
    isStreaming = true;
#endif

    while (handlers[task_id].flag)
    {
#if CONFIG_ESP_FACE_DETECT_ENABLED
        detected = false;
        face_id = 0;
#endif
        fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
#if CONFIG_ESP_FACE_DETECT_ENABLED
            fr_start = esp_timer_get_time();
            fr_ready = fr_start;
            fr_face = fr_start;
            fr_encode = fr_start;
            fr_recognize = fr_start;
            if (!detection_enabled || fb->width > 400)
            {
#endif
                if (fb->format != PIXFORMAT_RGB565)
                {
                    // printf("len: %d\n", fb->len);
                    // bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    // esp_camera_fb_return(fb);
                    // fb = NULL;
                    // if (!jpeg_converted)
                    // {
                    //     ESP_LOGE(TAG, "JPEG compression failed");
                    //     res = ESP_FAIL;
                    // }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
#if CONFIG_ESP_FACE_DETECT_ENABLED
            }
            else
            {

                image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);

                if (!image_matrix)
                {
                    ESP_LOGE(TAG, "dl_matrix3du_alloc failed");
                    res = ESP_FAIL;
                }
                else
                {
                    if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_matrix->item))
                    {
                        ESP_LOGE(TAG, "fmt2rgb888 failed");
                        res = ESP_FAIL;
                    }
                    else
                    {
#if CONFIG_ESP_FACE_DETECT_LSSH
                        // lssh_update_config(&lssh_config, min_face, image_matrix->h, image_matrix->w);
#endif
                        fr_ready = esp_timer_get_time();
                        box_array_t *net_boxes = NULL;
                        if (detection_enabled)
                        {
#if CONFIG_ESP_FACE_DETECT_MTMN
                            net_boxes = face_detect(image_matrix, &mtmn_config);
#endif

#if CONFIG_ESP_FACE_DETECT_LSSH
                            net_boxes = lssh_detect_object(image_matrix, lssh_config);
#endif
                        }
                        fr_face = esp_timer_get_time();
                        fr_recognize = fr_face;
                        if (net_boxes || fb->format != PIXFORMAT_JPEG)
                        {
                            if (net_boxes)
                            {
                                detected = true;
#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
                                if (recognition_enabled)
                                {
                                    face_id = run_face_recognition(image_matrix, net_boxes);
                                }
                                fr_recognize = esp_timer_get_time();
#endif
                                draw_face_boxes(image_matrix, net_boxes, face_id);
                                dl_lib_free(net_boxes->score);
                                dl_lib_free(net_boxes->box);
                                if (net_boxes->landmark != NULL)
                                    dl_lib_free(net_boxes->landmark);
                                dl_lib_free(net_boxes);
                            }
                            if (!fmt2jpg(image_matrix->item, fb->width * fb->height * 3, fb->width, fb->height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len))
                            {
                                ESP_LOGE(TAG, "fmt2jpg failed");
                            }
                            esp_camera_fb_return(fb);
                            fb = NULL;
                        }
                        else
                        {
                            _jpg_buf = fb->buf;
                            _jpg_buf_len = fb->len;
                        }
                        fr_encode = esp_timer_get_time();
                    }
                    dl_matrix3du_free(image_matrix);
                }
            }
#endif
        }
        if (res == ESP_OK)
        {
            // _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec
            // res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);

            // grayscale to rgb565
            //  int index = 0, index1 = 0;
            //  for (int i = 0; i < fb->height; i++)
            //  {
            //      for (int j = 0; j < fb->width; j++)
            //      {
            //          index = (i * fb->width + j);
            //          index1 = (i * 2 * fb->width + (2*j));
            //          uint8_t c = fb->buf[index]; //grayscale
            //          uint16_t rgb565 = ((c & 0xF8) << 8) | ((c & 0xFC) << 3) | ((c & 0xF8) >> 3);

            //         rgb565_buf[index1] = rgb565 >> 8;
            //         rgb565_buf[index1+1] = rgb565 & 0xff;
            //     }
            // }

            // camera_buf.buf = rgb565_buf;
            // camera_buf.buf_len = (fb->len * 2);
            camera_buf.buf = _jpg_buf;
            camera_buf.buf_len = _jpg_buf_len;
            camera_buf.buf_w = fb->width;
            camera_buf.buf_h = fb->height;
            handlers[task_id].handler(&cp_t);
        }
        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK)
        {
            break;
        }
        int64_t fr_end = esp_timer_get_time();
#if CONFIG_ESP_FACE_DETECT_ENABLED
        int64_t ready_time = (fr_ready - fr_start) / 1000;
        int64_t face_time = (fr_face - fr_ready) / 1000;
        int64_t recognize_time = (fr_recognize - fr_face) / 1000;
        int64_t encode_time = (fr_encode - fr_recognize) / 1000;
        int64_t process_time = (fr_encode - fr_start) / 1000;
#endif

        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);

        //         ESP_LOGI(TAG, "MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)"
        // #if CONFIG_ESP_FACE_DETECT_ENABLED
        //                       ", %u+%u+%u+%u=%u %s%d"
        // #endif
        //                  ,
        //                  (uint32_t)(_jpg_buf_len),
        //                  (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
        //                  avg_frame_time, 1000.0 / avg_frame_time
        // #if CONFIG_ESP_FACE_DETECT_ENABLED
        //                  ,
        //                  (uint32_t)ready_time, (uint32_t)face_time, (uint32_t)recognize_time, (uint32_t)encode_time, (uint32_t)process_time,
        //                  (detected) ? "DETECTED " : "", face_id
        // #endif
        //         );
    }

#ifdef CONFIG_LED_ILLUMINATOR_ENABLED
    isStreaming = false;
    enable_led(false);
#endif

    last_frame = 0;
    handlers[task_id].flag = 0;
    // delete task
    vTaskDelete(handlers[task_id].xHandle);
}

static void config_handler(void *pvParameters)
{
    static uint8_t init_flag = 0;
    ESP_LOGI(TAG, "config handler!\n");
    uint8_t task_id = *((uint8_t *)pvParameters);

    if (!init_flag)
    {
        camera_cfg.cfg_type = VIDEO_SIZE;
        camera_cfg.val = FRAMESIZE_240X240;
        init_flag = 1;
    }
    else
    {
        handlers[task_id].handler(&cp_t);
    }

    int res = 0;
    sensor_t *s = esp_camera_sensor_get();

    switch (camera_cfg.cfg_type)
    {
    case VIDEO_SIZE:
        if (s->pixformat == PIXFORMAT_RGB565)
        {
            res = s->set_framesize(s, (framesize_t)camera_cfg.val); // set_framesize
            if (res == ESP_OK)
            {
                ESP_LOGD(TAG, "camera framesize update success");
            }
        }
        break;
    case SPECIAL_EFFECT:
        res = s->set_special_effect(s, camera_cfg.val);
        if (res == ESP_OK)
        {
            ESP_LOGD(TAG, "camera special_effect update success");
        }
        break;
    default:
        break;
    }

    handlers[task_id].flag = 0;
    // callback
    //
    //  delete task
    vTaskDelete(handlers[task_id].xHandle);
}

static void capture_handler(void *pvParameters)
{
    uint8_t task_id = *((uint8_t *)pvParameters);
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_FAIL;
    int64_t fr_start = esp_timer_get_time();

    do
    {

        enable_led(true);
        vTaskDelay(150 / portTICK_PERIOD_MS); // The LED needs to be turned on ~150ms before the call to esp_camera_fb_get()
        fb = esp_camera_fb_get();             // or it won't be visible in the frame. A better way to do this is needed.
        enable_led(false);

// #ifdef CONFIG_LED_ILLUMINATOR_ENABLED
//         enable_led(true);
//         vTaskDelay(150 / portTICK_PERIOD_MS); // The LED needs to be turned on ~150ms before the call to esp_camera_fb_get()
//         fb = esp_camera_fb_get();             // or it won't be visible in the frame. A better way to do this is needed.
//         enable_led(false);
// #else
//         fb = esp_camera_fb_get();
// #endif

        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        if (fb->format == PIXFORMAT_RGB565)
        {
            res = ESP_OK;
        }
        else
        {
            res = ESP_FAIL;
        }
        int64_t fr_end = esp_timer_get_time();
        ESP_LOGI(TAG, "RGB565: %ums", (uint32_t)((fr_end - fr_start) / 1000));
        break;

    } while (true);

    // delete task
    handlers[task_id].flag = 0;
    if (res == ESP_OK)
    {
        camera_buf.buf = fb->buf;
        camera_buf.buf_len = fb->len;
        camera_buf.buf_w = fb->width;
        camera_buf.buf_h = fb->height;
        handlers[task_id].handler(&cp_t);
    }
    else
    {
        handlers[task_id].handler(NULL);
    }
    esp_camera_fb_return(fb);
    fb = NULL;
    vTaskDelete(handlers[task_id].xHandle);
}

static void qrcode_handler(void *pvParameters)
{
    ESP_LOGI(TAG, "qrcode handler");
    uint8_t task_id = *((uint8_t *)pvParameters);

    camera_fb_t *fb = NULL;
    uint8_t *qr_image = NULL;
    uint8_t *gs_buf = NULL;
    int id_count = 0;
    static struct quirc *qr_t = NULL;

    sensor_t *s = esp_camera_sensor_get();

    uint8_t qr_len = sizeof(cp_t.qrcode) / sizeof(char);
    memset(cp_t.qrcode, 0x00, qr_len);

    if (qr_t == NULL)
    {
        ESP_LOGI(TAG, "Construct a new QR-code recognizer(quirc).");
        qr_t = quirc_new();
    }

    static int old_width = 0;
    static int old_height = 0;

    do
    {

        // Use QVGA Size currently, but quirc can support other frame size.(eg:
        // FRAMESIZE_QVGA,FRAMESIZE_HQVGA,FRAMESIZE_QCIF,FRAMESIZE_QQVGA2,FRAMESIZE_QQVGA,etc)
        if ((qr_t == NULL) || (s->status.framesize > FRAMESIZE_QVGA))
        {
            ESP_LOGE(TAG, "Camera Frame Size err %d, support maxsize is QVGA", (s->status.framesize));
            break;
        }

        fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            break;
        }

        if(old_width != fb->width || old_height != fb->height)
        {
            if(quirc_resize(qr_t, fb->width, fb->height) < 0)
            {
                break;
            }else
            {
                old_width = fb->width;
                old_height = fb->height;
            }
        }

        // rgb565 to grayscale
        int index = 0, index1 = 0;
        int w2 = 2 * fb->width;
        gs_buf = (uint8_t *)malloc((fb->width * fb->height) * sizeof(uint8_t)); //申请内存
        if (!gs_buf)
        {
            break;
        }
        for (int i = 0; i < fb->height; i++)
        {
            for (int j = 0; j < fb->width; j++)
            {
                index = (i * w2 + (2 * j));
                index1 = (i * fb->width + j);
                uint16_t rgb565 = (fb->buf[index] << 8) | (fb->buf[index + 1]);

                uint8_t r = (rgb565 & 0xF800) >> 8;
                uint8_t g = (rgb565 & 0x07E0) >> 3;
                uint8_t b = (rgb565 & 0x001F) << 3;

                *(gs_buf + index1) = (uint8_t)((r * 77 + g * 150 + b * 29 + 128) / 256);
            }
        }

        qr_image = quirc_begin(qr_t, NULL, NULL);
        memcpy(qr_image, gs_buf, (fb->width * fb->height));
        quirc_end(qr_t);

        id_count = quirc_count(qr_t);
        if (id_count == 0)
        {
            ESP_LOGE(TAG, "Error: not a valid qrcode");
            break;
        }

        dump_info(qr_t, id_count, cp_t.qrcode, qr_len);
        break;

    } while (true);

    if (gs_buf != NULL)
    {
        free(gs_buf);
        gs_buf = NULL;
    }
    esp_camera_fb_return(fb);
    handlers[task_id].flag = 0;
    // callback
    handlers[task_id].handler(&cp_t);
    // delete task
    vTaskDelete(handlers[task_id].xHandle);
}

void controller_start_task(uint8_t id)
{
    uint8_t task_flag = handlers[id].flag;
    if (task_flag == 0)
    {
        handlers[id].flag = 1;
        //任务没有运行，创建任务
        switch (handlers[id].id)
        {
        case CF_CAPTURE:
            xTaskCreate(&capture_handler,
                        "CAPTURE_TASK",
                        4096,
                        &handlers[id].id,
                        5,
                        &handlers[id].xHandle);
            break;
        case CF_VIDEO:
        case CF_STREAM:
            xTaskCreate(&stream_handler,
                        "VS_TASK",
                        4096,
                        &handlers[id].id,
                        5,
                        &handlers[id].xHandle);
            break;
        case CF_CONFIG:
            xTaskCreate(&config_handler,
                        "CONFIG_TASK",
                        4096,
                        &handlers[id].id,
                        5,
                        &handlers[id].xHandle);
            break;
        case CF_QRCODE:
            xTaskCreate(&qrcode_handler,
                        "QRCODE_TASK",
                        4096 * 10,
                        &handlers[id].id,
                        5,
                        &handlers[id].xHandle);
            break;
        }

        ESP_LOGI(TAG, "Task %d is created!\n", id);
    }
    else
    {
        //任务正在运行
        ESP_LOGI(TAG, "Task %d is running...\n", id);
    }
}

void controller_stop_task(uint8_t id)
{
    if (handlers[id].flag == 1)
    {
        handlers[id].flag = 0;
        //等待任务删除
        while (true)
        {
            if (eTaskGetState(handlers[id].xHandle) == eDeleted)
            {
                break;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Task %d delete successful!\n", id);
    }
}

/**
 * 初始化函数
 */
esp_err_t app_controller_main()
{

#if CONFIG_ESP_FACE_DETECT_ENABLED

#if CONFIG_ESP_FACE_DETECT_MTMN
    mtmn_config.type = FAST;
    mtmn_config.min_face = 80;
    mtmn_config.pyramid = 0.707;
    mtmn_config.pyramid_times = 4;
    mtmn_config.p_threshold.score = 0.6;
    mtmn_config.p_threshold.nms = 0.7;
    mtmn_config.p_threshold.candidate_number = 20;
    mtmn_config.r_threshold.score = 0.7;
    mtmn_config.r_threshold.nms = 0.7;
    mtmn_config.r_threshold.candidate_number = 10;
    mtmn_config.o_threshold.score = 0.7;
    mtmn_config.o_threshold.nms = 0.7;
    mtmn_config.o_threshold.candidate_number = 1;
#endif

#if CONFIG_ESP_FACE_DETECT_LSSH
    lssh_config = lssh_get_config(min_face, 0.7, 0.3, 240, 320);
#endif

#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
    face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);

    // read_face_id_from_flash(&id_list);
#endif

#endif

    cp_t.buf_t = &camera_buf;
    cp_t.cfg_t = &camera_cfg;

    return ESP_OK;
}

/**
 * 注册函数
 */
void controller_reg_handler(const control_handle_t *control_handler)
{
    uint8_t id = control_handler->id;
    handlers[id].id = id;
    handlers[id].handler = control_handler->handler;
    ESP_LOGD(TAG, "handler reg ok: %d\n", handlers[id].id);
}

static void sound_callback()
{
    ESP_LOGI(TAG, "sound task");
    BELL_ON;
    vTaskDelay(50 / portTICK_RATE_MS);
    BELL_OFF;

    vTaskDelete(NULL);
}

void controller_sound_task()
{
    if (!app_config.sound_state)
    {
        ESP_LOGI(TAG, "sound disable");
        return;
    }
    xTaskCreate(&sound_callback, "SOUND_TASK", 2048, NULL, 4, NULL);
}