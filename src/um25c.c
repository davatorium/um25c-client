/*******
 * UM25C
 *
 * MIT/X11 License
 * Copyright © 2019 Qball Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/** termios */
#include <termios.h>

/** Error reporting */
#include <errno.h>
#include <string.h>

/** endianess conversion */
#include <arpa/inet.h>

/** Signal trapping. */
#include <signal.h>

/** Option parsing */
#include <getopt.h>

/* Time stamping. */
#include <time.h>

/** Time calculations */
#include <math.h>


const uint8_t msg_data_dump = 0xf0;
const uint8_t msg_clear_sum = 0xf4;
const uint8_t data_dump_length = 130;


// Quit on signal.
static int quit =  0;

/**
 * Data format of UM25C
 * https://sigrok.org/wiki/RDTech_UM_series
 */
typedef struct __UMC_MES
{
    uint32_t milliamps;
    uint32_t milliwatts;
} __attribute__((packed,aligned(1))) UMC_MES;


/**
 * Total 130 byte.
 * Attributes enforces this.
 */
struct _UMC
{
    uint16_t unknown1;
    //2
    uint16_t millivolts;
    //4
    uint16_t tenths_milliamps;
    // 6
    uint32_t milliwatts;
    // 10
    uint16_t temp_celsius;
    // 12
    uint16_t temp_fahrenheit;
    //14
    uint16_t current_datagroup;
    //16
    UMC_MES mes[10];
    // 96
    uint16_t pline_centivolts;
    // 98
    uint16_t nline_centivolts;
    // 100
    uint16_t charge_mode;
    // 102
    uint32_t milliamps_threshold;
      // 106
    uint32_t milliwatts_threshold;
    // 110
    uint16_t current_threshold_centivolt;
    // 112
    uint32_t recording_time;
    // 116
    uint16_t recording_active;
    // 118
    uint16_t screen_timeout;
    // 120
    uint16_t screen_backlight;
    // 122
    uint32_t resistance_deciohm;
    // 126
    uint16_t current_screen;
    // 128
    uint16_t unknown2;
    // 130

} __attribute__((packed,aligned(1)));

typedef struct _UMC UMC;

typedef union {
    uint8_t raw[130];
    UMC umc;
} UMC_READ;


/**
 * Convert endianess.
 */
static void convert ( UMC *umc )
{
    umc->millivolts                  = ntohs(umc->millivolts);
    umc->tenths_milliamps            = ntohs(umc->tenths_milliamps);
    umc->temp_celsius                = ntohs(umc->temp_celsius);
    umc->temp_fahrenheit             = ntohs(umc->temp_fahrenheit);
    umc->current_datagroup           = ntohs(umc->current_datagroup);
    umc->pline_centivolts            = ntohs(umc->pline_centivolts);
    umc->nline_centivolts            = ntohs(umc->nline_centivolts);
    umc->charge_mode                 = ntohs(umc->charge_mode);
    umc->current_threshold_centivolt = ntohs(umc->current_threshold_centivolt);
    umc->recording_active            = ntohs(umc->recording_active);
    umc->screen_timeout              = ntohs(umc->screen_timeout);
    umc->screen_backlight            = ntohs(umc->screen_backlight);
    umc->current_screen              = ntohs(umc->current_screen);
    for ( int i = 0; i < 10; i++ )
    {
        umc->mes[i].milliamps        = ntohl(umc->mes[i].milliamps);
        umc->mes[i].milliwatts       = ntohl(umc->mes[i].milliwatts);
    }
    umc->resistance_deciohm          = ntohl(umc->resistance_deciohm);
    umc->milliwatts                  = ntohl(umc->milliwatts);
    umc->milliamps_threshold         = ntohl(umc->milliamps_threshold);
    umc->milliwatts_threshold        = ntohl(umc->milliwatts_threshold);
    umc->recording_time              = ntohl(umc->recording_time);
}

/**
 * Signal handler for handling interrupt.
 */
static void signal_handler ( int signal )
{
    if ( signal == SIGINT )
    {
        quit = 1;
    }

}

/**
 * output to standard out.
 */
