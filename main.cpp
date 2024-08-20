#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <zlib.h>
#include <cstdint>

using namespace std;

struct ChunkLocation {
    int offset;
    int size;
    uint32_t timestamp;
};

// Function to read the entire file into a vector of chars
vector<char> readFile(const string& filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Failed to open file");
    }
    return vector<char>((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

// Function to decompress chunk data using zlib
vector<char> decompressChunk(const vector<char>& data) {
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.data()));
    zs.avail_in = static_cast<uInt>(data.size());

    int ret = inflateInit(&zs);
    if (ret != Z_OK) {
        throw runtime_error("inflateInit failed");
    }

    vector<char> outBuffer(1024 * 1024); // Initial buffer size
    zs.next_out = reinterpret_cast<Bytef*>(outBuffer.data());
    zs.avail_out = outBuffer.size();

    ret = inflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) {
        inflateEnd(&zs);
        throw runtime_error("inflate failed");
    }

    inflateEnd(&zs);
    outBuffer.resize(zs.total_out);

    return outBuffer;
}

// Function to parse the region file header
vector<ChunkLocation> parseHeader(const vector<char>& data) {
    vector<ChunkLocation> chunkLocations;

    for (int i = 0; i < 4096; i += 4) {
        int offset = (static_cast<unsigned char>(data[i]) << 16) |
                     (static_cast<unsigned char>(data[i + 1]) << 8) |
                     static_cast<unsigned char>(data[i + 2]);
        int size = static_cast<unsigned char>(data[i + 3]);
        uint32_t timestamp = (static_cast<unsigned char>(data[4096 + i]) << 24) |
                             (static_cast<unsigned char>(data[4096 + i + 1]) << 16) |
                             (static_cast<unsigned char>(data[4096 + i + 2]) << 8) |
                             static_cast<unsigned char>(data[4096 + i + 3]);

        // Include only valid entries
        if (offset != 0 && size != 0) {
            chunkLocations.push_back({ offset * 4096, size * 4096, timestamp });
        }
    }

    return chunkLocations;
}

int main() {
    try {
        string filename = "r.0.0.mca";
        vector<char> data = readFile(filename);
        vector<ChunkLocation> chunkLocations = parseHeader(data);

        ofstream outputFile("decompressed_chunks.dat", ios::binary);
        if (!outputFile.is_open()) {
            throw runtime_error("Failed to open output file");
        }

        for (const auto& chunkLocation : chunkLocations) {
            if (chunkLocation.offset == 0 || chunkLocation.size == 0) continue;

            // Extract the chunk data from the file
            vector<char> chunkData(data.begin() + chunkLocation.offset, data.begin() + chunkLocation.offset + chunkLocation.size);

            // Get the compressed chunk length and compression type
            int chunkLength = (static_cast<unsigned char>(chunkData[0]) << 24) |
                              (static_cast<unsigned char>(chunkData[1]) << 16) |
                              (static_cast<unsigned char>(chunkData[2]) << 8) |
                              static_cast<unsigned char>(chunkData[3]);

            char compressionType = chunkData[4];

            if (compressionType == 2) {  // Zlib compression
                vector<char> compressedData(chunkData.begin() + 5, chunkData.end());
                vector<char> decompressedData = decompressChunk(compressedData);

                // Write the decompressed NBT data to the output file
                outputFile.write(decompressedData.data(), decompressedData.size());
                outputFile.flush();
            } else {
                cout << "Unsupported compression type at chunk starting at " << chunkLocation.offset << endl;
            }
        }

        outputFile.close();
        cout << "Decompressed chunk data saved to 'decompressed_chunks.dat'" << endl;

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}
