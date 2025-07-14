/**
 * rom_to_header.cpp
 * Utility to convert a ROM file to a C++ header and source file containing the ROM data
 * 
 * Usage: rom_to_header input_rom_file output_base_name variable_name
 * Example: rom_to_header "Super Mario Bros.nes" SMBRom smbRomData
 * 
 * This will generate SMBRom.hpp and SMBRom.cpp
 */

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cctype>
#include <vector>
#include <ctime>

// Function to convert a filename to a valid C identifier
std::string fileToIdentifier(const std::string& filename) {
    std::string identifier;
    
    // Get the base filename without extension
    size_t lastSlash = filename.find_last_of("/\\");
    size_t lastDot = filename.find_last_of('.');
    std::string base = filename.substr(
        lastSlash == std::string::npos ? 0 : lastSlash + 1,
        lastDot == std::string::npos ? filename.length() : 
            (lastDot > lastSlash ? lastDot - lastSlash - 1 : lastDot)
    );
    
    // Convert to a valid C identifier
    for (char c : base) {
        if (std::isalnum(c)) {
            identifier += c;
        } else {
            identifier += '_';
        }
    }
    
    // Ensure it starts with a letter
    if (!identifier.empty() && !std::isalpha(identifier[0])) {
        identifier = "rom_" + identifier;
    }
    
    return identifier;
}

// Get current date as a string
std::string getCurrentDate() {
    time_t now = time(0);
    struct tm* timeinfo = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " input_rom_file output_base_name [variable_name]" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputBaseName = argv[2];
    std::string headerFile = outputBaseName + ".hpp";
    std::string sourceFile = outputBaseName + ".cpp";
    std::string variableName = (argc >= 4) ? argv[3] : fileToIdentifier(inputFile) + "_data";
    
    // Open the ROM file for reading in binary mode
    std::ifstream romFile(inputFile, std::ios::binary);
    if (!romFile) {
        std::cerr << "Error: Could not open ROM file: " << inputFile << std::endl;
        return 1;
    }
    
    // Get the size of the file
    romFile.seekg(0, std::ios::end);
    std::streamsize fileSize = romFile.tellg();
    romFile.seekg(0, std::ios::beg);
    
    // Read the entire file into a buffer
    std::vector<unsigned char> buffer(fileSize);
    if (!romFile.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        std::cerr << "Error: Failed to read ROM file" << std::endl;
        return 1;
    }
    
    // Generate a header guard based on the output filename
    std::string headerGuard = fileToIdentifier(headerFile);
    for (char& c : headerGuard) {
        c = std::toupper(c);
    }
    headerGuard += "_HPP";
    
    // Open and write the header file (declarations only)
    std::ofstream headerOut(headerFile);
    if (!headerOut) {
        std::cerr << "Error: Could not create output header file: " << headerFile << std::endl;
        return 1;
    }
    
    headerOut << "#ifndef " << headerGuard << "\n";
    headerOut << "#define " << headerGuard << "\n\n";
    
    headerOut << "#include <cstdint>\n";
    headerOut << "#include <cstddef>\n\n";
    
    // Add file information as a comment
    headerOut << "/**\n";
    headerOut << " * ROM data for: " << inputFile << "\n";
    headerOut << " * Size: " << fileSize << " bytes\n";
    headerOut << " * Generated on: " << getCurrentDate() << "\n";
    headerOut << " */\n\n";
    
    // Write the array size constant
    headerOut << "constexpr size_t " << variableName << "_size = " << fileSize << ";\n\n";
    
    // Declare the ROM data array but don't define it
    headerOut << "extern const uint8_t " << variableName << "[" << variableName << "_size];\n\n";
    
    headerOut << "#endif // " << headerGuard << "\n";
    headerOut.close();
    
    // Open and write the source file (definitions)
    std::ofstream sourceOut(sourceFile);
    if (!sourceOut) {
        std::cerr << "Error: Could not create output source file: " << sourceFile << std::endl;
        return 1;
    }
    
    sourceOut << "#include \"" << headerFile << "\"\n\n";
    
    // Define the ROM data array
    sourceOut << "const uint8_t " << variableName << "[" << variableName << "_size] = {\n";
    
    // Format the data as a hex array with comments for offsets
    const int BYTES_PER_LINE = 16;
    for (std::streamsize i = 0; i < fileSize; ++i) {
        if (i % BYTES_PER_LINE == 0) {
            // Add an offset comment at the start of each line
            sourceOut << "    /* 0x" << std::hex << std::setw(8) << std::setfill('0') << i << " */ ";
        }
        
        // Write the byte value in hex
        sourceOut << "0x" << std::hex << std::setw(2) << std::setfill('0') 
                 << static_cast<int>(buffer[i]);
        
        // Add a comma if this is not the last byte
        if (i < fileSize - 1) {
            sourceOut << ", ";
        }
        
        // End the line after BYTES_PER_LINE bytes or at the end of the array
        if ((i + 1) % BYTES_PER_LINE == 0 || i == fileSize - 1) {
            sourceOut << "\n";
        }
    }
    
    sourceOut << "};\n";
    sourceOut.close();
    
    std::cout << "Successfully converted ROM file to header and source:\n";
    std::cout << "  Input: " << inputFile << " (" << fileSize << " bytes)\n";
    std::cout << "  Output Header: " << headerFile << "\n";
    std::cout << "  Output Source: " << sourceFile << "\n";
    std::cout << "  Variable: " << variableName << "[" << fileSize << "]\n";
    
    return 0;
}
