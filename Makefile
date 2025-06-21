# Standard Makefile for smbc project
# Converted from CMake-generated Makefile

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2
LDFLAGS = 
LIBS = 

# Directories
SRCDIR = source
OBJDIR = obj
BINDIR = .

# Target executable
TARGET = smbc

# Source files
SOURCES = \
	$(SRCDIR)/Configuration.cpp \
	$(SRCDIR)/Emulation/APU.cpp \
	$(SRCDIR)/Emulation/Controller.cpp \
	$(SRCDIR)/Emulation/MemoryAccess.cpp \
	$(SRCDIR)/Emulation/PPU.cpp \
	$(SRCDIR)/Main.cpp \
	$(SRCDIR)/SMB/SMB.cpp \
	$(SRCDIR)/SMB/SMBData.cpp \
	$(SRCDIR)/SMB/SMBEngine.cpp \
	$(SRCDIR)/SMBRom.cpp \
	$(SRCDIR)/Util/Video.cpp \
	$(SRCDIR)/Util/VideoFilters.cpp

# Object files
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Create necessary directories
OBJDIRS = $(OBJDIR) $(OBJDIR)/Emulation $(OBJDIR)/SMB $(OBJDIR)/Util

# Default target
.PHONY: all clean help install

all: $(TARGET)

# Create target executable
$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LIBS)

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIRS)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create object directories
$(OBJDIRS):
	@mkdir -p $@

# Generate assembly files
$(OBJDIR)/%.s: $(SRCDIR)/%.cpp | $(OBJDIRS)
	@echo "Generating assembly for $<..."
	$(CXX) $(CXXFLAGS) -S $< -o $@

# Generate preprocessed files
$(OBJDIR)/%.i: $(SRCDIR)/%.cpp | $(OBJDIRS)
	@echo "Preprocessing $<..."
	$(CXX) $(CXXFLAGS) -E $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning..."
	rm -rf $(OBJDIR)
	rm -f $(TARGET)

# Install target (customize as needed)
install: $(TARGET)
	@echo "Installing $(TARGET)..."
	# Add installation commands here
	# cp $(TARGET) /usr/local/bin/

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build the project (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install the executable"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Individual file targets:"
	@echo "  obj/path/file.o - Compile specific source file"
	@echo "  obj/path/file.s - Generate assembly for specific file"
	@echo "  obj/path/file.i - Generate preprocessed file"

# Dependency tracking (optional - uncomment if you want automatic dependency generation)
# -include $(OBJECTS:.o=.d)

# Rule to generate dependency files
# $(OBJDIR)/%.d: $(SRCDIR)/%.cpp | $(OBJDIRS)
# 	@$(CXX) $(CXXFLAGS) -MM -MT $(@:.d=.o) $< > $@
