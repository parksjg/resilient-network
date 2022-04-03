///////////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////////
//parsing files
#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>

//network stuff
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <thread>
#include <chrono>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#define sockerrno errno

#define BUFFER_SIZE 320
#define SAMPLE_RATE 16000

///////////////////////////////////////////////////////////////////////////////
// Script Parameters
//////////////////////////////////////////////////////////////////////////////
// Most of these variables will be input via a GUI, command line arguments, or
// setup file eventually.

// Client data interface: enp0s3
//const char *client_data_channel_ip    = "192.168.131.52";
const char *client_data_channel_ip    = "192.168.100.4";

// Client control interface: enp0s8
//const char *client_control_channel_ip = "192.168.131.53";
const char *client_control_channel_ip = "192.168.200.3";

// Server data interface: enp0s3
//const char *server_data_channel_ip    = "192.168.131.50";
const char *server_data_channel_ip    = "192.168.100.2";

// Server control interface: enp0s8
//const char *server_control_channel_ip = "192.168.131.51";
const char *server_control_channel_ip = "192.168.200.2";

// Ports for data and control channels
unsigned short dataPort    = 21323;
unsigned short controlPort = 8080;

// Data Socket
int iNetSock = INVALID_SOCKET;
int iRequest = 1;
struct sockaddr_in sAddr, cAddr;
socklen_t sAddrLen = sizeof(sAddr);
socklen_t cAddrLen = sizeof(cAddr);

// Control Socket
int iNetSockCC = INVALID_SOCKET;
int iRequestCC = 1;
struct sockaddr_in sAddrCC, cAddrCC;
socklen_t sAddrLenCC = sizeof(sAddrCC);
socklen_t cAddrLenCC = sizeof(cAddrCC);


///////////////////////////////////////////////////////////////////////////////
// Function Definitions
///////////////////////////////////////////////////////////////////////////////
float movingAverage(int newVal) {
    const int N = 250;
    static int index = -1;
    static int values[N] = {0};
    int sum = 0;

    index = (index+1)%N;
    values[index] = newVal;
    for (int i = 0; i < N; i++) {
        sum += values[i];
    }
    return (float) sum/N*100.0;
}

