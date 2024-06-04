#ifdef _WIN64
#include "windows.h"
#endif

/*
 * Copied from OCVWarp.cpp
 * 
 * 
 * testing for an opencv bug
 * https://github.com/opencv/opencv/issues/10694
 * 
 * first commit:
 * Hari Nandakumar
 * 23 May 2024
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


int main(int argc,char *argv[])
{	
	Mat m(960,1280,CV_16U);
	randn(m,10000, 500);
	Scalar M,D;
	meanStdDev(m,M,D);
	std::cout << M(0) << " " << D(0) << std::endl;
	Mat m2 = m.reshape(0,1);
	meanStdDev(m2,M,D);
	std::cout << M(0) << " " << D(0);
		
	std::cout << std::endl << "The above was for randn(m,10000,500)" << std::endl;
	std::cout << "Now testing for 1,000,000 with the bug report code from asitjain" << std::endl;
	std::cout << "a dataset comprising approximately one million 1x1024D vectors. The dataset is normalized within the 0-255 range ..." << std::endl;

	// generate the data
	Mat u8DataMat(1024,1000000,CV_8U);
	randn(u8DataMat,128, 10);
	std::cout << "randn(u8DataMat,128, 10); --> generated data has a mean of 128 and stddev of 10" << std::endl;	

	// Calculate the mean
	cv::Scalar mean = cv::mean(u8DataMat);
	
	// Calculate the standard deviation
	cv::Mat stdmat = u8DataMat - mean[0];
	cv::Mat sqmat;
	cv::multiply(stdmat, stdmat, sqmat);
	
	// Sum the squared differences
	cv::Scalar sum = cv::sum(sqmat);
	double dSum = sum[0];
	double stdDev = dSum / (static_cast<double>(u8DataMat.rows) * u8DataMat.cols);
	stdDev = sqrt(stdDev);
	
	std::cout << "Mean: " << mean[0] << " StdDev: " << stdDev << std::endl;
	std::cout << "The above mean and std dev were as calculated using cv::sum, using static_cast<double> etc." << std::endl;

	meanStdDev(u8DataMat,M,D);
	std::cout << M(0) << " " << D(0) << " This is using meanStdDev(u8DataMat,M,D);" << std::endl;
	Mat fdm2 = u8DataMat.reshape(0,1);
	meanStdDev(fdm2,M,D);
	std::cout << M(0) << " " << D(0) << " This is using meanStdDev(u8DataMat.reshape(0,1),M,D);" << std::endl;
	
    return 0;
	   
	   
} // end main
