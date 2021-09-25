#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// Linux headers
#include <fcntl.h>   // Contains file controls like O_RDWR
#include <errno.h>   // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
//#include <termiWin.h> //
#include <unistd.h> // write(), read(), close()
//#include "hotspring.h"
#include <curses.h>
/*
Compile 

cd ~/Dokumente/Sync/Dokumente_sync_sven/Programmierung/Whirlpool/hotspring-hawk
gcc hawk_curses.c -o hawk_curses -lcurses
./hawk_curses

debug bei 428

*/

// Curses

char c;
WINDOW *my_win_display;
WINDOW *my_win_keyboard;
WINDOW *my_win_config;
WINDOW *my_win_log;
int startx, starty, width, height, logrow=1, max_logrow=25, max_logcol=160;

WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);
void init_display();
void print_display(WINDOW *local_win);
void set_reverse(WINDOW *local_win, int display_item);
void print_config(WINDOW *local_win);
void init_ncurses();
void end_ncurses();
void set_config(char key);
void print_keyboard(WINDOW *local_win, char key);

    //
char daten[200]; // zwichenspeichern des Buffers
char *ptrdaten = daten;
int datenpos = 0;
int datenlen = 0;
int buflen = 0; // gelesene Daten von Serial
int bufpos = 0;
char null[16] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
unsigned int last_kb_crc = 0;
unsigned int last_mb_crc = 0;
unsigned char last_display[16] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
int datasize;
int opt = 0;
int keyid = 6; // None Key
char last_c=1;
/* ARGV DEFAULTS */
char serial_path2[30] = "/dev/ttyUSB0";
char serial_path1[30] = "/dev/ttyUSB1";
int lock_set_key = 0; // Send the Set Key to Mainboard 0=NO 1=YES
int supress_dup = 1;  // supress duplicate Output 0=NO 1=YES
int supress_log = 0;  // supress Logging Output 0=NO 1=YES
int serial2_aktiv = 1;
/* END ARGV DEFAULTS */
char device_mb[4] = "XSM"; // 0x58, 0x53, 0x4d  Mainboard
char device_kb[4] = "XMS"; // 0x58, 0x4d, 0x53  Keyboard

