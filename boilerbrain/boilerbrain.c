/*********************************************************************
 *                                                                   *
 *       HEATING SYSTEM LOGGER PROGRAM                               *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *       Program name:      heatlogger                               *
 *       Module name:       heatlogger                               *
 *                                                                   *
 *                                                                   *
 *       Author:           Zoltan Fekete                             *
 *       Revised by:                                                 *
 *       Latest change:    11. 03. 2024                              *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *       Module description:                                         *
 *       This program creates hourly statistics on heating system    *
 *       operation and generates csv output which can be fed to      *
 *       a database                                                  *
 *       developed from heatlogger                                   *
 *                                                                   *
 *       Version history:                                            *
 *       0.1 - initial version                                       *
 *       0.2 - MQTT publisher interface (through FIFO) added         *
 *                                                                   *
 *                                                                   *
 *********************************************************************
 *       Copyright (C) Zoltan Fekete 2023-24                         *
 *********************************************************************/

/***** Standard header include files *****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h> 
#include <gpiod.h>

#include <zrpicommon.h>


/***** Program specific header include files *****/

/***** Module macro definitions *****/

/* Definition of program variant and version */

#ifdef __linux__
    #define OSTYPE "Linux"
#endif // __linux__

/* Note: this program will not compile under Win32/dos */
#ifdef _WIN32
    #define OSTYPE "Win32"
#endif // _WIN32

#define PGM_VER "0.2.01"

/* Definition of logical constants - if not defined already */

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* GPIO chip related definitions */
#define MAX_GPIOCHIPNAMELEN 20
#define MAX_GPIONAMELEN 20
#define NO_OF_GPIOCHIPS ((int)(sizeof (GpioChipDesc) / sizeof (GpioChipDesc)))

/* GPIO definitions - BCM2835 numbering */
#define GPIO_LED_HEARTBEAT 17
#define GPIO_LED_HOTWATER 4
#define GPIO_LED_HEATUPSTAIRS 2
#define GPIO_LED_HEATDOWNSTAIRS 3
#define GPIO_IN_UPSTAIRSHEATING 27
#define GPIO_IN_DOWNSTAIRS_HEATING 22
#define GPIO_RELAY_UPSTAIRSHEAT 19
#define GPIO_RELAY_DOWNSTAIRS_HEAT 26

/* MQTT FIFO interface */
#define FIFO_FILE "/tmp/fifo2mqttpub_interface" 
#define FIFO_MODE 0666

#define FIFOCMD_TOKENSTART     "#TOPIC#"
#define FIFOCMD_TOKENMID       "#PAYLOAD#"
#define FIFOCMD_TOKENEND       "#END#"

#define MQTT_TOPIC "BoilerStats"
#define MQTT_CMDLEN 500
 
/* Other defines */

#define MAX_DATETIMESTRLENGTH 50

/***** Data type declarations *****/

/* Suppress padding warnings for this definition as we do not want to play around with member orders... */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef unsigned char byte;

typedef enum {Staterec_down, Staterec_up, Staterec_no} StateRec_t;

typedef struct {
   int PrevState;
   int OnPeriod;
   int SwitchOnCtr;
   double OnPercentage;
 } StateData_t;
 
/** Datatype for GPIO chip descriptors */
typedef struct GpioChipDesc_t {
   char Name [MAX_GPIOCHIPNAMELEN];
   struct gpiod_chip *Chip;
} GpioChipDesc_t;

/** Datatype for initialising GPIO ports */
typedef struct GpioInitData_t {
   byte ChipIndex;                     /* Defines index in GpioChipDesc structure */
   byte GpioIndex;                     /* Indicates the native GPIO index to be programmed */
   char GpioName [MAX_GPIONAMELEN + 1];
   struct gpiod_line *LineDesc;        /* pointer to line description structure (filled at initialisation */
   enum {OUTPUT0 = 0, OUTPUT1 = 1, INPUT = 2} GpioPinType;
   byte ActiveLow;                     /* 1 if output is active low, 0 if active high */
                                       /* Defines pin function. For output, also defines initial value */
   int  GpioReqFlags;                  /* Request flags, like pull_up/down, etc for input, init state (inact/act) for output */
} GpioInitData_t;

