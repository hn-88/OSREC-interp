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
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>
//#include <sys/stat.h>
// this is for mkdir
#include <vector>

#include "tinyfiledialogs.h"
#define CV_PI   3.1415926535897932384626433832795

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
static const size_t lineBufferForGetlineSize = 2560;

std::ofstream destfileout;
std::string tempstring;
std::stringstream tempstringstream;
std::string word;
std::vector<std::string> words;
// https://stackoverflow.com/questions/5607589/right-way-to-split-an-stdstring-into-a-vectorstring
double dvalue[11];
double prevdvalue[11];

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
	char const * FilterPatterns[2] =  { "*.osrectxt","*.osrec" };

	SaveFileName = tinyfd_saveFileDialog(
		"Choose the name and path of the destination file",
		"final.osrectxt",
		2,
		FilterPatterns,
		NULL);
	try {
		destfileout.open(SaveFileName, std::ofstream::out | std::ofstream::app);		
	} catch (int) {
		std::cerr << "An error occured creating destination file."<< std::endl ; 
		return false;
	}

	
	OpenFileName = tinyfd_openFileDialog(
				"Open the initial osrectxt file",
				"",
				2,
				FilterPatterns,
				NULL,
				0);
	//////////////////////////////////////////////////
	// from https://github.com/OpenSpace/OpenSpace/blob/0ff646a94c8505b2b285fdd51800cdebe0dda111/src/interaction/sessionrecording.cpp#L363
	// checking the osrectxt format etc
	/////////////////////////
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
	std::cerr << "Sorry, currently only Ascii data type is supported.";
        return false;
	    
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
        std::cerr << "Unable to open file";        
        return false;
    }
	//////////////////////////////////////////////////////
	// start the copy from initial osrectxt to destination
	// and also find the last camera keyframe.
	//////////////////////////////////////////////////////

	_playbackFile.close();
	_playbackFile.open(_playbackFilename, std::ifstream::in);
	char line[lineBufferForGetlineSize];
	while(_playbackFile.getline(line, lineBufferForGetlineSize)) {
		destfileout << line << std::endl;
		if(line[0] == 'c') {
			if(line[1] == 'a') {
				if(line[2] == 'm') {
					tempstring = line;
				}
			}
		}
		
	}
	// tempstring now contains the last camera keyframe
	// separate it out to get each of the "words" of the line
	// words[0] = the string camera,
	// words[1] = string serialized number of seconds since openspace has been launched
	// etc as in https://docs.openspaceproject.com/en/releases-v0.20.0/content/session-recording.html#ascii-file-format
	std::stringstream ss(tempstring);
	while(getline(ss, word, ' ')) {
		words.push_back(word);
	}
	for (int i = 1; i < 12; i++) {
		prevdvalue[i-1] = atof(words[i].c_str());
	}

	for (int i = 1; i < 12; i++) {
		std::cout << prevdvalue[i-1] << " ";
	}
	
	   
	   
} // end main