int const_count = 11; // Count Data
// crc, text, payload rawdata, rawdata length
char *const_text[][4] = {
    {"   ", " unknown  ", "", ""},
    // all Keyboard combinations
    {"360", "KEYB UP   ", "\x58\x4d\x53\x00\x03\x6b\x00\x02\x01\x68", "10"}, // Keyid=1
    {"366", "KEYB DOWN ", "\x58\x4d\x53\x00\x03\x6b\x00\x08\x01\x6e", "10"}, // Keyid=2
    {"374", "KEYB JETS ", "\x58\x4d\x53\x00\x03\x6b\x00\x10\x01\x76", "10"}, // Keyid=3
    {"362", "KEYB SET  ", "\x58\x4d\x53\x00\x03\x6b\x00\x04\x01\x6a", "10"}, // Keyid=4
    {"390", "KEYB LIGHT", "\x58\x4d\x53\x00\x03\x6b\x00\x20\x01\x86", "10"}, // Keyid=5
    // richtig wäre hier 390 aber die Tastatur sendet FF FF FF 86. puefen ob ein defekt vorliegt!!! 134
    {"358", "KEYB NONE ", "\x58\x4d\x53\x00\x03\x6b\x00\x00\x01\x66", "10"}, // Keyid=6
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
struct Struct_Display
{
    int SET; // all integer Values 1 or 0
    int LUNA;
    int UPPER_DOT;
    int READY;
    int F1;
    int F2;
    char TEXT[10]; // Displaytext
    int JETS;
    int LOWER_DOT;
    int SUN;
    int CLEAN;
};
int serial_port;
int serial_port2;
//Create new termios struc, we call it 'tty' for convention
struct termios tty;
// Allocate memory for read buffer, set size according to your needs
char read_buf[128]; // for readbuffer
int num_bytes = 0;  // Number of Bytes read
struct Struct_Data mydata;

struct Struct_Display mydisplay;

int check_crc(struct Struct_Data crc);
struct Struct_Data find_payload(char *daten, int buflen);
void print_data(struct Struct_Data mydata, int print_dup, int print_off);
void print_ascii();
void print_buf(char *buffer, int bytes);
void init_args(int argc, char *argv[]);
void init_serial();
void init_display();
void set_display(int crc);
void print_screen();
void end_serial();
void read_data(int keyid);
void set_keyid(char key);

//********************************* M  A  I  N *********************************
int main(int argc, char **argv[])
{
    init_args(argc, *argv);

    init_ncurses();
    //height, width, starty, startx
    my_win_keyboard = create_newwin(7, 20, 1, 0);
    print_keyboard(my_win_keyboard, c);
    my_win_display = create_newwin(7, 24, 1, 21);
    init_display();
    print_display(my_win_display);

    my_win_config = create_newwin(7, 60, 1, 25+21);
    my_win_log = create_newwin(max_logrow, max_logcol, 9, 0);
    wborder(my_win_log, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    mvwprintw(my_win_log,0,0 , "");
    scrollok(my_win_log,  TRUE);
    wrefresh(my_win_log);
    //wprintw(my_win_log,  "test");
    //wrefresh(my_win_log);
    print_config(my_win_config);
    getch();

    
    init_serial();
    //init_display();
    mydata.crc = 0;
//    mvwprintw(my_win_log, 2, 1, "test");
//    wrefresh(my_win_log);
    //################## Start LOOP ####################
    while (1)
    {
        read_data(keyid);
        timeout(0);
        c=getch();
        if (c=='q') {
            break;
        }
        set_keyid(c);

        if (c=='d' || c=='s' || c=='f'){
          set_config(c);
          print_config(my_win_config);
        } 
        if (c=='h' || c=='n' || c=='u' || c=='j'|| c=='m' ||keyid==6) {
          print_keyboard(my_win_keyboard,c);
        }
    }
    end_serial();
    end_ncurses();
    return 0; // success
}
   
   

   //wprintw(my_win_log, "keypad %d", tastatur);
   //wrefresh(my_win_log);
/* #################### *** Funktionen *** ####################### */
void set_keyid(char key){
  switch (key)
  {
  case 'h':
      keyid=1;
      /* code */
      break;
  case 'n':
      keyid=2;
      /* code */
      break;
  case 'u':
      keyid=3;
      /* code */
      break;
  case 'j':
      keyid=4;
      /* code */
      break;
  case 'm':
      keyid=5;
      /* code */
      break;                        
  
  default:
      keyid=6;
      break;
  }
}
void read_data(int keyid)
{
    //    mvwprintw(my_win_log, 2, 1, "bytes %d",num_bytes);
    //wrefresh(my_win_log);
    mydata.pos = 0;
    mydata.len = 0;
    num_bytes = read(serial_port, &read_buf, sizeof(read_buf));
    //mvwprintw(my_win_log, 2, 1, "bytes %d",num_bytes);
    //wrefresh(my_win_log);
    if (num_bytes < 0)
    {
        printf("Error reading: %s", strerror(errno));
    }
    else
    {
        //printf("\nRead %i bytes. Received message: %s HEX-> ", num_bytes, read_buf);
        // restdaten nach vorn!!
        // code *** letze datenpos ist?
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
                print_data(mydata, supress_dup, supress_log); // unterdruecke Duplikate 0=nein / 1=ja
                datasize = mydata.len + 7;
                if (serial2_aktiv == 1 & mydata.device[1] == 'M') // MITM is aktiv
                {
                    if (lock_set_key == 1  & mydata.crc == 362) // Set Key suppress enabled and Device is Keyboard.
                    {
                        wprintw(my_win_log,"KEY SET DROP!!!! \n" );
                        //logrow = logrow % max_logrow;
                        //mvprintw(my_win_log,++logrow,1,"");
                        wrefresh(my_win_log);
                        write(serial_port2, const_text[6][2], 10);
                    }
                    else if (mydata.device[1] == 'M' & keyid < 6) {
                        write(serial_port2, const_text[keyid][2], 10);
                        if (keyid != 6){
                            wprintw(my_win_log,"Key override id %d\n",keyid);    
                        }
                    }
                    else // sonst alles senden
                    {
                        write(serial_port2, mydata.data, datasize);
                    }
                }
            }
        }
    }
}

void end_serial()
{
    close(serial_port);
    if (serial2_aktiv == 1)
    {
        close(serial_port2);
    }
    printf("\n Ende \n");
}