///////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////
int main(int argc, const char * argv[]) {

    char buffer[BUFFER_SIZE+2*sizeof(int)];
    // this is the char array for the control channel counter
    int currentFrameNumber = 0;
    int receivedFrameNumber = 0;
    int encodingMode = 1;
    
    // Open all three audio files and get their size.
    std::ifstream mode1File, mode2File, mode3File;
    std::ifstream *pFile = NULL;
    const int *pFileSize = NULL;


    mode1File.open("/home/qoe/Documents/DARPA_QOE-master/DARPA_QOE_Client/Code/mode1.chn", std::ios::in | std::ios::binary | std::ios::ate);
    mode2File.open("/home/qoe/Documents/DARPA_QOE-master/DARPA_QOE_Client/Code/mode2.chn", std::ios::in | std::ios::binary | std::ios::ate);
    mode3File.open("/home/qoe/Documents/DARPA_QOE-master/DARPA_QOE_Client/Code/mode3.chn", std::ios::in | std::ios::binary | std::ios::ate);
    const int mode1FileSize = mode1File.tellg();
    const int mode2FileSize = mode2File.tellg();
    const int mode3FileSize = mode3File.tellg();

    
    ///////////////////////////////////////////////////////////////////////////
    // network stuff
    // Data Channel
    ///////////////////////////////////////////////////////////////////////////
            
    //establish the IP component
    memset((char *)&sAddr, 0, sizeof(sAddr));
    sAddr.sin_family      = AF_INET;
    sAddr.sin_addr.s_addr = inet_addr(server_data_channel_ip);
    sAddr.sin_port        = htons(dataPort);
    
    memset((char *)&cAddr, 0, sizeof(cAddr));
    cAddr.sin_family      = AF_INET;
    cAddr.sin_addr.s_addr = inet_addr(client_data_channel_ip);
    cAddr.sin_port        = htons(dataPort);

    struct hostent *pHost = gethostbyname(client_data_channel_ip);
    memcpy(&cAddr.sin_addr.s_addr, pHost->h_addr, pHost->h_length);
    
    //creates the UDP socket to send data to
    if ((iNetSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "Error: could not create data socket." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Bind the socket with the server address 
    if (bind(iNetSock, (const struct sockaddr *)&cAddr, sizeof(cAddr)) < 0) {
        std::cout << "Error: could not bind server channel socket and address." << std::endl;
        exit(EXIT_FAILURE); 
    } 
            
    // Control Channel
    memset((char *)&sAddrCC, 0, sizeof(sAddrCC));
    sAddrCC.sin_family      = AF_INET;
    sAddrCC.sin_addr.s_addr = inet_addr(server_control_channel_ip);
    sAddrCC.sin_port        = htons(controlPort);
    
    memset((char *)&cAddrCC, 0, sizeof(cAddrCC));
    cAddrCC.sin_family      = AF_INET;
    cAddrCC.sin_addr.s_addr = inet_addr(client_control_channel_ip);
    cAddrCC.sin_port        = htons(controlPort);

    struct hostent *pHostCC = gethostbyname(client_control_channel_ip);
    memcpy(&cAddrCC.sin_addr.s_addr, pHostCC->h_addr, pHostCC->h_length);
    
    //creates the UDP socket to send data to
    if ((iNetSockCC = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "Cannot create socket for control channel." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Bind the socket with the server address 
    if (bind(iNetSockCC, (const struct sockaddr *)&cAddrCC, sizeof(cAddrCC)) < 0) { 
        std::cout << "Cannot bind control socket and address." << std::endl;
        exit(EXIT_FAILURE); 
    }

    
    // Make the control channel socket time out.
    struct timeval timeoutControlChannel;
    timeoutControlChannel.tv_sec = 0;
    timeoutControlChannel.tv_usec = 200;
    setsockopt(iNetSockCC, SOL_SOCKET, SO_RCVTIMEO, &timeoutControlChannel, sizeof(timeoutControlChannel));
    


   
    int nBytesToSend = -1;
    int bytesRemaining = -1;
    ssize_t nCC = 0;
    int framesPerPacket = -1;
    int packetsSent = 0;
    //int packetsReceived = 0;
    //int elapsedAudioTime = 0;
    float packetLossRate = 1;

    
    while (true) {

        std::cout << "currentFrameNumber:" << currentFrameNumber << std::endl;
        std::cout << "encodingMode = " << encodingMode << std::endl;
        

        /*//Delete. For testing mode switching handling.
        //encodingMode = (encodingMode+1)%3+1; // Increment through the encoding modes to test the code.
        int randNum = rand()%100;
        if (0 <= randNum && randNum < 33) {
            encodingMode = 1;
        } else if (33 <= randNum && randNum < 66) {
            encodingMode = 2;
        } else {
            encodingMode = 3;
        }
        */
        
        switch (encodingMode) {
            case 1:
                pFile     = &mode1File;
                pFileSize = &mode1FileSize;
                framesPerPacket= 1;
                break;
            case 2:
                pFile     = &mode2File;
                pFileSize = &mode2FileSize;
                framesPerPacket = 2;
                break;
            case 3:
                pFile     = &mode3File;
                pFileSize = &mode3FileSize;
                framesPerPacket = 4;
                break;
            default:
                std::cout << "Error: unsupported encoding mode " << encodingMode << "." << std::endl;
                std::cout << "Exiting." << std::endl;
                exit(EXIT_FAILURE);
        }
        
        // Prepend the currentFrameNumber and encoding mode to buffer to send.
        memcpy(&buffer[0],           (int *)&currentFrameNumber, sizeof(int));
        memcpy(&buffer[sizeof(int)], (int *)&encodingMode,       sizeof(int));
     
        // Read the selected input file into buffer to send.
        pFile->seekg(currentFrameNumber*BUFFER_SIZE/framesPerPacket, pFile->beg);
        bytesRemaining = *pFileSize - pFile->tellg();
        if (bytesRemaining < BUFFER_SIZE) {
            nBytesToSend = bytesRemaining;
        } else {
            nBytesToSend = BUFFER_SIZE;
        }
        pFile->read(&buffer[2*sizeof(int)], nBytesToSend);

        // Send the data to the server.
        int b = sendto(iNetSock, &buffer[0], 2*sizeof(int)+nBytesToSend, 0, (struct sockaddr *)&sAddr, sAddrLen);
        packetsSent++;
        printf("Sent %u bytes.\n", b);

        switch (encodingMode) {
            case 1:
                std::this_thread::sleep_for(std::chrono::nanoseconds(18000000));
                break;
            case 2:
                std::this_thread::sleep_for(std::chrono::nanoseconds(38000000));
                break;
            case 3:
             	std::this_thread::sleep_for(std::chrono::nanoseconds(78000000));
                break;
            default:
                std::cout << "Error: unsupported encoding mode " << encodingMode << "." << std::endl;
                std::cout << "Exiting." << std::endl;
                exit(EXIT_FAILURE);
        }
	//std::this_thread::sleep_for(std::chrono::nanoseconds(18000000));
	

	//std::this_thread::sleep_for(std::chrono::nanoseconds(20000000));
    	// second socket for control channel
        // Get data
    	nCC = recvfrom(iNetSockCC, &receivedFrameNumber, 4, 0, (struct sockaddr *) &sAddrCC, &sAddrLenCC); 

        //nCC *= 1; // Delete. Just use nCC to avoid compiler warning. Should check later.
        if (nCC == 4) {
            //if (rand()%100 > 70) { // Delete. For testing packet loss rate.
            //    packetLossRate = movingAverage(1);
            //} else {
                packetLossRate = movingAverage(0);
            //}
        } else {
            packetLossRate = movingAverage(1);
            std::cout << "Error: received " << nCC << " bytes of data on control channel." << std::endl;
            std::cout << "receivedFrameNumber = " << receivedFrameNumber << std::endl;
        }
    
        std::cout << "receivedFrameNumber: " << receivedFrameNumber << std::endl;
        std::cout << "packetLossRate: " << packetLossRate << "%" << std::endl;

       // Change the encoding mode based on the packet loss rate.
       switch (encodingMode) {
           case 1:
               if (packetLossRate >= 15) {
                   encodingMode = 2;
               }
               break;
           case 2:
               if (packetLossRate <= 10) {
                   encodingMode = 1;
               } else if (packetLossRate >= 30) {
                   encodingMode = 3;
               }
               break;
           case 3:
               if (packetLossRate <= 25) {
                   encodingMode = 2;
               }
               break;
           default:
               std::cout << "Error: unaccounted for possibility of encodingMode = " << encodingMode 
                         << " and packetLossRate = " << packetLossRate << "." << std::endl;
               exit(EXIT_FAILURE);
           }
           


        /*
        if (receivedFrameNumber != currentFrameNumber) {
            std::cout << "Continuing..." << std::endl;
            continue;
        }
        */

        /*//Delete. For testing packet loss handling.
        if (currentFrameNumber%20 == 0) { // Delete. For testing packet loss handling.
            currentFrameNumber += 4;
        }
        */

        /*
        if (rand()%100 > 70) {
            currentFrameNumber -= 3;
            if (currentFrameNumber < 0) {
                currentFrameNumber = 0;
            }
        }
        */

        // Increment the current frame number. 
        // If the end of the file is encountered, loop back to the beginning.
        currentFrameNumber++;
        if (currentFrameNumber*BUFFER_SIZE/framesPerPacket >= *pFileSize) {
            currentFrameNumber = 0;
        }
    }        

       

    //close input file
    std::cout << "Closing all files and connections..." << std::endl;
    mode1File.close();
    mode2File.close();
    mode3File.close();
    shutdown(iNetSock,   2);
    shutdown(iNetSockCC, 2);
    
    return 0;
}
