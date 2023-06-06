// MIDI2OSC.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "oscpp/client.hpp"

#include <iostream>
#include "rtmidi.h"

#include <atltrace.h>
#include <string>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>

// extern ATL::CTrace TRACE;
#define TRACE ATLTRACE


size_t makePacket(void* buffer, size_t size) {
	// Construct a packet
	OSCPP::Client::Packet packet(buffer, size);
	packet
		// Open a bundle with a timetag
		.openBundle(1234ULL)
		// Add a message with two arguments and an array with 6 elements;
		// for efficiency this needs to be known in advance.
		.openMessage("/s_new", 2 + OSCPP::Tags::array(6))
		// Write the arguments
		.string("sinesweep")
		.int32(2)
		.openArray()
		.string("start-freq")
		.float32(330.0f)
		.string("end-freq")
		.float32(990.0f)
		.string("amp")
		.float32(0.4f)
		.closeArray()
		// Every `open` needs a corresponding `close`
		.closeMessage()
		// Add another message with one argument
		.openMessage("/n_free", 1)
		.int32(1)
		.closeMessage()
		// And nother one
		.openMessage("/n_set", 3)
		.int32(1)
		.string("wobble")
		// Numeric arguments are converted automatically
		// (see below)
		.int32(31)
		.closeMessage()
		.closeBundle();
	return packet.size();
}

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment (lib, "Ws2_32.lib")
#include <iostream>
#include <string.h>
#include <sstream>
#include <WinSock2.h>
#include <WS2tcpip.h>
using namespace std;


int main() {
	WSAData wsaData;
	WORD DllVersion = MAKEWORD(2, 1);
	if (WSAStartup(DllVersion, &wsaData) != 0) {
		cout << "Winsock Connection Failed!" << endl;
		exit(1);
	}

	string getInput = "";
	SOCKADDR_IN addr;
	int addrLen = sizeof(addr);
	IN_ADDR ipvalue;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//addr.sin_addr.s_addr = inet_addr("[::]");
	addr.sin_port = htons(8004);
	addr.sin_family = AF_INET;

	SOCKET connection = socket(AF_INET, SOCK_STREAM, NULL);
	int err;
	if ((err = connect(connection, (SOCKADDR*)&addr, addrLen)) != 0) {
		cout << "Error Connecting to Host: " << err << endl;
		return 1;
	}
	TRACE("Connected!\n");

	return 0;
}

struct MidiProcessor {
};

void OnMidiIn(double timeStamp, std::vector<unsigned char>* message, void* userData) {
	std::stringstream debugMessage;
	std::copy(message->begin(), message->end(), std::ostream_iterator<int>(debugMessage, " "));
	MidiProcessor* this_ = static_cast<MidiProcessor*>(userData);
	//IMidiControllerEventReceiver* eventReceiver = this_->eventReceiver;
	//IMidiControllerEventReceiver* eventReceiver = static_cast<IMidiControllerEventReceiver*> (userData);

	if (message->size() != 3) {
		TRACE("Got a message with weird length: %s\n", debugMessage.str().c_str());
		return;
	}

	// https://github.com/mixxxdj/mixxx/wiki/MIDI%20clock%20output
	unsigned char m0 = message->at(0);
	unsigned char m1 = message->at(1);
	unsigned char m2 = message->at(2);

	int NOTE_ON = 144;
	int NOTE_OFF = 128;

	if (m0 == NOTE_ON && m1 == 52) {
		int bpm = m2 + 50;
		TRACE("BPM: %d\n", bpm);
	//} else if (m0 == 128 && m1 == 48) { 
	}
	else if (m0 == NOTE_ON && m1 == 48 && m2 == 0) {
	}
	else if (m0 == NOTE_ON && m1 == 48 && m2 > 0) {
		int deck = m2 - 100;
		TRACE("deck switched to %d\n", deck);
	} else if (m0 == NOTE_OFF && m1 == 48) {
		//TRACE("Deck switch\n");
	} else {
		TRACE("Unhandled MIDI message: %s\n", debugMessage.str().c_str());
	}


	
}


int main_nope()
{

	std::unique_ptr<RtMidiOut> midiout;
	std::unique_ptr<RtMidiIn> midiin;

	MidiProcessor midiProcessor;

	// RtMidiIn constructor
	try {
		midiin = std::make_unique<RtMidiIn>();
	}
	catch (RtMidiError& error) {
		TRACE("midiout error: %s", error.getMessage().c_str());
		return 1;
	}
	midiin->setCallback(&OnMidiIn, &midiProcessor);
	// Try find our MidiIn device.
	unsigned int nPorts = midiin->getPortCount();
	TRACE("There are %d MIDI input sources available.\n", nPorts);
	std::string portName;
	for (unsigned int i = 0; i < nPorts; i++) {
		try {
			portName = midiin->getPortName(i);
		}
		catch (RtMidiError& error) {
			TRACE("midiin error: %s", error.getMessage().c_str());
			break;
		}
		TRACE("  Input Port #%d: %s\n", i + 1, portName.c_str());
		if (portName == "loopMIDI Port 1 yay 0") {
			TRACE("    using this one.\n");
			try {
				midiin->openPort(i, portName);
			}
			catch (RtMidiError& error) {
				TRACE("midiin port open error: %s", error.getMessage().c_str());
			}
			//break;
		}
	}
	TRACE("\n");

	// RtMidiOut constructor
	try {
		midiout = std::make_unique<RtMidiOut>();
	}
	catch (RtMidiError& error) {
		error.printMessage();
		return 1;
	}
	// Check outputs.
	nPorts = midiout->getPortCount();
	TRACE("There are %d MIDI output ports available.\n", nPorts);
	for (unsigned int i = 0; i < nPorts; i++) {
		try {
			portName = midiout->getPortName(i);
		}
		catch (RtMidiError& error) {
			TRACE("midiout error: %s", error.getMessage().c_str());
			break;
		}
		TRACE("  Output Port #%d: %s\n", i + 1, portName.c_str());
		if (portName == "X-Touch-Ext 1") {
			TRACE("    using this one.\n");
			try {
				midiout->openPort(i, portName);
			}
			catch (RtMidiError& error) {
				TRACE("midiout port open error: %s", error.getMessage().c_str());
				break;
			}
			//break;
		}
	}
	TRACE("\n");

	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	return 0;

}



// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