static void print ( const char * const print_format, const UMC * const umc, const struct timespec *const now )
{
    const char *p;
    for ( p = print_format; *p != '\0'; )
    {
        if ( strncmp ( p, "Volt", strlen("Volt")) == 0 )
        {
            printf("%.03f", umc->millivolts/1000.0);
            p+= strlen("Volt");
        } else if ( strncmp  ( p, "Amp", strlen("Amp")) == 0 ) {
            printf("%.04f", umc->tenths_milliamps/10000.0);
            p+= strlen("Amp");
        } else if ( strncmp  ( p, "Watt", strlen("Watt")) == 0 ) {
            printf("%.03f", umc->milliwatts/1000.0);
            p+= strlen("Watt");
        } else if ( strncmp  ( p, "Temp", strlen("Temp")) == 0 ) {
            printf("%d", umc->temp_celsius);
            p+= strlen("Temp");
        } else if ( strncmp ( p, "Time", strlen("Time")) == 0 ) {
            printf("%lu.%03ld",
                   now->tv_sec,
                   now->tv_nsec/1000000L);
            p+= strlen("Time");
        } else if ( strncmp ( p, "SumWatt", strlen("SumWatt")) == 0 ) {
            printf("%.03f", umc->mes[umc->current_datagroup].milliwatts/1000.0);
            p += strlen("SumWatt");
        } else if ( strncmp ( p, "SumAmp", strlen("SumAmp")) == 0 ) {
            printf("%.03f", umc->mes[umc->current_datagroup].milliamps/1000.0);
            p += strlen("SumAmp");
        } else {
            putchar(*p);
            p++;
        }

    }
    printf("\n");
}


static int um25c_write ( int fd, uint8_t msg )
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    int retv = select ( fd+1, NULL, &wfds, NULL, NULL );
    if ( retv == -1 ) {
        fprintf(stderr, "Failed to read from serial port: %s\n", strerror(errno));
        return 1;
    }

    if ( FD_ISSET(fd, &wfds) ) {
        write(fd, &msg, 1);
        return 0;
    }

    return 1;
}


/**
 * Helper function to do add to timespec's.
 * Has no overflow for tv_sec.
 */
static struct timespec  timespec_add ( const struct timespec a, const struct timespec b)
{
    struct timespec retv;
    retv.tv_sec  = a.tv_sec + b.tv_sec;
    retv.tv_nsec = a.tv_nsec+b.tv_nsec;
    while ( retv.tv_nsec > 1e9){
        retv.tv_sec  += 1;
        retv.tv_nsec -= 1e9;
    }
    return retv;
}


/**
 * The main applications.
 */
