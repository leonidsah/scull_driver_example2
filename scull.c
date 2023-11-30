#include <linux/module.h> 	
#include <linux/init.h> 	
#include <linux/fs.h> 		
#include <linux/cdev.h> 	
#include <linux/slab.h> 	
#include <asm/uaccess.h>
#include <linux/wait.h>

MODULE_AUTHOR("анютик");
MODULE_LICENSE("GPL");

int scull_major = 0;		
int scull_minor = 0;		
int scull_nr_devs = 3;		
int scull_quantum = 4;	
int scull_qset = 15;	

// Новые переменные для условий блокировки
DECLARE_WAIT_QUEUE_HEAD(read_queue);
DECLARE_WAIT_QUEUE_HEAD(write_queue);
DEFINE_MUTEX(scull_mutex);

struct scull_qset {
	void **data;			
	struct scull_qset *next; 	
};

struct scull_dev {
  struct scull_qset *data; 
  int quantum; 
  int qset; 
  unsigned long size; 
  unsigned int access_key; 
  struct semaphore sem; 
  struct cdev cdev; 
  loff_t position;

};

struct scull_dev *scull_device;
int scull_trim(struct scull_dev *dev);
static void scull_setup_cdev(struct scull_dev *dev, int index);
void scull_cleanup_module(void);
static int scull_init_module(void);
int scull_open(struct inode *inode, struct file *flip);
int scull_release(struct inode *inode, struct file *flip);
ssize_t scull_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos);
struct scull_qset *scull_follow(struct scull_dev *dev, int n);

struct file_operations scull_fops = {		
	.owner = THIS_MODULE,			
	.read = scull_read,
	.write = scull_write,
	.open = scull_open,
	.release = scull_release,
};

int scull_trim(struct scull_dev *dev) {
	struct scull_qset *next, *dptr;
	int qset = dev->qset; 
	int i;

	for (dptr = dev->data; dptr; dptr = next) { 
		if (dptr->data) {
			for (i = 0; i < qset; i++)
				kfree(dptr->data[i]);

			kfree(dptr->data);
			dptr->data = NULL;
		}

		next = dptr->next;
		kfree(dptr);
	}

	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;

	return 0;
}

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, scull_minor + index);
	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err)
	printk(KERN_NOTICE "Ошибка %d добавления scull%d", err, index);
}

void scull_cleanup_module(void) {
	int i;
	dev_t devno = MKDEV(scull_major, scull_minor);

	if (scull_device) {
		for (i = 0; i < scull_nr_devs; i++) {
			scull_trim(scull_device + i);		
			cdev_del(&scull_device[i].cdev);	
		}
		
		kfree(scull_device);
	}

	unregister_chrdev_region(devno, scull_nr_devs); 
	printk(KERN_INFO "scull: выход\n");
}

static int scull_init_module(void) {
  int rv, i;
  dev_t dev;

  rv = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");  
  if (rv) {
    printk(KERN_WARNING "scull: не получен мажор %d\n", scull_major);
    return rv;
  }

  scull_major = MAJOR(dev);

  scull_device = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);  
  if (!scull_device) {
    rv = -ENOMEM;
    goto fail;
  }

  memset(scull_device, 0, scull_nr_devs * sizeof(struct scull_dev));    

  for (i = 0; i < scull_nr_devs; i++) {            
    scull_device[i].quantum = scull_quantum;
    scull_device[i].qset = scull_qset;
    sema_init(&scull_device[i].sem, 1);
    scull_setup_cdev(&scull_device[i], i);          
    scull_device[i].position = 0; // устанавливаем начальную позицию для каждого устройства

  dev = MKDEV(scull_major, scull_minor + i);  
  printk(KERN_INFO "scull: мажор = %d минор = %d\n", scull_major, i);
  }

  return 0;

fail:
  scull_cleanup_module();
  return rv;
}

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;

	/* установить в конец устройства, если открыто для записи */
	if (filp->f_mode & FMODE_WRITE) {
		filp->f_pos = dev->size; 	/* открыть для записи, перейти в конец */
	}

	printk(KERN_INFO "scull: устройство открыто\n");
	return 0;
}