void init_serial()
{
    /* Init Serial */
    // Open the serial port. Change device path as needed (currently set to an standard FTDI USB-UART cable type device)
    // int serial_port = open("/dev/ttyUSB0", O_RDWR);
    // int serial_port2 = open("/dev/ttyUSB1", O_RDWR);
    serial_port = open(serial_path1, O_RDWR);
    serial_port2 = open(serial_path2, O_RDWR);

    // Read in existing settings, and handle any error
    if (tcgetattr(serial_port, &tty) != 0)
    {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        //return 1;
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
        //return 1;
    }
    // Save tty settings, also checking for error
    if (serial2_aktiv == 1)
    {
        if (tcsetattr(serial_port2, TCSANOW, &tty) != 0)
        {
            printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
            //return 1;
        }
    }
    /* Init Serial END */
    write(serial_port, const_text[9][2], 8);
    if (serial2_aktiv == 1)
    {
        write(serial_port2, "\x58\x4d\x53\x00\x06\x54\x48\x41\x57\x4b\x00\x02\x7d", 13);
    }
}

void init_args(int argc, char *argv[])
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
            printf("\n\n defaults call hawk -s %s -S %s -d %d -l %d", serial_path1, serial_path2, supress_dup, lock_set_key);
            printf("\n s path of Serial e.g. /dev/ttyUSB0");
            printf("\n S if none only Serial 1 is use.");
            printf("\n d suppress double Output");
            printf("\n l lock the Set Button only make sense with s2\n");
            //return 0; // success
            break;
        }
    }
    printf("\n\nhawk running with -s %s -S %s -d %d -l %d\n", serial_path1, serial_path2, supress_dup, lock_set_key);
}

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
        char test;
        //printf("datenpos : %d ", datenpos);
        int rest = buflen - datenpos;
        if ((text[i] == 'X' & text[i + 1] == 'M' & text[i + 2] == 'S') | (text[i] == 'X' & text[i + 1] == 'S' & text[i + 2] == 'M'))
        {
            //printf("\n Anfang gefunden an stelle %d Zeichenkette %c%c%c\n", i,text[i],text[i+1],text[i+2]);
            //printf("buflen:%d datenpos: %d rest:%d",buflen, datenpos, rest);
            if ((buflen - datenpos) < (3 + 2 + ((text[i + 3] * 256) + text[i + 4]) + 2))
            {
                //printf("abbruch nicht genug daten");
                md.len = -1;
                return md;
            }
            memcpy(md.device, text + i, 3);
            md.len = (text[i + 3] * 256) + text[i + 4];
            memcpy(md.payload, text + i + 5, md.len);
            if(text[i + 4 + md.len + 2] > 0)
              md.crc = (text[i + 4 + md.len + 1] * 256) + text[i + 4 + md.len + 2];
            else
              md.crc = (text[i + 4 + md.len + 1] * 256) + (text[i + 4 + md.len + 2]+256);

            md.pos = i + 4 + md.len + 3;
            datenpos = i + 4 + md.len + 3; // merken der wo wir stehen
            memcpy(md.data, text + i, md.len + 7);
            if (md.payload[0] == 5)
            {
                memcpy(last_display, text + i + 5, md.len);
                memcpy(mydisplay.TEXT, text + i + 5, md.len);
            }

            return md;
        }
        //printf("%02X ", text[i]);
    }
    // printf(" buffer null\”");
    md.len = -1;
    return md;
}
/* ##############  PRINT DATA    ############### */
void print_data(struct Struct_Data mydata, int print_dup, int print_off)
{
    //printf("## device %c%c%c dup:%d", mydata.device[0], mydata.device[1], mydata.device[2], print_dup);
    //char payload [15] = {"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};
    //memcpy(payload, mydata.payload, mydata.len-1);

    if (mydata.device[2] == 'M')
    {
        if (last_mb_crc == mydata.crc & print_dup == 1 )
            return;
        set_display(mydata.crc);    
        print_display(my_win_display);    
        if (print_off ==1 )
          return;
        wprintw(my_win_log,"Mainboard last_crc:%6d cur_crc:%6d", last_mb_crc, mydata.crc);
        last_mb_crc = mydata.crc;
        
    }
    else
    {
        if (last_kb_crc == mydata.crc & print_dup == 1 )
            return;
        wprintw(my_win_log,"Keyboard  last_crc:%6d cur_crc:%6d", last_kb_crc, mydata.crc);
        last_kb_crc = mydata.crc;
    }
    if (print_off ==1 )
      return;
    wprintw(my_win_log," %c%c%c Databytes:%d  Payload: ", mydata.device[0], mydata.device[1], mydata.device[2], mydata.len);
    for (int i = 0; i <= mydata.len - 1; i++)
    {
        wprintw(my_win_log,"%02X", mydata.payload[i]);
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
            wprintw(my_win_log," %s", const_text[i][1]);
            break;
        }
        if (i == const_count)
        {
            wprintw(my_win_log," %s", const_text[0][1]);
        }
    }

    wprintw(my_win_log," ASCII:");
    for (int i = 0; i <= mydata.len - 1; i++)
    {
        if (mydata.payload[i] == 0)
        {
            wprintw(my_win_log,"°");
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
            wprintw(my_win_log,"%c", 94);
        }
        else
        {
            /* code */
            wprintw(my_win_log,"%c", mydata.payload[i]);
        }
    }
    wprintw(my_win_log," Display: %s", last_display);

    wprintw(my_win_log,"  rawdata: ");
    for (int i = 0; i < datasize; i++)
    {
        wprintw(my_win_log,"%02x", mydata.data[i]);
    }

    wprintw(my_win_log,"\n");
    //logrow = logrow % (max_logrow-2);
    //mvwprintw(my_win_log,++logrow,1 , "");
    wrefresh(my_win_log);
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
void print_ascii()
{
    printf("\njetzt hier\n");

    for (int i = 1; i <= 256; i++)
    {
        printf("Ascii id:%d  Zeichen:%c %c\n", i, i, 94);
    }
}

