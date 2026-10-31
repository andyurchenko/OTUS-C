#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "json/cJSON.h"

#define DEFAULT_MAX_MEMORY_LIMIT 0xFFFF

/*
    !!!
    If WEATHER_SERVICE_BASE_ADDRESS changed,
    change WEATHER_SERVICE_BASE_ADDRESS_LENGTH and WEATHER_SERVICE_MIN_STRING_LENGTH
    !!!
*/
#define WEATHER_SERVICE_BASE_ADDRESS "ru.wttr.in/"
#define WEATHER_SERVICE_BASE_ADDRESS_LENGTH 11

/*
    !!!
    If WEATHER_SERVICE_REQUEST_OPTIONS changed,
    change WEATHER_SERVICE_REQUEST_OPTIONS_LENGTH and WEATHER_SERVICE_MIN_STRING_LENGTH
    !!!
*/
#define WEATHER_SERVICE_REQUEST_OPTIONS "?format=j2"
#define WEATHER_SERVICE_REQUEST_OPTIONS_LENGTH 10

#define WEATHER_SERVICE_MIN_STRING_LENGTH 21

#define CITY_INDEX 1
#define BUFFER_SIZE_INDEX 2
#define ZERO_SYMBOL 1

typedef struct memory_struct {
    unsigned char *memory_block;
    size_t byte_position; // Starting at 0 position
    size_t cur_limit;     // Current limit of bytes available to write into
    size_t max_limit;     // Max limit for the memory in bytes
} curl_buffer;

typedef struct {
    const char *name;
    char *value;
} named_value;

typedef struct {
    named_value temp_c;
    named_value feels_like_c;
    named_value humidity;
    named_value pressure;
    named_value precip_mm;
    named_value cloud_cover;
    named_value wind_dir;
    named_value wind_speed_kmph;
    named_value weather_desc;
    named_value observation_time;
    named_value country;
    named_value city;
} current_weather_info;

static size_t process_curl_response(const char *buffer_read_from, size_t element_size, size_t elements_number, void *buffer_write_to);
static int parse_size_t(const char * s, size_t* out);
static int construct_weather_service_address(const char *city_name, unsigned char **out);
static current_weather_info *create_current_weather_struct(void);
static int fill_struct_with_data_from_json(current_weather_info *info, const cJSON *json);
static char * get_object_string_field(const cJSON * object_field);
static void print_weather_info(const current_weather_info *info);
static void print_if_set(const named_value *nv);
static void print_error(const char *);

int main(const int argc, char *argv[]) {
    if (argc < 2 || 3 < argc) {
        fprintf(stderr, "Usage: %s <city_name> [buffer_limit_in_bytes].\n", argv[0]);
        fprintf(stderr,"File name: %s. Line number: %d\n", __FILE__, __LINE__);
        return EXIT_FAILURE;
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        print_error("curl_global_init() failed.");
        return EXIT_FAILURE;
    }

    int exit_code = EXIT_SUCCESS;
    CURL *curl = NULL;
    CURLcode curl_code = 0;
    cJSON *json_full_answer = NULL;
    current_weather_info *current_weather_info = NULL;
    unsigned char *weather_service_url = NULL;

    curl = curl_easy_init();
    if (curl == NULL) {
        print_error("curl_easy_init() failed.");
        return EXIT_FAILURE;
    }

    size_t buffer_size = 0;
    if (argc == 3) {
        if (parse_size_t(argv[BUFFER_SIZE_INDEX], &buffer_size)) {
            print_error("parse_size_t() failed.");
            return EXIT_FAILURE;
        }
    }

    curl_buffer buf = {
        .memory_block = NULL,
        .byte_position = 0,
        .cur_limit = 0,
        .max_limit = (buffer_size != 0) ? buffer_size : DEFAULT_MAX_MEMORY_LIMIT
    };

    if (construct_weather_service_address(argv[CITY_INDEX], &weather_service_url)) {
        exit_code = EXIT_FAILURE;
        goto EXIT_CLEANUP;
    }

    curl_code = CURLE_OK;
    curl_code |= curl_easy_setopt(curl, CURLOPT_URL, weather_service_url);
    curl_code |= curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_curl_response);
    curl_code |= curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buf);
    curl_code |= curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_code |= curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    if (curl_code != CURLE_OK) {
        fprintf(stderr, "Failed to configure curl: %s\n", curl_easy_strerror(curl_code));
        fprintf(stderr, "File name: %s. Line number: %d\n", __FILE__, __LINE__);
        exit_code = EXIT_FAILURE;
        goto EXIT_CLEANUP;
    }

    curl_code = curl_easy_perform(curl);
    if (curl_code != CURLE_OK) {
        fprintf(stderr, "Failed to querry the weather service: %s\n", curl_easy_strerror(curl_code));
        fprintf(stderr, "File name: %s. Line number: %d\n", __FILE__, __LINE__);
        exit_code = EXIT_FAILURE;
        goto EXIT_CLEANUP;
    }

    json_full_answer = cJSON_Parse((char*)buf.memory_block);
    if (json_full_answer == NULL) {
        fprintf(stderr, "JSON parsing failed\n");
        fprintf(stderr, "Beginning of the text where error was caused: %.10s", cJSON_GetErrorPtr());
        fprintf(stderr, "File name: %s. Line number: %d\n", __FILE__, __LINE__);
        exit_code = EXIT_FAILURE;
        goto EXIT_CLEANUP;
    }

    current_weather_info = NULL;
    current_weather_info = create_current_weather_struct();
    fill_struct_with_data_from_json(current_weather_info, json_full_answer);
    print_weather_info(current_weather_info);