typedef enum {odr_LedHb,  odr_LedHw, odr_LedHUs, odr_LedHDs, odr_RlyUs, odr_RlyDs} GpioOutDataIndices_t;

#pragma clang diagnostic pop
   
/***** Module function prototypes *****/

int InitIO (GpioInitData_t *gid, int n);
int Reset_GpioOutputs (GpioInitData_t *gid, int n);
int Set_OutputOn (GpioInitData_t *gid);
int Set_OutputOff (GpioInitData_t *gid);
int ReadInput (GpioInitData_t *gid);
int OpenInterfacePipe (char *filename);

void PrintUsage (char *name);

int main (int argc, char *argv []);

/***** Global variable declarations *****/

/* GPIO definition structures */

static struct GpioChipDesc_t GpioChipDesc [] = {
   {"/dev/gpiochip0", NULL}
};

static struct GpioInitData_t GpioOutputsData [] = {
   {0, GPIO_LED_HEARTBEAT, "LED_Heartbeat", NULL, OUTPUT0, 1, 1},
   {0, GPIO_LED_HOTWATER, "LED_HotWater", NULL, OUTPUT0, 1, 1},
   {0, GPIO_LED_HEATUPSTAIRS, "LED_HeatUp", NULL, OUTPUT0, 1, 1},
   {0, GPIO_LED_HEATDOWNSTAIRS, "lED_HeatDown", NULL, OUTPUT0, 1, 1},
   {0, GPIO_RELAY_UPSTAIRSHEAT, "Relay_HeatUp", NULL, OUTPUT0, 0, 0},
   {0, GPIO_RELAY_DOWNSTAIRS_HEAT, "Relay_HeatDown", NULL, OUTPUT0, 0, 0}
};

static struct GpioInitData_t GpioInputsData [] = {
   {0, GPIO_IN_DOWNSTAIRS_HEATING, "Status_HeatDown", NULL, INPUT, 1, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP},
   {0, GPIO_IN_UPSTAIRSHEATING, "Status_HeatUp", NULL, INPUT, 1, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP}
};

/***** Functions of module *****/


/***
 * @brief Initializes GPIO ports from descriptor structure
 * @param gid pointer to array of GpioInitData_t structures, holding configuration
 * @param n number of items in array
 * @return: 0 if all operations successful, otherwise the number of errors
 * 
 */
int InitIO (GpioInitData_t *gid, int n)
{
   uint16_t i;
   int Result = 0;
   
   for (i = 0; i < n; i++) {
      /* Open GPIO line first */
      if ((gid + i)->ChipIndex >= NO_OF_GPIOCHIPS) {
         Result++;
         continue;
      }
      (gid + i)->LineDesc = gpiod_chip_get_line(GpioChipDesc [(gid + i)->ChipIndex].Chip, (gid + i)->GpioIndex);
      if (!(gid + i)->LineDesc) {
         Result++;
         continue;
      }
      switch ((gid + i)->GpioPinType) {
         case OUTPUT0:
         case OUTPUT1:
            if (gpiod_line_request_output ((gid + i)->LineDesc, (gid + i)->GpioName, (gid + i)->GpioReqFlags) < 0) {
               Result++;
            }
            else {
               if (gpiod_line_set_value ((gid + i)->LineDesc, (((gid + i)->GpioPinType) ? 1 : 0) ^ (gid + i)->ActiveLow) < 0) {
                  Result++;
               }
            }
            break;
         case INPUT:
            if (gpiod_line_request_input_flags ((gid + i)->LineDesc, (gid + i)->GpioName, (gid + i)->GpioReqFlags) < 0) {
               Result++;
            }
            break;
      }
   }
   return (Result);
}

/***
 * @brief Resets outputs to the originally initialised state.
 *        This function is called as a cleanup function, when program exits
 * @param gid pointer to array of GpioInitData structure
 * @param n size of the GpioInitStructure array
 * @return 0 if all operations successful, otherwise the number of errors
 *
 */
int Reset_GpioOutputs (GpioInitData_t *gid, int n)
{
   int i;
   int Result = 0;

   for (i = 0; i < n; i++) {
      if (gpiod_line_set_value ((gid + i)->LineDesc, ((gid + i)->GpioPinType) ? 1 : 0) < 0) {
         Result++;
      }
   }
   return (Result);
}

