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
#include <iomanip>

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
std::string word, timeincrstr;
double timeincr;
std::vector<std::string> words;
std::vector<std::string> prevwords;
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

bool checkIfValidRecFile(std::string _playbackFilename) {
	//////////////////////////////////////////////////
	// from https://github.com/OpenSpace/OpenSpace/blob/0ff646a94c8505b2b285fdd51800cdebe0dda111/src/interaction/sessionrecording.cpp#L363
	// checking the osrectxt format etc
	/////////////////////////
	// std::string _playbackFilename = OpenFileName;
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
	_playbackFile.close();
	return true;
	
}

std::string getLastCameraKfstring(std::string _playbackFilename) {
	std::ifstream _playbackFile;
	_playbackFile.open(_playbackFilename, std::ifstream::in);
	char line[lineBufferForGetlineSize];
	while(_playbackFile.getline(line, lineBufferForGetlineSize)) {
		if(line[0] == 'c') {
			if(line[1] == 'a') {
				if(line[2] == 'm') {
					tempstring = line;
				}
			}
		}
		
	}
	return tempstring;	
}


class CameraKeyFrame {
  public:
	Timestamps ts;
	std::string position;
  
	void incrementOnlyTwoTimestamps(double incrementval) {
		ts.timeOs  += incrementval;
		ts.timeRec += incrementval;
	};
	void incrementAllTimestamps(double incremValtimeRec, double incremValtimeSim) {
		ts.timeOs  += incremValtimeRec;
		ts.timeRec += incremValtimeRec;
		ts.timeSim += incremValtimeSim;
	};
	void setTimestampsFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		//ts.timeSim = kf.ts.timeSim; this should not be set, since we would need it if !ignoreTime
		
	};
	std::string getCamkfAscii() {
		// https://github.com/OpenSpace/OpenSpace/blob/95b4decccad31f7f703bcb8141bd854ba78c7938/src/interaction/sessionrecording.cpp#L839
		std::stringstream ss;
		ss << HeaderCameraAscii << " " << ts.timeOs << " " << ts.timeRec << " " 
			<< std::fixed << std::setprecision(3) << ts.timeSim << " " << position << std::endl;
		return ss.str();
	};
	void populateCamkfAscii(std::string line) {
		std::stringstream ss(line);
		std::string w;
		std::vector<std::string> vwords;
		while(getline(ss, w, ' ')) {
			vwords.push_back(w);
		}
		ts.timeOs  = atof(vwords[1].c_str());
		ts.timeRec = atof(vwords[2].c_str());
		ts.timeSim = atof(vwords[3].c_str());
		position = vwords[4];
		for (int i = 5; i < vwords.size(); i++) {
			position += " " + vwords[i];
		}		
	};	// end populateCamkfAscii
	// copyTo does not work. copyFrom works. Probably due to not passing by reference / passing by value, which would create a temp copy of the var
	// https://www.geeksforgeeks.org/cpp-functions-pass-by-reference/
	// void copyTo(CameraKeyFrame kf) {
	// 	std::cout << "debugging copyTo: LHS timeOS=" << kf.ts.timeOs << " RHS timeOS=" << ts.timeOs << std::endl;
	// 	kf.ts.timeOs  = ts.timeOs;
	// 	std::cout << "after assignment operator: LHS timeOS=" << kf.ts.timeOs << " RHS timeOS=" << ts.timeOs << std::endl;
	// 	kf.ts.timeRec = ts.timeRec;
	// 	kf.ts.timeSim = ts.timeSim;
	// 	kf.position = position.c_str();
	// };
	void copyFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		ts.timeSim = kf.ts.timeSim;
		position = kf.position.c_str();
	};
    
}; // end class CameraKeyFrame

class ScriptKeyFrame {
  public:
	Timestamps ts;
	std::string scr;
  
	void incrementOnlyTwoTimestamps(double incrementval) {
		ts.timeOs  += incrementval;
		ts.timeRec += incrementval;
	};
	void incrementAllTimestamps(double incremValtimeRec, double incremValtimeSim) {
		ts.timeOs  += incremValtimeRec;
		ts.timeRec += incremValtimeRec;
		ts.timeSim += incremValtimeSim;
	};
	void setTimestampsFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		ts.timeSim = kf.ts.timeSim;
	};
	std::string getScrkfAscii() {
		// https://github.com/OpenSpace/OpenSpace/blob/95b4decccad31f7f703bcb8141bd854ba78c7938/src/interaction/sessionrecording.cpp#L839
		std::stringstream ss;
		ss << HeaderScriptAscii << " " << ts.timeOs << " " << ts.timeRec << " " 
			<< std::fixed << std::setprecision(3) << ts.timeSim << " " << scr << std::endl;
		// which should look like
		// script 188.098 0 768100269.188  1 openspace.time.setPause(true)
		// script 188.098 0 768100269.188  1 openspace.time.setDeltaTime(-3600)
		return ss.str();
	};
	
	void setScriptString(std::string s) {
		scr = s.c_str();
	};
    
}; // end class ScriptKeyFrame

CameraKeyFrame prevkf;