// Единая точка очистки для всех путей завершения.
EXIT_CLEANUP:
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    if (current_weather_info) {
        free(current_weather_info);
        current_weather_info = NULL;
    }
    if (json_full_answer) {
        cJSON_Delete(json_full_answer);
        json_full_answer = NULL;
    }
    if (buf.memory_block) {
        free(buf.memory_block);
        buf.memory_block = NULL;
    }
    if (weather_service_url) {
        free(weather_service_url);
        weather_service_url = NULL;
    }

    return exit_code;
}

static size_t process_curl_response(const char *buffer_read_from, const size_t element_size, const size_t elements_number, void *buffer_write_to) {
    if (buffer_read_from == NULL || buffer_write_to == NULL) {
        print_error("No buffer to read from or to write to.");
        return 0;
    }

    const size_t new_chunk_size = element_size * elements_number;
    if (new_chunk_size == 0) {
        print_error("No data to save.");
        return 0;
    }

    curl_buffer *buf = buffer_write_to;
    /*
        Переменная byte_position в структуре MemoryStruct всегда указывает на следующий свободный для записи байт.
        Минимальное количество байт(min_bytes_capacity) - это старые данные + новые данные,
        при этом в конце, где стоит текущий указатель, мы запишем '\0' для обозначения конца строки.
    */
    const size_t min_bytes_capacity = buf->byte_position + new_chunk_size;
    if (buf->max_limit < min_bytes_capacity) {
        print_error("Buffer is full. Not all data is saved.");
        return 0;
    }

    if (buf->cur_limit < min_bytes_capacity) {
        while (buf->cur_limit < min_bytes_capacity) {
            if (buf->cur_limit > buf->max_limit) {
                buf->cur_limit = buf->max_limit;
                return 0;
            }
            /*
                 Увеличение емкости памяти происходит не ровно в два раза, а чуть больше/быстрее, за счет смещения влево
                 на одну позицию и прибавления единицы.
                 0000(0) -> 0001(1) -> 0011(3) -> 0111(7) -> 1111(15)
             */
            buf->cur_limit = (buf->cur_limit << 1) | 1;
        }

        unsigned char *ptr = realloc(buf->memory_block, buf->cur_limit);
        if (ptr == NULL) {
            print_error("Buffer expansion failed.");
            return 0;
        }

        buf->memory_block = ptr;
    }

    memcpy(buf->memory_block + buf->byte_position, buffer_read_from, new_chunk_size);
    buf->byte_position += new_chunk_size;
    buf->memory_block[buf->byte_position] = '\0';

    return new_chunk_size;
}

