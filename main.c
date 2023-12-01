#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFF_SIZE 100
#define DEV0 "/dev/scull0"

int main()
{
    int fd;
    char ch, write_buf[BUFF_SIZE], read_buf[BUFF_SIZE];

    printf("Устройство %s: \n", DEV0);

    fd = open(DEV0, O_RDWR);

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