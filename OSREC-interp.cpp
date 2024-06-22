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

struct CameraPos {
	double xpos;
	double ypos;
	double zpos;
	double xrot;
	double yrot;
	double zrot;
	double wrot;
	double scale;	
};

struct XYZ {
	double x;
	double y;
	double z;
};

struct RThetaPhi {
	double r;
	double theta;
	double phi;
};

RThetaPhi toSpherical (XYZ p) {
	// https://gamedev.stackexchange.com/questions/66906/spherical-coordinate-from-cartesian-coordinate
	// https://neutrium.net/mathematics/converting-between-spherical-and-cartesian-co-ordinate-systems/ has the wrong formula
	// https://mathworld.wolfram.com/SphericalCoordinates.html
	RThetaPhi a;
	a.r 		= std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
	a.phi 		= std::acos(p.z / a.r);
	a.theta 	= std::atan2(p.y, p.x);	
	return a;
}

XYZ toCartesian (RThetaPhi a) {
	// https://neutrium.net/mathematics/converting-between-spherical-and-cartesian-co-ordinate-systems/
	XYZ p;
	p.x = a.r * std::sin(a.phi) * std::cos(a.theta);
	p.y = a.r * std::sin(a.phi) * std::sin(a.theta);
	p.z = a.r * std::cos(a.phi);
	return p;
}

class CameraKeyFrame {
  public:
	Timestamps ts;
	CameraPos pos;
	std::string fstring;
  
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
		// https://github.com/OpenSpace/OpenSpace/blob/807699a8902fcb9368a495650b99d537fea8dd0e/src/interaction/sessionrecording.cpp#L820
		std::stringstream ss;
		ss << HeaderCameraAscii << " " << ts.timeOs << " " << ts.timeRec << " " 
			<< std::fixed << std::setprecision(3) << ts.timeSim << " " ;
		// << "position" << std::endl;
		// https://github.com/OpenSpace/OpenSpace/blob/807699a8902fcb9368a495650b99d537fea8dd0e/include/openspace/network/messagestructures.h#L190
		ss << std::setprecision(std::numeric_limits<double>::max_digits10);
		ss << pos.xpos << ' ' << pos.ypos << ' ' << pos.zpos << ' ';
	        // Add camera rotation
	        ss << pos.xrot << ' '
	            << pos.yrot << ' '
	            << pos.zrot << ' '
	            << pos.wrot << ' ';
	        ss << std::scientific << pos.scale;
		ss << ' ' << fstring << std::endl;			
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

		pos.xpos	= atof(vwords[4].c_str());
		pos.ypos	= atof(vwords[5].c_str());
		pos.zpos	= atof(vwords[6].c_str());

		pos.xrot	= atof(vwords[7].c_str());
		pos.yrot	= atof(vwords[8].c_str());
		pos.zrot	= atof(vwords[9].c_str());
		pos.wrot	= atof(vwords[10].c_str());
		pos.scale	= atof(vwords[11].c_str());
		
		fstring = vwords[12];
		for (int i = 13; i < vwords.size(); i++) {
			fstring += " " + vwords[i];
		}		
	};	// end populateCamkfAscii
	
	void copyFrom(CameraKeyFrame kf) {
		ts.timeOs  = kf.ts.timeOs;
		ts.timeRec = kf.ts.timeRec;
		ts.timeSim = kf.ts.timeSim;
		pos.xpos = kf.pos.xpos;
		pos.ypos = kf.pos.ypos;
		pos.zpos = kf.pos.zpos;
		pos.xrot = kf.pos.xrot;
		pos.yrot = kf.pos.yrot;
		pos.zrot = kf.pos.zrot;
		pos.wrot = kf.pos.wrot;
		pos.scale = kf.pos.scale;
			
		fstring = kf.fstring.c_str();
	};
	CameraPos getCameraPos() {
		return pos;
	};
	void setCameraPos(CameraPos p) {
		// set the postition string from the data in struct pos 
		pos = p;
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

void interpolatebetween(CameraKeyFrame kf1, CameraKeyFrame kf2) {
	// optional - add a check to make sure kf1 and kf2 have the same object in Focus
	// add 99 points between kf1 and kf2 equi-spaced in time and spherical co-ords 

	// first find the increments for each of the coords
	
	double xrotincr = (kf2.pos.xrot - kf1.pos.xrot) / 100;
	double yrotincr = (kf2.pos.yrot - kf1.pos.yrot) / 100;
	double zrotincr = (kf2.pos.zrot - kf1.pos.zrot) / 100;
	double wrotincr = (kf2.pos.wrot - kf1.pos.wrot) / 100;
	double scaleincr = (kf2.pos.scale - kf1.pos.scale) / 100;

	double timeincr = (kf2.ts.timeOs - kf1.ts.timeOs) / 100;
	double timeSimincr = (kf2.ts.timeSim - kf1.ts.timeSim) / 100;

	XYZ p1, p2;
	RThetaPhi a1, a2;
	p1.x = kf1.pos.xpos;
	p1.y = kf1.pos.ypos;
	p1.z = kf1.pos.zpos;
	p2.x = kf2.pos.xpos;
	p2.y = kf2.pos.ypos;
	p2.z = kf2.pos.zpos;
	a1 = toSpherical(p1);
	a2 = toSpherical(p2);
	double rincr = (a2.r - a1.r) / 100;
	double thetaincr = (a2.theta - a1.theta) / 100;
	double phiincr = (a2.phi - a1.phi) / 100;

	// now loop through the intermediate points
	CameraKeyFrame ikf;
	XYZ p;
	RThetaPhi a;
	ikf.copyFrom(kf1);
	
	for (int i = 1; i < 100; i++) {
		// add suitable increment to interpolated CameraKeyFrame
		ikf.incrementAllTimestamps(timeincr, timeSimincr);
		
		p.x = ikf.pos.xpos;
		p.y = ikf.pos.ypos;
		p.z = ikf.pos.zpos;
		a = toSpherical(p);
		a.r += rincr;
		a.theta += thetaincr;
		a.phi += phiincr;
		p = toCartesian(a);
		ikf.pos.xpos = p.x;
		ikf.pos.ypos = p.y;
		ikf.pos.zpos = p.z;
		ikf.pos.xrot += xrotincr;
		ikf.pos.yrot += yrotincr;
		ikf.pos.zrot += zrotincr;
		ikf.pos.wrot += wrotincr;
		ikf.pos.scale += scaleincr;
		
		destfileout << ikf.getCamkfAscii();
	}
	
}

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

	tinyfd_messageBox("Please Note", 
			"First input the destination OSRECTXT file. This tool then takes an initial osrectxt file, appends to it camera keyframes from subsequent osrectxt files with suitable interpolation, and saves to the destination OSRECTXT file.", 
			"ok", "info", 1);
	
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
		destfileout.open(SaveFileName, std::ofstream::out );		
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
	bool validf = checkIfValidRecFile(_playbackFilename);
	if (!validf) {
		return false;
	}
	
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
	while(true) {
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
			"Please Input", "Desired playback time till next keyframe in seconds", "100.0");
			if (!lTmp) return 1 ;	
		timeincrstr =  lTmp;
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

		// test interpolatebetween
		interpolatebetween(prevkf, kf);
		
		// write the kf out to destfile
		destfileout << kf.getCamkfAscii();
		
		// update prevkf
		prevkf.copyFrom(kf);
		
	} // end while loop for new keyframes
	   
} // end main