int main(int argc,char *argv[])
{
	std::cout << "This tool takes an initial osrectxt file, appends to it camera keyframes from subsequent osrectxt files with suitable interpolation, and saves as a new osrectxt file.";
	std::cout << std::endl << std::endl;
	std::cout << "https://github.com/OpenSpace/OpenSpace/discussions/3294" << std::endl;
	std::cout << "https://github.com/hn-88/openspace-scripts/wiki" << std::endl;
	std::cout << "https://docs.openspaceproject.com/en/releases-v0.20.0/content/session-recording.html#ascii-file-format" << std::endl;
	std::cout << std::endl;
	std::cout << "Documentation is at https://github.com/hn-88/OSREC-interp/wiki" << std::endl;

	char const * SaveFileName = "";
	char const * OpenFileName = "";
	char const * FilterPatterns[2] =  { "*.osrectxt","*.osrec" };
	// adding arrays for saving ini file
	std::string SaveFileNamestr;
	std::string OpenFileNamestr[100];
	std::string timeincrstr[100];
	std::string ignoreTimestr[100];
	int index = 1;
	bool skipinputs = 0;
	std::string tempstring, inistr;
	
	if(argc <= 1) {
		char const * FilterPatternsini[2] =  { "*.ini","*.*" };
		char const * OpenFileNameini;
		
		OpenFileNameini = tinyfd_openFileDialog(
				"Open an ini file if it exists",
				"",
				2,
				FilterPatternsini,
				NULL,
				0);

		if (! OpenFileNameini)
		{
			skipinputs = 0;
		}
		else
		{
			skipinputs = 1;
			inistr = OpenFileNameini;
		}
	}
	    
	if(argc > 1) {
			// argument can be ini file path
			skipinputs = 1;
			inistr = argv[1];
	}
	
	if(skipinputs) {
		std::ifstream infile(inistr);
		if (infile.is_open())
		  {
			  try
			  {
				std::getline(infile, tempstring);
				std::getline(infile, tempstring);
				std::getline(infile, tempstring);
				// first three lines of ini file are comments
				std::getline(infile, SaveFileNamestr);
				// https://stackoverflow.com/questions/6649852/getline-not-working-properly-what-could-be-the-reasons
				// dummy getline after the >> on previous line
				std::getline(infile, tempstring);
				std::getline(infile, OpenFileNamestr[0]);
				int i = 1;
				std::getline(infile, tempstring);				
				while(tempstring.size() > 3)
				{
					std::getline(infile, timeincrstr[i]);
					std::getline(infile, tempstring);
					std::getline(infile, ignoreTimestr[i]);
					std::getline(infile, tempstring);
					std::getline(infile, OpenFileNamestr[i]);
					std::getline(infile, tempstring);
				}
				infile.close();
				skipinputs = 1;
			  } // end try
			  catch(std::ifstream::failure &readErr) 
				{
					std::cerr << "\n\nException occured when reading ini file\n"
						<< readErr.what()
						<< std::endl;
					skipinputs = 0;
				} // end catch
			 
		  } // end if isopen
		  
	  } // end if argc > 1
	
	if(!skipinputs)
	{
	// better to use getline than cin
	// https://stackoverflow.com/questions/4999650/c-how-do-i-check-if-the-cin-buffer-is-empty
	tinyfd_messageBox("Please Note", "ini file not supplied or unreadable. So, manual inputs ...", "ok", "info", 1);
	
	tinyfd_messageBox("Please Note", 
			"First input the destination OSRECTXT file. This tool then takes an initial osrectxt file, appends to it camera keyframes from subsequent osrectxt files with suitable interpolation, and saves to the destination OSRECTXT file.", 
			"ok", "info", 1);
	
	
	SaveFileName = tinyfd_saveFileDialog(
		"Choose the name and path of the destination file",
		"final.osrectxt",
		2,
		FilterPatterns,
		NULL);
	try {
		destfileout.open(SaveFileName, std::ofstream::out );		
	} catch (int) {
		std::cerr << "An error occured creating destination file."<< std::endl ; 
		return false;
	}
	SaveFileNamestr = std::string(SaveFileName);

	
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
	bool validf = checkIfValidRecFile(_playbackFilename);
	if (!validf) {
		return false;
	}
	OpenFileNamestr[0] = std::string(OpenFileName);
	
	//////////////////////////////////////////////////////
	// start the copy from initial osrectxt to destination
	// and also find the last camera keyframe.
	//////////////////////////////////////////////////////

	std::ifstream _playbackFile;
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
	prevkf.populateCamkfAscii(tempstring);

	bool appendAnother, ignoreTime;
	bool ignoreAll=false;
	bool keepSimTimeforAll=false;
	bool donotaskagain=false;
	
	// since the variables used to save to ini file are
	// arrays of size 100, we don't want to cross that number
	// in the while loop
	while(index<99) {
		appendAnother = tinyfd_messageBox(
		"Append next keyframe osrectxt" , 
		"Append one more osrectxt?"  , 
		"yesno" , 
		"question" , 
		1 ) ;

		if (!appendAnother) {
			break;
		}
		// else, append the next keyframe, that is
		// the last camera keyframe of next osrectxt file
		CameraKeyFrame kf;
		char const * lTmp;
		lTmp = tinyfd_inputBox(
			"Please Input", "Desired playback time till next keyframe in seconds", "10.0");
			if (!lTmp) return 1 ;	
		timeincrstr[index] =  std::string(lTmp);
		timeincr = atof(lTmp);
		if (ignoreAll) {
			ignoreTime = true;
		}			
		else {
			ignoreTime = false;
			if (!donotaskagain)
		ignoreTime = tinyfd_messageBox(
			"Ignore simulation time?" , 
			"Ignore the next keyframe's simulation time?"  , 
			"yesno" , 
			"question" , 
			1 ) ;
			
		if(ignoreTime) {
			ignoreTimestr[index]="true";
		} else {
			ignoreTimestr[index]="false";
		}
		
		if(ignoreTime && !donotaskagain) {
			ignoreAll = tinyfd_messageBox("Ignore for all?", 
			"Ignore every keyframe's simulation time during this run? If yes, you won't be asked again. If not, you will be asked for each keyframe.", 
			"yesno", "question", 1);
			if (ignoreAll) donotaskagain = true;
			//script 956.698 0 768100268.890  1 openspace.time.setPause(true)
			std::string scriptstring = "1 openspace.time.setPause(true)";
			ScriptKeyFrame skf;
			skf.setTimestampsFrom(prevkf);
			skf.setScriptString(scriptstring);
			destfileout << skf.getScrkfAscii();
		} else {
			// do not ignore simu time
			if(!ignoreTime && !donotaskagain)
			keepSimTimeforAll = tinyfd_messageBox("Keep Sim Time for all?", 
			"Keep every keyframe's simulation time during this run? If yes, you won't be asked again. If not, you will be asked for each keyframe.", 
			"yesno", "question", 1);
			if (keepSimTimeforAll) {
				donotaskagain = true;				
			}

			std::string scriptstring = "1 openspace.time.setPause(false)";
			ScriptKeyFrame skf;
			skf.setTimestampsFrom(prevkf);
			skf.setScriptString(scriptstring);
			destfileout << skf.getScrkfAscii();			
		}
		}
		OpenFileName = tinyfd_openFileDialog(
				"Open the next osrectxt file",
				"",
				2,
				FilterPatterns,
				NULL,
				0);
		std::string pbFilename = OpenFileName;
		bool validf = checkIfValidRecFile(pbFilename);
		if (!validf) {
			tinyfd_messageBox("Sorry!", 
			"That doesn't seem to be a valid osrec file. Exiting.", 
			"ok", "info", 1);
			return false;
		}
		OpenFileNamestr[index] = pbFilename;
		std::string nextKfstr = getLastCameraKfstring(pbFilename);
		kf.populateCamkfAscii(nextKfstr);
		// set timeOS & timeRec to previous keyframe's values
		kf.setTimestampsFrom(prevkf);
		if (!ignoreTime) {
			double timeRecdiff = timeincr;
			double timeSimdiff = kf.ts.timeSim - prevkf.ts.timeSim;
			int deltatime = (int)(timeSimdiff / timeRecdiff);
			std::string scriptstring = "1 openspace.time.setDeltaTime(" + std::to_string(deltatime) + ")";
			ScriptKeyFrame skf;
			skf.setTimestampsFrom(prevkf);
			skf.setScriptString(scriptstring);
			destfileout << skf.getScrkfAscii();
			
		}
		// increment timeOS & timeRec
		kf.incrementOnlyTwoTimestamps(timeincr);
		// write the kf out to destfile
		destfileout << kf.getCamkfAscii();
		
		// update prevkf
		prevkf.copyFrom(kf);
		index++;
	} // end while loop for new keyframes

	// save parameters as an ini file
	std::string::size_type pAt = SaveFileNamestr.find_last_of('.');                  // Find extension point
	std::string ininame = SaveFileNamestr.substr(0, pAt) + ".ini";   // Form the ini name 
	std::ofstream inioutfile(ininame, std::ofstream::out);
	inioutfile << "#ini_file_for_OSREC-interpv2--Comments_start_with_#" << std::endl;
	inioutfile << "#Each_parameter_is_entered_in_the_line_below_the_comment." << std::endl;
	inioutfile << "#Output_file_path" << std::endl;
	inioutfile << SaveFileNamestr << std::endl;
	inioutfile << "#Initial_osrectxt_file" << std::endl;
	inioutfile << OpenFileNamestr[0] << std::endl;
	int i = 1;
	while (OpenFileNamestr[i].size()>0) {
		inioutfile << "#time_increment" << i << std::endl;
		inioutfile << timeincrstr[i] << std::endl;
		inioutfile << "#ignoreKeyframeCalendarTime_should_be_true_or_false_in_small_case" << i << std::endl;
		inioutfile << ignoreTimestr[i] << std::endl;
		inioutfile << "Appended_osrectxt_file" << i << std::endl;
		inioutfile << OpenFileNamestr[i] << std::endl;
		i++;
	} // end while loop for writing ini file
	
	} // !skipinputs
			   
} // end main
