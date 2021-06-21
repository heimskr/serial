COMPILER ?= clang++
OUTPUT   := serial
SOURCES  := main.cpp
OBJECTS  := $(SOURCES:.cpp=.o)

$(OUTPUT): $(OBJECTS)
	$(COMPILER) $^ -o $@

%.o: %.cpp
	$(COMPILER) -c $< -o $@

clean:
	rm -f $(OUTPUT) $(OBJECTS)
