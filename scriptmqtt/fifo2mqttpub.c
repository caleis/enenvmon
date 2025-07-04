/*********************************************************************
 *                                                                   *
 *       MQTT PUBLISHER FROM LINUX NAMED PIPE (FIFO)                 *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *       Program name:     FIFO2MQTTpub                              *
 *       Module name:      FIFO2MQTTpub                              *
 *                                                                   *
 *                                                                   *
 *       @author           Zoltan Fekete                             *
 *       Date              10. 03. 2024                              *
 *                                                                   *
 *********************************************************************
 *                                                                   *
 *       Module description:                                         *
 *       This utility takes topic and payload from Linux named pipe  *
 *       and publishes it to MQTT broker                             *
 *       Based on own version of Paho MQTT continuous publisher      *
 *                                                                   *
 *       @version          0.1                                       *
 *       0.1 - initial version                                       *
 *       0.2 - clientid as parameter                                 *
 *                                                                   *
 *                                                                   *
 *********************************************************************
 *       Copyright (C) Zoltan Fekete 2023-2024                       *
 *********************************************************************
 */

/***** Standard header include files *****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>



/***** Program specific header include files *****/

#include <zrpicommon.h>

#include "MQTTAsync.h"

/***** Module macro definitions *****/

/* Definition of logical constants - if not defined already */

#define PGM_VER "0.2.00"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* Macros to silence compiler warnings (SCW) on unused parameters */

#define SCW_Internal_UnusedStringify(macro_arg_string_literal) #macro_arg_string_literal
#define SCW_UnusedParameter(macro_arg_parameter) _Pragma(SCW_Internal_UnusedStringify(unused(macro_arg_parameter)))

/* Program specific macros */

#define FIFOCMD_FNAME          "/tmp/fifo2mqttpub_interface"
#define FIFOCMD_MODE           0666
#define FIFOCMD_RXBUFLEN       500

#define MAX_URLLENGTH         200

#define BROKER_ADDRESS        "127.0.0.1:1883"
#define CLIENTID_DEFAULT      "Fifo2MQTTPublisher"
#define CLIENTID_MAXLEN			200
#define QOS                   1

#define FIFOCMD_TOKENSTART     "#TOPIC#"
#define FIFOCMD_TOKENMID       "#PAYLOAD#"
#define FIFOCMD_TOKENEND       "#END#"

#define MQTT_TICK             25000                     /* basic tick rate, in usec - 25 msec) */
#define CONN_TIMEOUT          (2000000L / MQTT_TICK)    /* Connection timeout, in ticks */
#define RECONNECT_DELAY       (3000000L / MQTT_TICK)    /* Reconnect delay, in ticks */

/***** Data type declarations *****/

typedef enum {MS_IDLE, MS_WF_CONNECTION, MS_RECONNECT_DELAY, MS_GETIFCMD, MS_SEND, MS_DISCONNECT, MS_FATALERR, MS_EXIT} MqttState_t;

/***** Module function prototypes *****/

int OpenInterfacePipe (char *filename);
void connlost (void *context, char *cause);
void onDisconnectFailure (void* context, MQTTAsync_failureData* response);
void onDisconnect ( void* context, MQTTAsync_successData* response);
void onSendFailure (void* context, MQTTAsync_failureData* response);
void onSend (void* context, MQTTAsync_successData* response);
void onConnectFailure (void* context, MQTTAsync_failureData* response);
void onConnect (void* context, MQTTAsync_successData* response);
int messageArrived (void* context, char* topicName, int topicLen, MQTTAsync_message* m);
void getTimeString (char *buf, size_t buflen);
void PrintUsage (char *name);
int main (int argc, char *argv []);

/***** Global variable declarations *****/

static volatile int Running = TRUE;
static volatile MqttState_t MqttState = MS_IDLE;
static MQTTAsync client;

/***** Functions of module *****/

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
      if (mkfifo (filename, FIFOCMD_MODE)) {
         PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Could not create Interface FIFO file (%s): %s\n", filename, strerror (errno));
         return (-1);
      }
      PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Interface FIFO file created '%s'\n", filename);
      if ((fd = open (filename, O_RDONLY | O_NONBLOCK)) < 0) {
         PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Could not open Interface FIFO file (%s): %s\n", filename, strerror (errno));
         return (-1);
      }
   }
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Interface FIFO file '%s' opened, file descriptor: %d\n", filename, fd);
   return (fd);
}

/**
 * @brief Callback function on connection lost
 *
 * @param context pointer to context
 * @param cause pointer to cause (currently always NULL, not used
 *
 */
 void connlost (void *context, char *cause)
{
   SCW_UnusedParameter (context)
   SCW_UnusedParameter (cause)

	PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Connection lost\n");
   MqttState = MS_RECONNECT_DELAY;
}

