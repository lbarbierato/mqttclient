#if !defined(MQTTSOCKET_H)
#define MQTTSOCKET_H

#include "MQTTmbed.h"
//#include "TCPSocketConnection.h"
#include "sockets/TCPStream.h"
#include "minar/minar.h"
#include "core-util/FunctionPointer.h"
#include "MQTTAsync.h"

char out_success[] = "{{success}}\n{{end}}\n";
class TCPClient;
class MQTTSocket;

using namespace mbed::Sockets::current;
typedef mbed::util::FunctionPointer2<void, bool, TCPClient*> fpterminate_t;
#define TEST_ASSERT_EQUAL_MESSAGE(_a_, _b_, _msg_) if ((_a_) != (_b_)) printf("%s\n", _msg_);

class TCPClient {
    public:
        TCPClient(socket_stack_t stack, mbed::util::FunctionPointer0<void> ptr):_stream(stack), _disconnected(true), ptr(ptr) {
            _stream.setOnError(TCPStream::ErrorHandler_t(this, &TCPClient::onError));
            
        }

        ~TCPClient(){
            disconnect();
        }
        
        void setCallback(MQTT::Async <MQTTSocket, Countdown, DummyThread,DummyMutex> *client){
        	this->client = client;
        }

        bool disconnect() {
            if (_stream.isConnected())
                _stream.close();
            return true;
        }

        void onError(Socket *s, socket_error_t err) {
            (void) s;
            printf("MBED: Socket Error: %s (%d)\r\n", socket_strerror(err), err);
        }

        void onDNS(Socket *s, struct socket_addr sa, const char* domain) {
            (void) s;
            _resolvedAddr.setAddr(&sa);
            /* TODO: add support for getting AF from addr */
            /* Open the socket */
            _resolvedAddr.fmtIPv4(_buffer, sizeof(_buffer));
            printf("MBED: Resolved %s to %s\r\n", domain, _buffer);
            /* Register the read handler */
            _stream.setOnReadable(TCPStream::ReadableHandler_t(this, &TCPClient::onRx));
            _stream.setOnSent(TCPStream::SentHandler_t(this, &TCPClient::onSent));
            _stream.setOnDisconnect(TCPStream::DisconnectHandler_t(this, &TCPClient::onDisconnect));
            /* Send the query packet to the remote host */
            socket_error_t err = _stream.connect(_resolvedAddr, _port, TCPStream::ConnectHandler_t(this,&TCPClient::onConnect));
            TEST_ASSERT_EQUAL_MESSAGE(SOCKET_ERROR_NONE, err, "MBED: Failed to connect host server!");
        }

        void onConnect(TCPStream *s) {
            (void) s;
            _disconnected = false;
            printf ("MBED: connected to host.");
            minar::Scheduler::postCallback(ptr.bind());
        }

        bool connect(char *host_addr, uint16_t port = 1883) {
            _disconnected = true;
            socket_error_t err = _stream.open(SOCKET_AF_INET4);
            _stream.setNagle(false);
            TEST_ASSERT_EQUAL_MESSAGE(SOCKET_ERROR_NONE, err, "MBED: Failed to open socket!");
            if (SOCKET_ERROR_NONE == err) {
                _port = port;
                err = _stream.resolve(host_addr,TCPStream::DNSHandler_t(this, &TCPClient::onDNS));
                TEST_ASSERT_EQUAL_MESSAGE(SOCKET_ERROR_NONE, err, "MBED: Failed to resolve host address!");
                if (SOCKET_ERROR_NONE == err) {
                    return true;
                }
            }
            return false;
        }

        void onRx(Socket* s) {
            (void) s;
            printf("Received something\n");
            if(client!=NULL)
            	client->run(NULL);
        }

        int recv(void * buffer, size_t len) {
            socket_error_t err = _stream.recv(buffer, &len);
            TEST_ASSERT_EQUAL_MESSAGE(SOCKET_ERROR_NONE, err, "MBED: TCPClient failed to recv data!");
            if (SOCKET_ERROR_NONE == err) {
                printf ("MBED: Rx (%d bytes)", len);
                return len;
            }
            return 0;
        }

        void onSent(Socket *s, uint16_t nbytes) {
            (void) s;
            _unacked -= nbytes;
            printf ("MBED: Sent %d bytes", nbytes);
            if (_unacked == 0) {
                printf ("MBED: all sent");
            }
        }

        void onDisconnect(TCPStream *s) {
            (void) s;
            _disconnected = true;
        }

        int send(const void *buff, int len) {
            //_stream.setNagle(false);
            _unacked += len;
            printf ("MBED: Sending (%d bytes) to host: %s", _unacked, out_success);
            socket_error_t err = _stream.send(buff, len);
            printf("Send - MBED: Socket Error: %s (%d)\r\n", socket_strerror(err), err);
            TEST_ASSERT_EQUAL_MESSAGE(SOCKET_ERROR_NONE, err, "MBED: TCPClient failed to send data!");
            if (SOCKET_ERROR_NONE == err) {
                return len;
            }
            return 0;
        }
    protected:
        TCPStream _stream;
        SocketAddr _resolvedAddr;
        char _buffer[1024];
        uint16_t _port;
        volatile bool _disconnected;
        volatile size_t _unacked=0;
	MQTT::Async <MQTTSocket, Countdown, DummyThread,DummyMutex> *client = NULL;
	mbed::util::FunctionPointer0<void> ptr;
};


class MQTTSocket
{
public:    
    int connect(char* hostname, int port, int timeout=1000)
    {
        (void) timeout;
        return mysock.connect(hostname, port);
    }

    int read(unsigned char* buffer, int len, int timeout)
    {
        (void) timeout;
        return mysock.recv(buffer, len);
    }
    
    int write(unsigned char* buffer, int len, int timeout)
    {
        (void) timeout;
        return mysock.send(buffer, len);
    }
    
    int disconnect()
    {
        return mysock.disconnect();
    }
    
    void setCallback(MQTT::Async <MQTTSocket, Countdown, DummyThread,DummyMutex> *client){
    	mysock.setCallback(client);
    }
    
    MQTTSocket(mbed::util::FunctionPointer0<void> ptr):mysock(SOCKET_STACK_LWIP_IPV4, ptr) {
    }
    
private:
    TCPClient mysock;
};


#endif
