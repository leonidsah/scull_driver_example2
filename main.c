#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFF_SIZE 100

int main()
{
    int fd;
    char ch, write_buf[BUFF_SIZE], read_buf[BUFF_SIZE], device_choice;

    printf("Выберите устройство: \n");
    printf("0 - устройство scull0\n");
    printf("1 - устройство scull1\n");
    printf("2 - устройство scull2\n");

    scanf("%c", &device_choice);

    char device_path[20];
    sprintf(device_path, "/dev/scull%c", device_choice);

    fd = open(device_path, O_RDWR);

    if (fd < 0) {
        perror("Ошибка открытия устройства");
        return -1;
    }
    printf("r - read, w - write\n");
    scanf(" %c", &ch);

    switch (ch) {
    case 'w':
        printf("Введите данные: ");
        scanf(" %[^\n]", write_buf);
        write(fd, write_buf, strlen(write_buf)); 
        break;
    case 'r':
        int bytes_read = read(fd, read_buf, sizeof(read_buf) - 1); // Читаем данные
        if (bytes_read < 0) {
            perror("Ошибка чтения из устройства");
        } else {
            read_buf[bytes_read] = '\0'; // Добавляем символ конца строки
            printf("scull: %s\n", read_buf); // Выводим прочитанные данные
        }
        break;
    }
    close(fd);
    return 0;
}