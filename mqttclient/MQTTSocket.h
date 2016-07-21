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
        TCPClient(socket_stack_t stack, mbed::util::FunctionPointer0<void> connCallback, mbed::util::FunctionPointer0<void> discCallback):_stream(stack), _disconnected(true), connectionCallback(connCallback), disconnectionCallback(discCallback) {
            
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
            _stream.close();
            _disconnected=true;
            minar::Scheduler::postCallback(disconnectionCallback.bind());
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
            printf("Trying to connect - MBED: Socket Error: %s (%d)\r\n", socket_strerror(err), err);
            if(err!= SOCKET_ERROR_NONE)
            	onError(s,err);
        }

        void onConnect(TCPStream *s) {
            (void) s;
            _disconnected = false;
            printf ("MBED: connected to host.");
            minar::Scheduler::postCallback(connectionCallback.bind());
            
        }

        bool connect(char *host_addr, uint16_t port = 1883) {
        	printf("Resolving hostname %s\n", host_addr);
            _disconnected = true;
            socket_error_t err = _stream.open(SOCKET_AF_INET4);
            printf("Connect - MBED: Socket Error: %s (%d)\r\n", socket_strerror(err), err);
            _stream.setNagle(false);
            _stream.setOnError(TCPStream::ErrorHandler_t(this, &TCPClient::onError));
            TEST_ASSERT_EQUAL_MESSAGE(SOCKET_ERROR_NONE, err, "MBED: Failed to open socket!");
            if (SOCKET_ERROR_NONE == err) {
                _port = port;
                printf("%s, port: %d", host_addr, port);
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
            if(client!=NULL){
                //mbed::util::FunctionPointer1<void, void const*> cycleptr(client, &MQTT::Async<MQTTSocket, Countdown, DummyThread, DummyMutex>::run);
                //minar::Scheduler::postCallback(cycleptr.bind(NULL));
            	client->run(NULL);
            }
        }

        int recv(void * buffer, size_t len) {
            socket_error_t err = _stream.recv(buffer, &len);
            printf("Recv - MBED: Socket Error: %s (%d)\r\n", socket_strerror(err), err);
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
            _stream.close();
            minar::Scheduler::postCallback(disconnectionCallback.bind());
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
            else if(err == SOCKET_ERROR_BAD_ALLOC || err == SOCKET_ERROR_NO_CONNECTION){
            	_stream.close();
            	minar::Scheduler::postCallback(disconnectionCallback.bind());
            	return -1;
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
	mbed::util::FunctionPointer0<void> connectionCallback, disconnectionCallback;
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
    
    MQTTSocket(mbed::util::FunctionPointer0<void> connectionCallback, mbed::util::FunctionPointer0<void> disconnectionCallback):mysock(SOCKET_STACK_LWIP_IPV4, connectionCallback, disconnectionCallback) {
    }
    
private:
    TCPClient mysock;
};


#endif