/***
 * @brief Sets the selected output to On state (taking into account the ActiveLow flag)
 * @param gid pointer GpioInitData structure of the output
 * @return TRUE if parameters were ok, FALSE otherwise
 *
 */
int Set_OutputOn (GpioInitData_t *gid)
{
   if (gpiod_line_set_value (gid->LineDesc, !(gid->ActiveLow)) < 0) {
      return (FALSE);
   }
   return (TRUE);
}

/***
 * @brief Sets the selected output to Off state (taking into account the ActiveLow flag)
 * @param gid pointer GpioInitData structure of the output
 * @return: TRUE if parameters were ok, FALSE otherwise
 *
 */
int Set_OutputOff (GpioInitData_t *gid)
{
   if (gpiod_line_set_value (gid->LineDesc, gid->ActiveLow) < 0) {
      return (FALSE);
   }
   return (TRUE);
}

/***
 * @brief Reads input taking into account the ActiveLow flag
 * @param gid pointer GpioInitData structure of the input
 * @return 1 if input active, or 0 if input inactive. Returns -1 in case of error
 *
 */
int ReadInput (GpioInitData_t *gid)
{
   int is;
   
  if ((is = gpiod_line_get_value (gid->LineDesc)) < 0) {
      return (-1);
   }
   return (is ^ gid->ActiveLow);
}

/**
 * @brief Open or create named pipe for receiving data to be published
 * @param filename name of FIFO file in the file system
 * @returns file descriptor for opened FIFO, or -1 if error occured
 *
 */
int OpenInterfacePipe (char *filename)
{
   int fd;
   
   if ((fd = open (filename, O_RDONLY | O_NONBLOCK)) < 0) {
      PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Interface FIFO file does not exist, creating...\n", filename);
      if (mkfifo (filename, FIFO_MODE)) {
         PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Could not create Interface FIFO file (%s): %s\n", filename, strerror (errno));
         return (-1);
      }
      PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Interface FIFO file created '%s'\n", filename);
	}
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "FIFO '%s' exists, opening...\n", filename);
	/* We open the FIFO for R/W so no blocking */
	if ((fd = open (filename, O_RDWR)) < 0) {
		PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Could not open Interface FIFO file (%s): %s\n", filename, strerror (errno));
		return (-1);
	}
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Interface FIFO file '%s' opened, file descriptor: %d\n", filename, fd);
   return (fd);
}

 /**
* @brief Print usage info to stderr (command line options)
* @param name pointer to program name string
*
*/
void PrintUsage (char *name)
{
   fprintf (stderr, "Usage: %s [options] [output file]\n", name);
   fprintf (stderr,
            "   -h   --help                 Display this usage information\n");
   fprintf (stderr,
            "   -b   --background           Run in background (non-interactive, script) mode\n");
   fprintf (stderr,
            "   -d   --debug                Set debug level\n");
}

/***
 * @brief main function of boilerbrain
 * @param argc number of command line arguments
 * @param argv command line parameters array
 *
 */
