//  ===========================================================================
//  Driver for Hokuyo URG-04LX-UG01 laser scanner.
//  ===========================================================================
/*
    Copyright 2017 Darren Faulke <darren@alidaf.co.uk>
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
//  ===========================================================================

#include "urg.h"
#include <unistd.h>	    // UNIX standard function definitions.
#include <stdio.h>	    // Standard Input/Output definitions.
#include <stdlib.h>
#include <string.h>	    // String function definitions.
#include <stdint.h>	    // Standard type definitions.
#include <fcntl.h>	    // File control definitions.
#include <stdbool.h>	// Boolean definitions.
#include <termios.h>	// POSIX terminal control definitions.

//  Commands ------------------------------------------------------------------

//  ===========================================================================
//  Clears serial port.
//  ===========================================================================
void serial_flush(serial_t *serial)
{
    tcdrain(serial->fd);
    tcflush(serial->fd, TCIOFLUSH);
}

//  ===========================================================================
//  Sets serial baud rate.
//  ===========================================================================
int serial_set_baud(serial_t *serial, long baud)
{
    long baud_val;

    switch (baud)
    {
    case 4800:
        baud_val = B4800;
        break;
    case 9600:
        baud_val = B9600;
        break;
    case 19200:
        baud_val = B19200;
        break;
    case 38400:
        baud_val = B38400;
        break;
    case 57600:
        baud_val = B57600;
        break;
    case 115200:
        baud_val = B115200;
        break;
    default:
        return -1;
    }

    cfsetospeed(&serial->settings, baud_val);
    cfsetispeed(&serial->settings, baud_val);

    tcsetattr(serial->fd, TCSADRAIN, &serial->settings);
    serial_flush(serial);

    return (0);
}

//  ===========================================================================
//  Initialises serial port.
//  ===========================================================================
int serial_open(serial_t *serial, const char *device, long baud)
{

    int flags = 0;
    int ret = 0;

    serial->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serial->fd < 0)
    {
        perror("USB open");
        return (-1);
    }

    flags = fcntl(serial->fd, F_GETFL, 0);
    fcntl(serial->fd, F_SETFL, flags & ~O_NONBLOCK);

    // Get current port options.
    tcgetattr(serial->fd, &serial->settings);

    // Set port options (lifted from the urg library source code).
    serial->settings.c_iflag = 0;
    serial->settings.c_oflag = 0;

    serial->settings.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
    serial->settings.c_cflag |= CS8 | CREAD | CLOCAL;
    serial->settings.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);

    serial->settings.c_cc[VMIN] = 0;
    serial->settings.c_cc[VTIME] = 0;

    ret = serial_set_baud(serial, baud);

    return ret;
}

//  ===========================================================================
//  Closes serial port.
//  ===========================================================================
int serial_close(serial_t *serial)
{
    return close(serial->fd);
}

//  ===========================================================================
//  Writes command to port.
//  ===========================================================================
int write_command(serial_t *serial, const char *data, int size)
{
    return write(serial->fd, data, size);
}

//  ===========================================================================
//  Returns data sum.
//  ===========================================================================
char get_data_sum(char *data)
{
    uint8_t  i;
    uint16_t val;
    uint8_t  sum;
    uint8_t  len;

    val = 0;
    len = strlen(data);

    for (i = 0; i < len; i++)
    {
        val += data[i];
    }

    sum = (val & 0x3f) + 0x30;

    return (sum);
}

//  ===========================================================================
//  Returns data block from sensor.
//  ===========================================================================
int get_data(serial_t *serial, char data[DATA_BLOCK_LEN])
{
    char c;
    int  i;

    i = 0;

    while (read(serial->fd, &c, 1) > 0 && (c != STRING_LF))
    {
        data[i++] = c;
    }

    return (i-1);

}

//  ===========================================================================
//  Returns version information in version_t.
//  ===========================================================================
int get_version(sensor_t *sensor, char string[16])
{
    char cmd[DATA_CMD_LEN + DATA_STRING_LEN];
    int  err;

    strcpy(cmd, CMD_GET_VERSION);
//    strcat(buf, string);  // Don't know why this kills the return data!
    strcat(cmd, LF);

    printf("Command = %s\n", CMD_GET_VERSION);

    serial_flush(&sensor->serial);
    err = write(sensor->serial.fd, cmd, strlen(cmd));

    if (err < 0)
    {
        printf("Error writing command.\n");
        perror("Write to port");
        return (err);
    }

    usleep( 100000 );   // Definitely needs this!
    /*
        It would be better to have a routine that waits
        for the port to be ready.
    */

    err = get_data(&sensor->serial, sensor->version.command);
    if (err < 0 ) return (err);
    err = get_data(&sensor->serial, sensor->version.string);
    if (err < 0 ) return (err);
    err = get_data(&sensor->serial, sensor->version.vendor);
    if (err < 0 ) return (err);
    err = get_data(&sensor->serial, sensor->version.product);
    if (err < 0 ) return (err);
    err = get_data(&sensor->serial, sensor->version.firmware);
    if (err < 0 ) return (err);
    err = get_data(&sensor->serial, sensor->version.protocol);
    if (err < 0 ) return (err);
    err = get_data(&sensor->serial, sensor->version.serial);

    return (err);
}

//  ===========================================================================
//  Main routine.
//  ===========================================================================
int main(void)
{
    int     err;

    sensor_t sensor;

    const char *device = "/dev/ttyACM0";
    long baud = 115200;

    char data[64];

    err = serial_open(&sensor.serial, device, baud);
    if (err < 0)
    {
        printf("Error initialising port.\n");
    }

    err = get_version(&sensor, "Jaguar");
    printf("Get version = %d\n", err);
    if (err >= 0)
    {
        printf("Sensor Information.\n\n");
        printf("\tVendor   : %s\n", sensor.version.vendor);
        printf("\tProduct  : %s\n", sensor.version.product);
        printf("\tFirmware : %s\n", sensor.version.firmware);
        printf("\tProtocol : %s\n", sensor.version.protocol);
        printf("\tSerial   : %s\n", sensor.version.serial);
        printf("\n");
    }

    err = serial_close(&sensor.serial);
    if (err < 0)
    {
        printf("Error closing port.\n");
    }

    return (0);
}

