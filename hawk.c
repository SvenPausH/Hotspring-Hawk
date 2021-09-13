#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// Linux headers
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
//#include <termiWin.h> // 
#include <unistd.h>  // write(), read(), close()
//#include "hotspring.h"

//
char daten[200]; // zwichenspeichern des Buffers
char *ptrdaten = daten;
int datenpos = 0;
int datenlen = 0;
int buflen = 0; // gelesene Daten von Serial
int bufpos = 0;
char null[15] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
unsigned int last_kb_crc = 0;
unsigned int last_mb_crc = 0;
unsigned char last_display[15] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
int datasize;
int opt = 0;
/* ARGV DEFAULTS */
char serial_path2[30] ="/dev/ttyUSB0";
char serial_path1[30] ="/dev/ttyUSB1";
int lock_set_key = 0;  // Send the Set Key to Mainboard 0=NO 1=YES
int supress_dup = 1;  // supress duplicate Output 0=NO 1=YES
int serial2_aktiv=1;
/* END ARGV DEFAULTS */
char device_mb[4] = "XSM"; // 0x58, 0x53, 0x4d  Mainboard
char device_kb[4] = "XMS"; // 0x58, 0x4d, 0x53  Keyboard

int const_count = 11; // Count Data
// crc, text, payload rawdata, rawdata length
char *const_text[][4] = {
    {"   ", " unknown  ", "", ""},
    {"360", "KEYB UP   ", "\x58\x4d\x53\x00\x03\x6b\x00\x02\x01\x68", "10"},
    {"366", "KEYB DOWN ", "\x58\x4d\x53\x00\x03\x6b\x00\x08\x01\x6e", "10"},
    {"374", "KEYB JETS ", "\x58\x4d\x53\x00\x03\x6b\x00\x10\x01\x76", "10"},
    {"362", "KEYB SET  ", "\x58\x4d\x53\x00\x03\x6b\x00\x04\x01\x6a", "10"},
    {"134", "KEYB LIGHT", "\x58\x4d\x53\x00\x03\x6b\x00\x20\x01\x86", "10"}, 
    // richtig wäre hier 390 aber die Tastatur sendet FF FF FF 86. puefen ob ein defekt vorliegt!!!
    {"358", "KEYB NONE ", "\x58\x4d\x53\x00\x03\x6b\x00\x00\x01\x66", "10"}, // all Keyboard combinations
    // now all Mainbaord Payloads
    {"358", "SET+RY ON ", "\x58\x53\x4d\x00\x03\x1a\x00\x51\x01\x66", "10"},
    {"278", "SET+RY OFF", "\x58\x53\x4d\x00\x03\x1a\x00\x01\x01\x16", "10"},
    {"269", "   init   ", "\x58\x53\x4d\x00\x01\x14\x01\x0d", "8"},
    {"277", " RY OFF   ", "\x58\x53\x4d\x00\x03\x1a\x00\x00\x01\x15", "10"},
    {"357", " RY ON    ", "\x58\x53\x4d\x00\x03\x1a\x00\x50\x01\x65", "10"},
};

struct Struct_Data
{
  char device[3];   // device_strg oder device_keyb
  int len;          // Playload Datenlaenge
  char payload[20]; // Daten
  unsigned int crc; // Pruefsumme
  unsigned int pos; // Aktuelle Stringposition
  char data[30];    // kompletter Datensatz
                    //unsigned int last_kb_crc; // letzte crc summe Keyboard
                    //unsigned int last_mb_crc; // letzte crc summe Mainboard
};
/*struct Struct_Const
{
    char text[20];
    unsigned int crc;
};
*/

int check_crc(struct Struct_Data crc);
struct Struct_Data find_payload (char *daten, int buflen);
void print_data(struct Struct_Data mydata, int dup );
void print_ascii ();
void print_buf(char *buffer, int bytes);

