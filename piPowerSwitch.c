#include "piPowerSwitch.h"
// add to /etc/rc.local

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> // file stuff
#include <stdio.h> // printf
// #include <stdlib.h>
#include <stdbool.h> // bool

#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1

#define PIN  26 /* Header Pin 37 = GPIO 26 */
#define POUT 16 /* Header Pin 36 = GPIO 16 */

int shutdownPin = PIN; // the default is PIN but can be overwritten with commandline param
int shutdownCount = 0;
bool isVerboseMode = false;

// based on RPi GPIO code sample at http://elinux.org/RPi_GPIO_Code_Samples
#define BUFFER_MAX 3
static int GPIOExport(int pin) {
  if (isVerboseMode) {
    printf("GPIO Export %d\n\r",pin);
  }
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for writing!\n");
    return(-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return(0);
}


static int GPIOUnexport(int pin){
  //printf("GPIO UExport %d \n\r",pin);
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/unexport", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open unexport for writing!\n");
    return(-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return(0);
}

static int GPIODirection(int pin, int dir){
#define DIRECTION_MAX 35
  if (isVerboseMode) {
    printf("GPIO Direction %d\n\r",pin);
  }
  //                                       0123456
  static const char s_directions_str[]  = "in\0out";
  char path[DIRECTION_MAX];
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction\0", pin);

  if (isVerboseMode) {
    printf(path);
    printf("\r\n");
  }

  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return(-1);
  }

  // direction = 0 (input) string starts at 0: "IN" and is two characters long; else starts at position 3 and is three characters long
  if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    fprintf(stderr, "Failed to set direction!\n");
    return(-1);
  }

  close(fd);
  return(0);
}

#define VALUE_MAX 30
//******************************************************************************/
static int GPIORead(int pin) {
  // printf("GPIO Read %d\n\r",pin);
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for reading!\n");
    return(-1);
  }

  if (-1 == read(fd, value_str, 3)) {
    fprintf(stderr, "Failed to read value!\n");
    return(-1);
  }

  close(fd);

  return(atoi(value_str));
}

static int GPIOWrite(int pin, int value) {
  if (isVerboseMode) {
    printf("GPIO Write %d\n\r",pin);
  }
  static const char s_values_str[] = "01";

  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for writing!\n");
    return(-1);
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    fprintf(stderr, "Failed to write value!\n");
    return(-1);
  }

  close(fd);
  return(0);
}

bool isValidGPIO(char * str){
  int i = 0;
  bool res = true;

  if (str[0] == '\0') {
    return false; // null strings are never valid numbers as there are no digits!
  }

  int thisValue = str[0] - '0'; // convert ascii value to integer by subtracting ascii value of zero
  while (str[i] != '\0') {
    res = res && (str[i] >= '0' && str[i] <= '9');
    if (res && (i > 0)) {
      thisValue = (thisValue * 10) + (str[i] - '0');
    }
    i++;
  }
  //if (res) {printf("Number!");} else {printf("Not a number!");}
  return res;
}



int main(int argc, char *argv[])
{
  int repeat = 4;

  // we apparently need to get GPIO pion first, as getopt appears to molest args!
  if ( (argc > 1) && (isValidGPIO(argv[1])) ) {
    shutdownPin = atoi(argv[1]);
    if (isVerboseMode) {
      printf("Using alternate GPIO%d...\n\r",shutdownPin);
    }
  }
  else {
    shutdownPin = PIN;
  }
  printf("Pi Shutdown Check on GPIO %d!\r\n", shutdownPin);

  int opt;
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
      case 'v': {
         isVerboseMode = true;
         printf("Verbose mode on\n");
         break;
      }
      case 'h': {}; break;
      default:
         fprintf(stderr, "Usage: %s [-vh]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }



  /*
  * Enable GPIO pins
  */
  if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN)) {
    printf("Error 1");
    return(1);
  }

  // perhaps we need to check that exported directories exist?
  //if( access( fname, F_OK ) != -1 ) {
      // file exists
  //} else {
      // file doesn't exist
  //}

  // sleep for 10ms, time for exports to "stabilize" ??
  usleep(100000); // this is a complete HACK! without this delay, app fails every other time?? why??


  /*
  * Set GPIO directions
  */
  if (-1 == GPIODirection(POUT, OUT) || -1 == GPIODirection(PIN, IN)) {
    printf("Error 2 - Failed to set GPIO Direction \r\n");
    return(2);
  }

  do {
    /*
    * Write GPIO value
    */
    if (-1 == GPIOWrite(POUT, repeat % 2)) {
      printf("Error 3 - Failed to blink light \r\n");
      return(3);
    }

    /*
    * Read GPIO value
    */
    if (isVerboseMode) {
      printf("Found value %d in GPIO %d\n", GPIORead(PIN), PIN);
    }
    shutdownCount += GPIORead(PIN);
    usleep(500 * 1000);
  } while (repeat--);


  /*
  * Set GPIO directions
  */
  if (-1 == GPIODirection(PIN, IN) || -1 == GPIODirection(POUT, IN)) {
    printf("Error 5 - Failed to reset GPIO \r\n");
    return(5);
  }

  /*
  * Disable GPIO pins
  */
  if (-1 == GPIOUnexport(PIN) || -1 == GPIOUnexport(POUT)) {
    printf("Error 4 - Failed to Unexport GPIO");
    return(4);
  }
  else {
    if (isVerboseMode) {
      printf("Done! \r\n");
    }
  }

  if (shutdownCount >= 4) {
    printf("Shutdown %d!\n\r",shutdownCount);
    // TODO put shutdown command here, add to 
  }
  return(0);
}


