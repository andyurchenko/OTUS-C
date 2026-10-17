#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define ERROR_NO_END_JPG_SIGNATURE_FOUND (-2)
#define ONE_BYTE 1
#define TWO_BYTES 2
#define FOUR_BYTES 4

#define JPG_END_SIGNATURE 0xFFD9
#define CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE "\x50\x4B\x01\x02"
#define CENTRAL_DIRECTORY_FILE_HEADER_FILE_NAME_LENGTH_OFFSET 0x1C
#define CENTRAL_DIRECTORY_FILE_HEADER_EXTRA_FIELD_LENGTH_OFFSET 0x1E
#define CENTRAL_DIRECTORY_FILE_HEADER_FILE_COMMENT_LENGTH_OFFSET 0x20
#define CENTRAL_DIRECTORY_FILE_HEADER_FILE_NAME_OFFSET 0x2E
#define CENTRAL_DIRECTORY_FILE_HEADER_BASE_LENGTH 46
#define EOCD_SIGNATURE "\x50\x4B\x05\x06"

typedef struct EOCD_struct {
    // Обязательная сигнатура, равна 0x504B0506
    uint32_t signature;
    // Номер диска
    uint16_t disk_number;
    // Номер диска, где находится начало Central Directory
    uint16_t start_disk_number;
    // Количество записей в Central Directory в текущем диске
    uint16_t number_central_directory_entries;
    // Всего записей в Central Directory
    uint16_t total_central_directory_entries;
    // Размер Central Directory
    uint32_t central_directory_size;
    // Смещение Central Directory
    uint32_t central_directory_offset;
    // Длина комментария
    uint16_t comment_length;
    // Комментарий (длиной commentLength)
    uint8_t *comment;
} eocd_type;

bool is_it_eocd_signature(const unsigned char *buffer);
eocd_type * create_empty_eocd(void);
int eocd_init_from_file(eocd_type *eocd, const FILE *f);
void free_eocd(eocd_type *eocd);
int64_t find_jpg_end_signature_offset(const FILE * f);
void free_allocated_resources(void);
uint16_t read_u16_le(const FILE *f);

char *file_name = NULL;
int exit_code = 0;

int is_it_file_header_signature(char signature[4]);

