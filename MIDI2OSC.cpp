#include "rtmidi.h"

#include <iostream>
#include <atltrace.h>
#include <string>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <thread>

#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

//#include "ip/win32/NetworkingUtils.cpp"
//#include "ip/win32/UdpSocket.cpp"

#define TRACE ATLTRACE

//#define ADDRESS "localhost"
#define ADDRESS_LOCALHOST "127.0.0.1"
//#define ADDRESS "2601:647:ce00:bce:a9cd:29d4:5e05:a276"
//#define ADDRESS 
#define ADDRESS_BUN "174.160.84.25"

#define PORT 6969
#define OUTPUT_BUFFER_SIZE 1024

class UdpTransmitSocketMultiplex {
public:
	UdpTransmitSocket a;
	UdpTransmitSocket b;
	UdpTransmitSocketMultiplex(UdpTransmitSocket a_, UdpTransmitSocket b_) : a(a_), b(b_) {}

	void Send(const char* data, std::size_t size) {
		a.Send(data, size);
		b.Send(data, size);
	}
};


struct MidiProcessorContext {
	UdpTransmitSocket* bunSocket;
	UdpTransmitSocket* domSocket;
	int toggle_each_measure;
	int current_deck;
};

void OnMidiIn(double timeStamp, std::vector<unsigned char>* message, void* userData) {
	std::stringstream debugMessage;
	std::copy(message->begin(), message->end(), std::ostream_iterator<int>(debugMessage, " "));
	//osc::OutboundPacketStream *p = static_cast<osc::OutboundPacketStream*>(userData);
	// 
	//MidiProcessor* this_ = static_cast<MidiProcessor*>(userData);
	//IMidiControllerEventReceiver* eventReceiver = this_->eventReceiver;
	//IMidiControllerEventReceiver* eventReceiver = static_cast<IMidiControllerEventReceiver*> (userData);

	if (message->size() != 3) {
		TRACE("Got a message with weird length: %s\n", debugMessage.str().c_str());
		return;
	}

	MidiProcessorContext* context = static_cast<MidiProcessorContext*>(userData);
	UdpTransmitSocket* transmitSocket = context->transmitSocket; //static_cast<UdpTransmitSocket*>(userData);
	char buffer[OUTPUT_BUFFER_SIZE];
	osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);



	// https://github.com/mixxxdj/mixxx/wiki/MIDI%20clock%20output
	unsigned char m0 = message->at(0);
	unsigned char m1 = message->at(1);
	unsigned char m2 = message->at(2);

	int NOTE_ON = 144;
	int NOTE_OFF = 128;

	if (m0 == NOTE_ON && m1 == 52) {
		int bpm = m2 + 50;
		TRACE("BPM: %d\n", bpm);
		p << osc::BeginBundleImmediate
			<< osc::BeginMessage("/BPM_int")
			<< bpm << osc::EndMessage
			<< osc::EndBundle;
	}
	else if (m0 == NOTE_ON && m1 == 48 && m2 == 0) {
	} else if (m0 == NOTE_ON && m1 == 50 && m2 == 100) {
		TRACE("beat\n");
		context->toggle_each_measure = 1 - context->toggle_each_measure;
		p << osc::BeginBundleImmediate
			<< osc::BeginMessage("/toggle_each_measure")
			<< context->toggle_each_measure
			<< osc::EndMessage
			<< osc::EndBundle;

	} else if (m0 == NOTE_ON && m1 == 50 && m2 == 0) {
	} else if (m0 == NOTE_OFF && m1 == 50) {
	} else if (m0 == NOTE_ON && m1 == 48 && m2 > 0) {
		int deck = m2 - 100;
		TRACE("deck switched to %d\n", deck);
		p << osc::BeginBundleImmediate
			<< osc::BeginMessage("/active_deck")
			<< deck << osc::EndMessage;
		if (deck != context->current_deck) {
			p << osc::BeginMessage("/deck_pulse")
				<< 1 << osc::EndMessage;
			p << osc::BeginMessage("/deck_pulse")
				<< 0 << osc::EndMessage;
			context->current_deck = deck;
		}

		p << osc::EndBundle;
	} else if (m0 == NOTE_OFF && m1 == 48) {
		//TRACE("Deck switch\n");
	} else {
		TRACE("Unhandled MIDI message: %s\n", debugMessage.str().c_str());
	}

	if (p.Size() > 0) {
		bunSocket->Send(p.Data(), p.Size());
		domSocket->Send(p.Data(), p.Size());
	}
}



int main()
{

	UdpTransmitSocket bunSocket(IpEndpointName(ADDRESS_BUN, PORT));
	UdpTransmitSocket domSocket(IpEndpointName(ADDRESS_LOCALHOST, PORT));

	std::unique_ptr<RtMidiOut> midiout;
	std::unique_ptr<RtMidiIn> midiin;

	MidiProcessorContext context{&bunSocket, &domSocket 0, -1};

	// RtMidiIn constructor
	try {
		midiin = std::make_unique<RtMidiIn>();
	}
	catch (RtMidiError& error) {
		TRACE("midiout error: %s", error.getMessage().c_str());
		return 1;
	}
	midiin->setCallback(&OnMidiIn, &context);
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
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		/*
		char buffer[OUTPUT_BUFFER_SIZE];
		osc::OutboundPacketStream p(buffer, OUTPUT_BUFFER_SIZE);
		p << osc::BeginBundleImmediate
			<< osc::BeginMessage("/test")
			<< osc::EndMessage
			<< osc::EndBundle;
		transmitSocket.Send(p.Data(), p.Size());
		std::cout << "sending /test\n";
		*/
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
