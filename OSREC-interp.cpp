#ifdef _WIN64
#include "windows.h"
#endif

/*
 * Copied and modified from OCVWarp.cpp - https://github.com/hn-88/OCVWarp
 * 
 * 
 * Take the camera keyframes from multiple osrectxt files and chain them together with minimal interpolation.
 * https://github.com/OpenSpace/OpenSpace/discussions/3294
 * 
 * first commit:
 * Hari Nandakumar
 * 4 June 2024
 * 
 * 
 */

//#define _WIN64
//#define __unix__


#include <stdio.h>
#include <stdlib.h>

#ifdef __unix__
#include <unistd.h>
#endif

#include <string.h>
#include <fstream>
#include <time.h>
//#include <sys/stat.h>
// this is for mkdir

#include <opencv2/opencv.hpp>
#include "tinyfiledialogs.h"
#define CVUI_IMPLEMENTATION
#include "cvui.h"
#define WINDOW_NAME "OCVWARP - HIT <esc> TO CLOSE"

#define CV_PI   3.1415926535897932384626433832795

using namespace cv;

static const std::string FileHeaderTitle = "OpenSpace_record/playback";
static const std::string HeaderCameraAscii = "camera";
static const std::string HeaderTimeAscii = "time";
static const std::string HeaderScriptAscii = "script";
static const size_t FileHeaderVersionLength = 5;
char FileHeaderVersion[FileHeaderVersionLength+1] = "01.00";
char TargetConvertVersion[FileHeaderVersionLength+1] = "01.00";
static const char DataFormatAsciiTag = 'A';
static const char DataFormatBinaryTag = 'B';
static const size_t keyframeHeaderSize_bytes = 33;
static const size_t saveBufferCameraSize_min = 82;

enum class DataMode {
        Ascii = 0,
        Binary,
        Unknown
    };

enum class SessionState {
        Idle = 0,
        Recording,
        Playback,
        PlaybackPaused
    };

struct Timestamps {
        double timeOs;
        double timeRec;
        double timeSim;
    };
DataMode _recordingDataMode = DataMode::Binary;


std::string readHeaderElement(std::ifstream& stream,
                                                size_t readLenChars)
{
    std::vector<char> readTemp(readLenChars);
    stream.read(readTemp.data(), readLenChars);
    return std::string(readTemp.begin(), readTemp.end());
}

std::string readHeaderElement(std::stringstream& stream,
                                                size_t readLenChars)
{
    std::vector<char> readTemp = std::vector<char>(readLenChars);
    stream.read(readTemp.data(), readLenChars);
    return std::string(readTemp.begin(), readTemp.end());
}

int main(int argc,char *argv[])
{
	std::cout << "This tool takes an initial osrectxt file, appends to it camera keyframes from subsequent osrectxt files with suitable interpolation, and saves as a new osrectxt file.";
	std::cout << std::endl << std::endl;
	std::cout << "https://github.com/OpenSpace/OpenSpace/discussions/3294" << std::endl;
	std::cout << "https://github.com/hn-88/openspace-scripts/wiki" << std::endl;
	std::cout << "https://docs.openspaceproject.com/en/releases-v0.20.0/content/session-recording.html#ascii-file-format" << std::endl;
	std::cout << std::endl;
	
	char const * SaveFileName = "";
	char const * OpenFileName = "";
	char const * FilterPatterns[2] =  { "*.osrec","*.osrectxt" };
	OpenFileName = tinyfd_openFileDialog(
				"Open the initial osrectxt file",
				"",
				2,
				FilterPatterns,
				NULL,
				0);
	// from https://github.com/OpenSpace/OpenSpace/blob/0ff646a94c8505b2b285fdd51800cdebe0dda111/src/interaction/sessionrecording.cpp#L363
	std::string _playbackFilename = OpenFileName;
	std::ifstream _playbackFile;
	std::string _playbackLineParsing;
	std::ofstream _recordFile;
	int _playbackLineNum = 1;

	_playbackFile.open(_playbackFilename, std::ifstream::in);
	// Read header
	const std::string readBackHeaderString = readHeaderElement(
        _playbackFile,
        FileHeaderTitle.length()
    );
    if (readBackHeaderString != FileHeaderTitle) {
        std::cerr << "Specified osrec file does not contain expected header" << std::endl;
        return false;
    }
    readHeaderElement(_playbackFile, FileHeaderVersionLength);
    std::string readDataMode = readHeaderElement(_playbackFile, 1);
    if (readDataMode[0] == DataFormatAsciiTag) {
        _recordingDataMode = DataMode::Ascii;
    }
    else if (readDataMode[0] == DataFormatBinaryTag) {
        _recordingDataMode = DataMode::Binary;
    }
    else {
        std::cerr << "Unknown data type in header (should be Ascii or Binary)";
        return false;
    }
	// throwaway newline character(s)
    std::string lineEnd = readHeaderElement(_playbackFile, 1);
    bool hasDosLineEnding = (lineEnd == "\r");
    if (hasDosLineEnding) {
        // throwaway the second newline character (\n) also
        readHeaderElement(_playbackFile, 1);
    }

    if (_recordingDataMode == DataMode::Binary) {
        // Close & re-open the file, starting from the beginning, and do dummy read
        // past the header, version, and data type
        _playbackFile.close();
        _playbackFile.open(_playbackFilename, std::ifstream::in | std::ios::binary);
        const size_t headerSize = FileHeaderTitle.length() + FileHeaderVersionLength
            + sizeof(DataFormatBinaryTag) + sizeof('\n');
        std::vector<char> hBuffer;
        hBuffer.resize(headerSize);
        _playbackFile.read(hBuffer.data(), headerSize);
    }

    if (!_playbackFile.is_open() || !_playbackFile.good()) {
        std::cerr << std::format(
            "Unable to open file '{}' for keyframe playback", _playbackFilename.c_str()
        ));
        
        return false;
    }
	   
	   
} // end main
