#ifndef GPSBCM4751_H
#define GPSBCM4751_H

#include <linux/ioctl.h>
#include <linux/proc_fs.h>
/* Use 100 as magic number */
#define GPS_IOC_MAGIC 100

/* Driver name */
#define GPS_DRIVER_NAME "agpsgpio"

/*gps ioctl cmd*/
#define GPS_IOCWRITEGPIO       _IOW(GPS_IOC_MAGIC, 1, char *)
#define GPS_IOCREADGPIO        _IOR(GPS_IOC_MAGIC, 2, char *)
#define GPS_IOC_MAXNR          2

#endif// GPSBCM4751_H