int scull_release(struct inode *inode, struct file *flip) {
	printk(KERN_INFO "scull: устройство освобождено\n");
	return 0;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    ssize_t retval = 0;
    unsigned long max_siz = scull_quantum * scull_qset;
    bool flag = 0;
    loff_t pos; 
    *f_pos = 0;
    pos = *f_pos;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    while (dev->size <= 0) {
        printk("Ого, в буфере пусто");
        if (filp->f_flags & O_NONBLOCK) {
            up(&dev->sem);
            return -EAGAIN;
        }
        
        /* Освобождаем блокировку и ждем изменения размера буфера */
        up(&dev->sem);
        wait_event_interruptible(read_queue, dev->size > 0);
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }

    if (dev->size >= max_siz) {
        flag = 1;
    }

    /* Определяем количество данных для чтения */
    if (*f_pos >= dev->size) {
        goto out;
    }
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    /* Находим списковый объект, индекс qset, и смещение в кванте */
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum; q_pos = rest % quantum;

    /* Следуем за списком до нужной позиции */
    dptr = scull_follow(dev, item);

    if (dptr == NULL || !dptr->data || !dptr->data[s_pos])
        goto out; /* не заполнять пустые пространства */

    /* Читаем только до конца этого кванта */
    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;

    /* Если буфер был полностью заполнен перед чтением, очищаем его */
    if (flag) {
        printk("Очищаем буфер");
        retval = count;
        count = 0;
        *f_pos = 0;
        dev->size = 0;
        scull_trim(dev);
    } else {
        retval = count;
    }

    out:
    up(&dev->sem);
    wake_up_interruptible(&write_queue); // разбудить процессы, ожидающие записи
    return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    struct scull_qset *dptr;
    int quantum = dev->quantum, qset = dev->qset;
    int itemsize = quantum * qset;
    int item, s_pos, q_pos, rest;
    int nbytes;
    ssize_t retval = -ENOMEM; /* значение используется в инструкции "goto out" */
    unsigned long max_size = scull_quantum * scull_qset;

    /* Определяем текущую позицию для записи, проверяем переполнение */
    if (dev->size + count > max_size) {
        count = max_size - dev->size;
    }

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    
    while (dev->size >= max_size) {
        printk("Буфер заполнен / переполнен");
        if (filp->f_flags & O_NONBLOCK) {
            up(&dev->sem);
            return -EAGAIN; // если неблокирующий режим
        }
        /* Освобождаем блокировку и ждем изменения размера буфера */
        up(&dev->sem);
        wait_event_interruptible(write_queue, dev->size < max_size);
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }

    /* Находим списковый объект, индекс qset, и смещение в кванте */
    item = (long)*f_pos / itemsize;
    rest = (long)*f_pos % itemsize;
    s_pos = rest / quantum;
    q_pos = rest % quantum;

    // Переходим к списковому объекту
    dptr = scull_follow(dev, item);
    // Проверяем что выделена память под кванты в этом списковом объекте
    if (!dptr->data) {
        dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dptr->data)
            goto out;
        memset(dptr->data, 0, qset * sizeof(char *));
    }

    if (!dptr->data[s_pos]) {
        dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dptr->data[s_pos])
            goto out;
    }

    /* копирование данных из пользовательского буфера */
    if (count < quantum - q_pos)
        nbytes = count;
    else    /* избегаем попытки записать в следующий квант */
        nbytes = quantum - q_pos;

    if (copy_from_user(dptr->data[s_pos] + q_pos, buf, nbytes)) {
        retval = -EFAULT; /* ошибочный адрес - вернуть ошибку */
        goto out;
    }
    *f_pos += nbytes; /* обновление позиции файла */

    if (dev->size < *f_pos)
        dev->size = *f_pos;

    retval = nbytes;

    out:
    up(&dev->sem);
    wake_up_interruptible(&read_queue); // разбудить процессы, ожидающие чтения
    return retval;
}


struct scull_qset *scull_follow(struct scull_dev *dev, int n) {
    struct scull_qset *qs = dev->data;

    /* Сначала проверяем, выделяется ли память под данные scull_qset и выделяем, если нет */
    if (!qs) {
        qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
        if (qs == NULL)
            return NULL;  
        memset(qs, 0, sizeof(struct scull_qset));
    }

    /* Далее следуем (или создаем при необходимости) по списку */
    while(n--) {
        if (!qs->next) {
            qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
            if (qs->next == NULL)
                return NULL;
            memset(qs->next, 0, sizeof(struct scull_qset));
        }
        qs = qs->next;
    }

    /* В данной точке мы достигли требуемой позиции списка. */
    /* Теперь нам нужно убедиться, что есть память под data и data[s_pos] */
    if (!qs->data) {
        qs->data = kmalloc(dev->qset * sizeof(void *), GFP_KERNEL);
        if (qs->data == NULL)
            return NULL;
        memset(qs->data, 0, dev->qset * sizeof(void *));
    }

    /* Проверяем первый квант данных */
    if (!qs->data[0]) {
        qs->data[0] = kmalloc(dev->quantum, GFP_KERNEL);
        if (qs->data[0] == NULL)
            return NULL;
    }

    return qs;
}

module_init(scull_init_module);		
module_exit(scull_cleanup_module);	