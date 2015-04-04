all: tcpserv tcpclnt

tcpclnt: src/tcpclnt.cpp include/chat_message.hpp
	g++ src/tcpclnt.cpp -lboost_system -o bin/tcpclnt --std=c++11 -lpthread -O2\
		-I/usr/local/Cellar/boost/1.57.0/include/ \
		-L/usr/local/Cellar/boost/1.57.0/lib/


tcpserv: src/tcpserv.cpp include/chat_message.hpp
	g++ src/tcpserv.cpp -lboost_system -o bin/tcpserv --std=c++11 -lpthread -O2\
		-I/usr/local/Cellar/boost/1.57.0/include/ \
		-L/usr/local/Cellar/boost/1.57.0/lib/

clean:
	rm bin/tcpserv
	rm bin/tcpclnt