int main ( int argc, char **argv )
{
    struct termios oldtio;
    int fp;
    /**
     * Commandline arguments.
     */
    static const struct option long_options[] = {
        {"help",     no_argument,       NULL, 'h'},
        {"clear",    no_argument,       NULL, 'c'},
        {"format",   required_argument, 0,    'f'},
        {"device",   required_argument, 0,    'd'},
        {"interval", required_argument, 0,    'i'},
        {0, 0, 0, 0}
    };
    /** Defaults */
    char *serial_port     = "/dev/rfcomm0";
    char *print_format    = "Volt, Amp";
    double interval = 1.0f;
    int  clear_sum        = 0;


    /**
     * Handle command-line arguments.
     */
    while ( 1 ) {
        int option_index = 0;
        int c = getopt_long ( argc, argv, "d:f:i:hc", long_options, &option_index);
        if ( c == -1 ) {
            // Parsing done.
            break;
        }
        switch (c) {
            case 'h':
                printf("Usage: um25c [<options>]\n\n");
                printf(" -f, --format:    Adjust the output format.\n");
                printf(" -d, --device:    Set the serial device.\n");
                printf(" -i, --interval:  Set the sampling interval.\n");
                printf(" -c, --clear:     Clear the sum value\n");
                printf("\n");
                printf("Format supports the following options:\n");
                printf(" * Time  - Unix timestamp\n");
                printf(" * Volt  - Current voltage\n");
                printf(" * Amp   - Current amperage\n");
                printf(" * Watt  - Current Wattage\n");
                printf(" * SumWatt - Current data group total Wattage (Wh)\n");
                printf(" * SumAmp - Current data group total Amperage (Ah)\n");
                printf(" * Temp  - Temperature (in Celsius)\n");
                printf("\n");
                printf("In session, press ctrl-c to quit.\n");
                return EXIT_SUCCESS;
            case 'f':
                print_format = optarg;
                fprintf(stderr, "Set output format: %s\n", optarg);
                break;
            case 'd':
                serial_port = optarg;
                fprintf(stderr, "Set serial port: %s\n", optarg);
                break;
            case 'i':
                interval = strtod(optarg, NULL);
                fprintf(stderr, "Set interval: %0.1f\n", interval);
                break;
            case 'c':
                clear_sum = 1;
                break;
            default:
                fprintf(stderr, "Invalid option passed.\n");
                return EXIT_FAILURE;
        }
    }


    // Setup signal handler.
    signal ( SIGINT, signal_handler);


    // Connect to serial port.
    fprintf(stderr, "Connecting...\n");
    fp = open ( serial_port, O_RDWR|O_NOCTTY );

    if ( fp < 0 )
    {
        fprintf(stderr, "Failed top open serial port: %s\n", strerror(errno) );
        return EXIT_FAILURE;
    }

    // save status port settings
    tcgetattr ( fp, &oldtio );
    // Setup the serial port.
    struct termios newtio = { 0, };
    newtio.c_cflag     = B9600| CS8 | CREAD | PARODD;
    newtio.c_iflag     = 0;
    newtio.c_oflag     = 0;
    newtio.c_lflag     = 0;                   //ICANON;
    newtio.c_cc[VMIN]  = 1;
    newtio.c_cc[VTIME] = 0;
    tcflush ( fp, TCIFLUSH | TCIOFLUSH );
    tcsetattr ( fp, TCSANOW, &newtio );



    // Start reading.
    fprintf(stderr, "Starting...\n");


    if ( clear_sum ) {
        fprintf(stderr, "Clear sum\n" );
        if ( um25c_write ( fp, msg_clear_sum ) ) {
            fprintf(stderr, "Failed to clear sum: %s\n", strerror ( errno )  );
            quit = 1;
        }
        // This sleep is required otherwise first data dump will fail.
        usleep(200000);
    }

    // Get initial timestamp.
    struct timespec start,now;
    int clk_retv = clock_gettime ( CLOCK_MONOTONIC, &start);
    if ( clk_retv < 0 ) {
        fprintf(stderr, "Failed to get time: %s\n", strerror ( errno )  );
        quit = 1;
    }
    while ( !quit )
    {
        UMC_READ umc;
        /**
         * Request a data dump.
         */
        if ( um25c_write ( fp, msg_data_dump ) ) {
            quit = 1;
            continue;
        }
        /**
         * Read response
         */
        ssize_t index = 0;
        while ( index < data_dump_length )
        {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fp, &rfds);
            int retv = select ( fp+1, &rfds, NULL, NULL, NULL );
            if ( retv == -1 ) {
                fprintf(stderr, "Failed to read from serial port: %s\n", strerror(errno));
                break;
            }
            /** If there is data to read, read it. */
            if ( FD_ISSET(fp, &rfds) )
            {
                ssize_t r = read( fp, &(umc.raw[index]), 130);
                if (r < 0 ) {
                    fprintf(stderr, "Failed top read from serial port: %s\n", strerror(errno) );
                    break;
                }
                index += r;
            }
            if(quit) {
                break;
            }
        }


        int clk_retv = clock_gettime ( CLOCK_MONOTONIC, &now);
        if ( clk_retv < 0 ) {
            fprintf(stderr, "Failed to get time: %s\n", strerror ( errno )  );
            break;
        }
        /** Convert the format from Network (Big) Endian to Host Endian. */
        convert ( &(umc.umc) );
        /** Print the result. */
        print ( print_format, &(umc.umc), &now );
        /** Flush the output. */
        fflush(stdout);

        /** Time calculations. */
        double s = trunc(interval);
        now.tv_sec  = (time_t)s;
        now.tv_nsec = (long)round((interval-s)/1e-9);
        start =  timespec_add (start, now);


        clk_retv = clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME,  &start, NULL);
        if ( clk_retv < 0 )
        {
            fprintf(stderr, "Failed to sleep: %s\n", strerror ( errno )  );
            quit = 1;
        }
    }

    fprintf(stderr, "Quiting...");
    // Closing.
    tcflush ( fp, TCIFLUSH );
    tcsetattr ( fp, TCSANOW, &oldtio );
    close(fp);
    return EXIT_SUCCESS;
}