/**
 * @brief Callback function on disconnect failure
 *
 * @param context pointer to context
 * @param response pointer to response structure
 *
 */
void onDisconnectFailure (void* context, MQTTAsync_failureData* response)
{
   SCW_UnusedParameter (context)
   SCW_UnusedParameter (response)
   
	PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Disconnect failed\n");
	MqttState = MS_FATALERR;
}

/**
 * @brief Callback function on successful disconnect
 *
 * @param context pointer to context
 * @param response pointer to response structure
 *
 */
void onDisconnect (void* context, MQTTAsync_successData* response)
{
   SCW_UnusedParameter (context)
   SCW_UnusedParameter (response)
   
	PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Successful disconnection\n");
	MqttState = MS_EXIT;
}

/**
 * @brief Callback function on MQTT sending failure
 *
 * @param context pointer to context
 * @param response pointer to response structure
 *
 */
void onSendFailure (void* context, MQTTAsync_failureData* response)
{
   SCW_UnusedParameter (context)

	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
	int rc;

	PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Message send failed token %d error code %d\n", response->token, response->code);
	opts.onSuccess = onDisconnect;
	opts.onFailure = onDisconnectFailure;
	opts.context = client;
	if ((rc = MQTTAsync_disconnect (client, &opts)) != MQTTASYNC_SUCCESS)
	{
		PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Failed to start disconnect, return code %d\n", rc);
      MqttState = MS_FATALERR;
	}
}

/**
 * @brief Callback function on successful MQTT send
 *
 * @param context pointer to context
 * @param response pointer to response structure
 *
 */
void onSend (void* context, MQTTAsync_successData* response)
{
   SCW_UnusedParameter (context)
   SCW_UnusedParameter (response)
   
   PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Successful message acknowledgement\n");
}


/**
 * @brief Callback function on error when connecting to broker
 *
 * @param context pointer to context
 * @param response pointer to response structure
 *
 */
void onConnectFailure (void* context, MQTTAsync_failureData* response)
{
   SCW_UnusedParameter (context)
   
	PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Connect failed, response code %d\n", response ? response->code : 0);
   MqttState = MS_RECONNECT_DELAY;
}


/**
 * @brief Callback function on successful connection to broker
 *
 * @param context pointer to context
 * @param response pointer to response structure
 *
 */
void onConnect (void* context, MQTTAsync_successData* response)
{
   SCW_UnusedParameter (context)
   
	PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Successful connection to MQTT broker: %s\n", response->alt.connect.serverURI);
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: GetIfCmd\n");
	MqttState = MS_GETIFCMD;
}

/**
 * @brief Callback function on message arrived (not expected here)
 *
 * @param context pointer to context
 * @param topicName pointer to topic string
 * @param topicLen length of topic string
 * @param m pointer to message structure
 * @return boolean value indicating that message has been safely received (here always TRUE)
 *
 */
int messageArrived (void* context, char* topicName, int topicLen, MQTTAsync_message* m)
{
   SCW_UnusedParameter (context)
   SCW_UnusedParameter (topicName)
   SCW_UnusedParameter (topicLen)
   SCW_UnusedParameter (m)
   
	/* not expecting any messages */
   PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Unexpected message arrived - ignored\n");
	return TRUE;
}

/**
 * @brief Get current date and time string formatted into buffer provided
 *
 * @param buf pointer to buffer large enough to hold the string
 * @param buflen length of buffer provided
 *
 */
void getTimeString (char *buf, size_t buflen)
{
   time_t curtime;
   struct tm* loccurtime;

   time (&curtime);
   loccurtime = localtime (&curtime);
   strftime (buf, buflen, "Message @%Y-%m-%d %H:%M:%S", loccurtime);
   return;
}

/**
* @brief Print usage info to stderr (command line options)
* @param name pointer to program name string
*
*/
void PrintUsage (char *name)
{
   fprintf (stderr, "Usage: %s [options]\n", name);
   fprintf (stderr,
            "   -h   --help                 Display this usage information\n");
   fprintf (stderr,
            "   -b   --background           Run in background (non-interactive, script) mode\n");
   fprintf (stderr,
            "   -m   --mqttbroker <URI>     Defines MQTT broker URI (host{:port})\n");
   fprintf (stderr,
            "   -c   --clientid <string>    Defines Client ID string used to connect to broker with (default: %s\n", CLIENTID_DEFAULT);
   fprintf (stderr,
            "   -d   --debug                Set debug level (%d..%d)\n", DBGMSG_ALWAYS, DBGMSG_DETAILED);
}

/***
 * @brief main function of boilerbrain
 * @param argc number of command line arguments
 * @param argv command line parameters array
 * @return return code
 *
 */
