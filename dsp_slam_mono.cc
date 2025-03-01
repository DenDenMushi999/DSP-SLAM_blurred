/**
* This file is part of https://github.com/JingwenWang95/DSP-SLAM
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include<iostream>
#include<algorithm>
#include<fstream>
#include<chrono>
#include<iomanip>

#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <Eigen/Dense>
#include <opencv2/core/eigen.hpp>
#include <opencv2/core/core.hpp>

#include"System.h"

using namespace std;

void LoadImages(const string &strSequence, const float &fps, vector<string> &vstrImageFilenames,
                vector<double> &vTimestamps);

int main(int argc, char **argv)
{
    if(argc != 5)
    {
        cerr << endl << "Usage: ./mono_kitti path_to_vocabulary path_to_settings path_to_sequence path_to_saved_trajectory" << endl;
        return 1;
    }

    cv::FileStorage fSettings(string(argv[2]), cv::FileStorage::READ);
    float fps = fSettings["Camera.fps"];

    // Retrieve paths to images
    vector<string> vstrImageFilenames;
    vector<double> vTimestamps;
    LoadImages(string(argv[3]), fps, vstrImageFilenames, vTimestamps);

    int nImages = vstrImageFilenames.size();

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM2::System SLAM(argv[1], argv[2], argv[3], argv[4], ORB_SLAM2::System::MONOCULAR);

    // Vector for tracking time statistics
    vector<float> vTimesTrack;
    vTimesTrack.resize(nImages);

    cout << endl << "-------" << endl;
    cout << "Start processing sequence ..." << endl;
    cout << "Images in the sequence: " << nImages << endl << endl;

    // Main loop
    cv::Mat im;
    const char* save_path = argv[4];
    const char* image_folder_name = "frames/";
    const int name_len = strlen(save_path) + strlen(image_folder_name) + 1;
    char *save_frames_path = new char[name_len];
    // char* save_frames_path = save_path + image_folder_name; 
    
    strcpy(save_frames_path, save_path);
    strcat(save_frames_path, image_folder_name);
    
    if (mkdir(argv[4], 0777) != -1)
        cout << "Map Directory created";
    if (mkdir(save_frames_path, 0777) != -1)
        cout << "Frames Directory created";
    

    for(int ni = 0; ni < nImages; ni++)
    {
        // Read image from file
        im = cv::imread(vstrImageFilenames[ni],CV_LOAD_IMAGE_UNCHANGED);
        double tframe = vTimestamps[ni];

        if(im.empty())
        {
            cerr << endl << "Failed to load image at: " << vstrImageFilenames[ni] << endl;
            return 1;
        }

        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

        // Pass the image to the SLAM system
        SLAM.TrackMonocular(im,tframe);

        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();

        double ttrack= std::chrono::duration_cast<std::chrono::duration<double> >(t2 - t1).count();

        vTimesTrack[ni]=ttrack;

        // Wait to load the next frame
        double T = 0.0;
        if(ni<nImages-1)
            T = vTimestamps[ni+1]-tframe;
        else if(ni>0)
            T = tframe-vTimestamps[ni-1];

        if(ttrack<T)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<size_t>((T- ttrack)*1e6)));
        }

        if (SLAM.GetTrackingState() == ORB_SLAM2::Tracking::OK) {
            // std::cout << string(argv[4]) + "frames" << '\n';

            SLAM.SaveMapCurrentFrame(save_frames_path, ni);
            
            // py::list detections = SLAM.pySequence.attr("get_frame_by_id")(ni);
            // py::list detections = mpSystem->pySequence.attr("get_frame_by_id")(pKF->mnFrameId);

            // int num_dets = detections.size();

            // for (int detected_idx = 0; detected_idx < num_dets; detected_idx++)
            // {
            //     stringstream ss;
            //     ss << setfill('0') << setw(6) << ni;
            //     auto py_det = detections[detected_idx];
            //     auto mask = py_det.attr("mask").cast<Eigen::MatrixXf>();
                
            //     cv::Mat mask_cv;
            //     cv::eigen2cv(mask, mask_cv);
            //     std::cout << string(argv[4]) + "/" + ss.str() + "-mask.png" << '\n';
            //     cv::imwrite(string(argv[4]) + "/" + ss.str() + "-mask.png", mask_cv);
            // }
        }
    }

    SLAM.SaveEntireMap(string(argv[4]));

    cv::waitKey(0);

    // Stop all threads
    SLAM.Shutdown();

    // Tracking time statistics
    sort(vTimesTrack.begin(),vTimesTrack.end());
    float totaltime = 0;
    for(int ni=0; ni<nImages; ni++)
    {
        totaltime+=vTimesTrack[ni];
    }
    cout << "-------" << endl << endl;
    cout << "median tracking time: " << vTimesTrack[nImages/2] << endl;
    cout << "mean tracking time: " << totaltime/nImages << endl;

    return 0;
}

void LoadImages(const string &strPathToSequence, const float &fps, vector<string> &vstrImageFilenames, vector<double> &vTimestamps)
{
    ifstream fTimes;
    string strPathTimeFile = strPathToSequence + "/times.txt";
    fTimes.open(strPathTimeFile.c_str());
    float dt = 1. / fps;
    float t = 0.;
    while(!fTimes.eof())
    {
        string s;
        getline(fTimes,s);
        if(!s.empty())
        {
            vTimestamps.push_back(t);
            t += dt;
        }
    }

    string strPrefixLeft = strPathToSequence + "/image_0/";

    const int nTimes = vTimestamps.size();
    vstrImageFilenames.resize(nTimes);

    for(int i=0; i<nTimes; i++)
    {
        stringstream ss;
        ss << setfill('0') << setw(6) << i;
        vstrImageFilenames[i] = strPrefixLeft + ss.str() + ".png";
        vTimestamps.push_back(t);
        t += dt;
    }
}