static int parse_size_t(const char * const s, size_t * const out) {
    if (s == NULL || *s == '\0') {
        print_error("String to parse is null or empty.");
        return EXIT_FAILURE;
    }

    const char *p = s;
    while (isspace((unsigned char)*p)) {
        p++;
    }

    if (*p == '-') {
        print_error("Buffer size has to be a positive number.");
        return EXIT_FAILURE;
    }

    char *end = NULL;
    errno = 0;
    unsigned long long ull = strtoull(p, &end, 10);
    if (errno == ERANGE) {
        print_error("Buffer size is too big.");
        return EXIT_FAILURE;
    }

    if (end == s || *end != '\0') {
        print_error("Buffer size is not a number.");
        return EXIT_FAILURE;
    }

    if (ull > SIZE_MAX) {
        print_error("Buffer size is too big.");
        return EXIT_FAILURE;
    }

    *out = (size_t)ull;
    return EXIT_SUCCESS;
}

static int construct_weather_service_address(const char * const city_name, unsigned char **out) {
    const unsigned char city_name_length = strlen((const char *) city_name);
    const unsigned char size = WEATHER_SERVICE_MIN_STRING_LENGTH + city_name_length;
    unsigned char *weather_service_url = malloc(size + ZERO_SYMBOL);
    if (weather_service_url == NULL) {
        print_error("Memory allocation failed.");
        return EXIT_FAILURE;
    }

    memcpy(
        weather_service_url,
        WEATHER_SERVICE_BASE_ADDRESS,
        WEATHER_SERVICE_BASE_ADDRESS_LENGTH
    );
    memcpy(
        weather_service_url + WEATHER_SERVICE_BASE_ADDRESS_LENGTH,
        city_name,
        city_name_length
    );
    memcpy(
        weather_service_url + WEATHER_SERVICE_BASE_ADDRESS_LENGTH + city_name_length,
        WEATHER_SERVICE_REQUEST_OPTIONS,
        WEATHER_SERVICE_REQUEST_OPTIONS_LENGTH
    );

    *out = weather_service_url;
    return EXIT_SUCCESS;
}

static void print_error(const char *s) {
    fprintf(stderr, "%s\n", s);
    fprintf(stderr, "File name: %s. Line number: %d\n", __FILE__, __LINE__);
}

static current_weather_info * create_current_weather_struct(void) {
    current_weather_info *info = malloc(sizeof (current_weather_info));
    if (!info) {
        return NULL;
    }
    memset(info, 0, sizeof(current_weather_info));
    info->temp_c.name              = "Температура: ";
    info->feels_like_c.name        = "Ощущается как: ";
    info->humidity.name            = "Влажность: ";
    info->pressure.name            = "Давление: ";
    info->precip_mm.name           = "Осадки (мм): ";
    info->cloud_cover.name         = "Облачность: ";
    info->wind_dir.name            = "Направление ветра: ";
    info->wind_speed_kmph.name     = "Скорость ветра (км/ч): ";
    info->weather_desc.name        = "Описание погоды: ";
    info->observation_time.name    = "Время наблюдения: ";
    info->country.name             = "Страна: ";
    info->city.name                = "Город: ";

    return info;
}

static void print_weather_info(const current_weather_info * const info) {
    print_if_set(&info->temp_c);
    print_if_set(&info->feels_like_c);
    print_if_set(&info->humidity);
    print_if_set(&info->pressure);
    print_if_set(&info->precip_mm);
    print_if_set(&info->cloud_cover);
    print_if_set(&info->wind_dir);
    print_if_set(&info->wind_speed_kmph);
    print_if_set(&info->weather_desc);
    print_if_set(&info->observation_time);
    print_if_set(&info->country);
    print_if_set(&info->city);
}

static void print_if_set(const named_value * const nv) {
    if (nv->name && nv->value) {
        printf("%s%s\n", nv->name, nv->value);
    }
}

