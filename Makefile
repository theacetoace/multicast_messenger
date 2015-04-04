all: tcpserv tcpclnt

tcpclnt: tcpclnt.cpp chat_message.hpp
	g++ tcpclnt.cpp -lboost_system -o tcpclnt --std=c++11 -lpthread -O2\
		-I/usr/local/Cellar/boost/1.57.0/include/ \
		-L/usr/local/Cellar/boost/1.57.0/lib/


tcpserv: tcpserv.cpp chat_message.hpp
	g++ tcpserv.cpp -lboost_system -o tcpserv --std=c++11 -lpthread -O2\
		-I/usr/local/Cellar/boost/1.57.0/include/ \
		-L/usr/local/Cellar/boost/1.57.0/lib/

clean:
	rm tcpserv
	rm tcpclnt