//
int main(int argc, char *argv[])
{
  while ((opt = getopt(argc, argv, ":s:S:d:l:?")) != -1)
  {
    printf("\n switch opt %c", opt);
    switch (opt)
    {
    case 's':
      strcpy(serial_path1, optarg);
      printf("\nInput option value=%s ", serial_path1);
      break;
    case 'S':
      strcpy(serial_path2, optarg);
      printf("\nInput option value=%s", serial_path2);
      break;
    case 'd':
      supress_dup = atoi(optarg);
      printf("\nInput option value=%d", supress_dup);
      break;
    case 'l':
      lock_set_key = atoi(optarg);
      printf("\nInput option value=%d", lock_set_key);
      break;
    case '?':
      printf("\nDatalogger for Whirlpool Hotspring HAWK by Sven Pastorik\n");
      printf("\n\n call hawk -s [tty1] -S [tty2/none] -d [yes=1/no=0] -l [yes=1/no=0]");
      printf("\n\n defaults call hawk -s %s -S %s -d %d -l %d",serial_path1, serial_path2, supress_dup, lock_set_key);
      printf("\n s path of Serial e.g. /dev/ttyUSB0");
      printf("\n S if none only Serial 1 is use.");
      printf("\n d suppress double Output");
      printf("\n l lock the Set Button only make sense with s2\n");
      return 0; // success
      break;
      
    }
  }  
  printf("\n\nhawk running with -s %s -S %s -d %d -l %d\n",serial_path1, serial_path2, supress_dup, lock_set_key);
    /* Init Serial */
    // Open the serial port. Change device path as needed (currently set to an standard FTDI USB-UART cable type device)
    // int serial_port = open("/dev/ttyUSB0", O_RDWR);
    // int serial_port2 = open("/dev/ttyUSB1", O_RDWR);
    int serial_port = open(serial_path1, O_RDWR);
    int serial_port2 = open(serial_path2, O_RDWR);

    // Create new termios struc, we call it 'tty' for convention
    struct termios tty;

    // Allocate memory for read buffer, set size according to your needs
    char read_buf[128]; // for readbuffer
    int num_bytes = 0;  // Number of Bytes read

    // Read in existing settings, and handle any error
    if (tcgetattr(serial_port, &tty) != 0)
    {
      printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
      return 1;
    }
    if (tcgetattr(serial_port2, &tty) != 0)
    {
      printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
      printf("Serialport2 deaktivated");
      serial2_aktiv = 0;
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE;  // Clear all bits that set the data size
    tty.c_cflag |= CS8;     // 8 bits per byte (most common)
    //tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                                                        // Disable echo
    tty.c_lflag &= ~ECHOE;                                                       // Disable erasure
    tty.c_lflag &= ~ECHONL;                                                      // Disable new-line echo
    tty.c_lflag &= ~ISIG;                                                        // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);                                      // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST;                                                       // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR;                                                       // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
    // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)
    tty.c_cc[VTIME] = 1; // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 1;
    // Read bytes. The behaviour of read() (e.g. does it block?,
    // how long does it block for?) depends on the configuration
    // settings above, specifically VMIN and VTIME
    // Set in/out baud rate to be 9600
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0)
    {
      printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return 1;
    }
    // Save tty settings, also checking for error
    if (serial2_aktiv == 1)
    {
      if (tcsetattr(serial_port2, TCSANOW, &tty) != 0)
      {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        return 1;
      }
    }
    /* Init Serial END */

    struct Struct_Data mydata;
    mydata.crc = 0;

    write(serial_port, const_text[9][2], 8);
    if (serial2_aktiv == 1)
    {
      write(serial_port2, "\x58\x4d\x53\x00\x06\x54\x48\x41\x57\x4b\x00\x02\x7d", 13);
    }
    //################## Start LOOP ####################
    //
   //int c = 0;
    // unbuffered stdin
    while (1)
    {
      mydata.pos = 0;
      mydata.len = 0;

      num_bytes = read(serial_port, &read_buf, sizeof(read_buf));
      if (num_bytes < 0)
      {
        printf("Error reading: %s", strerror(errno));
      }
      else
      {
        //printf("\nRead %i bytes. Received message: %s HEX-> ", num_bytes, read_buf);
        // restdaten nach vorn!!
        // code *** letze datenpos ist?
        //printf("#datenpos %d datenlen %d mydata.len %d\n",datenpos, datenlen, mydata.len);
        //print_buf(read_buf, num_bytes);
        if (datenpos < datenlen)
        {
          // Wir haben noch Daten übrig also sichern
          //printf("\nAchtung Restdaten datenpos: %d datenlen:%d Restdaten ",datenpos, datenlen);
          //for (int r=datenpos; r < datenlen;r++){
          //   printf("0x%02X ", daten[r]);
          //}
          memcpy(daten, daten + datenpos, datenlen - datenpos);
        }

        memcpy(daten + (datenlen - datenpos), read_buf, num_bytes); // Buffer nach daten sichern
        //memcpy(daten, read_buf, num_bytes); // Buffer nach daten sichern
        datenlen = num_bytes + (datenlen - datenpos);
        //datenlen = num_bytes;
        datenpos = 0;

        while (mydata.len >= 0)
        {
          mydata = find_payload(daten, datenlen);

          if (mydata.len >= 0)
          {
            print_data(mydata, supress_dup); // unterdruecke Duplikate 0=nein / 1=ja
            datasize = mydata.len + 7;
            if (serial2_aktiv == 1)
            {
              if (lock_set_key == 1 & mydata.device[1] == 'M' & mydata.crc == 362)
              {
                printf("KEY SET DROP!!!!\n");
                write(serial_port2, const_text[6][2], 10);
              }
              else
              {
                write(serial_port2, mydata.data, datasize);
              }
            }
          }
        }
      }      
    }

    close(serial_port);
    if (serial2_aktiv == 1)
    {
      close(serial_port2);
    }
    printf("\n Ende \n");
    return 0; // success
  }

