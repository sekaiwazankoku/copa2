# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall

# Target binaries
TARGETS = sender receiver

# Source files
SENDER_SRC = sender.cc udp-socket.cc
RECEIVER_SRC = receiver.cc udp-socket.cc

# Object files
SENDER_OBJ = $(SENDER_SRC:.cc=.o)
RECEIVER_OBJ = $(RECEIVER_SRC:.cc=.o)

# Compile sender
sender: $(SENDER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SENDER_OBJ)

# Compile receiver
receiver: $(RECEIVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(RECEIVER_OBJ)

# Rule to clean up compiled files
clean:
	rm -f $(TARGETS) *.o