int main (int argc, char *argv [])
{
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	MQTTAsync_responseOptions pub_opts = MQTTAsync_responseOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;

	int rc;
   int ToutCtr = 0;
   byte Interactive = TRUE;
   int ErrorOccurred = FALSE;
   char BrokerUrl [MAX_URLLENGTH + 1] = BROKER_ADDRESS;
	char ClientId [CLIENTID_MAXLEN + 1] = CLIENTID_DEFAULT;
   
   int FifoCmdFd;
   ssize_t NFifoCmdCh;
   char FifoCmdChar;
   char FifoCmdBuf [FIFOCMD_RXBUFLEN + 1] = "";
   char *FifoCmdTopic;
   char *FifoCmdPayload;
   char *FifoCmdPayloadEnd;
   int DataInFifo = FALSE;
   int RecBufIdx = 0;
   struct timeval tv;
   fd_set fds;

   
   fprintf (stderr,
            "%s [%s] - MQTT Publisher from Named Pipe - Z Fekete (C) 2023\n\n", argv [0], PGM_VER);

   /* Process command line options: */
   int next_option;

   /* A string listing valid short options letters. */
   const char* const short_options = "hbm:c:d:";

   /* An array describing valid long options. */
   const struct option long_options[] = {
      { "help", 0, NULL, 'h' },
      { "background", 0, NULL, 'b' },
      { "mqttbroker", 0, NULL, 'm' },
		{"clientid", 0, NULL, 'c' },
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
         case 'm':
            strncpy (BrokerUrl, optarg, MAX_URLLENGTH);
            break;
         case 'c':
            strncpy (ClientId, optarg, CLIENTID_MAXLEN);
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

   PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Starting up in %sinteractive mode\n", (Interactive) ? "" : "non-");
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Debug level: %d\n", DebugLevel);
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Broker address: '%s'\n", BrokerUrl);
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Client ID used: '%s'\n", ClientId);

   if (Interactive) {
      InputModeCanonical (FALSE);
   }

   /* Start real work here */
	if ((rc = MQTTAsync_create (&client, BrokerUrl, ClientId, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS)
	{
		PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Failed to create client object, return code %d. Exiting...\n", rc);
      PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Program exiting with code %d\n", EXIT_FAILURE);
		return (EXIT_FAILURE);
	}
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "MQTT client object created\n");

	if ((rc = MQTTAsync_setCallbacks (client, NULL, connlost, messageArrived, NULL)) != MQTTASYNC_SUCCESS)
	{
		PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Failed to set generic callbacks (connlost, msgarrived), return code %d. Exiting...\n", rc);
      PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Program exiting with code %d\n", EXIT_FAILURE);
		return (EXIT_FAILURE);
	}
   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "MQTT callbacks set successfully\n");
   
   /* Open Input Interface FIFO */
   if ((FifoCmdFd = OpenInterfacePipe (FIFOCMD_FNAME)) < 0) {
      PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: could not open/create FIFO for input interface, exiting...\n");
      exit (1);
   }
   PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Input Interface FIFO (%s, %d) opened or created successfully\n", 
                   FIFOCMD_FNAME, FifoCmdFd);

   PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Start real work now\n");
   
   while (Running) {
      if (Interactive && kbhit ()) {
         fgetc(stdin); /* just drop the character from buffer */
         if ((MqttState == MS_SEND) || (MqttState == MS_GETIFCMD)) {
            MqttState = MS_DISCONNECT;
         }
         else {
            MqttState = MS_EXIT;
         }
      }
      switch (MqttState) {
         case MS_IDLE:
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: Idle\n");
            conn_opts.keepAliveInterval = 20;
            conn_opts.cleansession = 1;
            conn_opts.onSuccess = onConnect;
            conn_opts.onFailure = onConnectFailure;
            conn_opts.context = client;
            if ((rc = MQTTAsync_connect (client, &conn_opts)) != MQTTASYNC_SUCCESS)
            {
               PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Failed to connect, return code %d. Delay before retrying\n", rc);
            }
            else {
               PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Waiting for connection..\n");
            }
            ToutCtr = CONN_TIMEOUT;
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: WF_Connection\n");
            MqttState = MS_WF_CONNECTION;
            break;
         case MS_WF_CONNECTION:
            if (!ToutCtr--) {
               PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Wf_Connection expired, retrying...\n");
               MqttState = MS_IDLE;
            }
            break;
         case MS_RECONNECT_DELAY:
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: Reconnect_Delay\n");
            ToutCtr = RECONNECT_DELAY;
            MqttState = MS_WF_CONNECTION;
            break;
         case MS_GETIFCMD:
            /* Check for remote command from FIFO */
            DataInFifo = TRUE;
            while (DataInFifo) {
               tv.tv_sec = 0;
               tv.tv_usec = 0;
               FD_ZERO(&fds);
               FD_SET(FifoCmdFd, &fds);
               select(FifoCmdFd + 1, &fds, NULL, NULL, &tv);
               if (FD_ISSET(FifoCmdFd, &fds)) {
                  if ((NFifoCmdCh = read (FifoCmdFd, &FifoCmdChar, 1)) == -1) {
                     PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Read Pipe Message (header) internal error: %s\n", strerror (errno));
                     DataInFifo = FALSE;
                  }
                  else {
                     if (NFifoCmdCh == 1) {
                        switch (FifoCmdChar) {
                           case '\r':     /* Command delimiters */
                           case '\n':
                              FifoCmdBuf [RecBufIdx] = '\0';
                              MqttState = MS_SEND;
                              RecBufIdx = 0;
                              DataInFifo = FALSE;
                              break;
                           default:
                              FifoCmdBuf [RecBufIdx] = FifoCmdChar;
                              if (RecBufIdx < FIFOCMD_RXBUFLEN) {
                                 RecBufIdx++;
                              }
                              break;
                        }
                     }
                     else {
                        DataInFifo = FALSE;
                     }
                  }
               }
               else {
                  DataInFifo = FALSE;
               }
            }
            break;
         case MS_SEND:
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: Send\n");
            /* Separate topic and payload value */
            if (strstr (FifoCmdBuf, FIFOCMD_TOKENSTART) != FifoCmdBuf) {
               PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Invalid FIFO command (start token not found or wrong place): '%s'\n", FifoCmdBuf);
               PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: GetIfCmd\n");
               MqttState = MS_GETIFCMD;
               break;
            }
            FifoCmdTopic = FifoCmdBuf + strlen (FIFOCMD_TOKENSTART);
            if ((FifoCmdPayload = strstr (FifoCmdTopic, FIFOCMD_TOKENMID)) == NULL) {
               PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Invalid FIFO command (mid token not found): '%s'\n", FifoCmdTopic);
               PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: GetIfCmd\n");
               MqttState = MS_GETIFCMD;
               break;
            }
            *(FifoCmdPayload) = '\0';
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Topic decoded: '%s'\n", FifoCmdTopic);
            FifoCmdPayload += strlen (FIFOCMD_TOKENMID);
            if ((FifoCmdPayloadEnd = strstr (FifoCmdPayload, FIFOCMD_TOKENEND)) == NULL) {
               PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Invalid FIFO command (end token not found): '%s'\n", FifoCmdPayload);
               PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: GetIfCmd\n");
               MqttState = MS_GETIFCMD;
               break;
            }
            *FifoCmdPayloadEnd = '\0';
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "Payload decoded: '%s'\n", FifoCmdPayload);
            PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Publishing topic: '%s', Payload: '%s'\n", FifoCmdTopic, FifoCmdPayload);
            pub_opts.onSuccess = onSend;
            pub_opts.onFailure = onSendFailure;
            pub_opts.context = client;
            pubmsg.payload = FifoCmdPayload;
            pubmsg.payloadlen = (int)strlen (FifoCmdPayload);
            pubmsg.qos = QOS;
            pubmsg.retained = 0;
            if ((rc = MQTTAsync_sendMessage (client, FifoCmdTopic, &pubmsg, &pub_opts)) != MQTTASYNC_SUCCESS)
            {
               PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Failed to send message, return code %d\n", rc);
            }
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: GetIfCmd\n");
            MqttState = MS_GETIFCMD;
            break;
         case MS_DISCONNECT:
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: Disconnect\n");
            disc_opts.onSuccess = onDisconnect;
            disc_opts.onFailure = onDisconnectFailure;
            if ((rc = MQTTAsync_disconnect (client, &disc_opts)) != MQTTASYNC_SUCCESS)
            {
               PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Failed to start disconnect, return code %d\n", rc);
               MqttState = MS_FATALERR;
            }
            break;
         case MS_FATALERR:
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: Fatalerr\n");
            PrintDebugInfo (DBGMSG_ALWAYS, TRUE, "Error: Fatal error occurred, exiting..\n");
            ErrorOccurred = TRUE;
            MqttState = MS_EXIT;
            break;
         case MS_EXIT:
            PrintDebugInfo (DBGMSG_DETAILED, TRUE, "State: Exit\n");
            Running = FALSE;
            break;
      }
      usleep (MQTT_TICK);
   }
	MQTTAsync_destroy (&client);
 	rc = (ErrorOccurred) ? EXIT_FAILURE: 0;
   InputModeCanonical (TRUE);
   PrintDebugInfo (DBGMSG_MAJOR, TRUE, "Program exiting with code %d\n", rc);
   return (rc);
}