static int fill_struct_with_data_from_json(current_weather_info * const info, const cJSON * const json) {
    const cJSON *json_current_condition_array = cJSON_GetObjectItem(json, "current_condition");
    if (!cJSON_IsArray(json_current_condition_array)) {
        print_error("There is no `current_condition` array in JSON answer.");
        return EXIT_FAILURE;
    }
    if (cJSON_GetArraySize(json_current_condition_array) != 1) {
        print_error("JSON `current_condition` section has a wrong structure.");
        return EXIT_FAILURE;
    }

    const cJSON *json_current_condition_obj = cJSON_GetArrayItem(json_current_condition_array, 0);
    if (!cJSON_IsObject(json_current_condition_obj)) {
        print_error("JSON `current_condition[0]` is not an JSON object.");
        return EXIT_FAILURE;
    }

    cJSON *object_field = cJSON_GetObjectItem(json_current_condition_obj, "temp_C");
    info->temp_c.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "FeelsLikeC");
    info->feels_like_c.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "humidity");
    info->humidity.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "pressure");
    info->pressure.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "precipMM");
    info->precip_mm.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "cloudcover");
    info->cloud_cover.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "winddir16Point");
    info->wind_dir.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "windspeedKmph");
    info->wind_speed_kmph.value = get_object_string_field(object_field);

    object_field = cJSON_GetObjectItem(json_current_condition_obj, "observation_time");
    info->observation_time.value = get_object_string_field(object_field);

    const cJSON *weather_desc_arr = cJSON_GetObjectItem(json_current_condition_obj, "lang_ru");
    if (cJSON_IsArray(weather_desc_arr)) {
        cJSON *first = cJSON_GetArrayItem(weather_desc_arr, 0);
        if (cJSON_IsObject(first)) {
            info->weather_desc.value = get_object_string_field(cJSON_GetObjectItem(first, "value"));
        }
    }

    const cJSON *json_nearest_area_array = cJSON_GetObjectItem(json, "nearest_area");
    if (!cJSON_IsArray(json_nearest_area_array)) {
        print_error("There is no `nearest_area` array in JSON answer.");
        return EXIT_FAILURE;
    }
    if (cJSON_GetArraySize(json_nearest_area_array) != 1) {
        print_error("JSON `nearest_area` section has a wrong structure.");
        return EXIT_FAILURE;
    }

    const cJSON *json_nearest_area_obj = cJSON_GetArrayItem(json_nearest_area_array, 0);
    if (!cJSON_IsObject(json_nearest_area_obj)) {
        print_error("JSON `nearest_area[0]` is not an object.");
        return EXIT_FAILURE;
    }

    const cJSON *area_name_array = cJSON_GetObjectItem(json_nearest_area_obj, "areaName");
    if (!cJSON_IsArray(area_name_array)) {
        print_error("There is no `areaName` array in JSON answer.");
        return EXIT_FAILURE;
    }
    const cJSON *area_name_obj = cJSON_GetArrayItem(area_name_array, 0);
    if (!cJSON_IsObject(area_name_obj)) {
        print_error("`areaName[0]` is not an JSON object.");
        return EXIT_FAILURE;
    }
    object_field = cJSON_GetObjectItem(area_name_obj, "value");
    info->city.value = get_object_string_field(object_field);

    const cJSON *country_name_array = cJSON_GetObjectItem(json_nearest_area_obj, "country");
    if (!cJSON_IsArray(area_name_array)) {
        print_error("There is no `country` array in JSON answer.");
        return EXIT_FAILURE;
    }
    const cJSON *country_name_obj = cJSON_GetArrayItem(country_name_array, 0);
    if (!cJSON_IsObject(area_name_obj)) {
        print_error("`country[0]` is not an JSON object.");
        return EXIT_FAILURE;
    }
    object_field = cJSON_GetObjectItem(country_name_obj, "value");
    info->country.value = get_object_string_field(object_field);

    return EXIT_SUCCESS;
}

static char * get_object_string_field(const cJSON *object_field) {
    if (object_field && cJSON_IsString(object_field) && object_field->valuestring != NULL) {
        return object_field->valuestring;
    }

    return NULL;
}