void init_ncurses()
{
    initscr(); /* Start curses mode            */
    cbreak();  /* Line buffering disabled, Pass on
                * everty thing to me           */
    noecho(); /* Don't echo() while we do getch */
    // raw();    /* Line buffering disabled      */
    keypad(stdscr, TRUE); /* We get F1, F2 etc..          */
    // my_win = create_newwin(height, width, starty, startx);
    if (has_colors() == FALSE)
    {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);

    mvprintw(0, 0, "  *** Keyboard ***     *** Pool Display ***      Config");
    mvprintw(8, 0, " LOG");
    height = 3;
    width = 10;
    starty = (LINES - height) / 2; /* Calculating for a center placement */
    startx = (COLS - width) / 2;   /* of the window                */
    //c= getch();
    // my_win2 = create_newwin(3, 20, 10, 1);
    refresh();
}
void end_ncurses()
{
    refresh();
    destroy_win(my_win_config);
    destroy_win(my_win_display);
    destroy_win(my_win_log);
    //getch();
    endwin();
}

WINDOW *create_newwin(int height, int width, int starty, int startx)
{
    WINDOW *local_win;
    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0); /* 0, 0 gives default characters 
                                         * for the vertical and horizontal
                                         * lines                        */
    wrefresh(local_win);  /* Show that box                */
    return local_win;
}
void destroy_win(WINDOW *local_win)
{
    /* box(local_win, ' ', ' '); : This won't produce the desired
         * result of erasing the window. It will leave it's four corners 
         * and so an ugly remnant of window. 
         */
    wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    /* The parameters taken are 
         * 1. win: the window on which to operate
         * 2. ls: character to be used for the left side of the window 
         * 3. rs: character to be used for the right side of the window 
         * 4. ts: character to be used for the top side of the window  
        * 5. bs: character to be used for the bottom side of the window 
         * 6. tl: character to be used for the top left corner of the window 
         * 7. tr: character to be used for the top right corner of the window 
         * 8. bl: character to be used for the bottom left corner of the window 
         * 9. br: character to be used for the bottom right corner of the window
         */
    wrefresh(local_win);
    delwin(local_win);
}
void set_config(char key){
  switch (key)
  {
      case 's':{
          lock_set_key = (++lock_set_key) %2;
          break;
      }
      case 'd': {
          supress_dup = (++supress_dup) %2;;
          break;
      }
      case 'f': {
          supress_log = (++supress_log) %2;;
          break;
      }
      default:{
          break;
      }

  }
}
void print_config(WINDOW *local_win)
{
    char y[5]="ON ";
    char n[5]="OFF";
    mvwprintw(local_win, 1, 2, "Serial 1   : %s", serial_path1);
    mvwprintw(local_win, 2, 2, "Serial 2   : %s", serial_path2);
    if (lock_set_key == 0 ) 
      mvwprintw(local_win, 3, 2, "[s] Set Lock   : %s", n);
    else
      mvwprintw(local_win, 3, 2, "[s] Set Lock   : %s", y);
    if (supress_dup == 0)    
      mvwprintw(local_win, 4, 2, "[d] Supress Dup: %s", n);
    else 
      mvwprintw(local_win, 4, 2, "[d] Supress Dup: %s", y);
    if (supress_log == 1)    
      mvwprintw(local_win, 5, 2, "[f] Logging    : %s", n);
    else 
      mvwprintw(local_win, 5, 2, "[f] Logging    : %s", y);      

    wrefresh(local_win);
}

