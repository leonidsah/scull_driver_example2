#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define BUFF_SIZE 100
#define DEV0 "/dev/scull0"
#define SCULL_IOCTL1 0x7701

int main()
{
    int fd;
    char ch, write_buf[BUFF_SIZE], read_buf[BUFF_SIZE];
    int offset;

    printf("Устройство %s: \n", DEV0);

    fd = open(DEV0, O_RDWR);

    if (fd < 0)
    {
        perror("Ошибка открытия устройства");
        return -1;
    }
choice:
    printf("r - read, w - write, o - offset\n");
    scanf(" %c", &ch);
    switch (ch)
    {
    case 'w':
        printf("Введите данные: ");
        scanf(" %[^\n]", write_buf);
        write(fd, write_buf, strlen(write_buf));
        goto choice;
        break;
    case 'r':
        int bytes_read = read(fd, read_buf, sizeof(read_buf) - 1); // Читаем данные
        if (bytes_read < 0)
        {
            perror("Ошибка чтения из устройства");
        }
        else
        {
            read_buf[bytes_read] = '\0';     // Добавляем символ конца строки
            printf("scull: %s :: bytes_read: %i\n", read_buf, bytes_read); // Выводим прочитанные данные
            goto choice;
        }
        break;
    case 'o':
        printf("Введите смещение для чтения: ");
        scanf("%d", &offset);
        ioctl(fd, SCULL_IOCTL1, offset);
        goto choice;
        break;
    }
    close(fd);
    return 0;
}