int main(const int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Error: not enough arguments\n");
        return EXIT_FAILURE;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        printf("Error: cannot open file %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    fseek(f, 0, SEEK_END);
    const long file_length_in_bytes = ftell(f);
    fprintf(stdout, "File length in bytes: %ld\\%#lX\n", file_length_in_bytes, (unsigned long) file_length_in_bytes);

    if (file_length_in_bytes < 22) {
        printf("Error: file too short\n");
        return EXIT_SUCCESS;
    }

    // Пытаемся найти сигнатуру конца JPEG — FF D9
    fseek(f, 0, SEEK_SET);
    const int64_t end_jpg_offset = find_jpg_end_signature_offset(f);
    if (end_jpg_offset == ERROR_NO_END_JPG_SIGNATURE_FOUND) {
        printf("Error: There is no end JPEG signature found in file.\n");
        exit_code = EXIT_FAILURE;
        goto EXIT;
    }

    // Текущая позиция равна длине файла? Значит дальше ничего нет.
    if (end_jpg_offset + 1 == file_length_in_bytes) {
        printf("There is no zip here.\n");
        exit_code = EXIT_FAILURE;
        goto EXIT;
    }

    eocd_type *eocd = NULL;
    // Пытаемся прочитать сигнатуру End Of Central Directory record.
    // Размер EOCD равен минимум 22 байтам + опциональный комментарий переменной длины.
    for (long offset = 22; offset < file_length_in_bytes; offset++) {
        fseek(f, -offset, SEEK_END);
        unsigned char buf[4];
        size_t bytes_read = fread(buf, 1, 4, f);
        if (bytes_read != 4) {
            printf("Error reading file %s.\n", argv[1]);
            exit_code = EXIT_FAILURE;
            goto EXIT;
        }

        if (is_it_eocd_signature(buf)) {
            printf("Found EOCD signature.\n");
            eocd = create_empty_eocd();
            if (eocd == NULL) {
                printf("Error: cannot allocate memory for EOCD structure.\n");
                exit_code = EXIT_FAILURE;
                goto EXIT;
            }
            eocd_init_from_file(eocd, f);
            break;
        }
    }

    if (eocd == NULL) {
        printf("There is no zip here.\n");
        exit_code = EXIT_FAILURE;
        goto EXIT;
    }

    const long central_dir_start_position_in_file = end_jpg_offset + 1 + eocd->central_directory_offset;
    uint16_t file_name_length = 0;
    file_name = malloc(sizeof(char) * 1024);
    if (file_name == NULL) {
        printf("Error: cannot allocate memory for file name.\n");
        exit_code = EXIT_FAILURE;
        goto EXIT;
    }

    char signature[4];
    int file_header_counter = 0;
    long current_position_in_file = central_dir_start_position_in_file;
    while (current_position_in_file < file_length_in_bytes && file_header_counter < eocd->total_central_directory_entries) {
        fseek(f, current_position_in_file, SEEK_SET);
        const size_t elements_read = fread(signature, ONE_BYTE, 4, f);
        if (elements_read == 0) {
            printf("Error reading file: central directory file header signature.");
            exit_code = EXIT_FAILURE;
            goto EXIT;
        }

        if (is_it_file_header_signature(signature)) {
            fseek(f, current_position_in_file + CENTRAL_DIRECTORY_FILE_HEADER_FILE_NAME_LENGTH_OFFSET, SEEK_SET);
            size_t elements_read = fread(&file_name_length, TWO_BYTES, 1, f);
            if (elements_read != 1) {
                printf("Error reading file: file name length.");
                exit_code = EXIT_FAILURE;
                goto EXIT;
            }
            fseek(f, current_position_in_file + CENTRAL_DIRECTORY_FILE_HEADER_FILE_NAME_OFFSET, SEEK_SET);
            elements_read = fread(file_name, ONE_BYTE, file_name_length, f);
            if (elements_read != file_name_length) {
                printf("Error reading file: file name.");
                exit_code = EXIT_FAILURE;
                goto EXIT;
            }
            file_name[file_name_length] = '\0';
            printf("%s\n", file_name);

            file_header_counter++;


            fseek(f, current_position_in_file + CENTRAL_DIRECTORY_FILE_HEADER_EXTRA_FIELD_LENGTH_OFFSET, SEEK_SET);
            const uint16_t extra_field_length = read_u16_le(f);
            if (extra_field_length == EXIT_FAILURE) {
                printf("Error reading file: extra field length.");
                exit_code = EXIT_FAILURE;
                goto EXIT;
            }

            fseek(f, current_position_in_file + CENTRAL_DIRECTORY_FILE_HEADER_FILE_COMMENT_LENGTH_OFFSET, SEEK_SET);
            const uint16_t file_comment_length = read_u16_le(f);
            if (file_comment_length == EXIT_FAILURE) {
                printf("Error reading file: extra field length.");
                exit_code = EXIT_FAILURE;
                goto EXIT;
            }

            current_position_in_file += CENTRAL_DIRECTORY_FILE_HEADER_BASE_LENGTH + file_name_length + extra_field_length + file_comment_length;
            continue;
        }
        current_position_in_file++;
    }

EXIT:
    free_eocd(eocd);
    free_allocated_resources();
    return exit_code;
}

bool is_it_eocd_signature(const unsigned char *buffer) {
    return memcmp(buffer, EOCD_SIGNATURE, 4) == 0;
}

eocd_type * create_empty_eocd(void) {
    eocd_type *eocd = calloc(1, sizeof (eocd_type));
    if (eocd == NULL) {
        printf("Error: cannot allocate memory for eocd\n");
    }

    return eocd;
}

void free_eocd(eocd_type *eocd) {
    if (eocd != NULL) {
        if (eocd->comment != NULL) {
            free(eocd->comment);
        }
        free(eocd);
    }
}

int eocd_init_from_file(eocd_type * const eocd, const FILE *f) {
    eocd->signature = 0x504B0506;

    // The number of this disk (containing the end of central directory record)
    size_t elements_read = fread(&eocd->disk_number, TWO_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD disk number from file.\n");
        return EXIT_FAILURE;
    }

    // Number of the disk on which the central directory starts
    elements_read = fread(&eocd->start_disk_number, TWO_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD start disk_number from file.\n");
        return EXIT_FAILURE;
    }

    // The number of central directory entries on this disk
    elements_read = fread(&eocd->number_central_directory_entries, TWO_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD number central directory entries disk_number from file.\n");
        return EXIT_FAILURE;
    }

    // Total number of entries in the central directory
    elements_read = fread(&eocd->total_central_directory_entries, TWO_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD total central directory entries file.\n");
        return EXIT_FAILURE;
    }

    // Size of the central directory in bytes
    elements_read = fread(&eocd->central_directory_size, FOUR_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD central directory size from file.\n");
        return EXIT_FAILURE;
    }

    // Offset of the start of the central directory on the disk on which the central directory starts
    elements_read = fread(&eocd->central_directory_offset, FOUR_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD central directory offset from file.\n");
        return EXIT_FAILURE;
    }

    // The length of the following comment field
    elements_read = fread(&eocd->comment_length, TWO_BYTES, 1, f);
    if (elements_read != 1) {
        printf("Error reading EOCD comment length from file.\n");
        return EXIT_FAILURE;
    }

    // Optional comment for the Zip file
    if (eocd->comment_length > 0) {
        eocd->comment = malloc(eocd->comment_length * sizeof(char) + 1);
        if (eocd->comment == NULL) {
            printf("Error: cannot allocate memory for comment.\n");
            return EXIT_FAILURE;
        }

        elements_read = fread(eocd->comment, ONE_BYTE, eocd->comment_length, f);
        if (elements_read != eocd->comment_length) {
            printf("Error reading EOCD comment from file.\n");
            return EXIT_FAILURE;
        }
        eocd->comment[eocd->comment_length] = '\0';
    }

    return EXIT_SUCCESS;
}

int64_t find_jpg_end_signature_offset(const FILE * const f) {
    fseek(f, 0, SEEK_SET);
    int64_t offset = 0;
    uint16_t two_bytes = 0;
    int byte_read;
    while ((byte_read = getc(f)) != EOF) {
        two_bytes <<= 8;
        two_bytes |= byte_read;
        if (two_bytes == JPG_END_SIGNATURE) {
            printf("End of JPG signature is found.\n");

            return offset;
        }
        offset++;
    }

    return ERROR_NO_END_JPG_SIGNATURE_FOUND;
}

void free_allocated_resources(void) {
    if (file_name) {
        free(file_name);
    }
}

int is_it_file_header_signature(char signature[4]) {
    return memcmp(signature, CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE, 4) == 0;
}

uint16_t read_u16_le(const FILE * const f) {
    unsigned char bytes[2];
    const size_t elements_read = fread(bytes, ONE_BYTE, 2, f);
    if (elements_read != 2) {
        return EXIT_FAILURE;
    }

    return (uint16_t)bytes[0] | (uint16_t)bytes[1] << 8;
}