/* #################### *** Funktionen *** ####################### */

int check_crc(struct Struct_Data crc)
{
    unsigned int ret = 0, crc_sum = 0;
    //crc_sum = crc.device + crc.payload + crc.len;
    printf("crc_summe :%u crc:%u", crc_sum, crc.crc);
    return ret;
}

struct Struct_Data find_payload(char *text, int buflen)
{
  struct Struct_Data md;
  //buflen--;
  //printf("\nChar groesse %d\n", buflen);
  /*sucht in einem char array den naechsten Datensatz*/
  // printf("datenpos %d buflen %d",datenpos, buflen);
      // printf("BUFFER RAW DATA->"); 
      // for ( int i = datenpos; i < buflen; i++ ) {
        // printf("%02X ",text[i]);
      // }  
      // printf("\n");
  for (int i = datenpos; i < buflen; i++)
  {
    int num1;
    int num2;
    //printf("datenpos : %d ", datenpos);
    int rest = buflen - datenpos;
    if ((text[i] == 'X' & text[i + 1] == 'M' & text[i + 2] == 'S') | (text[i] == 'X' & text[i + 1] == 'S' & text[i + 2] == 'M'))
    {
      //printf("\n Anfang gefunden an stelle %d Zeichenkette %c%c%c\n", i,text[i],text[i+1],text[i+2]);
      //printf("buflen:%d datenpos: %d rest:%d",buflen, datenpos, rest);
      if ((buflen - datenpos  ) < (3 + 2 + ((text[i + 3] * 256) + text[i + 4]) + 2))
      {
        //printf("abbruch nicht genug daten");
        md.len = -1;
        return md;
      }
      memcpy(md.device, text + i, 3);
      md.len = (text[i + 3] * 256) + text[i + 4];
      memcpy(md.payload, text + i + 5, md.len);
      md.crc = (text[i + 4 + md.len + 1] * 256) + text[i + 4 + md.len + 2];
      md.pos = i + 4 + md.len + 3;
      datenpos = i + 4 + md.len + 3; // merken der wo wir stehen
      memcpy(md.data, text + i, md.len + 7);
      if (md.payload[0]==5){
        memcpy(last_display, text + i + 5, md.len);
      }
      // printf("daten %s", md.device);
      // printf("laenge %d ", md.len);
      // printf("crc %d crc1 %02x crc2 %02x\n ", md.crc, text[i + 4 + md.len + 1], text[i + 4 + md.len + 2]);
      // printf("RAW DATA->"); 
      // for ( int x = 0 ; x < (3 + 2 + ((text[i + 3] * 256) + text[i + 4]) + 2),x++ ){
      // for ( int x = i ; x < 10;x++ ) {
        // printf("%02X ",text[x]);
      // }
      //strcpy(tmpchar,text[wert]);
      //tmpchar = {text[i]};
      // printf("\n");
      return md;
    }
    //printf("%02X ", text[i]);
  }
  // printf(" buffer null\”");
  md.len = -1;
  return md;
}
/* ##############  PRINT DATA    ############### */
void print_data(struct Struct_Data mydata, int print_dup)
{
  //printf("## device %c%c%c dup:%d", mydata.device[0], mydata.device[1], mydata.device[2], print_dup);
  //char payload [15] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
  //memcpy(payload, mydata.payload, mydata.len-1);

  if (mydata.device[2] == 'M')
  {
    if (last_mb_crc == mydata.crc & print_dup == 1)
      return;
    printf("Mainboard last_crc:%6d cur_crc:%6d", last_mb_crc, mydata.crc);
    last_mb_crc = mydata.crc;
  }
  else
  {
    if (last_kb_crc == mydata.crc & print_dup == 1)
      return;
    printf("Keyboard  last_crc:%6d cur_crc:%6d", last_kb_crc, mydata.crc);
    last_kb_crc = mydata.crc;
  }

  printf(" %c%c%c Databytes:%d  Payload: ", mydata.device[0], mydata.device[1], mydata.device[2], mydata.len);
  for (int i = 0; i <= mydata.len - 1; i++)
  {
    printf("%02X", mydata.payload[i]);
  }
  //printf(" CRC %ld",mydata.crc);
  /* KEYBOARD MAINBOARD TEXT DATA */
  for (int i = 1; i <= const_count; i++)
  {
    if (mydata.device[2] == 'M' & i < 7)
    {
      continue;
    }
    if (mydata.crc == atol(const_text[i][0]))
    {
      printf(" %s", const_text[i][1]);
      break;
    }
    if (i == const_count)
    {
      printf(" %s", const_text[0][1]);
    }
  }

  printf(" ASCII:");
  for (int i = 0; i <= mydata.len - 1; i++)
  {
    if (mydata.payload[i] == 0)
    {
      printf("°");
    }
    else if (
        (mydata.payload[i] == 13) ||
        (mydata.payload[i] == 10) ||
        (mydata.payload[i] == 11) ||
        (mydata.payload[i] == 12) ||
        (mydata.payload[i] == 16) ||
        (mydata.payload[i] == 26) ||
        (mydata.payload[i] == 28))
    {
      /* code */
      printf("%c", 94);
    }
    else
    {
      /* code */
      printf("%c", mydata.payload[i]);
    }
  }
  printf(" Display: %s", last_display);

  printf("  rawdata: ");
  for (int i = 0; i < datasize; i++)
  {
    printf("%02x", mydata.data[i]);
  }

  printf("\n");
}
/* ###### ##### */
void print_buf(char *buffer, int bytes)
{
  printf("\nRead %i bytes. HEX-> ", bytes);
  for (int wert = 0; wert <= bytes - 1; wert++)
  {
    printf("0x%02X ", buffer[wert]);
  }
  printf("\n");
}
/* ###### ##### */
void  print_ascii ()
{
    printf("\njetzt hier\n");
    
    for (int i = 1; i <= 256; i++)
    {
        printf("Ascii id:%d  Zeichen:%c %c\n",i,i,94);
    }
}