int main (int argc, char *argv [])
{
   FILE *OutFile;

   uint i;
   
   byte Interactive = TRUE;
   int  termchar = 0;
   byte KeyPressed = FALSE;

   StateData_t StateData [Staterec_no];

   time_t AbsTime;
   int Hour, PrevHour;
   int Minute, PrevMinute;
   int Second, PrevSecond;

   struct tm* tmTimeStamp;
   char TimeStr [MAX_DATETIMESTRLENGTH + 1];

   struct timespec ProgStartTime;
   struct sigaction st;
	
   int fifofd;
	char MqttCmd [MQTT_CMDLEN];
   
   int is;
   int PeriodCtr = 0;

   /* Initialise clock measuring runtime */
   Init_Clock (&ProgStartTime);

   DebugLevel = DBGMSG_MAJOR;
   fprintf (stderr,
            "%s - Boiler Status Logger [%s v%s] - Z Fekete (C) 2023-24\n\n", argv [0], OSTYPE, PGM_VER);

   /* Process command line options: */
   int next_option;

   /* A string listing valid short options letters. */
   const char* const short_options = "hbd:";

   /* An array describing valid long options. */
   const struct option long_options[] = {
      { "help", 0, NULL, 'h' },
      { "background", 0, NULL, 'b' },
      { "debug", 0, NULL, 'd' },
      { NULL, 0, NULL, 0 } /* Required at end of array. */
   };

   do {
      next_option = getopt_long (argc, argv, short_options, long_options, NULL);
      switch (next_option) {
         case '?': /* invalid option. */
         case 'h':
            PrintUsage (argv [0]);
            exit (0);
         case 'b':
            Interactive = FALSE;
            break;
         case 'd':
            DebugLevel = atoi (optarg);
            break;
         case -1: /* Done with options. */
            break;
         default: /* Something else: unexpected. */
            fprintf (stderr, "Command-line processor internal error [unexpected option: %d]\n", next_option);
            exit (1);
      }
   }
   while (next_option != -1);
   
   if (argc - optind == 1) {
      if ((OutFile = fopen (argv [optind], "wt")) == NULL) {
         fprintf (stderr, "Error: cannot create output file\n");
         exit (1);
      }
   }
   else {
     OutFile = stdout;
   }
   
   /* Install term signal handler - needs to cleanup and gracefully exit */
   InitSignalHandler (&st, SIGTERM, &term_handler);
   
   /* Open all GPIO chips */
   for (i = 0; i < NO_OF_GPIOCHIPS; i++) {
      if (!(GpioChipDesc [i].Chip = gpiod_chip_open (GpioChipDesc [i].Name))) {
         PrintDebugInfo (DBGMSG_ALWAYS, TRUE,
                         "Error: GPIO chip %02d - '%s' could not be initialised! Physical I/O functions inoperable\n",
                         i, GpioChipDesc [i].Name);
         if (CleanupRegisteredFunctions ()) {
            return (0);
         }
         return (1);
      }
   }
   /* Initialise GPIO */
   InitIO (GpioInputsData, sizeof (GpioInputsData) / sizeof (GpioInitData_t));
   InitIO (GpioOutputsData, sizeof (GpioOutputsData) / sizeof (GpioInitData_t));

   PrintDebugInfo (DBGMSG_MAJOR, TRUE, "GPIO initialised...\n");
	
   /* Opening FIFO to send command to MQTT publisher process. We open for R/W nevertheless,
      so it does not block in case publisher process is not running */
   if ((fifofd = OpenInterfacePipe (FIFO_FILE)) == -1) {
      PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: opening FIFO: %s\n", strerror (errno));
      exit (1);
   } 

   /* Start real work here */
   PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Starting up in %sinteractive mode\n", (Interactive) ? "" : "non-");
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Output file: %s\n", (OutFile == stdout) ? "Standard-out" : argv [optind]);
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Debug level: %d\n", DebugLevel);

   if (Interactive) {
      InputModeCanonical (FALSE);
   }

   /* main program */
   
   /* Print csv header */
   fPrintDebugInfo (OutFile, DBGMSG_ALWAYS, FALSE, "Timestamp,TotalSeconds,DownOn,DownOnCycles,DownOnPercentage,UpOn,UpOnCycles,UpOnPercentage\n");
   fflush (OutFile);
   
   /* Initialise state */
   if ((AbsTime = GetAbsTime ()) == -1) {
      PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: could not get valid local absolute time\n");
   }
   PrevHour = GetHourOfDay (AbsTime);
   PrevMinute = GetMinuteOfHour (AbsTime);
   PrevSecond = GetSecondOfMinute (AbsTime);
   for (i = 0; i < Staterec_no; i++) {
      StateData [i].PrevState = 0;
      StateData [i].OnPeriod = 0;
      StateData [i].SwitchOnCtr = 0;
      StateData [i].OnPercentage = 0.0;
   }
   /* main loop */
   while (!Interactive || !kbhit ()) {
      if ((AbsTime = GetAbsTime ()) == -1) {
         PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: could not get valid local absolute time\n");
      }

      /* Handling once a second tasks */
      Second = GetSecondOfMinute (AbsTime);
      if (PrevSecond != Second) {
         /* Set heartbeat on */
         Set_OutputOn (&GpioOutputsData [odr_LedHb]);
         PrevSecond = Second;
         PeriodCtr++;
         for (i = 0; i < Staterec_no; i++) {
            if ((is = ReadInput (&GpioInputsData [i]))) {
               StateData [i].OnPeriod++;
               if (is != StateData [i].PrevState) {
                  StateData [i].SwitchOnCtr++;
               }
            }
            StateData [i].PrevState = is;
            if (i == Staterec_down) {
               (is) ? Set_OutputOn (&GpioOutputsData [odr_LedHDs]) : Set_OutputOff (&GpioOutputsData [odr_LedHDs]);
            }
            if (i == Staterec_up) {
               (is) ? Set_OutputOn (&GpioOutputsData [odr_LedHUs]) : Set_OutputOff (&GpioOutputsData [odr_LedHUs]);
            }
         }
         PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Current Counter Status -  Period: %d, Down: %d (%d), Up %d (%d)\n", 
                         PeriodCtr, StateData [Staterec_down].OnPeriod, StateData [Staterec_down].SwitchOnCtr,
                         StateData [Staterec_up].OnPeriod, StateData [Staterec_up].SwitchOnCtr);
      }

      /* Handling once in a minute todos */
      Minute = GetMinuteOfHour (AbsTime);
      if (PrevMinute != Minute) {
         PrevMinute = Minute;
         /* nothing to do for now */
      }

      /* Handling once in an hour todos */
      Hour = GetHourOfDay (AbsTime);
      if (PrevHour != Hour) {
         PrevHour = Hour;
         tmTimeStamp = localtime (&AbsTime);
         strftime(TimeStr, MAX_DATETIMESTRLENGTH, "%Y-%m-%dT%H:%M:%SZ", tmTimeStamp);
         for (i = 0; i < Staterec_no; i++) {
            StateData [i].OnPercentage = (double)100 * StateData [i].OnPeriod / PeriodCtr;
         }
         fPrintDebugInfo (OutFile, DBGMSG_ALWAYS, FALSE, "%s,%0000d,%0000d,%00d,%0.2f,%0000d,%00d,%0.2f\n", 
                          TimeStr, PeriodCtr, 
                          StateData [Staterec_down].OnPeriod, StateData [Staterec_down].SwitchOnCtr, StateData [Staterec_down].OnPercentage,
                          StateData [Staterec_up].OnPeriod, StateData [Staterec_up].SwitchOnCtr, StateData [Staterec_up].OnPercentage);
			/* Send stats to MQTT queue, format #TOPIC#<topic>#PAYLOAD#<json data>#END#\n, parsing starts only after newline */
         sprintf (MqttCmd, "%s%s%s{\"timestamp\":\"%s\",\"periodctr\":\"%000d\","
			         "\"downstairs\":{\"onperiod\":\"%0000d\",\"oncounter\":\"%00d\",\"onpercentage\":\"%0.2f\"},"
			         "\"upstairs\":{\"onperiod\":\"%0000d\",\"oncounter\":\"%00d\",\"onpercentage\":\"%0.2f\"}}%s\n",
			         FIFOCMD_TOKENSTART, MQTT_TOPIC, FIFOCMD_TOKENMID, TimeStr, PeriodCtr, 
					   StateData [Staterec_down].OnPeriod, StateData [Staterec_down].SwitchOnCtr, StateData [Staterec_down].OnPercentage,
					   StateData [Staterec_up].OnPeriod, StateData [Staterec_up].SwitchOnCtr, StateData [Staterec_up].OnPercentage, FIFOCMD_TOKENEND);
			if (write (fifofd, MqttCmd, strlen (MqttCmd)) < 0) {
				PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "[STQ] Error: writing to FIFO: %s\n", strerror (errno));
			}
			PeriodCtr = 0;
         fflush (OutFile);
         for (i = 0; i < sizeof (Staterec_no); i++) {
            StateData [i].OnPeriod = 0;
            StateData [i].SwitchOnCtr = 0;
         }
      }
      /* Check for input from keyboard in interactive mode */
      if (Interactive && kbhit()) {
         termchar = fgetc(stdin);
         KeyPressed = TRUE;
      }
      usleep (50000);
      /* Set heartbeat off */
      Set_OutputOff (&GpioOutputsData [odr_LedHb]);
   }
   fclose (OutFile);
   InputModeCanonical (TRUE);
   if (CleanupRegisteredFunctions ()) {
      return (0);
   }
   return (1);
}