void init_display()
{
    mydisplay.F1 = 0;
    mydisplay.CLEAN = 0;
    mydisplay.F2 = 0;
    mydisplay.JETS = 0;
    mydisplay.LOWER_DOT = 0;
    mydisplay.LUNA = 0;
    mydisplay.READY = 0;
    mydisplay.SET = 0;
    mydisplay.SUN = 0;
    mydisplay.TEXT[0] = 'T';
    mydisplay.TEXT[1] = 'r';
    mydisplay.TEXT[2] = 'X';
    mydisplay.TEXT[3] = 'T';
    mydisplay.TEXT[4] = '\0';
    mydisplay.UPPER_DOT = 0;
}
void set_display(int crc){

    switch (crc)
    {
    case 358: 
        mydisplay.SET = 1;
        mydisplay.READY = 1;
        break;
    case 278:
        mydisplay.SET = 1;
        mydisplay.READY = 0;
        break;
    case 277:
        mydisplay.SET = 0;
        mydisplay.READY = 0;
        break;
    case 357:   
        mydisplay.SET = 0;
        mydisplay.READY = 1;
        break;
    default:
        break;
    }
}
void print_display(WINDOW *local_win)
{
    set_reverse(local_win, mydisplay.SET);
    mvwprintw(local_win, 1, 2, "SET");
    set_reverse(local_win, mydisplay.LUNA);
    mvwprintw(local_win, 1, 6, "LUNA");
    set_reverse(local_win, mydisplay.UPPER_DOT);
    mvwprintw(local_win, 1, 11, "UDOT");
    set_reverse(local_win, mydisplay.READY);
    mvwprintw(local_win, 1, 17, "READY");
    set_reverse(local_win, mydisplay.F1);
    mvwprintw(local_win, 2, 1, "F1");
    set_reverse(local_win, 1);
    mvwprintw(local_win, 3, 4, "%s", mydisplay.TEXT);
    set_reverse(local_win, mydisplay.F2);
    mvwprintw(local_win, 4, 1, "F2");
    set_reverse(local_win, mydisplay.JETS);
    mvwprintw(local_win, 5, 2, "JETS");
    set_reverse(local_win, mydisplay.LOWER_DOT);
    mvwprintw(local_win, 5, 7, "LDOT");
    set_reverse(local_win, mydisplay.SUN);
    mvwprintw(local_win, 5, 12, "SUN");
    set_reverse(local_win, mydisplay.CLEAN);
    mvwprintw(local_win, 5, 17, "CLEAN");

    wrefresh(local_win);
}

void set_reverse(WINDOW *local_win, int display_item)
{
    if (display_item == 0)
    {
        wattron(local_win, COLOR_PAIR(1));
        // wattroff(local_win, COLOR_RED);
    }
    else
    {
        wattron(local_win, COLOR_PAIR(2));
    }
}
void print_keyboard(WINDOW *local_win, char key)
{
   if (last_c == key)
     return;
   last_c=key;  
   if (key == 'h') {
     set_reverse(local_win,1);
   }
   else   
     set_reverse(local_win,0);
   mvwprintw(local_win, 2, 2, " UP(H)"); //1-360

   if (key == 'n'){ 
     set_reverse(local_win,1);
   }  
   else   
     set_reverse(local_win,0);   
   mvwprintw(local_win, 4, 2, "DOWN(N)"); //2-366

   if (key == 'u') {
     set_reverse(local_win,1);
   }  
   else   
     set_reverse(local_win,0);
   mvwprintw(local_win, 2, 10, "JETS(U)"); //3-374
   if (key == 'j') {
     set_reverse(local_win,1);
   }       
   else   
     set_reverse(local_win,0);
   mvwprintw(local_win, 3, 10, "SET(J)"); //4-362

   if (key == 'm') {
     set_reverse(local_win,1);
   }       
   else      
     set_reverse(local_win,0);
   mvwprintw(local_win, 4, 10, "LIGHT(M)"); //5-390

   wrefresh(local_win);
}