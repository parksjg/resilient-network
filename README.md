# resilient-network

This contains one server (s2.cpp) and one client (c2.cpp). The server and client have 2 NICs. This is being developed on a MacBook Pro using two virtual machines running Fedora 26. Then it is further tested on a private network with separate laptops running Fedora 26.

Note: example IPs
+ Server IPs and ports: 10.0.0.209:21323 (data channel) and 10.0.0.121:8080 (control channel)

+ Client IPs and ports: 10.0.0.210:21323 (data channel) and 10.0.0.122:8080 (control channel)

I am sending a buffer from the client containing a packet number (qualitySequenceCounter, it is 4 bytes) and 320 bytes of an encoded audio file (.chn) over the "data channel" 10.0.0.210:21323 -> 10.0.0.209:21323. When the server receives the data, it unpacks the qualitySequenceCounter and the audio data. Then the server sends the qualitySequenceCounter back to the client over the "control channel" 10.0.0.121:8080 -> 10.0.0.122:8080. 

To know:

  + There are three files being sent over UDP. The files are the same speech but encoded with different rates and redundancy.     The 3 files are mode1.chn (highest sample rate, no redundancy), mode2.chn (mid sample rate, double redundancy), mode3.chn     (lowest sample rate, quadruple redundancy). The .chn extension was chosen by steve to be short for channel. These files       are the raw encoded audio data.
	

To do:

  + s2.cpp needs to decode the audio data using G.711 (maybe we should also encode it?)
	
  + s2.cpp needs to play the audio data as it is received-ish (each packet of encoded audio data contains 20 milliseconds of        audio)
	
Once audio is decoded and playing...

  + c2.cpp needs logic based on packet loss data (from the control channel) to dynamically switch between the 3 modes, and it needs to send packets from the same place in the file as where it was in the other file.

  + s2.cpp needs to know which audio file it is receiving and decode the data appropriately (easiest for mode1.chn but            mode2.chn has 2 frames per packet and mode3.chn has 4 frames per packet. See "Notes.pdf" for more information.)
  
# High-level Architecture
  ![Image of NetUpBlock1](/images/NetUpBlock1.jpg)
  
# Audio Packet Contents
  ![Image of PacketContents](/images/PacketContents.jpg)
